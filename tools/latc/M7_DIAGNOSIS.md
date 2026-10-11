# AOT v2 M7 performance diagnosis

## Conclusion

The M6 four-mode comparison did not load the per-benchmark AOT v2 module.
`compare-native-train.py` executed the bundled LAT runner but omitted
`LATX_AOT_V2_MODULE`, `LATX_AOT_V2_SOURCE`, strict AOT, and pretranslation
disable settings. The measured mode translated 142851 TBs at startup and was
therefore JIT/pretranslation performance mislabeled as AOT v2 performance.

This explains the reported 42.47% geometric-mean efficiency. It is a benchmark
configuration defect, not evidence that the registered AOT v2 module hot path
is 2-3x slower than M4.

## Fast reproduction

On `3a6000-25g`, CPU 4, direct `176.gcc` train execution produced identical
SHA-256 output in both modes:

```text
output sha256 = 961773e367dc915a69beb876142c0cd2e83e5a973ef5a15f749d1f94c5849240
M4 median = 2.118240 s
misconfigured AOT v2 median = 5.825361 s
misconfigured AOT v2 / M4 runtime = 2.7501x
```

The slower mode reported `pretranslated=142851` and no runtime TB generation.
The zero runtime counter was misleading: translation happened during startup
pretranslation, before normal runtime generation was needed.

## Hypotheses and results

| rank | falsifiable cause | probe | result |
|---|---|---|---|
| 1 | the benchmark does not load its AOT module | inspect wrapper environment and profile translation symbols | confirmed: wrapper exported none of the required module variables |
| 2 | a complete module still falls back to runtime JIT | inspect strict-mode stats and translation counters | refuted for correct configuration: pretranslated=0 and runtime attempts/calls=0 |
| 3 | module verification or registration dominates runtime | compare verification time with total delta | refuted: bundle verification was 11-18 ms, versus a multi-second false delta |
| 4 | registered AOT dispatch doubles instructions or branches | compare perf counters after loading the module | refuted as the primary cause: correct AOT v2 is close to M4 |

The misconfigured profile was dominated by LAT translation symbols including
`decodeInstruction`, `X86_getInstruction`, `gitcapstone_get_from_insn`,
`ir2_assemble`, and `ir1_translate`.

## Counter comparison

One paired `176.gcc` sample gave:

| counter | M4 | misconfigured AOT v2 | correct AOT v2 |
|---|---:|---:|---:|
| cycles | 2.739 B | 7.074 B | 2.804 B |
| instructions | 5.640 B | 11.068 B | 5.831 B |
| branches | 0.905 B | 2.314 B | 0.885 B |
| branch misses | 17.20 M | 23.61 M | 15.41 M |

Correct AOT v2 used about 3.4% more instructions and 2.4% more cycles than the
paired M4 sample, not the 96% and 158% increases seen in the misconfigured run.

## Strict focused comparison

The new focused script sets the same module environment as
`run-specint-aot-v2.py`, disables ASLR for stable M4 relocations, alternates
modes, and requires valid SPEC output.

```text
176.gcc, five rounds:
  M4 median       1.133487 s
  AOT v2 median   1.145002 s
  AOT v2/M4       0.989943x

252.eon, five rounds:
  M4 median       2.087673 s
  AOT v2 median   2.269790 s
  AOT v2/M4       0.919765x
```

All 20 executions passed SPEC output validation. The `176.gcc` comparison
finished in 16.7 seconds and `252.eon` in 27.3 seconds.

## Required implementation order

1. Keep the strict module environment in both focused and full comparison
   tools. Missing module and runtime directories must be command-line errors.
2. Add test coverage that the generated AOT v2 wrapper exports module/source,
   strict AOT, pretranslation-disable, and runtime-library settings.
3. Repeat all 12 benchmarks before deciding whether the remaining 1-8% gap is
   a production hot-path issue. Only profile a real residual gap.
4. Re-evaluate dynamic Bash separately because it intentionally uses partial
   modules and its JIT fallback problem remains real.

Correctness risk is low for the benchmark fix but high for any later runtime
optimization. No runtime optimization is justified by the original M6 data.

## Benchmark guard implementation

`specint.strict_aot_v2_env_lines()` is now the single source for AOT v2
wrapper settings. Both focused and full comparison tools use it. The full tool
requires explicit module and runtime directories and records module SHA-256
values. The old-AOT wrapper also sets the runner library directory instead of
depending on the caller's environment.

The shared SPEC tool test asserts all six required exports. A one-round full
four-mode `176.gcc` smoke test with ASLR disabled passed:

```text
M4       1.119701 s
AOT v2   1.126363 s
old AOT  1.147122 s
native   0.655398 s
AOT v2/M4 = 0.994085x
```

The local complete `make -C tools/latc test` gate also passed, including the
100000-case ELF mutation test and registry tests.

## Dynamic partial-module result

The Bash test now records five warm samples and reports their median. This
replaces the M6 single cold/single warm comparison, whose direction was not
repeatable. Three independent cold/warm pairs produced:

```text
pair 1: cold 7626 ms, warm 8805 ms
pair 2: cold 7842 ms, warm 7452 ms
pair 3: cold 7930 ms, warm 8378 ms
```

An additional startup-only run reported a 10130 ms warm median from
`9094, 10130, 12565, 10696, 8470 ms`. The complete resource run reported a
10416 ms median from `9672, 9866, 21111, 12817, 10416 ms`. These are 1.3%
faster and 1.5% slower than the M6 single warm result of 10265 ms. The spread
is much larger than either difference, so there is no repeatable startup
speedup or regression.

Alternating daemon-free `perf stat` samples separated the cached-module path
from machine warm-up. With an empty module cache the workload generated 7366
runtime TB attempts and executed about 16.203 billion instructions. With four
cached modules it generated 6984 attempts and executed about 16.180 billion
instructions. AOT therefore removed 382 attempts but only about 0.15% of the
instruction count. Steady paired elapsed times and cycles differed by about
1-2%; the first sample of each mode was slower regardless of cache state.

The complete run registered four modules, but recorded only 144 AOT lookups,
8473 JIT fallbacks, and 6992 runtime TB generations. Registration took
1-189 us per module. The remaining elapsed time is dominated by untranslated
guest paths and the Bash workload rather than module registration or the AOT
execution path. Changing the AOT lookup or dispatch code cannot reasonably
produce the requested 15% gain at this coverage level.

The long run passed with 153 valid samples: RSS was 31968-43680 KiB and PSS
was 28721-40101 KiB. Both maxima were below the M6 observations. Two processes
mapped the same four modules; their combined module RSS was 9184 KiB, combined
PSS was 4959 KiB, and Shared_Clean was 8336 KiB. Shared clean pages were 128 KiB
higher than M6. The sampler also handles a process exiting between the
`/proc` readability check and the actual `smaps_rollup` read.

No production runtime change is justified by these measurements. The durable
changes are stricter benchmark configuration, repeated startup sampling, and
the `/proc` sampling race fix. A future dynamic-performance task should first
increase compiled TB coverage and then repeat the paired measurement.
