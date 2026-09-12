#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise release preparation and publication without network mutations."""

import copy
import hashlib
import importlib.util
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("lat_ci_release", ROOT / "scripts/ci/lat_ci_release.py")
RELEASE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RELEASE)
SHA = "a" * 40
TAG = "1.6.8"


class PrepareAssetsTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.artifacts = self.root / "artifacts"
        self.archives = []
        for distro in RELEASE.DISTROS:
            directory = self.artifacts / f"latx-runner-{distro}-build-release-{SHA[:11]}"
            directory.mkdir(parents=True)
            archive = directory / "lat-1.6.8-20260912.tar.xz"
            archive.write_bytes(distro.encode())
            self.archives.append(archive)

    def prepare(self):
        return RELEASE.prepare_assets(self.artifacts, self.root / "output", TAG, TAG, SHA)

    def test_same_named_distro_packages_get_unique_names_and_checksums(self):
        assets = self.prepare()
        self.assertEqual({item.name for item in assets}, {
            "lat-1.6.8-aosc-loongarch64.tar.xz", "lat-1.6.8-debian-loongarch64.tar.xz",
            "lat-1.6.8-fedora-loongarch64.tar.xz", "SHA256SUMS",
        })
        for line in assets[-1].read_text().splitlines():
            digest, name = line.split("  ")
            self.assertEqual(digest, hashlib.sha256((assets[-1].parent / name).read_bytes()).hexdigest())

    def test_missing_distro_fails_before_preparing_output(self):
        self.archives[0].unlink()
        self.archives[0].parent.rmdir()
        with self.assertRaisesRegex(ValueError, "exactly one aosc"):
            self.prepare()
        self.assertFalse((self.root / "output").exists())

    def test_wrong_artifact_commit_is_rejected(self):
        directory = self.archives[0].parent
        directory.rename(directory.with_name("latx-runner-aosc-build-release-bbbbbbb"))
        with self.assertRaisesRegex(ValueError, "tested commit"):
            self.prepare()

    def test_duplicate_package_is_rejected(self):
        self.archives[0].with_name("lat-1.6.8-20260913.tar.xz").write_bytes(b"duplicate")
        with self.assertRaisesRegex(ValueError, "exactly one package"):
            self.prepare()

    def test_wrong_package_version_is_rejected(self):
        self.archives[0].rename(self.archives[0].with_name("lat-1.6.5-20260912.tar.xz"))
        with self.assertRaisesRegex(ValueError, "Invalid package"):
            self.prepare()

    def test_empty_package_is_rejected(self):
        self.archives[0].write_bytes(b"")
        with self.assertRaisesRegex(ValueError, "Invalid package"):
            self.prepare()

    def test_release_and_prerelease_tag_version_rules(self):
        for tag in ("1.6.8", "v1.6.8"):
            self.assertEqual(RELEASE.release_version(tag, "1.6.8"), ("1.6.8", False))
        for version in ("1.6.8", "1.6.8-rc1"):
            self.assertEqual(RELEASE.release_version("v1.6.8-rc1", version), ("1.6.8-rc1", True))
        for tag, version in (("1.6.8", "1.6.5"), ("nightly", "1.6.8"), ("1.6.8-", "1.6.8")):
            with self.subTest(tag=tag, version=version), self.assertRaises(ValueError):
                RELEASE.release_version(tag, version)


class FakeGitHub:
    def __init__(self):
        self.release = None
        self.calls = []
        self.sha = SHA
        self.annotated = False
        self.upload_failure = False
        self.move_after_upload = False
        self.list_failure = False

    def request(self, method, path, payload=None):
        self.calls.append((method, path, payload))
        if path.startswith("git/ref/tags/"):
            return {"object": {"type": "tag" if self.annotated else "commit",
                               "sha": "tag-object" if self.annotated else self.sha}}
        if path == "git/tags/tag-object":
            return {"object": {"type": "commit", "sha": self.sha}}
        if path.startswith("releases?"):
            if self.list_failure:
                raise OSError("API unavailable")
            return [copy.deepcopy(self.release)] if self.release else []
        if method == "POST" and path == "releases":
            self.release = dict(payload, id=1, assets=[], html_url="https://github.com/example/release")
            return copy.deepcopy(self.release)
        if method == "DELETE":
            asset_id = int(path.rsplit("/", 1)[1])
            self.release["assets"] = [item for item in self.release["assets"] if item["id"] != asset_id]
            return None
        if method == "PATCH":
            self.release.update(payload)
            return copy.deepcopy(self.release)
        raise AssertionError((method, path, payload))

    def upload(self, release, asset):
        self.calls.append(("UPLOAD", asset.name, None))
        if self.upload_failure:
            raise OSError("upload interrupted")
        item = {"id": len(self.release["assets"]) + 1, "name": asset.name,
                "size": asset.stat().st_size, "state": "uploaded"}
        self.release["assets"].append(item)
        if self.move_after_upload:
            self.sha = "b" * 40
        return item


class PublishTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.assets = []
        for name in ("aosc.tar.xz", "debian.tar.xz", "fedora.tar.xz", "SHA256SUMS"):
            asset = Path(self.directory.name) / name
            asset.write_bytes(name.encode())
            self.assets.append(asset)
        self.api = FakeGitHub()

    def publish(self, prerelease=False):
        return RELEASE.publish(self.api, TAG, SHA, self.assets, prerelease, "https://github.com/example/run/1")

    def test_create_draft_upload_all_then_publish(self):
        self.publish()
        methods = [call[0] for call in self.api.calls]
        self.assertLess(methods.index("POST"), methods.index("UPLOAD"))
        self.assertEqual(methods[-1], "PATCH")
        self.assertEqual(methods.count("UPLOAD"), 4)
        create = next(call[2] for call in self.api.calls if call[0] == "POST")
        self.assertTrue(create["draft"])
        self.assertFalse(self.api.release["draft"])

    def test_prerelease_is_not_published_as_stable(self):
        self.publish(prerelease=True)
        self.assertTrue(self.api.release["prerelease"])

    def test_annotated_tag_resolves_to_tested_commit(self):
        self.api.annotated = True
        self.publish()
        self.assertFalse(self.api.release["draft"])

    def test_moved_tag_is_rejected_before_release_creation(self):
        self.api.sha = "b" * 40
        with self.assertRaisesRegex(ValueError, "tested commit"):
            self.publish()
        self.assertIsNone(self.api.release)

    def test_tag_moved_during_upload_stays_draft(self):
        self.api.move_after_upload = True
        with self.assertRaisesRegex(ValueError, "tested commit"):
            self.publish()
        self.assertTrue(self.api.release["draft"])
        self.assertNotIn("PATCH", [call[0] for call in self.api.calls])

    def test_failed_upload_stays_draft_and_rerun_resumes(self):
        self.api.upload_failure = True
        with self.assertRaises(OSError):
            self.publish()
        self.assertTrue(self.api.release["draft"])
        self.api.upload_failure = False
        self.publish()
        self.assertFalse(self.api.release["draft"])
        self.assertEqual(sum(call[0] == "POST" for call in self.api.calls), 1)

    def test_partial_draft_assets_can_be_replaced_on_retry(self):
        self.api.move_after_upload = True
        with self.assertRaises(ValueError):
            self.publish()
        self.api.move_after_upload = False
        self.api.sha = SHA
        self.publish()
        self.assertEqual(sum(call[0] == "DELETE" for call in self.api.calls), 4)
        self.assertFalse(self.api.release["draft"])

    def test_published_release_rerun_makes_no_mutations(self):
        self.publish()
        self.api.calls.clear()
        self.publish()
        self.assertTrue(all(call[0] == "GET" for call in self.api.calls))

    def test_missing_published_asset_is_not_silently_accepted(self):
        self.publish()
        self.api.release["assets"].pop()
        self.api.calls.clear()
        with self.assertRaisesRegex(ValueError, "missing expected assets"):
            self.publish()
        self.assertTrue(all(call[0] == "GET" for call in self.api.calls))

    def test_user_owned_release_is_not_modified(self):
        self.publish()
        self.api.release.update(draft=True, body="Maintainer's release draft")
        self.api.calls.clear()
        with self.assertRaisesRegex(ValueError, "not created by this workflow"):
            self.publish()
        self.assertTrue(all(call[0] == "GET" for call in self.api.calls))

    def test_api_failure_does_not_create_duplicate_release(self):
        self.api.list_failure = True
        with self.assertRaises(OSError):
            self.publish()
        self.assertIsNone(self.api.release)


if __name__ == "__main__":
    unittest.main()
