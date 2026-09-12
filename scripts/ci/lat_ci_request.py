#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Resolve a manually approved upstream LAT pull request safely."""

import argparse
import json
import os
import re
import urllib.request


EXPECTED_TARGET_REPOSITORY = "lat-opensource/lat"
SHA_PATTERN = re.compile(r"[0-9a-f]{40,64}", re.IGNORECASE)
POSITIVE_INTEGER_PATTERN = re.compile(r"[1-9][0-9]*")


class LatCIRequestError(ValueError):
    """The manually approved pull request cannot be dispatched safely."""


def _one_line_string(value, name):
    if not isinstance(value, str) or not value.strip() or "\n" in value or "\r" in value:
        raise LatCIRequestError(f"{name} is invalid")
    return value


def _sha(value, name):
    if not isinstance(value, str) or not SHA_PATTERN.fullmatch(value):
        raise LatCIRequestError(f"{name} must be a Git commit SHA")
    return value.lower()


def get_json(path, token, opener=urllib.request.urlopen):
    """Read GitHub API JSON using the workflow token, not anonymous access."""
    if not isinstance(token, str) or not token.strip():
        raise LatCIRequestError("GITHUB_TOKEN is required")
    request = urllib.request.Request(
        f"https://api.github.com/{path}",
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "User-Agent": "lat-ci-request",
            "X-GitHub-Api-Version": "2022-11-28",
        },
    )
    with opener(request, timeout=30) as response:
        return json.load(response)


def resolve_pull_request(pr_number_value, target, fetch_json):
    """Return immutable dispatch values after validating a GitHub PR response."""
    if not isinstance(pr_number_value, str) or not POSITIVE_INTEGER_PATTERN.fullmatch(pr_number_value.strip()):
        raise LatCIRequestError("pr_number must be a positive integer")
    pr_number = int(pr_number_value)
    if target != EXPECTED_TARGET_REPOSITORY:
        raise LatCIRequestError(f"This workflow must run in {EXPECTED_TARGET_REPOSITORY}")

    pr = fetch_json(f"repos/{target}/pulls/{pr_number}")
    if not isinstance(pr, dict):
        raise LatCIRequestError("The GitHub pull request response is invalid")
    if pr.get("state") != "open":
        raise LatCIRequestError("The PR must be open")
    if pr.get("number") != pr_number:
        raise LatCIRequestError("The GitHub pull request number is invalid")

    head = pr.get("head")
    base = pr.get("base")
    if not isinstance(head, dict) or not isinstance(base, dict):
        raise LatCIRequestError("The GitHub pull request refs are invalid")
    head_repo = head.get("repo")
    base_repo = base.get("repo")
    if not isinstance(head_repo, dict):
        raise LatCIRequestError("The PR head repository is unavailable")
    if not isinstance(base_repo, dict) or base_repo.get("full_name") != target:
        raise LatCIRequestError("The PR base repository does not match target_repository")

    # full_name is authoritative GitHub API data.  Do not impose an incorrect
    # local owner/repository grammar on it; only reject values unsafe for the
    # line-based GITHUB_OUTPUT format.
    source_repository = _one_line_string(head_repo.get("full_name"), "The PR head repository")
    source_repository_id = head_repo.get("id")
    if isinstance(source_repository_id, bool) or not POSITIVE_INTEGER_PATTERN.fullmatch(str(source_repository_id or "")):
        raise LatCIRequestError("The PR head repository ID is invalid")
    head_sha = _sha(head.get("sha"), "The PR head SHA")
    base_sha = _sha(base.get("sha"), "The PR base SHA")
    head_ref = _one_line_string(head.get("ref"), "The PR head ref")

    return {
        "should_dispatch": "true",
        "source_repository": source_repository,
        "source_repository_id": str(source_repository_id),
        "pr_number": str(pr_number),
        "head_sha": head_sha,
        "base_sha": base_sha,
        "head_ref": head_ref,
        "target_repository": target,
    }


def write_github_output(values, output_path):
    """Write validated single-line values to the GitHub Actions output file."""
    with open(output_path, "a", encoding="utf-8") as output:
        for key, value in values.items():
            print(f"{key}={value}", file=output)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pr-number", required=True)
    parser.add_argument("--target-repository", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()

    try:
        values = resolve_pull_request(
            args.pr_number,
            args.target_repository,
            lambda path: get_json(path, os.environ.get("GITHUB_TOKEN", "")),
        )
        write_github_output(values, args.output)
    except LatCIRequestError as error:
        raise SystemExit(f"LAT CI request rejected: {error}") from error

    print(f"Resolved {args.target_repository} PR #{values['pr_number']} at {values['head_sha']}")


if __name__ == "__main__":
    main()
