# Experimental Quicktrans exit-time AOT generation

This opt-in mode records stable executable file mappings while Quicktrans runs,
then generates legacy TU/AOT code in a forked worker at normal final process exit.
It does not load AOT during collection or replace running Quicktrans blocks.

Build with `./latxbuild/build64.sh -c -f`. Collection is disabled by default.
For an isolated test cache, provide a separate HOME directory to the process:

```sh
env HOME=/path/to/test-home LATX_FAST_TRANSLATOR=1 \
  LATX_FAST_TRANSLATOR_STRICT=1 LATX_KZT=0 LATX_AOT=0 LATX_LOAD_AOT=0 \
  LATX_QUICKTRANS_AOT_COLLECT=1 /path/to/latx-x86_64 /path/to/program
```

To verify the generated cache using the same binary's legacy path:

```sh
env HOME=/path/to/test-home LATX_FAST_TRANSLATOR=0 LATX_KZT=0 \
  LATX_AOT=1 LATX_LOAD_AOT=1 LATX_AOT_GENERATE=0 \
  /path/to/latx-x86_64 /path/to/program
```

`LATX_LOAD_AOT` explicitly controls existing-cache loading; its previous default
is unchanged. `LATX_AOT_GENERATE` controls ordinary AOT generation. The separate
experimental collection switch explicitly requests generation even if that
ordinary control is off.

Collection requires strict Quicktrans, hard floating point, runtime AOT loading
off, and Wine/KZT disabled. It records coverage, not execution frequency. No
per-execution counters are inserted. Existing cached startup flags route guest
exit_group through normal cleanup only when collection is requested.

The worker is waited for at final exit, so process exit can take longer. Thread
exits, signals, and TB flushes do not trigger collection. Collection skips
initially writable mappings and removes coverage when permissions change or
mappings are unmapped/remapped. Anonymous/JIT code, SMC, unloaded modules and
previously flushed TBs are not supported coverage sources. A failed exclusive
snapshot or fork can skip generation. Cache compatibility remains subject to the
existing AOT version and source-file checks.

The current validation covers minimal static and PIE programs, both normal exit
syscalls, abnormal exits, disabled mode, and actual cache-load markers. It does
not establish application startup overhead, general mixed-backend correctness,
or multithread stress safety. Use isolated caches until those checks are added.
