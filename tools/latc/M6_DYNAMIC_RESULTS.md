# AOT v2 M6 dynamic application results

## Environment and workload

- Date: 2026-08-27
- Host: `3a6000-25g`, LoongArch 3A6000 at 2.5 GHz, 16 KiB host pages
- Runner: `/home/zenglu/latc-m6-runner/latx-x86_64`
- Rootfs: `/home/zenglu/loongrun-linux64-runtime/rootfs`
- Application: x86-64 `/bin/bash` with the dynamic loader, libc, and libtinfo
- Test: `tests/test-aot-v2-real-app.sh`
- Workload: Bash arithmetic loop. Cold and warm runs use 250000 iterations;
  the resource run uses 500000 iterations; each sharing process uses 700000
  iterations.

All four runs produced the exact expected sum and exited successfully. The
test submitted and compiled four ELF modules through `latcd`.

## Cold and warm observations

| condition | elapsed |
|---|---:|
| empty cache, JIT while `latcd` compiles | 8653 ms |
| four cached AOT modules | 10265 ms |

The warm run was not faster. This result is expected from the measured partial
coverage and must not be presented as a performance gain: the four modules
recorded only 141 AOT lookups and 9519 JIT fallbacks. Runtime translation
recorded 7772 attempts and 7772 generated TBs.

| source SHA-256 prefix | AOT lookups | JIT fallbacks | registration |
|---|---:|---:|---:|
| `6b4a45352fd0c` | 57 | 2516 | 2 us |
| `5c19747909b381` | 1 | 12 | 111 us |
| `02bcda52c1a5d` | 66 | 2050 | 91 us |
| `55b89ab22bee` | 17 | 4941 | 175 us |

## Long-run memory samples

The test collected 133 `smaps_rollup` samples over 16.35 seconds.

- RSS: 31984 KiB minimum, 44112 KiB maximum.
- PSS: 28736 KiB minimum, 40532 KiB maximum.
- Last 20 samples: RSS 43920-44112 KiB and PSS 40340-40532 KiB.

The initial rise corresponds to runtime TB generation. The last 20 samples
remain within 192 KiB, so this run did not show continued linear growth after
the translation set became hot. This is a bounded observation for this
workload and duration, not a general proof for every application.

## Two-process executable-page sharing

Both concurrent processes mapped the same four cache module paths. Summed over
the AOT module mappings:

- combined RSS: 9328 KiB
- combined PSS: 5167 KiB
- combined Shared_Clean: 8208 KiB

PSS was substantially lower than RSS and 8208 KiB was reported as clean shared
memory. This proves that these two processes shared the AOT module pages in
this run.

The complete machine-readable report remains at
`/home/zenglu/latc-25g/tools/latc/build/aot-v2-real-app-test/report.json` on
`3a6000-25g`.
