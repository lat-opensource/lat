#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Publish the tested tag's three packages, keeping partial uploads in a draft."""

import argparse
import hashlib
import json
import os
import re
import shutil
import urllib.parse
import urllib.request
from pathlib import Path


DISTROS = ("aosc", "debian", "fedora")


def release_version(tag, version):
    match = re.fullmatch(r"v?(\d+\.\d+\.\d+)(-[0-9A-Za-z][0-9A-Za-z.-]*)?", tag)
    if not match:
        raise ValueError("Expected a version tag such as 1.6.8 or 1.6.8-rc1")
    normalized = match[1] + (match[2] or "")
    if version not in (match[1], normalized):
        raise ValueError(f"VERSION {version!r} does not match tag {tag!r}")
    return normalized, bool(match[2])


def prepare_assets(artifact_dir, output_dir, tag, version, sha):
    normalized, _ = release_version(tag, version)
    if not re.fullmatch(r"[0-9a-f]{40}", sha):
        raise ValueError("Expected the exact tested commit SHA")
    sources = []
    for distro in DISTROS:
        prefix = f"latx-runner-{distro}-build-release-"
        directories = list(artifact_dir.glob(f"{prefix}*"))
        if len(directories) != 1:
            raise ValueError(f"Expected exactly one {distro} release artifact")
        directory = directories[0]
        short_sha = directory.name[len(prefix):]
        if len(short_sha) < 7 or not sha.startswith(short_sha):
            raise ValueError(f"Artifact does not belong to tested commit: {directory.name}")
        archives = list(directory.rglob("*.tar.xz"))
        if len(archives) != 1:
            raise ValueError(f"Expected exactly one package for {distro}")
        archive = archives[0]
        if (archive.is_symlink() or not archive.is_file() or archive.stat().st_size == 0
                or not re.fullmatch(rf"lat-{re.escape(version)}-\d{{8}}\.tar\.xz", archive.name)):
            raise ValueError(f"Invalid package for {distro}: {archive.name}")
        sources.append((archive, f"lat-{normalized}-{distro}-loongarch64.tar.xz"))
    output_dir.mkdir(parents=True, exist_ok=False)
    assets = []
    checksums = []
    for source, name in sources:
        destination = output_dir / name
        shutil.copyfile(source, destination)
        digest = hashlib.sha256()
        with destination.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(chunk)
        checksums.append(f"{digest.hexdigest()}  {name}\n")
        assets.append(destination)
    checksum_file = output_dir / "SHA256SUMS"
    checksum_file.write_text("".join(checksums), encoding="utf-8")
    return assets + [checksum_file]


class GitHub:
    def __init__(self, repository, token):
        self.repository = repository
        self.token = token

    def request(self, method, path, payload=None):
        url = f"https://api.github.com/repos/{self.repository}/{path}"
        data = None if payload is None else json.dumps(payload).encode()
        return self.send(method, url, data, "application/json")

    def send(self, method, url, data, content_type):
        request = urllib.request.Request(url, data=data, method=method, headers={
            "Authorization": f"Bearer {self.token}",
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
            "Content-Type": content_type,
        })
        with urllib.request.urlopen(request, timeout=120) as response:
            body = response.read()
        return json.loads(body) if body else None

    def upload(self, release, asset):
        base_url = release["upload_url"].split("{", 1)[0]
        if urllib.parse.urlsplit(base_url).hostname != "uploads.github.com":
            raise ValueError("Unexpected GitHub asset upload host")
        url = base_url + "?" + urllib.parse.urlencode({"name": asset.name})
        return self.send("POST", url, asset.read_bytes(), "application/octet-stream")


def verify_tag(api, tag, sha):
    result = api.request("GET", "git/ref/tags/" + urllib.parse.quote(tag, safe=""))
    obj = result["object"]
    # Dereference annotated tags too; the tag object SHA is not the commit SHA.
    for _ in range(10):
        if obj["type"] == "commit":
            if obj["sha"] != sha:
                raise ValueError("Remote tag no longer points to the tested commit")
            return
        if obj["type"] != "tag":
            break
        obj = api.request("GET", f"git/tags/{obj['sha']}")["object"]
    raise ValueError("Version tag does not resolve to a commit")


def find_release(api, tag):
    # Listing with a write token includes drafts; the release-by-tag endpoint
    # is documented for published releases and cannot be used to resume drafts.
    for page in range(1, 101):
        releases = api.request("GET", f"releases?per_page=100&page={page}")
        matches = [release for release in releases if release["tag_name"] == tag]
        if len(matches) > 1:
            raise ValueError("More than one release uses this tag")
        if matches:
            return matches[0]
        if len(releases) < 100:
            return None
    raise ValueError("Release history is too large to inspect completely")


def publish(api, tag, sha, assets, prerelease, run_url):
    verify_tag(api, tag, sha)
    marker = f"<!-- lat-ci-release commit={sha} -->"
    release = find_release(api, tag)
    if release is None:
        release = api.request("POST", "releases", {
            "tag_name": tag, "target_commitish": sha, "name": f"LAT {tag}",
            "draft": True, "prerelease": prerelease, "generate_release_notes": True,
            "body": f"{marker}\n\nCommit: `{sha}`\n\n"
                    f"[Full CI validation]({run_url})\n\n"
                    "15 GCC builds, 3 lat-pr-fast jobs and 1 Clang build passed.\n",
        })
    if marker not in (release.get("body") or ""):
        raise ValueError("Existing release was not created by this workflow for this commit")
    if release.get("prerelease") != prerelease:
        raise ValueError("Existing release has a different prerelease classification")
    existing_assets = release.get("assets", [])
    names = {asset.name for asset in assets}
    if not release["draft"]:
        existing_names = {item["name"] for item in existing_assets if item["size"] > 0}
        if not names.issubset(existing_names):
            raise ValueError("Published release is missing expected assets; refusing to overwrite it")
        print(f"Release {tag} is already published for this commit")
        return release["html_url"]
    # Only replace assets in this workflow's own unpublished draft. A failed
    # upload leaves a draft that can be completed by rerunning the failed job.
    for item in existing_assets:
        if item["name"] not in names:
            raise ValueError("Draft contains unexpected assets; refusing to modify it")
    for item in existing_assets:
        api.request("DELETE", f"releases/assets/{item['id']}")
    for asset in assets:
        uploaded = api.upload(release, asset)
        if uploaded.get("state") != "uploaded" or uploaded.get("size") != asset.stat().st_size:
            raise ValueError(f"Upload incomplete for {asset.name}")
    verify_tag(api, tag, sha)
    result = api.request("PATCH", f"releases/{release['id']}", {"draft": False})
    if result.get("draft") is not False:
        raise ValueError("Release remained a draft after the publish request")
    return result["html_url"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=("check-version", "publish"))
    parser.add_argument("--artifact-dir", type=Path, default=Path("release-artifacts"))
    parser.add_argument("--output-dir", type=Path, default=Path("release-assets"))
    args = parser.parse_args()
    tag = os.environ["GITHUB_REF_NAME"]
    version = Path("VERSION").read_text().strip()
    _, prerelease = release_version(tag, version)
    if args.command == "check-version":
        print(f"Release tag {tag} matches VERSION {version}")
        return
    sha = os.environ["GITHUB_SHA"]
    assets = prepare_assets(args.artifact_dir, args.output_dir, tag, version, sha)
    repository = os.environ["GITHUB_REPOSITORY"]
    api = GitHub(repository, os.environ["GITHUB_TOKEN"])
    run_url = f"https://github.com/{repository}/actions/runs/{os.environ['GITHUB_RUN_ID']}"
    url = publish(api, tag, sha, assets, prerelease, run_url)
    with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as summary:
        summary.write(f"Published [{tag}]({url}) from commit `{sha}`.\n")


if __name__ == "__main__":
    main()
