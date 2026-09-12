#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Select PR or full CI coverage, falling back to full on incomplete evidence."""

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
    {"name": "latx-runner-aosc", "tag": "loong64", "sanitizers": False,
     "prepare_meson": "command -v meson >/dev/null || oma install -y meson"},
    {"name": "latx-runner-debian", "tag": "loong64", "sanitizers": True,
     "prepare_meson": "command -v meson >/dev/null || { apt-get update && apt-get install -y meson; }"},
    {"name": "latx-runner-fedora", "tag": "loongarch64", "sanitizers": False,
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


def needs_full_matrix(files):
    for item in files:
        for name in (item["filename"], item.get("previous_filename", "")):
            if (name in BUILD_FILES or name.startswith(BUILD_PREFIXES)
                    or name.endswith(("/meson.build", ".mak"))):
                return True
        # GitHub may omit patches for large diffs or binary files. Do not
        # silently narrow coverage when the diff cannot be inspected.
        patch = item.get("patch")
        if patch is None or PREPROCESSOR_CHANGE.search(patch):
            return True
        for field, prefix in (("additions", "+"), ("deletions", "-")):
            if field in item and sum(line.startswith(prefix) for line in
                                     patch.splitlines()) < item[field]:
                return True
    return False


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


def select_matrix(event_name, event, token="", fetch_files=pull_request_files):
    if event_name != "pull_request":
        return True, "Full coverage for master, scheduled, release or manual runs"
    pr = event["pull_request"]
    try:
        files = fetch_files(event["repository"]["full_name"], pr["number"],
                            token, pr["changed_files"])
        full = needs_full_matrix(files)
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
