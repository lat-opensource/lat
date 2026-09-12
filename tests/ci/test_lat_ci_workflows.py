#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Unit tests for the trusted upstream LAT CI workflow helpers."""

import importlib.util
import io
import json
import os
import subprocess
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def load_helper(name):
    path = ROOT / "scripts" / "ci" / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


REQUEST = load_helper("lat_ci_request")
CALLBACK = load_helper("lat_ci_callback")
TARGET = "lat-opensource/lat"
HEAD_SHA = "a" * 40
BASE_SHA = "b" * 40


def pull_request(source_repository=TARGET, head_sha=HEAD_SHA):
    return {
        "number": 466,
        "state": "open",
        "head": {
            "sha": head_sha,
            "ref": "review/contains-s",
            "repo": {"full_name": source_repository, "id": 123456},
        },
        "base": {"sha": BASE_SHA, "repo": {"full_name": TARGET}},
    }


class Response(io.BytesIO):
    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        return False


class LatCIRequestTest(unittest.TestCase):
    def resolve(self, pr, pr_number="466"):
        return REQUEST.resolve_pull_request(
            pr_number, TARGET, lambda path: pr
        )

    def test_accepts_upstream_branch_with_s_in_repository_name(self):
        result = self.resolve(pull_request(TARGET))
        self.assertEqual(result["source_repository"], "lat-opensource/lat")

    def test_accepts_external_fork_with_s_in_owner_and_repository_name(self):
        result = self.resolve(pull_request("reviewers/test-suite"))
        self.assertEqual(result["source_repository"], "reviewers/test-suite")

    def test_resolves_current_head_without_manual_sha(self):
        result = self.resolve(pull_request(head_sha="c" * 40))
        self.assertEqual(result["head_sha"], "c" * 40)

    def test_rejects_invalid_pr_number(self):
        with self.assertRaisesRegex(REQUEST.LatCIRequestError, "positive integer"):
            self.resolve(pull_request(), pr_number="466x")

    def test_rejects_closed_pr_base_mismatch_and_unavailable_head_repository(self):
        closed = pull_request()
        closed["state"] = "closed"
        with self.assertRaisesRegex(REQUEST.LatCIRequestError, "must be open"):
            self.resolve(closed)

        wrong_base = pull_request()
        wrong_base["base"]["repo"]["full_name"] = "example/lat"
        with self.assertRaisesRegex(REQUEST.LatCIRequestError, "base repository"):
            self.resolve(wrong_base)

        missing_head_repository = pull_request()
        missing_head_repository["head"]["repo"] = None
        with self.assertRaisesRegex(REQUEST.LatCIRequestError, "head repository is unavailable"):
            self.resolve(missing_head_repository)

    def test_uses_workflow_token_for_github_api_calls(self):
        requests = []

        def opener(request, timeout):
            requests.append(request)
            return Response(json.dumps({"ok": True}).encode("utf-8"))

        self.assertEqual(REQUEST.get_json("repos/lat-opensource/lat", "test-token", opener), {"ok": True})
        self.assertEqual(requests[0].get_header("Authorization"), "Bearer test-token")


class LatCICallbackTest(unittest.TestCase):
    def setUp(self):
        self.secret = "test-callback-secret"
        self.timestamp = "1700000000"
        self.values = {
            "repository": TARGET,
            "head_sha": HEAD_SHA,
            "conclusion": "success",
            "summary": "All selected LAT CI suites completed successfully.",
            "details_url": "https://github.com/xiezyang/lat-ci-hub/actions/runs/1",
        }

    def signature(self):
        return CALLBACK.create_signature(self.secret, timestamp=int(self.timestamp), **self.values)

    def test_accepts_a_signed_callback(self):
        CALLBACK.verify_callback(
            self.secret,
            timestamp_value=self.timestamp,
            signature=self.signature(),
            now=int(self.timestamp) + 10,
            **self.values,
        )

    def test_rejects_manual_or_tampered_success_callback(self):
        with self.assertRaisesRegex(CALLBACK.LatCICallbackError, "signature"):
            CALLBACK.verify_callback(
                self.secret,
                timestamp_value=self.timestamp,
                signature="sha256=" + "0" * 64,
                now=int(self.timestamp) + 10,
                **self.values,
            )
        with self.assertRaisesRegex(CALLBACK.LatCICallbackError, "required"):
            CALLBACK.create_signature("", timestamp=int(self.timestamp), **self.values)


class WorkflowDefinitionTest(unittest.TestCase):
    def test_dispatch_defaults_and_explicit_request(self):
        workflow = (ROOT / ".github/workflows/request-lat-ci.yml").read_text()
        # Execute the actual payload builder embedded in the workflow.
        import textwrap
        code = textwrap.dedent(workflow.split("<<'PY'\n", 1)[1].split("\n          PY", 1)[0])
        environment = dict(os.environ, **{name: "test" for name in (
            "REQUEST_KIND", "SOURCE_REPOSITORY", "SOURCE_REPOSITORY_ID",
            "TARGET_REPOSITORY", "PR_NUMBER", "HEAD_SHA", "BASE_SHA",
            "HEAD_REF", "REQUESTER",
        )})
        explicit = '{"schema_version":1,"suites":[]}'
        for value in ("", "   ", explicit):
            environment["TEST_REQUEST"] = value
            payload = json.loads(subprocess.check_output(
                ["python3", "-c", code], env=environment, text=True
            ))
            request = payload["inputs"]["test_request"]
            if value == explicit:
                self.assertEqual(request, explicit)
            else:
                self.assertEqual(json.loads(request), {
                    "schema_version": 1,
                    "suites": [{"suite_key": "spec2000", "parameters": {
                        "input_size": "ref", "runs_per_invocation": 3,
                        "test_set": "all", "times": 2,
                        "performance_analysis": True,
                    }, "environment": {}}],
                })

    def test_request_workflow_resolves_sha_and_uses_github_token(self):
        workflow = (ROOT / ".github" / "workflows" / "request-lat-ci.yml").read_text(
            encoding="utf-8"
        )
        self.assertNotIn("expected_head_sha", workflow)
        self.assertNotIn("EXPECTED_HEAD_SHA", workflow)
        self.assertIn("GITHUB_TOKEN: ${{ github.token }}", workflow)
        self.assertIn("scripts/ci/lat_ci_request.py", workflow)
        self.assertIn("actions/checkout@v5", workflow)
        self.assertNotIn("[^/\\\\s]+/[^/\\\\s]+", workflow)

    def test_callback_workflow_authenticates_before_creating_a_check_run(self):
        workflow = (ROOT / ".github" / "workflows" / "lat-ci-check.yml").read_text(
            encoding="utf-8"
        )
        self.assertIn("callback_timestamp:", workflow)
        self.assertIn("callback_signature:", workflow)
        self.assertIn(
            "LAT_CI_CHECK_CALLBACK_SECRET: ${{ secrets.LAT_CI_CHECK_CALLBACK_SECRET }}",
            workflow,
        )
        self.assertIn("scripts/ci/lat_ci_callback.py", workflow)
        self.assertLess(
            workflow.index("Authenticate the LAT CI hub callback"),
            workflow.index("Create Check Run on the tested LAT commit"),
        )


if __name__ == "__main__":
    unittest.main()
