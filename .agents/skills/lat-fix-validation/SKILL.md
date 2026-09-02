---
name: lat-fix-validation
description: Use for diagnosing and validating LAT crashes, hangs, wrong results, compatibility regressions, intermittent failures, and failing tests. Requires trusted reproduction and regression closure; not for performance-only work or read-only review.
---

# LAT fix validation

Produce the smallest general fix whose conclusion stays within the evidence.

## Read additional guidance when relevant

- For binary, runtime, cache, harness, and environment identity, read
  [references/reproduction-integrity.md](references/reproduction-integrity.md).
- For translator first-divergence analysis, current dump/log options, GDB,
  disassembly, and watchpoints, read
  [references/translator-root-cause.md](references/translator-root-cause.md).
- Before changing translator metadata, layout, registers, state, or path gates,
  read [references/translator-invariant-checklist.md](references/translator-invariant-checklist.md).
- For regression surface, test-seam exceptions, risk-driven matrices, and
  synthetic-versus-field evidence, read
  [references/regression-and-matrix.md](references/regression-and-matrix.md).
- For guest/native, loader, wrapper, callback, shared-state, ownership, and
  lifecycle fixes, read
  [references/cross-boundary-state-fixes.md](references/cross-boundary-state-fixes.md).
- For evidence levels, terminal states, CI truth, and handoff, read
  [references/evidence-and-closure.md](references/evidence-and-closure.md).
- When a repeatable identity manifest is useful, run the bundled
  `scripts/capture_fix_context.py` from this skill directory with `--repo` and
  the relevant artifact or runtime inputs. It emits JSON to stdout unless
  `--output` is provided. Use `--redact-paths` and review the result before
  sharing.

## Establish the task

- Pin the source revision, worktree state, build, guest/runtime configuration,
  switches, target environment, and exact reproduction when they affect the
  result.
- State the expected behavior, observed failure, task scope, and required
  acceptance level and responsible owner. Identify the project source for any
  claim that a configuration or application is supported. Discover missing
  facts read-only; do not invent them.
- Preserve unrelated changes and use a clean worktree when the current checkout
  cannot safely contain the fix.
- Before modifying production code, prove that the reproducer uses the intended
  artifact and runtime and reaches the target path. Use a control to classify
  harness or environment failures when possible.

## Reconstruct intent and prior art

- Before changing a code path, inspect its local history. Determine whether the
  behavior has existed since the project baseline or was introduced later.
- When a later change introduced the behavior, identify the commit, issue, test,
  or failure it addressed and the invariant it protects. The new fix must retain
  that protection and re-run or recreate its regression coverage.
- Identify the authority that defines the behavior: x86 ISA, ABI, Linux UAPI,
  runtime or library upstream, or QEMU, depending on the problem.
- When upstream QEMU addresses the same problem, compare its assumptions with
  LAT's version and translator-specific changes before adapting a fix; do not
  cherry-pick a result without this analysis.
- For a long-standing bug without an applicable authority or QEMU solution,
  inspect how other binary translators or emulators model the same behavior.
  Treat those implementations as design evidence, not proof; verify architecture
  fit, invariants, license, and provenance before adapting code.

## Diagnose before changing behavior

1. Reproduce the original failure and record a deterministic RED when feasible;
   for an intermittent failure, record the baseline rate and predeclare the
   candidate repetition or soak duration, exposure conditions, acceptance
   threshold, and stop rule.
2. For a recent regression, identify or narrow the last-known-good and first-bad
   revisions under the same configuration. Isolate translation and AOT caches
   between compared revisions.
3. Minimize the difference between a working and failing case. Find the earliest
   observable divergence between expected semantics and actual execution rather
   than focusing only on the final crash PC. Distinguish an expected guest signal,
   a translated guest exception, and a host translator or runtime failure.
4. Identify the layer that owns the broken invariant. Fix a shared translation,
   ABI, namespace, state-lifecycle, runtime, or harness rule at that layer rather
   than patching the nearest application or crash site. If LAT does not own the
   defect, report that boundary instead of forcing a LAT change.
5. Write falsifiable hypotheses and prefer single-variable experiments that
   distinguish them. Mode toggles, disabled optimizations, forced TB boundaries,
   timeout changes, extra logging, and temporary bypasses are diagnostic probes,
   not GREEN or root-cause fixes. Remove probes and revalidate the original path.
6. For concurrency or shared-state defects, define the owner, object identity,
   sharing scope, lock, publication point, invalidation point, and transaction
   boundary before changing behavior.

## Implement and validate

1. Make the smallest change based on general guest/host semantics. Do not add
   application names, benchmark addresses, or machine-specific paths.
2. Keep behavior-neutral preparation separate and verify that it does not change
   behavior.
3. Derive the regression surface from the broken invariant, call graph, shared
   entry points, configuration branches, fallback paths, and boundary values,
   not only from the triggering application or first failing function.
4. Add focused regression coverage when a durable test seam exists. Demonstrate
   that it fails without the fix and passes with it; retain or recreate coverage
   for prior fixes that protect the same path. Do not add test-infrastructure
   churn for a narrow compile-only fix without a stable seam; report that limited
   evidence boundary instead.
5. Build only the risk-relevant validation matrix from current compile-time gates,
   runtime switches, cache states, guest widths, runtimes, concurrency, and
   lifecycle transitions. Each result needs an observable sentinel showing that
   the intended path ran.
6. When the field application is unavailable, build a self-contained seam that
   preserves the relevant ABI, thread, loader, callback, or lifecycle topology.
   Keep the field gate open until the original application is revalidated.
7. Re-run the original application or full reproduction and verify its expected
   output, state, and final termination, not merely the absence of the original
   crash or movement to another error. For intermittent failures, apply the
   predeclared protocol; do not turn a short lucky run into closure.
8. Run the affected focused tests and the complete `lat-pr-fast` suite before a
   pull request. Run relevant `latx-integration` tests when the environment is
   available; otherwise name the remaining gate.
9. Use LoongArch evidence for LoongArch-specific build or runtime conclusions.
10. A compatibility runtime may fail open by design, but validation of a target
    component, binding, callback, or endpoint must fail closed when that path is
    not observed. Fallback success is not equivalent completion.

## Report the result

Report these separately:

- fixed source, build, runtime, switches, and reproduction;
- RED and GREEN observations;
- confirmed root cause versus remaining hypotheses;
- local history, upstream QEMU, and other-translator findings, including the
  existing behavior that must be preserved;
- reproducer integrity, target-path sentinel, environment controls, ownership,
  and lifecycle assumptions;
- change scope and regression coverage;
- risk-driven validation matrix and original-application terminal result;
- tests executed, tests not executed, and exact results;
- what is proved, what is not proved, and the next validation gate.

Use an honest terminal state:

- **continue investigating** when the root cause or fix is not established;
- **evidence-bounded diagnosis** when static, synthetic, or environment-limited
  evidence is valid but the required runtime or field gate is unavailable;
- **fix validated within scope** when the defined evidence passes but a broader
  integration gate remains;
- **product-accepted** only after the required real path and responsible owner
  acceptance are complete.

Do not post comments or reviews, push branches, open pull requests, publish
releases, or change target systems unless explicitly authorized by the task.
