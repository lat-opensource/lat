# LAT agent guidance

## Project scope

- LAT supports only x86_64 and i386 user-mode guests on LoongArch Linux hosts.
  Files exclusive to other guest architectures are cleanup candidates only after
  proving that they are not shared or required by supported builds, tests,
  tooling, or upstream maintenance.

## Project skills

- Explicitly invoke `$lat-fix-validation` for crashes, hangs, wrong results,
  compatibility regressions, intermittent failures, and failing-test work.
- Explicitly invoke `$lat-performance-experiment` for optimizations, benchmarks,
  PMU work, AOT/TU performance, and performance regressions.

## Repository and change safety

- Before editing, inspect the active branch, status, and worktrees. Never discard
  or include out-of-scope changes or untracked files.
- Reuse a clean worktree when possible. Use a separate one for history rewrites,
  destructive branch operations, or work that overlaps a dirty checkout; remove
  it only after its work is integrated or abandoned and the tree is clean.
- Update submodules only when the change modifies their recorded revisions or
  required validation depends on the update.
- Keep changes focused and general; do not hard-code application names,
  benchmark addresses, or machine-specific paths.
- Separate behavior-neutral preparation from behavioral fixes, and verify that
  the preparation does not change behavior.

## Fixes and optimizations

- A fix should reproduce the failure and, when feasible, add a focused test that
  fails without the fix and passes with it.
- An optimization must preserve correctness and apply only where its safety is
  proven. Compare fixed and isolated before-and-after builds in the same
  environment, using representative workloads and multiple interleaved runs;
  report mechanism and end-to-end results separately.
- Keep independent fixes and optimizations in separate commits.

## Builds and tests

- Before opening or updating a pull request, pass the complete `lat-pr-fast`
  suite and all affected focused tests locally, and include the commands and
  results in the pull request. Run affected `latx-integration` tests when the
  required environment is available; otherwise state the remaining gate.
- Keep test-only programs and fixtures out of product builds and register them
  according to `tests/README.md`. LoongArch-specific build or runtime claims
  require evidence from a LoongArch host.

## Evidence and completion

- Tie conclusions to the tested source revision and worktree state and, when
  relevant, the build, binary, runtime, switches, cache, target, and workload.
- Separate static, build, regression, target-runtime, real-application, and
  product evidence. Report what is proved, what is not proved, and what remains;
  a passing build, green CI, smoke test, or moved error is not product
  completion. Preserve stable negative results.

## LAT-specific evidence boundaries

- AOT evidence must prove that the intended artifact was loaded; file creation
  or a second successful launch is insufficient if stale cache or JIT fallback
  can pass.
- KZT evidence must trace process enablement, wrapper eligibility, binding, and
  the final host, guest, or mixed endpoint; initialization or a moved error is
  not host-passthrough proof.

## Review and external changes

- Reviews are read-only unless implementation or submission is explicitly
  requested. Pin the diff and use project-tracked requirements or standards.
- Comments, reviews, pushes, pull-request changes, merges, published-history
  rewrites, CI reruns, releases, and target-system changes must be explicitly
  authorized by the task.
- Do not alter `Signed-off-by` trailers without the author's authorization.
  Remove local paths, hostnames, local user names, internal URLs, and unrelated
  workstream details before publication.
