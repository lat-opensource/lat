# AOT v2 M3 test plan

## Post-startup loading

- Build a PIE main executable and a separate x86-64 DSO.
- Generate cache modules for main, interpreter, libc, libdl where applicable,
  and the test DSO.
- Run cold and warm cache cases with ASLR enabled and disabled.
- Require the DSO to be discovered only after `dlopen()` and registered once
  its executable mappings are complete.
- Compare stdout and exit status with native x86 execution.

## Cross-module semantics

- Call a function from a startup DSO and from a `dlopen()` DSO.
- Pass a main-program callback into the DSO and call it from DSO code.
- Exercise one TLS variable, one IFUNC resolver, one versioned symbol, symbol
  interposition, and `LD_PRELOAD` in focused fixtures.
- Require guest addresses and guest PLT/GOT resolution; reject Host symbol
  resolution as a substitute.

## Close and reload

- Loop load, call, close, and reload at least 100 times.
- Force or observe a different guest load bias and verify the new instance.
- After close, assert that registry and dispatcher-cache lookup cannot return
  the inactive instance.
- Deliver a signal inside the reloaded DSO and verify PC-map recovery selects
  the active load bias.

## Regression and performance

- Run static glibc hello, dynamic three-module hello, and AOT signal recovery.
- Run SPECint2000 train 12/12 with a 60-second per-benchmark limit and no ref.
- Require zero runtime TB generation for strict static modules.
- Compare focused cross-module call and callback costs before and after the
  module-aware fast path; reject a clear dispatcher regression.

## WI-2262 result

- Native x86 fixture output: `dlopen callback result=40`.
- `3a6000-25g` cold cache: main, interpreter, libc, and plugin were discovered;
  output matched native execution and each missing module used local JIT.
- Warm cache with ASLR and no-ASLR: all four modules registered and executed
  AOT targets; the plugin had nonzero AOT lookups and
  `compat_tb_allocations=0`.

## WI-2263 result

- Native and AOT output matched exactly:
  `startup=11 tls=7,9 ifunc=23 versions=31,32 hook=10 preload=77`.
- The startup DSO called a main callback; the plugin preserved thread-local
  state, selected its IFUNC implementation, and resolved both `LATC_1.0` and
  default `LATC_2.0` symbol versions.
- `hook=10` proves main-executable interposition won over the preload DSO's
  competing value of 100. `preload=77` proves the guest preload was active.
- Warm cache registered main, startup DSO, preload DSO, interpreter, libc, and
  plugin. All six reported nonzero AOT lookups and no compatibility TBs.

## WI-2264 result

- The fixture performed 100 load/call/close cycles. After each close it
  reserved the old five-page guest range, forcing the next load to a different
  bias. Native and AOT output both reported `loads=100 result=40 moved=1`.
- Warm-cache and no-ASLR runs registered and executed 100 plugin instances,
  then reported 100 deactivations and 100 inactive module-stat records.
- The tracker unit test removes one overlapping ELF range, preserves the other
  two, and reports no second removal for the same range.

## WI-2265 result

- The focused fixture performs 100,000 calls from a `dlopen()` DSO back into
  the main executable, causing 200,000 cross-module transfers.
- Before the fast path, six 1,000,000-loop runs on `3a6000-25g` took
  326, 327, 323, 323, 323, and 323 ms. The first context-check-only version
  regressed to 424-436 ms and was rejected.
- The final inline context switch took 97, 92, 94, 89, 88, and 92 ms for the
  same workload, about 71% below the stable 323 ms baseline.
- The 100,000-loop regression run reported `direct_targets=169` and
  `compat_tb_allocations=0`. The test requires fewer than 10,000 dispatcher
  targets, which distinguishes the inline path from the old per-transfer
  dispatcher behavior without depending on wall-clock noise.
- The `x64-v3` runner passed SPECint2000 train 12/12 on `3a6000-25g`:
  `164.gzip` 9.967 s, `175.vpr` 5.971 s, `176.gcc` 1.112 s,
  `181.mcf` 3.631 s, `186.crafty` 6.268 s, `197.parser` 2.164 s,
  `252.eon` 2.099 s, `253.perlbmk` 19.693 s, `254.gap` 1.830 s,
  `255.vortex` 3.127 s, `256.bzip2` 9.210 s, and `300.twolf` 3.778 s.
  Every benchmark completed within 60 seconds and reported zero runtime TB
  generation. SPEC ref was not run.
