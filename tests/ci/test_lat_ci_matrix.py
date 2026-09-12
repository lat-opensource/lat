#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Regression tests for CI coverage selection and incomplete PR evidence."""

import importlib.util
import io
import json
import os
import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
HELPER = ROOT / "scripts/ci/lat_ci_matrix.py"
SPEC = importlib.util.spec_from_file_location("lat_ci_matrix", HELPER)
MATRIX = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MATRIX)


def changed_file(name="target/i386/latx/translator/translate.c", patch="+return 1;"):
    return {"filename": name, "patch": patch}


class MatrixSelectionTest(unittest.TestCase):
    def select(self, files):
        event = {"repository": {"full_name": "lat-opensource/lat"},
                 "pull_request": {"number": 123, "changed_files": len(files)}}
        return MATRIX.select_matrix("pull_request", event,
                                    fetch_files=lambda *args: files)[0]

    def test_ordinary_pr_preserves_both_architectures_avx_and_sanitizers(self):
        outputs = MATRIX.matrix_outputs(self.select([changed_file()]))
        self.assertEqual(outputs["build_types"], [
            {"NAME": "build32", "OPT": "-c -a"},
            {"NAME": "build64", "OPT": "-c -a"},
        ])
        self.assertEqual(len(outputs["test_containers"]), 1)
        self.assertEqual(outputs["test_containers"][0]["name"], "latx-runner-debian")
        self.assertTrue(outputs["test_containers"][0]["sanitizers"])

    def test_non_pr_events_always_run_full_without_api(self):
        def unexpected_api(*args):
            self.fail("Non-PR coverage must not depend on the PR API")

        for event in ("push", "schedule", "release", "workflow_dispatch", "workflow_call"):
            with self.subTest(event=event):
                full, _ = MATRIX.select_matrix(event, {}, fetch_files=unexpected_api)
                self.assertTrue(full)
                outputs = MATRIX.matrix_outputs(full)
                self.assertEqual(len(outputs["build_types"]), 5)
                self.assertEqual(len(outputs["test_containers"]), 3)

    def test_build_runtime_and_ci_changes_expand_coverage(self):
        for name in ("configure", "meson.options", "meson_options.txt",
                     "tests/unit/meson.build", "configs/targets/i386.mak",
                     "latxbuild/build64-dbg.sh", ".gitmodules",
                     ".github/.ci/clang/Dockerfile", ".github/workflows/tests.yml",
                     "scripts/ci/lat_ci_matrix.py", "tests/ci/test_lat_ci_matrix.py",
                     "runtime/latu-runtime-manager", "tests/runtime/fixture.sh"):
            with self.subTest(name=name):
                self.assertTrue(self.select([changed_file(name)]))

    def test_rename_out_of_build_directory_expands_coverage(self):
        item = changed_file("docs/old-script.txt")
        item["previous_filename"] = "latxbuild/build32.sh"
        self.assertTrue(self.select([item]))

    def test_preprocessor_additions_and_removals_expand_coverage(self):
        for directive in ("if X", "ifdef X", "ifndef X", "elif X", "else",
                          "endif", "define X 1", "undef X"):
            for sign in ("+", "-"):
                with self.subTest(directive=directive, sign=sign):
                    self.assertTrue(self.select([changed_file(
                        patch=f"@@ -1 +1 @@\n{sign}  # {directive}\n")]))

    def test_unchanged_preprocessor_context_keeps_pr_coverage(self):
        self.assertFalse(self.select([changed_file(
            patch="@@ -1,3 +1,3 @@\n #ifdef DEBUG\n-old();\n+new();\n #endif\n")]))

    def test_missing_patch_expands_coverage(self):
        self.assertTrue(self.select([{"filename": "large.c"}]))

    def test_truncated_patch_expands_coverage(self):
        item = changed_file(patch="@@ -1 +1 @@\n-old();\n+new();\n")
        item.update(additions=2, deletions=1)
        self.assertTrue(self.select([item]))

    def test_api_failure_expands_coverage(self):
        def failing_api(*args):
            raise OSError("network unavailable")

        event = {"repository": {"full_name": "lat-opensource/lat"},
                 "pull_request": {"number": 123, "changed_files": 1}}
        full, _ = MATRIX.select_matrix("pull_request", event, fetch_files=failing_api)
        self.assertTrue(full)

    def test_api_paginates_before_deciding_coverage(self):
        requested = []
        batches = [[changed_file() for _ in range(100)],
                   [changed_file("configure")]]

        def opener(request, timeout):
            requested.append(request.full_url)
            return io.BytesIO(json.dumps(batches.pop(0)).encode())

        files = MATRIX.pull_request_files("lat-opensource/lat", 123, "test-token",
                                          101, opener=opener)
        self.assertTrue(MATRIX.needs_full_matrix(files))
        self.assertEqual(len(requested), 2)
        self.assertTrue(requested[-1].endswith("page=2"))

    def test_incomplete_api_list_is_rejected(self):
        def opener(request, timeout):
            return io.BytesIO(b"[]")

        with self.assertRaisesRegex(ValueError, "incomplete"):
            MATRIX.pull_request_files("lat-opensource/lat", 123, "test-token",
                                      1, opener=opener)

    def test_api_limit_is_rejected_without_fetching(self):
        def unexpected_api(*args, **kwargs):
            self.fail("Do not fetch a file list known to be incomplete")

        with self.assertRaisesRegex(ValueError, "limit"):
            MATRIX.pull_request_files("lat-opensource/lat", 123, "test-token",
                                      3000, opener=unexpected_api)

    def test_cli_emits_single_line_json_outputs(self):
        with tempfile.TemporaryDirectory() as directory:
            event = Path(directory) / "event.json"
            output = Path(directory) / "outputs"
            event.write_text("{}")
            subprocess.run(["python3", "-B", str(HELPER)], check=True,
                           capture_output=True, env=dict(os.environ,
                           GITHUB_EVENT_NAME="workflow_dispatch",
                           GITHUB_EVENT_PATH=str(event), GITHUB_OUTPUT=str(output)))
            values = dict(line.split("=", 1) for line in output.read_text().splitlines())
            self.assertEqual(len(json.loads(values["build_types"])), 5)
            self.assertEqual(len(json.loads(values["test_containers"])), 3)


class ClangImageSelectionTest(unittest.TestCase):
    def run_selection(self, pull_status):
        workflow = (ROOT / ".github/workflows/clang.yml").read_text()
        step = workflow.split("      - name: Pull matching clang image\n", 1)[1]
        code = textwrap.dedent(step.split("        run: |\n", 1)[1].split(
            "\n      - name:", 1)[0])
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            docker = path / "docker"
            docker.write_text("#!/bin/sh\n"
                              'printf "%s\\n" "$*" >> "$DOCKER_LOG"\n'
                              'if [ "$1" = pull ]; then exit "$PULL_STATUS"; fi\n')
            docker.chmod(0o755)
            # macOS has shasum even when GNU sha256sum is not installed.
            checksum = path / "sha256sum"
            checksum.write_text('#!/bin/sh\nexec shasum -a 256 "$@"\n')
            checksum.chmod(0o755)
            output = path / "outputs"
            log = path / "docker.log"
            subprocess.run(["bash", "-e", "-c", code], check=True, cwd=ROOT,
                           capture_output=True, env=dict(os.environ,
                           PATH=f"{path}:{os.environ['PATH']}",
                           REPOSITORY_OWNER="lat-opensource",
                           GITHUB_OUTPUT=str(output), DOCKER_LOG=str(log),
                           PULL_STATUS=str(pull_status)))
            return output.read_text(), log.read_text()

    def test_published_image_uses_dockerfile_hash_and_skips_local_build(self):
        import hashlib
        digest = hashlib.sha256((ROOT / ".github/.ci/clang/Dockerfile").read_bytes()).hexdigest()
        output, log = self.run_selection(0)
        self.assertEqual(output.strip(), "build_local=false")
        self.assertIn(f"loongarch64-{digest}", log)
        self.assertIn("tag ghcr.io/lat-opensource/", log)
        self.assertIn("lat-ci-clang:local", log)

    def test_unpublished_image_requests_local_build_without_tagging(self):
        output, log = self.run_selection(1)
        self.assertEqual(output.strip(), "build_local=true")
        self.assertNotIn("\ntag ", log)


class ReleaseValidationGateTest(unittest.TestCase):
    def run_gate(self, **overrides):
        workflow = (ROOT / ".github/workflows/release.yml").read_text()
        code = textwrap.dedent(workflow.split("<<'PY'\n", 1)[1].split(
            "\n          PY", 1)[0])
        outputs = MATRIX.matrix_outputs(True)
        with tempfile.TemporaryDirectory() as directory:
            summary = Path(directory) / "summary.md"
            environment = dict(os.environ, SELECT_RESULT="success",
                               BUILD_RESULT="success", TEST_RESULT="success",
                               CLANG_RESULT="success", GITHUB_SHA="a" * 40,
                               BUILD_TYPES=json.dumps(outputs["build_types"]),
                               TEST_CONTAINERS=json.dumps(outputs["test_containers"]),
                               GITHUB_STEP_SUMMARY=str(summary))
            environment.update(overrides)
            result = subprocess.run(["python3", "-c", code], env=environment,
                                    capture_output=True, text=True)
            return result, summary.read_text() if summary.exists() else ""

    def test_success_records_exact_commit_and_full_coverage(self):
        result, summary = self.run_gate()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("a" * 40, summary)
        self.assertIn("15 GCC builds, 3 lat-pr-fast jobs and 1 Clang build", summary)

    def test_failed_cancelled_or_skipped_dependencies_cannot_pass(self):
        for key in ("SELECT_RESULT", "BUILD_RESULT", "TEST_RESULT", "CLANG_RESULT"):
            for value in ("failure", "cancelled", "skipped"):
                with self.subTest(key=key, value=value):
                    result, summary = self.run_gate(**{key: value})
                    self.assertNotEqual(result.returncode, 0)
                    self.assertEqual(summary, "")

    def test_reduced_gcc_matrix_cannot_pass(self):
        result, _ = self.run_gate(BUILD_TYPES=json.dumps(
            MATRIX.matrix_outputs(False)["build_types"]))
        self.assertNotEqual(result.returncode, 0)

    def test_reduced_test_matrix_cannot_pass(self):
        result, _ = self.run_gate(TEST_CONTAINERS=json.dumps(
            MATRIX.matrix_outputs(False)["test_containers"]))
        self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
