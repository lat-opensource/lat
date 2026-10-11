# SPECint2000 train performance baseline

This is the performance baseline for LATC changes after commit `1f9e39be7b`.
Future comparisons must use translation efficiency:

```text
translation efficiency = LA native runtime / translated runtime * 100%
```

Higher is faster. LA native is 100%. Each runtime below is the median of
three measured rounds. The suite score is the geometric mean of the twelve
per-benchmark efficiency percentages.

## Test conditions

- Host: Loongson 3A6000 at 2.5 GHz (`zenglu-pc`)
- OS/kernel: Loongnix 20, `4.19.0-19-loongson-3`
- Workload: SPECint2000 train
- CPU affinity: CPU 2
- ASLR: disabled for all three modes
- LAT mode: AOT cache enabled and TU code loaded from the warm cache
- LAT runner SHA-256:
  `a40c95e95eab2db1cd06727b8bfc12508a5389a4dc569095dfca048c12c5fdd7`
- LATC binary-set SHA-256:
  `75741a8b9e13ad54fa370f2d7632150fd78dc197076fad13cd235f9cf293770c`

Before LAT AOT measurement, each benchmark uses a fresh isolated `HOME`.
The comparison script repeats the generation run until that benchmark's
`~/.cache/latx/` contains a non-empty `.aot2`, then runs one additional hot
cache verification. Measured rounds start only after this check. All 12 cache
checks passed; every cache contained one non-empty benchmark AOT file.

All 108 measured executions (12 benchmarks, 3 modes, 3 rounds) passed SPEC
output validation. The largest three-round coefficient of variation was
0.50% for LATC, 0.36% for LAT AOT, and 0.46% for LA native.

## Results

| Benchmark | LATC runtime (s) | LATC efficiency | LAT AOT runtime (s) | LAT AOT efficiency | LA native runtime (s) | LA native efficiency |
|---|---:|---:|---:|---:|---:|---:|
| 164.gzip | 9.953886 | 94.67% | 9.835301 | 95.81% | 9.423102 | 100.00% |
| 175.vpr | 5.943429 | 78.45% | 5.988289 | 77.86% | 4.662660 | 100.00% |
| 176.gcc | 1.077409 | 60.08% | 1.112949 | 58.16% | 0.647304 | 100.00% |
| 181.mcf | 3.522801 | 100.09% | 3.636585 | 96.96% | 3.525933 | 100.00% |
| 186.crafty | 6.266811 | 56.25% | 6.140847 | 57.41% | 3.525237 | 100.00% |
| 197.parser | 2.149886 | 72.02% | 2.105218 | 73.54% | 1.548261 | 100.00% |
| 252.eon | 2.100206 | 59.87% | 2.112687 | 59.51% | 1.257320 | 100.00% |
| 253.perlbmk | 18.365572 | 51.77% | 20.171574 | 47.13% | 9.507302 | 100.00% |
| 254.gap | 1.656873 | 73.11% | 1.803804 | 67.15% | 1.211263 | 100.00% |
| 255.vortex | 2.881117 | 54.25% | 3.659978 | 42.70% | 1.562884 | 100.00% |
| 256.bzip2 | 9.092407 | 58.74% | 9.047386 | 59.03% | 5.340811 | 100.00% |
| 300.twolf | 3.673861 | 77.55% | 3.690232 | 77.21% | 2.849128 | 100.00% |
| **Geometric mean** | - | **68.20%** | - | **65.74%** | - | **100.00%** |

LATC's geometric-mean runtime is 3.62% lower than warm LAT AOT at this
baseline. This comparison is secondary to translation efficiency; future
performance reports must lead with the three efficiency percentages.

## Reproduction

The comparison tool now measures M4, AOT v2, old AOT, and native LoongArch.
Use at least five rounds, warm old-AOT cache checks, a pinned CPU, and ASLR
disabled when the host permits it:

```sh
python3 spec2000/compare-native-train.py \
  --runner /path/to/latx-x86_64 \
  --latc-dir /path/to/latc/specbin-native \
  --aot-v2-dir /path/to/latc/specbin-aot-v2 \
  --aot-v2-module-dir /path/to/latc/modules \
  --aot-v2-runtime-dir /path/to/latc/runtime \
  --native-dir /path/to/specbin/la_gcc12_2_0 \
  --spec-root /path/to/spec2000-x64 \
  --workdir /path/to/results \
  --rounds 5 --cpu 2 --timeout 90 --lat-cache warm --disable-aslr
```

The generated `baseline.md` is the comparison table. `report.json` retains
all samples, coefficients of variation, cache filenames, cache sizes, and
runner hashes.
