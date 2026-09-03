---
name: lat-fix-validation
description: Use for diagnosing and validating LAT crashes, hangs, wrong results, compatibility regressions, intermittent failures, and failing tests. Requires trusted reproduction and regression closure; not for performance-only work or read-only review.
---

# LAT fix validation

Produce the smallest general fix whose conclusion stays within the evidence.

## Determine the task mode and claim

- Set the mode: diagnosis, implementation, or existing-candidate validation.
  Diagnosis or validation does not authorize edits to repository, source, test,
  or tracked documentation files, and temporary instrumentation also requires
  edit authorization. Without it, stop with a bounded conclusion and plan.
- State expected and observed behavior, support source, scope, and acceptance
  level. Follow the root `AGENTS.md` and preserve unrelated work.

## Always-enforced contract

- Pin the source, build, runtime, switches, target, and reproduction identities
  needed for the claim.
- Establish a trustworthy RED and prove the identities and path relevant to the
  claim. Before changing runtime behavior, prove that the intended artifact,
  runtime, and target path execute.
- Inspect history to recover prior intent, the protected invariant, and its
  regression. Identify the defining authority; check assumptions, architecture
  fit, and provenance before using QEMU or other translator precedent. Precedent
  is not proof.
- Find the earliest observable divergence and the layer that owns the broken
  invariant. If LAT does not own the defect, report that boundary instead of
  forcing a LAT change.
- Use falsifiable, preferably single-variable experiments. Diagnostic probes
  may distinguish hypotheses, but they are not GREEN or root-cause fixes;
  remove them before final validation.

## Read conditional guidance

| Condition | Read |
| --- | --- |
| Non-trivial, uncertain, intermittent, target-host, or field reproduction | [reproduction-integrity.md](references/reproduction-integrity.md) |
| Translator/codegen or translator signal handling; dump, GDB, or disassembly | [translator-root-cause.md](references/translator-root-cause.md) |
| Translator metadata, layout, registers, gates, or path symmetry | [translator-invariant-checklist.md](references/translator-invariant-checklist.md) |
| Regression, test seam, matrix, sentinel, intermittent protocol, or field gate | [regression-and-matrix.md](references/regression-and-matrix.md) |
| Guest/native, loader, wrapper, callback, shared state, or lifecycle | [cross-boundary-state-fixes.md](references/cross-boundary-state-fixes.md) |
| Evidence-class dispute, support/acceptance owner, CI/review identity, product acceptance, or handoff | [evidence-and-closure.md](references/evidence-and-closure.md) |

When a repeatable identity manifest is useful, run the bundled
`scripts/capture_fix_context.py` from this skill directory with `--repo` and
the relevant artifact or runtime inputs. It emits JSON to stdout unless
`--output` is provided. Use `--redact-paths` and review it before sharing.

## Diagnose, implement, and validate

Execute only the stages authorized by the task and supported by current
evidence. Diagnosis-only work may gather evidence and rerun reproductions, but
it must not edit repository files or enter fix-specific RED/GREEN validation.

1. Reproduce the exact failure and build the smallest useful working/failing
   comparison. Narrow recent regressions under the same configuration and
   isolated reusable caches.
2. Trace expected semantics to the first confirmed divergence. Name the broken
   invariant, responsible layer, evidence, ruled-out alternatives, and remaining
   hypotheses before calling the root cause established.
3. If implementation is authorized, make the smallest general change at the
   owning layer. Do not add application names, benchmark addresses, or
   machine-specific paths; keep diagnostic and behavior-neutral preparation
   separately attributable.
4. Prove focused RED/GREEN and protect the earlier invariant. Derive coverage
   from behavior and risk; each result must prove its intended path. A synthetic
   seam proves only that seam.
5. Re-run the original application or complete reproduction to its expected
   output, state, and natural termination. An absent crash, moved error, or
   unobserved target path is not completion.
6. Run affected focused tests and the complete `lat-pr-fast` suite before a pull
   request. Run relevant `latx-integration` tests when available; otherwise name
   the remaining gate. Use LoongArch evidence for LoongArch-specific claims.
7. Compatibility may fail open by design, but validation must fail closed when
   the requested component, binding, callback, or endpoint is not observed.

## Report the result

Report separately:

- pinned identity, scope, support source, and expected versus observed behavior;
- confirmed root cause, owning invariant, prior protection, and open hypotheses;
- change or proposed change, RED/GREEN, actual-path proof, and regression scope;
- original-reproduction result and exact tests run, not run, failed, or blocked;
- what is proved, what is not proved, and the next gate or authorized action.

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
