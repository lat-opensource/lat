# AOT v2 M7 test plan

## Reproduction and profiling

- Verify the remote host is idle enough for measurement and record the runner,
  M4 ELF, AOT v2 ELF, module, runtime, and guest SHA-256 values.
- Run alternating M4/AOT v2 `176.gcc` samples at least five times on CPU 4 and
  require valid SPEC output. Repeat enough times to show the slowdown is stable.
- Confirm `252.eon` shows the same class of slowdown before generalising the
  cause.
- Collect cycles, instructions, IPC, branches, branch misses, task-clock and
  page faults with `perf stat`; collect call-graph or sampled-PC profiles with
  `perf record` where the kernel permits it.

## Focused regression

- Convert the shortest real reproducer into a remote performance test that
  alternates modes and rejects invalid SPEC output.
- Add correctness coverage for any changed lookup, cache, dispatch, address
  conversion or generated-code path.
- Measure before and after with identical input hashes and CPU affinity.

## Full validation

- Run M4, AOT v2, old AOT and true native on all 12 SPECint2000 train programs,
  five measured rounds each, with warm old-AOT cache verification.
- Require AOT v2/M4 geometric-mean speed ratio of at least `0.95x` and 12/12
  valid SPEC results.
- Re-run the dynamic Bash cold, warm, long-run and two-process sharing test;
  report startup, registrations, AOT hits, JIT fallbacks, runtime TBs and RSS/PSS.
- Run `make -C tools/latc test`, `test-aot-v2-tsan`, and the complete remote
  AOT v2 regression. Search for and remove all temporary debug tags.
