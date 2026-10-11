# AOT v2 M4 test plan

## Protocol and snapshot tests

- Accept version 2 `SUBMIT_KEYS`, `FLUSH_SOURCE`, and `FLUSH_ALL` packets with
  exactly two, one, and zero read-only regular-file descriptors respectively.
- Reject bad magic/version/size, truncated packets, no FD, multiple FDs,
  writable FD, directory, pipe, oversized source, and non-x86 ELF.
- Prove the digest is computed from the copied bytes, not a submitted path.
- Detect source metadata changes during snapshot creation.
- Leave no snapshot or output temporary file after every failure path.

## Publication tests

- Compile one static fixture through `latcd --once` on 3A6000, inspect the
  result, and compare its embedded source digest with the submitted ELF.
- Interrupt the compiler before completion and prove the final cache path is
  absent or still contains the prior valid module.
- Feed malformed compiler output and prove it is rejected before rename.
- Replace an invalid existing cache entry atomically with a valid module.

## Resident service tests

- Submit the same bytes through different file names and prove one compile.
- Submit distinct startup and DSO jobs and verify priority order.
- Fill the queue and verify new low-priority requests fail quickly.
- Trigger repeated deterministic failure and verify the negative-cache delay;
  change source or codegen identity and verify compilation is allowed.
- Stop the service during a job, restart it, and verify temporary cleanup and a
  later successful publication.

## Runner tests

- With no daemon, run static hello, dynamic hello, and the dlopen fixture and
  verify JIT results are unchanged.
- On an empty cache with a running daemon, verify the first dynamic hello exits
  before compilation is required to finish and valid modules appear later.
- Run dynamic hello again and verify the main, interpreter, and libc modules
  load from AOT with no source mismatch.
- Verify anonymous executable memory and guest JIT mappings are not submitted.
- Verify each source identity is submitted at most once per process.

## Concurrency and regression

- Launch 20 cold processes for the same program and prove one compilation per
  job key and one valid final artifact.
- Run M1 static, M2 dynamic, M3 dlopen/reload, signal recovery, and SPECint
  train 12/12. Each SPEC test is limited to 60 seconds; SPEC ref is excluded.
- Record AOT hits, JIT fallback reasons, queue counts, deduplicated requests,
  compiler failures, negative-cache hits, and publication counts.

## WI-2273 result

On 2026-08-26, `make test-latcd-once` passed on `3a6000-25g` with the M3
`x64-v3` runner. The published static hello module had source SHA-256
`b78483d6116cc715513278c745e1779883c750074cdf29f824e3123a10e28632`,
3 TBs, 263 precise PC-map records, and mode `0444`. Strict execution reported
the module as registered, `aot_lookups=2`, `jit_fallbacks=0`, and
`compat_tb_allocations=0`.

The same test rejected compiler failure, a non-ELF source, a writable source
FD, and malformed compiler output without publishing a new module or leaving
temporary files. It also replaced a corrupt cache entry and then recognized
the valid module as a cache hit while the configured compiler was `/bin/false`.

Source metadata mutation during a large copy remains a later hardening test.

## WI-2274 result

On 2026-08-26, `make test-latcd-service` passed on `3a6000-25g`. Twenty
concurrent submissions of the same static hello produced `requests=20`,
`queued=1`, `compiled=1`, and `failed=0`. The duplicate and post-publication
cache-hit counts sum to 19, so only the first request entered the compiler;
their individual counts depend on whether publication finishes while the
remaining requests are being accepted. Alternating requests used two paths
with identical bytes, proving that the key is content SHA-256 rather than path.

The test also proved that a failed SHA is rejected during its negative-cache
delay, the CPU limit terminates a busy compiler, priority 200 runs before a
previously queued priority 1 job, task-count and total-byte queue limits reject
new work, and SIGTERM terminates an active compiler process group in less than
five seconds while removing queued and temporary snapshots. A connected client
that sends no request is rejected after one second instead of blocking the
service. Published modules have matching atomic `.current` indexes.

## WI-2275 result

On 2026-08-26, the M4 runner was rebuilt from a fresh staging tree on
`3a6000-25g`. The architecture-independent nonblocking client test and the
complete local `make test` passed.

With no daemon, static hello completed through JIT and reported one local
submission failure. With a daemon deliberately delaying compilation by one
second, static hello exited in 12 ms with its module still missing; the daemon
published the module later. Dynamic hello exited in 43 ms and submitted exactly
three identities: main and interpreter at priority 200, libc at priority 100.
The daemon then reported `requests=3`, `queued=3`, `compiled=3`, and `failed=0`,
with three modules, three current indexes, and no temporary files.

The next dynamic run registered all three modules, reported AOT lookups and
`direct_targets=101`, and made no compiler submission. Loader and libc remain
partial modules and can still report JIT fallbacks, matching the established M2
acceptance. Existing M2 dynamic and M3 100-cycle dlopen/reload tests also passed
with the new runner.

## M4 final regression

The final 3A6000-25g regression passed static glibc, precise signal recovery,
M2 dynamic, M3 100-cycle dlopen/reload, dynamic TLS/IFUNC/version/interposition
semantics, the latcd five-phase service test, and the runner cold/warm test.

All twelve SPECint2000 train modules were regenerated with the M4 runner and
passed. Every manifest entry was `valid=true`, `module_tbs` equalled
`native_tbs`, and both runtime TB-generation counters were zero. Times were:

```text
164.gzip       9.965333 s    175.vpr       6.044121 s
176.gcc        1.122332 s    181.mcf       3.640545 s
186.crafty     6.223693 s    197.parser    2.166699 s
252.eon        2.091868 s    253.perlbmk  19.550169 s
254.gap        1.826500 s    255.vortex    3.049790 s
256.bzip2      9.183712 s    300.twolf     3.721174 s
```

No test exceeded 60 seconds. SPEC ref was not run. The tested runner SHA-256 is
`99e33ae789a4b93559aaf86d89fe3839337dd4cebf95fcda883c79a957e2d4ec`;
its runtime library SHA-256 is
`255b2ff160d32a96ed2d5d4e77bb7efa55bb19019e69fba902d4f1278f34dbca`.
