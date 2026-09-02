# EFLAGS CALL/RET: mechanism evidence versus product performance

This is a historical LAT performance case study, not a current-master benchmark
claim. Revalidate every source revision, binary, cache, runtime, host, and
workload before reusing its quantitative results.

## The apparent contradiction

The candidate allowed selected EFLAGS analysis to continue across proven-safe
CALL relationships. Correctness tests and generated-code inspection showed that
redundant state computation could be removed.

In the historical experiment:

- the generated AOT code set became modestly smaller;
- the complete SPEC workload became modestly slower.

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

## The later explanation and its boundary

In a controlled `crafty` run, the strongest observed explanation pointed to code
layout and L1I effects: retired instructions were nearly unchanged, while L1I
misses and cycles increased. That explained the controlled case better than the
original instruction-count story, but it did not prove L1I was the only cause of
the complete SPEC result. BTB, iTLB, address phase, and workload-specific effects
could still matter.

A later layout candidate improved one AOT mode but was neutral or mixed in the
default mode, so it remained off by default.

## Reusable lesson

For comparable optimization work:

1. define correctness/mechanism and end-to-end scorecards separately;
2. prove the candidate artifact and runtime path;
3. retain contradictory and negative results;
4. keep a mechanism conclusion even when the product gate fails;
5. do not generalize a microarchitectural explanation beyond the workload and
   evidence that support it.
