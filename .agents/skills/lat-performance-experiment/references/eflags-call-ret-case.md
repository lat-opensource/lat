# EFLAGS CALL/RET: mechanism evidence versus product performance

This worked example captures a reusable decision pattern, not a benchmark claim
about any source revision or workload. Establish new source, binary, cache,
runtime, host, and workload evidence before applying it.

## The apparent contradiction

Suppose a candidate safely carries selected EFLAGS analysis across proven CALL
relationships. Correctness and generated-code evidence show less redundant
state computation, while controlled end-to-end measurements become slower.

These measurements answer different questions. Static code reduction showed the
mechanism worked; slower complete runtime showed the product-performance gate did
not pass.

## The correct decision

Use **stop, keep, ask**:

- **stop** describing or merging the candidate as a completed product
  optimization;
- **keep** the correctness and mechanism result that safe redundant computation
  can be removed;
- **ask** which new cost offset the saved computation.

Do not choose between “discard the entire direction” and “merge because one
metric improved.” Keep each conclusion within its evidence.

## Investigate the offsetting cost

Treat layout, L1I, BTB, iTLB, branch behavior, register pressure, and other
effects as hypotheses, not automatic explanations. Select measurements that can
distinguish them, predict which mechanism metric and end-to-end result should
change, and reject an explanation when its prediction fails.

A follow-up candidate that helps only a non-default mode or one narrow workload
does not repair the product result. Keep it off by default unless its own support
scope and maintenance cost are explicitly accepted.

## Reusable lesson

For comparable optimization work:

1. define correctness/mechanism and end-to-end scorecards separately;
2. prove the candidate artifact and runtime path;
3. retain contradictory and negative results;
4. keep a mechanism conclusion even when the product gate fails;
5. do not generalize a microarchitectural explanation beyond the workload and
   evidence that support it.
