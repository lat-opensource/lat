# AOT v2 M2 test plan

## ET_DYN and load bias

- Build a small x86-64 PIE fixture and generate an AOT v2 module.
- Run `make test-aot-v2-pie` to generate the image twice under ASLR and require
  byte-identical output before executing the module at two load biases.
- Register the module at two synthetic guest load biases and verify entry/TB
  lookup returns the expected Host address for both instances.
- Reject source SHA-256, codegen identity, ABI, and CPU feature mismatches.
- Re-run the existing `ET_EXEC` static runner tests.

## Mapping discovery

- Trace executable mappings for the PIE main program, `PT_INTERP`, and libc.
- Verify each completed ELF mapping produces one module instance with the
  correct guest range and load bias.
- Verify a missing or rejected module falls back only for that guest ELF.
- Verify repeated mmap notifications do not duplicate a registration.

## Dynamic hello

- Run the dynamic glibc hello with ASLR enabled and with
  `setarch loongarch64 -R`.
- Compare stdout and exit status with native x86 execution.
- Run cold and warm module-cache cases and retain per-module AOT/JIT counters.
- Exercise one libc TLS access and the normal libc startup IFUNC path.

## Execution and regressions

- Assert that the final direct-target path does not allocate compatibility
  `TranslationBlock` objects for AOT hits.
- Exercise Host signal recovery inside module text and compare the recovered
  guest PC and state with the existing LAT path.
- Run no-libc hello, static glibc hello, and SPECint2000 train 12/12 in strict
  M1 mode with zero runtime TB generation.
- Record focused startup and steady-state measurements; reject a clear
  regression, but do not make general performance tuning an M2 exit gate.

## 2026-08-26 result

- `test-aot-v2-glibc-runner`: passed; explicit module and warm-cache paths
  reported zero runtime translation and `compat_tb_allocations=0`.
- `test-aot-v2-dynamic-runner`: passed for cold cache, warm main/loader/libc
  cache, ASLR, and no-ASLR execution.
- `test-aot-v2-signal-runner`: passed; an AOT integer divide exception reached
  the guest `SIGFPE` handler through the generated PC map with zero JIT.
- SPECint2000 train: 12/12 valid. Final clean-runner times in seconds were gzip
  9.94, vpr 6.08, gcc 1.14, mcf 3.99, crafty 7.07, parser 3.08, eon 4.29,
  perlbmk 21.14, gap 1.79, vortex 2.89, bzip2 9.08, and twolf 3.67. Ref was
  not run.
- The removed proxy and direct FastTB path ran the same gzip input in 9.93 and
  9.94 seconds respectively on the same host.
