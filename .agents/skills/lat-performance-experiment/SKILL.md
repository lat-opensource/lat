---
name: lat-performance-experiment
description: Use for LAT performance optimization from bottleneck discovery through candidate validation and product decision, including benchmark, AOT/TU, EFLAGS, PMU, and regression work. Requires correctness gates and controlled measurement; not for ordinary fixes or read-only review.
---

# LAT performance experiment

Determine separately whether an optimization mechanism works and whether the
complete workload improves.

For the worked example behind this distinction, read
[references/eflags-call-ret-case.md](references/eflags-call-ret-case.md) when the
task involves EFLAGS/CALL/RET or when mechanism and end-to-end results conflict.

When the bottleneck or candidate is not yet established, read
[references/bottleneck-discovery-and-candidate.md](references/bottleneck-discovery-and-candidate.md).

## Establish or receive a candidate

- Require a product scenario, representative workload, user-visible outcome,
  primary metric, and stable baseline with understood variation. Do not assume
  that a familiar pass, instruction, or subsystem is the bottleneck.
- If no evidence-backed candidate exists, read the bottleneck-discovery
  reference. Without implementation authorization, stop with its candidate
  contract and implementation plan.
- Require the candidate to predict a mechanism metric, end-to-end effect, and
  disconfirming result, with an actual-path sentinel and correct fallback. Keep
  it reversible and attributable, but do not prescribe the profiler, pass, or
  implementation; choose or invent what best distinguishes the hypotheses.

## Define the experiment

- Write two scorecards before measuring:
  1. correctness and intended mechanism;
  2. end-to-end product performance.
- Define whether the decision concerns an enabled product optimization or
  default-off research scaffolding, and identify the responsible decision owner.
- Pin baseline and candidate source revisions, binary identities, build options,
  host, guest runtime, switches, caches, workload, and input.
- Keep baseline and candidate artifacts and caches isolated. Do not compare
  mixed binaries, stale AOT artifacts, or changing runtime roots.
- Inspect relevant history and invariants before transplanting or reviving an
  old optimization.
- Before running, define the primary metric, run order and repetition rule,
  aggregation and uncertainty method, acceptable regression budget, minimum
  desired gain, and stopping condition.

## Validate correctness and path identity

1. Run focused correctness and fallback tests before performance measurement.
2. Prove that the candidate path actually ran. For AOT, file creation or a
   second successful launch is insufficient when stale cache or JIT fallback
   can pass.
3. Reject or repair a candidate that changes correctness; do not trade product
   semantics for benchmark speed.

## Measure the candidate

1. Use representative workloads. Prefer SPEC `ref` or provenance-preserved
   precompiled x86 applications when small training inputs or synthetic tests do
   not represent the product question.
2. Measure baseline and candidate in the same environment with multiple
   interleaved paired runs. Retain raw per-run results and abnormal terminations.
3. Verify every expected process and output completed; do not compute a general
   result from a partial run.
4. Isolate the candidate's incremental effect. Treat instruction counts, code
   size, cache misses, and PMU counters as mechanism evidence rather than
   substitutes for end-to-end time. Define the exact code-size or counter metric
   before using it as evidence.
5. Investigate contradictory evidence with falsifiable hypotheses. Do not turn a
   workload-specific explanation such as L1I behavior into a universal cause.

## Report and decide

Report these separately:

- baseline and candidate identities and environment;
- correctness and path-identity evidence;
- workload provenance, run order, raw results, and aggregate comparison;
- mechanism result and end-to-end result;
- contradictory or negative evidence;
- decision, default state, and remaining production gate.

Use one of these terminal states:

- **continue investigating** when evidence is insufficient;
- **mechanism validated, product gate failed** when the idea works internally but
  the complete workload does not improve; retain the research and keep it off by
  default;
- **candidate rejected** when the measured direction is not worth retaining;
- **product candidate** only when correctness, representative performance, and
  required integration evidence pass.

Merging default-off research scaffolding requires a separate explicit rationale
and maintenance-cost decision; do not infer that authorization from a request to
evaluate product performance.

Do not publish or enable a candidate by default unless the task explicitly
authorizes that state change.
