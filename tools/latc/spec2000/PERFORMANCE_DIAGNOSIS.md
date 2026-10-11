# LATC SPECint2000 train performance diagnosis

The baseline at commit `1f9e39be7b` is 68.20% translation efficiency. Reaching
70.00% requires a 2.57% geometric-mean runtime reduction:

```text
required speedup = 70.00 / 68.20 = 1.02639
required runtime reduction = 1 - 1 / 1.02639 = 2.57%
```

The first implementation target is to generate LATC code through LAT's full TU
translator. Static pretranslation currently calls `tb_gen_code()` once for
each CFG TB, while warm LAT AOT calls `translate_by_tu()` from
`translate_seg()`. The native images confirm that LATC is effectively in TB
mode: only 0.0% to 0.1% of exported TB records are internal TU members.

## Test conditions

- Host: Loongson 3A6000 at 2.5 GHz (`zenglu-pc`)
- Kernel: Loongnix `4.19.0-19-loongson-3`
- Workload: SPECint2000 train, CPU 2, ASLR disabled
- LAT comparison: warm AOT cache with `LATX_AOT=1` and `LATX_TU=1`
- LATC image commit: `1f9e39be7b`
- Samples: two measured rounds; counters below are the median

The wrappers and AOT cache are the same validated inputs used by
`PERFORMANCE_BASELINE.md`. All measured commands passed their official SPEC
output checks before counter collection.

## Core counters

| Benchmark | Mode | Cycles (B) | Instructions (B) | IPC | Branches (B) | Branch misses (M) | Miss rate |
|---|---|---:|---:|---:|---:|---:|---:|
| 186.crafty | LATC | 15.653 | 45.199 | 2.888 | 4.651 | 121.178 | 2.605% |
| | LAT AOT | 15.413 | 43.923 | 2.850 | 4.653 | 120.313 | 2.586% |
| 197.parser | LATC | 5.374 | 12.400 | 2.308 | 1.814 | 57.958 | 3.195% |
| | LAT AOT | 5.242 | 12.190 | 2.325 | 1.889 | 57.140 | 3.025% |
| 253.perlbmk | LATC | 45.574 | 119.241 | 2.616 | 17.203 | 107.648 | 0.626% |
| | LAT AOT | 54.480 | 117.959 | 2.165 | 18.647 | 109.391 | 0.587% |
| 254.gap | LATC | 4.134 | 11.152 | 2.698 | 1.616 | 17.832 | 1.103% |
| | LAT AOT | 4.460 | 11.529 | 2.585 | 1.872 | 16.863 | 0.901% |
| 255.vortex | LATC | 7.206 | 26.196 | 3.635 | 3.284 | 9.402 | 0.286% |
| | LAT AOT | 9.141 | 26.691 | 2.920 | 4.120 | 8.959 | 0.217% |
| 256.bzip2 | LATC | 22.706 | 86.801 | 3.823 | 7.810 | 98.152 | 1.257% |
| | LAT AOT | 22.622 | 87.149 | 3.852 | 8.436 | 98.290 | 1.165% |

LATC executes 2.9% more instructions than warm LAT AOT in crafty and 1.7% more
in parser. Branch counts are already lower in parser, bzip2, gap, vortex, and
perlbmk. The large gap and vortex wins come from fewer branches and higher IPC,
not fewer branch misses.

Two additional cache-counter rounds do not support an instruction-cache miss
regression as the primary cause. LATC L1 instruction-cache misses were lower
than LAT AOT for crafty, parser, and bzip2. The Loongnix 4.19 iTLB event was
noisy between rounds and is not used to rank candidates.

## Native-image structure

`analyze-native-relocations.py` scanned all twelve native images. Every
`TB_TARGET` relocation resolved to the exact `(guest_pc, flags)` entry; no
target needed C lookup at image load or remained missing. Representative TU
composition is:

| Benchmark | TB records | TU heads | Internal TU members | Internal members |
|---|---:|---:|---:|---:|
| 186.crafty | 51,876 | 51,838 | 38 | 0.07% |
| 197.parser | 54,226 | 54,189 | 37 | 0.07% |
| 253.perlbmk | 85,909 | 85,816 | 93 | 0.11% |
| 255.vortex | 73,016 | 72,954 | 62 | 0.08% |
| 256.bzip2 | 37,860 | 37,823 | 37 | 0.10% |

All two-slot direct edges retain the original source semantics. Edges from a
JIRL epilogue write register `$a0`; edges from a branch epilogue write `$zero`.
Changing every pair to `nop; b` is invalid because the JIRL target uses the
`$a0 = PC + 8` value. The earlier eon failure demonstrated this dependency.

No `JRRA_TARGET` relocation exists in these twelve images. This confirms that
return-address-stack work is a later optimization and is not the present LATC
versus LAT AOT difference.

## Ranked implementation targets

1. **Use the full TU translator for static pretranslation.** Replace the
   per-CFG-TB `tb_gen_code()` loop in
   `tools/latc/lat/linux-user/latc-bundle-loader.c` with a LAT-owned TU entry
   that accepts the validated static TB set. Expected counters: fewer dynamic
   instructions in crafty/parser and fewer TB-exit branches across the suite.
   Main risk: TU exploration must not decode outside validated executable
   ranges or lose supplemental profile targets.
2. **Reduce the remaining indirect-dispatch glue after TU generation.** The
   `epilogue_ret_0` relocation still routes indirect x86 targets through the
   eight-instruction cache hit in `native/runtime/dispatch-x86.S`. Measure hit
   frequency after TU is enabled, then specialize only if it can materially
   contribute to the remaining gap. Main risk: preserving all guest registers
   and cache-collision behavior.
3. **Revisit code placement only after instruction counts are aligned.** bzip2
   is 0.4% slower in cycles with nearly equal instruction count, but its L1 I
   cache misses are not higher. Layout may affect branch prediction or fetch,
   but current counters do not justify changing global placement first.
4. **Defer RAS and PGO.** LAT does not currently translate x86 `ret` using a
   hardware return-address stack path, and the images contain no JRRA target
   relocations. The sparse PGO experiment remains stashed and outside this
   work.

TU generation is the only candidate that both matches the observed extra
instructions and applies broadly enough to supply the required 2.57% suite
runtime reduction. The next work item should implement it first and rerun all
twelve train benchmarks with three measured rounds.

## Reproduction

Collect counters with the validated comparison wrappers:

```sh
tools/latc/spec2000/perf-specint-selected.sh \
  /home/zenglu/spec2000-x64 \
  /tmp/latc-baseline-1f9e39be7b/results-r3/wrappers \
  /tmp/latc-perf-selected.csv
```

Inspect the generated native images:

```sh
python3 tools/latc/spec2000/analyze-native-relocations.py \
  '/tmp/latc-images-1f9e39be7b/*.latnative' \
  --output /tmp/latc-relocation-stats.json
```
