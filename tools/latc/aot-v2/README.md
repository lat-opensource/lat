# AOT v2 M0 and M1 test slice

## ELF validator fuzzing

The normal AOT v2 test runs 100,000 deterministic mutations of a valid
synthetic module. Clang builds two additional sanitizer targets:

```sh
make test-aot-v2-sanitize
make test-aot-v2-libfuzzer
```

The libFuzzer target starts from the valid synthetic ELF, so mutations reach
the note, dynamic table, dynamic symbols, strings, and relocations. Both
commands are bounded and keep their generated corpus below `build/tests/`.

This directory contains the first implementation slice of the AOT v2 design.

- `include/lat-aot-v2.h` is the stable ELF note, module descriptor, TB, and PC
  map ABI. LBT and LSX are mandatory; LASX is an optional artifact variant.
- `runtime/elf-validate.c` parses and checks an artifact through an already
  opened fd before `dlopen()`.
- `runtime/module-loader.c` loads the validated fd, resolves the versioned
  descriptor, and verifies all descriptor ranges against mapped ELF segments.
- `runtime/registry.c` publishes immutable guest-range snapshots and supports
  multiple guest load biases for one shared Host artifact. It also fills the
  guest-address slots immediately before the existing `$fp` jump-cache base.
- `compiler/module-pack.c` converts supported TBs from the current
  `.latnative` intermediate into position-independent module text and metadata.
  Runtime helper calls use a local trampoline and guest addresses use negative
  `$fp` offsets; neither operation requires runtime `.text` changes.
- `compiler/module-inspect.c` validates an AOT ELF without executing it and
  reports its identity, feature requirements, TB count, and PC-map count.
- `scripts/link-aot-v2-module.sh` links the generated objects into a
  deterministic LoongArch `ET_DYN` file.
- `tests/fixture.c` and `tests/fixture-entry.S` build a real LoongArch
  `ET_DYN` fixture whose synthetic TB returns `42` through the versioned runtime
  dependency.

The M1 tests execute real LAT-generated TBs for `x86-exit42`, the no-libc
static hello, and a static glibc hello. The module-level test captures x86
syscall state after translated code saves it. The dynamic runner routes the
same syscall exit to LAT's existing `helper_raise_syscall`; the normal
linux-user CPU loop then executes `linux-user/syscall.c`.

The packager publishes every supported TB from the native image. Direct edges
remain inside the module. On the first indirect lookup the runner creates a
thread-local compatibility `TranslationBlock`, then fills both LAT's fast jump
cache and QEMU's TB cache. Repeated indirect jumps go directly to read-only
module text. Missing TBs still reach the normal lookup path and are rejected by
the strict M1 tests.

The AOT ELF does not contain LAT `TranslationBlock` objects, and module loading
does not register all TBs in QEMU's qht or TCG Host-PC tree. The current
`cpu_tb_exec()` interface still accepts `TranslationBlock *`, so the adapter
creates compatibility objects only for selected TBs. A later execution API
should accept `LatAotTargetV2` directly and remove them.

The copied LAT code generator saves vector state with LASX instructions, so
these modules declare `LAT_AOT_FEATURE_LASX`. A separate LSX-only lowering is
still required for CPUs without LASX; LBT and LSX remain mandatory for every
variant.

Native intermediate v2 contains stable instruction-level Host ranges and guest
PCs decoded from LAT's existing search data. The packager copies the complete
map for every published TB into `.rodata.lat.map`; the loader checks that these
ranges are ordered, non-overlapping, inside read-only module text, and use the
supported dynamic-state record. Such modules declare
`LAT_AOT_MODULE_PRECISE_PC_MAP` and no longer carry
`LAT_AOT_MODULE_M1_TEST_ONLY`.

LAT's signal recovery now searches the module PC map and calls the existing
`restore_state_to_opc()` path. The loader validates PC-map coverage in host
offset order without the former TB-count times map-count scan. AOT v2 also
disables the guest vDSO for M1, so libc uses syscall instructions already
present in the static main ELF instead of requiring JIT for runner-provided
vDSO code.

On `3a6000-25g`, all official SPECint2000 train workloads passed on 2026-08-25
with a 60-second limit and zero runtime translation attempts and calls:

| benchmark | SPEC reported time (s) |
| --- | ---: |
| 164.gzip | 10.040804 |
| 175.vpr | 6.155109 |
| 176.gcc | 1.296400 |
| 181.mcf | 3.692219 |
| 186.crafty | 7.028920 |
| 197.parser | 2.204061 |
| 252.eon | 2.489437 |
| 253.perlbmk | 26.198676 |
| 254.gap | 1.880917 |
| 255.vortex | 3.947464 |
| 256.bzip2 | 9.063077 |
| 300.twolf | 3.837499 |

M1 remains a static `ET_EXEC` milestone. Dynamic `PT_INTERP` startup, shared
object discovery and registration, executable mapping invalidation, unloading,
direct `LatAotTargetV2` execution, and an LSX-only artifact remain M2 or later
work. Ref inputs have not been run.

On LoongArch, after producing a `.latnative` image with the existing exporter:

```sh
make -C tools/latc test-aot-v2-translated \
  NATIVE_IMAGE=/path/to/program.latnative
```

The normal M1 command accepts the x86 ELF directly:

```sh
build/latc compile-module /path/to/x86-program -o program.so \
  --runner /path/to/static-exporter/latx-x86_64 \
  --runtime-dir /path/to/aot-v2-runtime
build/latc inspect-module --json program.so
```

`LATC_AOT_RUNNER` and `LATC_AOT_RUNTIME_DIR` may replace the two command-line
options.

Build and test the disposable dynamic LAT runner with:

```sh
tools/latc/scripts/build-aot-v2-runner.sh \
  /path/to/full/lat /path/to/aot-v2-runner
make -C tools/latc test-aot-v2-runner \
  AOT_V2_RUNNER=/path/to/aot-v2-runner/latx-x86_64 \
  AOT_V2_RUNTIME_DIR=/path/to/aot-v2-runner \
  X86_GUEST=build/tests/x86-static-hello \
  NATIVE_IMAGE=/path/to/x86-static-hello.latnative
```

The static glibc and SPEC M1 tests are:

```sh
make -C tools/latc test-aot-v2-glibc-runner \
  AOT_V2_RUNNER=/path/to/aot-v2-runner/latx-x86_64 \
  AOT_V2_RUNTIME_DIR=/path/to/aot-v2-runner \
  X86_GUEST=/path/to/static-glibc-hello \
  NATIVE_IMAGE=/path/to/static-glibc-hello.latnative
make -C tools/latc test-specint-aot-v2 \
  RUNNER=/path/to/static-exporter/latx-x86_64 \
  AOT_V2_RUNNER=/path/to/aot-v2-runner/latx-x86_64 \
  AOT_V2_RUNTIME_DIR=/path/to/aot-v2-runner \
  SPEC_ROOT=/path/to/spec2000
```
