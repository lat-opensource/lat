# AOT v2 M7 performance design

## Feedback loop

Use `176.gcc` and `252.eon` as focused workloads because M6 measured AOT v2
at about 2.8x and 3.2x the M4 runtime respectively. Pin every run to CPU 4.
The first deliverable is a focused differential runner that executes the exact
SPEC train command, validates output, and reports at least five alternating M4
and AOT v2 samples. It must finish in under 30 seconds for `176.gcc`.

Collect `perf stat` counters for both modes and `perf record` profiles for the
AOT v2 mode. Attribute symbols through the AOT runtime and generated module.
Keep raw perf data on the remote host and record binary hashes in the summary.

## Hypothesis testing

Rank three to five falsifiable causes after reproducing the focused slowdown.
Each probe changes one variable. Likely categories to test, without assuming
any is correct, are hot lookup cost, extra dispatch transitions, module text
layout or I-cache behaviour, and runtime fallback accidentally remaining on a
supposedly complete module.

## Optimization rules

Optimize only a path shown by profile or counter evidence. Preserve immutable
published snapshots, signal-safe reads, generation checks, HWCAP selection,
and safe invalidation from M6. Add a focused performance guard for the exact
lookup or dispatch path that changes when it can be stable on the 3A6000 host;
keep correctness assertions in normal tests and machine-dependent timing
thresholds in the remote performance test.

## Validation

After focused workloads improve, run the real dynamic Bash test and the full
four-mode SPEC comparison with five samples per benchmark. Compare against the
committed M6 raw report, not an older single-run result. Re-run local unit and
ThreadSanitizer tests plus the complete remote AOT v2 regression.
