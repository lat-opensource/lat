#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Select PR or full CI coverage, falling back to full on incomplete evidence."""

import base64
import hashlib
import json
import os
import re
import urllib.request
from pathlib import Path


BUILD_TYPES = [
    {"NAME": "build-release", "OPT": ""},
    {"NAME": "build32", "OPT": "-c -a"},
    {"NAME": "build32-dbg", "OPT": "-c"},
    {"NAME": "build64", "OPT": "-c -a"},
    {"NAME": "build64-dbg", "OPT": "-c"},
]
TEST_CONTAINERS = [
    {"name": "latx-runner-aosc",
     "dockerfile": ".github/.ci/aosc/Dockerfile",
     "tag": "loong64", "sanitizers": False,
     "prepare_meson": "command -v meson >/dev/null || oma install -y meson"},
    {"name": "latx-runner-debian",
     "dockerfile": ".github/.ci/debian/Dockerfile",
     "tag": "loong64", "sanitizers": True,
     "prepare_meson": "command -v meson >/dev/null || { apt-get update && apt-get install -y meson; }"},
    {"name": "latx-runner-fedora",
     "dockerfile": ".github/.ci/fedora/Dockerfile",
     "tag": "loongarch64", "sanitizers": False,
     "prepare_meson": "command -v meson >/dev/null"},
]
BUILD_FILES = {
    "configure", "meson.build", "meson.options", "meson_options.txt",
    ".gitmodules", "Makefile", "GNUmakefile", "meson",
}
BUILD_PREFIXES = (
    ".github/.ci/", ".github/workflows/", "latxbuild/", "configs/",
    "scripts/ci/", "tests/ci/", "runtime/", "tests/runtime/", "meson/",
)
PREPROCESSOR_CHANGE = re.compile(
    r"^[+-]\s*#\s*(?:if|ifdef|ifndef|elif|else|endif|define|undef)\b",
    re.MULTILINE,
)
MESON_TOKEN = re.compile(
    r"\s+|\#[^\n]*|'''[\s\S]*?'''|'(?:\\[^\n]|[^'\\\n])*'|"
    r"[A-Za-z_][A-Za-z_0-9]*|[0-9]+|==|!=|<=|>=|\+=|[-+*/%=,:.()\[\]{}<>]"
)


def meson_tokens(source):
    """Lex without evaluating Meson; reject syntax we do not understand."""
    tokens = []
    offset = 0
    while offset < len(source):
        match = MESON_TOKEN.match(source, offset)
        if not match:
            raise ValueError("Unrecognized Meson token")
        token = match.group()
        if not token.isspace() and not token.startswith('#'):
            tokens.append(token)
        offset = match.end()
    return tokens


def meson_top_level(tokens):
    brackets, blocks = [], []
    for token in tokens:
        if token in ('(', '[', '{'):
            brackets.append(token)
        elif token in (')', ']', '}'):
            if not brackets or brackets.pop() != {')': '(', ']': '[', '}': '{'}[token]:
                return False
        elif not brackets:
            if token in ('if', 'foreach'):
                blocks.append(token)
            elif token in ('endif', 'endforeach'):
                if not blocks or blocks.pop() != {'endif': 'if', 'endforeach': 'foreach'}[token]:
                    return False
    return (not brackets and not blocks and
            (not tokens or tokens[-1] not in ('=', '+=', '+', '-', '*', '/', ',', '.', ':', '\\')))


def fast_test_calls(source):
    """Recognize only standalone test() calls and side-effect-free arguments.

    This deliberately is not a general Meson parser. Assignments, conditionals,
    methods and calls such as run_command()/custom_target() keep full coverage.
    """
    tokens = meson_tokens(source)
    position = 0

    def take(expected=None):
        nonlocal position
        if position == len(tokens):
            raise ValueError("Incomplete test registration")
        token = tokens[position]
        position += 1
        if expected is not None and token != expected:
            raise ValueError("Unexpected test registration token")
        return token

    def value():
        token = take()
        if token == '[':
            while tokens[position] != ']':
                value()
                if tokens[position] != ']':
                    take(',')
            take(']')
        elif re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*', token):
            if position < len(tokens) and tokens[position] == '(':
                if token not in ('files', 'find_program'):
                    raise ValueError("Non-registration Meson call")
                take('(')
                while tokens[position] != ')':
                    value()
                    if tokens[position] != ')':
                        take(',')
                take(')')
            elif position < len(tokens) and tokens[position] == '[':
                take('[')
                if not take().startswith("'"):
                    raise ValueError("Nonliteral index")
                take(']')
        elif not (token.startswith("'") or token.isdecimal()):
            raise ValueError("Non-registration Meson expression")

    calls = 0
    while position < len(tokens):
        take('test')
        take('(')
        if not take().startswith("'"):
            return False
        take(',')
        value()
        keywords = set()
        fast_suite = False
        while tokens[position] != ')':
            take(',')
            if tokens[position] == ')':
                break
            key = take()
            if key in keywords or key not in (
                    'args', 'suite', 'timeout', 'depends', 'env', 'is_parallel',
                    'priority', 'protocol', 'should_fail', 'workdir'):
                return False
            keywords.add(key)
            take(':')
            if key == 'suite':
                fast_suite = tokens[position] == "'lat-pr-fast'"
            value()
        take(')')
        if not fast_suite:
            return False
        calls += 1
    return calls > 0


def appended_fast_tests(item, read_file):
    """Exempt a verified EOF append to the existing unit-test registrar only."""
    if (item['filename'] != 'tests/unit/meson.build' or
            item.get('status') != 'modified' or item.get('previous_filename') or
            item.get('deletions') != 0 or not item.get('additions') or
            not re.fullmatch(r'[0-9a-f]{40}', item.get('sha', '')) or read_file is None):
        return False
    lines = item['patch'].splitlines()
    header = re.fullmatch(r'@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@.*', lines[0])
    if not header or any(not line.startswith((' ', '+')) for line in lines[1:]):
        return False
    old_start, old_count, new_start, new_count = (int(x) if x is not None else 1
                                                for x in header.groups())
    first_added = next(i for i, line in enumerate(lines) if line.startswith('+'))
    if any(not line.startswith('+') for line in lines[first_added:]):
        return False
    if (old_start != new_start or old_count != first_added - 1 or
            new_count != len(lines) - 1 or
            len(lines) - first_added != item['additions']):
        return False
    source = read_file(item).splitlines()
    if (new_start < 1 or new_start - 1 + new_count != len(source) or
            source[new_start - 1:] != [line[1:] for line in lines[1:]]):
        return False
    added = '\n'.join(line[1:] for line in lines[first_added:])
    prefix = '\n'.join(source[:len(source) - item['additions']])
    return meson_top_level(meson_tokens(prefix)) and fast_test_calls(added)


def test_script(name):
    return (name.startswith(('tests/unit/', 'tests/integration/')) and
            Path(name).suffix in ('.py', '.sh'))


def needs_full_matrix(files, read_file=None):
    for item in files:
        # GitHub may omit patches for large diffs or binary files. Do not
        # silently narrow coverage when the diff cannot be inspected.
        patch = item.get("patch")
        if patch is None:
            return True
        for field, prefix in (("additions", "+"), ("deletions", "-")):
            if field in item and sum(line.startswith(prefix) for line in
                                     patch.splitlines()) < item[field]:
                return True
        names = [item['filename']]
        if item.get('previous_filename'):
            names.append(item['previous_filename'])
        if any(name in BUILD_FILES or name.startswith(BUILD_PREFIXES) or
               name.endswith(('/meson.build', '.mak')) for name in names):
            try:
                if not appended_fast_tests(item, read_file):
                    return True
            except (ValueError, IndexError, StopIteration, OSError, KeyError, RecursionError):
                return True
        # Embedded fixture C in test scripts does not change product feature
        # guards. Product generators and C/assembly tests remain conservative.
        if PREPROCESSOR_CHANGE.search(patch) and not all(test_script(name) for name in names):
            return True
    return False


def read_blob(repository, sha, token, opener=urllib.request.urlopen):
    request = urllib.request.Request(
        f'https://api.github.com/repos/{repository}/git/blobs/{sha}',
        headers={'Authorization': f'Bearer {token}',
                 'Accept': 'application/vnd.github+json',
                 'X-GitHub-Api-Version': '2022-11-28'})
    with opener(request, timeout=30) as response:
        blob = json.load(response)
    if blob['encoding'] != 'base64' or blob['size'] > 1024 * 1024:
        raise ValueError('Unsupported Meson blob')
    content = base64.b64decode(''.join(blob['content'].split()), validate=True)
    identity = hashlib.sha1(f'blob {len(content)}\0'.encode() + content).hexdigest()
    if len(content) != blob['size'] or identity != sha:
        raise ValueError('Meson blob identity mismatch')
    return content.decode('utf-8')


def pull_request_files(repository, number, token, expected_count,
                       opener=urllib.request.urlopen):
    # The list-files endpoint returns at most 3,000 files.
    if expected_count >= 3000:
        raise ValueError("PR file list may exceed the GitHub API limit")
    files = []
    for page in range(1, 31):
        request = urllib.request.Request(
            f"https://api.github.com/repos/{repository}/pulls/{number}/files"
            f"?per_page=100&page={page}",
            headers={"Authorization": f"Bearer {token}",
                     "Accept": "application/vnd.github+json",
                     "X-GitHub-Api-Version": "2022-11-28"},
        )
        with opener(request, timeout=30) as response:
            batch = json.load(response)
        files.extend(batch)
        if len(batch) < 100:
            break
    if len(files) != expected_count:
        raise ValueError("PR file list is incomplete or changed during selection")
    return files


def select_matrix(event_name, event, token="", fetch_files=pull_request_files,
                  fetch_blob=read_blob):
    if event_name != "pull_request":
        return True, "Full coverage for master, scheduled, release or manual runs"
    pr = event["pull_request"]
    try:
        files = fetch_files(event["repository"]["full_name"], pr["number"],
                            token, pr["changed_files"])
        full = needs_full_matrix(files, read_file=lambda item: fetch_blob(
            event['repository']['full_name'], item['sha'], token))
    except Exception:
        # Fail closed without logging API credentials or response bodies.
        return True, "Full coverage: PR changes could not be inspected completely"
    return full, ("Full coverage: build, runtime or preprocessor changes"
                  if full else "PR coverage: non-debug builds and Debian sanitizers")


def matrix_outputs(full):
    return {
        "build_types": [entry for entry in BUILD_TYPES if full or
                        entry["NAME"] in ("build32", "build64")],
        "test_containers": [entry for entry in TEST_CONTAINERS if full or
                            entry["sanitizers"]],
    }


def main():
    event = json.loads(Path(os.environ["GITHUB_EVENT_PATH"]).read_text())
    full, reason = select_matrix(os.environ["GITHUB_EVENT_NAME"], event,
                                 os.environ.get("GITHUB_TOKEN", ""))
    with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
        for key, value in matrix_outputs(full).items():
            output.write(f"{key}={json.dumps(value, separators=(',', ':'))}\n")
    print(reason)


if __name__ == "__main__":
    main()
