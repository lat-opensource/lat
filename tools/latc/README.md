# latc

`latc` is an internal x86-64 to LoongArch64 binary compiler experiment. It is
built independently and does not modify or link against objects from the
parent LAT build. `latc compile` builds a startup-pretranslation bundle;
`compile-aot.sh` runs LAT's relocatable code generator on a LoongArch build host
and embeds the resulting native code and relocation records.

## AOT v2 module prototype

See [candidate validation](VALIDATION.md) for build, installation and runtime
gates, evidence requirements, and the boundaries of the tested support.

### Automatic runtime setup

Legacy `LATX_AOT` remains the default. Set `LATX_AOT_V2=1` to select
AOT v2 without configuring a cache/socket or starting `latcd` manually.
An explicitly enabled `LATX_AOT` together with enabled v2 is an error before
guest execution. `LATX_AOT=0 LATX_AOT_V2=0` selects pure JIT. An explicit
`LATX_AOT_V2=0` also disables v2 when legacy cache/socket overrides remain.
Command-line options override environment values, which override LAT config.
Older explicit cache/socket/module configurations still imply v2 when the
mode is unset; they are subject to the same mutual exclusion check.

The default cache is `${XDG_CACHE_HOME:-$HOME/.cache}/lat-aot-v2/<build-id>/live`.
State and `daemon.log` are under
`${XDG_STATE_HOME:-$HOME/.local/state}/lat-aot-v2/<build-id>`.
The private socket lives under `$XDG_RUNTIME_DIR/lat-aot-v2`, or the state
root's `lat-aot-v2/run` directory when a private runtime directory is absent.
Socket names identify both the toolchain and canonical cache directory.
Long directory paths use an open directory FD for Unix socket bind/connect,
without relocating the cache or changing the working directory.
These paths are application-independent; incompatible builds use separate caches.

The runner starts matching installed tools on demand, checks the daemon's
user/build/cache identity, and serializes concurrent starts. Compilation
continues after the guest exits. A managed daemon exits after 60 seconds idle,
only when queued compilation and precompile requests have completed.
A fully covered warm run does not start a daemon. Ordinary failures retain
usable cached modules and fall back to JIT; strict AOT checks still fail.
Privileged execution does not automatically activate v2.

`LATX_AOT_V2_AUTOSTART=0` disables automatic starts but can still connect to
an existing matching daemon. Explicit `LATX_AOT_V2_LATCD_SOCKET` or
`LATX_AOT_V2_MODULE` selects manual operation; the runner never starts or
replaces that daemon. Explicit cache paths may still use automatic startup.
Precompile, collect-only and strict diagnostics remain opt-in and are not
enabled by automatic setup.

Installed tools must share one build identity. System deployment uses normal
library loader configuration; private install tests use `LD_LIBRARY_PATH`.
The automatic daemon receives only the configured runtime library directory,
not the application's loader environment.

```sh
make -C tools/latc test-aot-v2-autostart \
  INSTALL_PREFIX=/path/to/install X86_GUEST=/path/to/x86-static-hello
```

AOT v2 is the new per-x86-ELF module design. It keeps the original x86 program
as the user-visible launch target and loads cached LoongArch `ET_DYN` modules
inside LAT. The design and confirmed runtime rules are in
[`AOT_V2_DESIGN.md`](AOT_V2_DESIGN.md).

Milestone M0 defines the standalone ABI, validates an artifact before
`dlopen()`, and provides a minimal multi-instance registry. On a LoongArch
host, build and execute the real shared-object fixture with:

```sh
make -C tools/latc test-aot-v2
```

On other hosts the same target runs the architecture-independent ELF format
and registry tests. M1 packages all supported TBs from real LAT output and
executes no-libc hello, static glibc hello, and SPECint2000 train through the
AOT registry and existing linux-user syscall path.

On a LoongArch build host, compile and inspect an AOT v2 module with:

```sh
build/latc compile-module /path/to/x86-program -o program.so \
  --runner /path/to/static-exporter/latx-x86_64 \
  --runtime-dir /path/to/aot-v2-runtime
build/latc inspect-module --json program.so
```

M4 adds a separate per-user compiler process. The first implemented step
accepts one read-only x86 ELF file descriptor over a Unix socket, compiles a
stable snapshot, validates the module source digest, and publishes it with an
atomic rename:

```sh
build/latcd/latcd --once --socket "$XDG_RUNTIME_DIR/latcd.sock" \
  --cache-dir /path/to/cache --latc build/latc \
  --runner /path/to/latx-x86_64 --runtime-dir /path/to/aot-v2-runtime
build/latcd/latcd --submit --socket "$XDG_RUNTIME_DIR/latcd.sock" \
  /path/to/x86-program
```

On a LoongArch host, `make test-latcd-once` validates compiler failure cleanup,
bad sources, writable descriptors, malformed compiler output, replacement of a
bad cache entry, strict AOT execution, and an existing-module cache hit. The
The resident service uses the same trusted configuration and adds one compiler
worker, SHA-256 deduplication, a priority queue, resource limits, bounded
failure delays, and atomic status counters:

```sh
build/latcd/latcd --serve --socket "$XDG_RUNTIME_DIR/latcd.sock" \
  --cache-dir "$HOME/.cache/latx/aot-v2" --latc build/latc \
  --runner /path/to/latx-x86_64 --runtime-dir /path/to/aot-v2-runtime \
  --x86-rootfs /path/to/x86-rootfs \
  --stats "$XDG_RUNTIME_DIR/latcd-stats.json"
```

The socket parent must be owned by the current user and have no group or other
permissions. Set the same socket on the runner to request missing modules:

```sh
LATX_AOT_V2_CACHE_DIR="$HOME/.cache/latx/aot-v2" \
LATX_AOT_V2_LATCD_SOCKET="$XDG_RUNTIME_DIR/latcd.sock" \
  /path/to/latx-x86_64 -L /path/to/x86-rootfs /path/to/x86-program
```

The runner sends the already-open ELF FD with a nonblocking `sendmsg` and does
not wait for an acknowledgement or compilation. `compiler_submissions` means
the packet reached the Unix socket; daemon counters and cache files report the
final result.

The module contains instruction-level Host-PC to guest-PC records in
`.rodata.lat.map`. The runner publishes every supported native-image TB,
restores x86 state from those records on a Host signal, and puts direct Host
addresses in LAT's fast indirect jump cache after the first lookup. It does not
create compatibility `TranslationBlock` objects. M2 also discovers dynamic
main, loader, and libc mappings by source SHA-256 and actual load bias.

Run all twelve official SPECint2000 train workloads on LoongArch with:

```sh
make -C tools/latc test-specint-aot-v2 \
  RUNNER=/path/to/static-exporter/latx-x86_64 \
  AOT_V2_RUNNER=/path/to/aot-v2-runner/latx-x86_64 \
  AOT_V2_RUNTIME_DIR=/path/to/aot-v2-runner \
  SPEC_ROOT=/path/to/spec2000
```

The test rejects partial modules, any runtime TB generation, invalid SPEC
output, and a benchmark that exceeds 60 seconds. M1 passed all twelve train
workloads on `3a6000-25g` on 2026-08-25. Ref inputs were not run.

After building an AOT v2 runner, rerun the complete M4 test set with:

```sh
make -C tools/latc test-aot-v2-m4 \
  RUNNER=/path/to/exporter/latx-x86_64 \
  AOT_V2_RUNNER=/path/to/aot-v2/latx-x86_64 \
  AOT_V2_RUNTIME_DIR=/path/to/aot-v2 \
  X86_ROOTFS=/path/to/x86-rootfs SPEC_ROOT=/path/to/spec2000
```

This includes cold/warm compiler service tests, static glibc, signal recovery,
dynamic loading and symbol semantics, and a fresh SPECint train 12/12 build and
run. It does not run SPEC ref.

`latc inspect` reports this existing format as
`execution_model=lat-aot-bundle`. It is deliberately not called a standalone
native ELF: the file still contains the LAT runner and can enter LAT's JIT.

The standalone native ELF work retains the versioned fallback interface in
`native/include/lat-fallback.h`, but direct static execution currently requires
every referenced guest TB to be present. Export, image validation, relocation,
and runtime dispatch reject a missing TB instead of silently entering LAT's
JIT or another translated address.

Generate the current stable native image on a LoongArch build host with:

```sh
tools/latc/scripts/compile-native-image.sh build/latc \
  /path/to/latx-x86_64 /path/to/x86-program program.latnative program.tbset
build/latc inspect-native --json program.latnative
```

This `.latnative` image is an intermediate file. Use `compile-native-elf.sh`
to link it with the small runtime and produce the executable LoongArch ELF.

Build the current LoongArch PIE shell with:

```sh
tools/latc/scripts/link-native-shell.sh program.latnative program.la64
program.la64 --latc-inspect
```

The normal one-step command accepts a static x86-64 ELF and writes the final
LoongArch executable directly:

```sh
tools/latc/scripts/compile-native-elf.sh build/latc \
  /path/to/latx-x86_64 /path/to/static-x86-program program.la64
./program.la64 arg1 arg2
```

The command generates the native image in a temporary directory, verifies and
marks the embedded static x86 ELF, then links the runtime shell. An optional
fifth argument supplies a profile file.

Set `LATC_LA64_LDFLAGS=-static-pie` when the LoongArch toolchain provides
static libc objects and the output must have no ELF interpreter or shared
library dependency.

The shell embeds the image in read-only `.latc.image` and validates it on the
target host. Images marked for static x86 execution run the guest directly and
pass normal command-line arguments and environment variables through the x86
Linux initial stack.

`--latc-map` validates and maps the embedded static x86 ELF `PT_LOAD` segments
at their recorded addresses, applies final page permissions, prints the mapped
range, then unmaps it. This is a loader test only; it does not enter guest code.

`--latc-relocate` copies the LoongArch code into an anonymous mapping near the
PIE, applies every stable guest-address and runtime-symbol relocation, flushes
the instruction cache, changes the mapping from RW to RX, then unmaps it.

The first execution test is intentionally independent of x86 and LAT context
switching. `tests/make-native-smoke.c` creates one TB containing two hand-coded
LoongArch instructions which return `42` under the normal C ABI. The PIE runs
it only when the image carries `LAT_NATIVE_IMAGE_C_ABI_SMOKE`; normal exported
LAT images cannot use this path.

`tests/make-native-state-smoke.c` is the next layer. Its TB receives a
`LatX86StateV1 *`, changes `gpr[0]` from 35 to 42, writes `rip=0x1234`, and
returns 42. This verifies the shared state layout and calling convention before
adding LAT's context-switch assembly.

`tests/make-native-dispatch-smoke.c` contains two TBs. The first changes
`gpr[0]` from 5 to 12 and sets `rip` to the second TB; the second doubles it to
24 and clears `rip`. The runtime performs two `(rip, flags)` lookups and stops
when `rip` becomes zero.

`tests/x86-exit42.S` is the first real x86 input. It performs only
`exit(42)`. `compile-native-image.sh` verifies that the embedded guest is an
x86-64 ELF without `PT_INTERP`, marks it for static execution, and records the
LAT build ID required by the runtime. Guest SHA-256 values are recorded for
identity and diagnostics, not used as an execution allowlist.
`tests/x86-exit-add42.S` uses real x86 arithmetic to compute 42 before the
same restricted exit syscall.
`tests/x86-exit-loop42.S` executes a six-iteration x86 loop across four CFG
TBs, adding seven each time before exiting with 42.
`tests/x86-exit-memory42.S` loads 35 from the static guest data segment through
RIP-relative addressing, adds seven, and exits with the result.
`tests/x86-exit-call42.S` uses the guest stack for a direct call and return. The
restricted entry path initialises x86 RSP before entering translated code.
`tests/x86-write-exit42.S` writes `OK` through the restricted syscall helper,
returns to translated code, and then exits with 42.
`tests/x86-stack-exit42.S` reads `argc` and `argv[0]` from the Linux-compatible
initial guest stack before exiting with 42.
`tests/x86-brk-exit42.S` and `tests/x86-mmap-exit42.S` allocate writable guest
memory through returning syscalls, store 35, add seven, and exit with 42.
`tests/x86-file-exit42.S` opens `/dev/zero`, reads and checks one byte, closes
the descriptor, and exits with 42.
`tests/x86-open-error42.S` checks that a missing file returns x86 Linux
`-ENOENT`, not the host libc's raw `-1`.
`tests/x86-c-exit42.c` is compiled C code with a loop, local state, and normal
function calls. A minimal assembly `_start` exits with the C return value.
`tests/x86-indirect-call42.S` loads a function pointer from the guest data
segment and calls it indirectly before exiting with 42.
`tests/x86-runtime-indirect-call42.S` derives two function pointers from
`argc`, performs two nested runtime indirect calls, and checks both x86 return
addresses and a callee-saved register before writing `INDIRECT OK`.
`tests/x86-helper-state42.S` executes `cpuid`, `pcmpistri`, and `pcmpistrm`,
checking their results, x86 flags, RSP, and a callee-saved register before
writing `HELPERS OK`.
`tests/x86-ifunc-like42.S` calls a resolver, invokes the returned function
pointer, and verifies RSP after both returns before writing `IFUNC OK`.
`tests/x86-auxv42.S` validates the Linux x86-64 initial stack, including the
program headers, page size, entry point, platform, random bytes, and executable
name supplied through auxv.
`tests/x86-addr32-call42.S` checks the address-size-prefixed direct call form
used by static glibc and verifies its pushed return address and restored RSP.
`tests/x86-tls42.c` parses `PT_TLS`, copies its initial image into a new static
TLS block, sets FS with `arch_prctl`, and verifies a `__thread` value before
writing `TLS OK`.
`tests/x86-prlimit42.S` queries `RLIMIT_STACK` through x86 `prlimit64` and
checks the returned limit before exiting with 42.
`tests/x86-getrandom42.S` fills 16 bytes through x86 `getrandom`, checks the
result, and exits with 42.
`tests/x86-readlinkat42.S` reads `/proc/self/exe` through x86 `readlinkat`,
checks that an absolute path was returned, and exits with 42.
`tests/x86-mprotect42.S`, `tests/x86-fstat42.S`, and
`tests/x86-exit-group42.S` cover the additional Linux calls used by static
glibc startup and shutdown. The syscall switch and x86 ABI structure
conversions are kept in `native/runtime/x86-linux-user.c`, following the
corresponding cases in `linux-user/syscall.c`.
`tests/x86-entry-regs42.S` checks that the initial guest GPRs are deterministic
and that static ELF `%rdx` is zero, as required for the dynamic-linker finalizer
hook passed to glibc startup.
`tests/x86-argv-env42.S` checks that normal LoongArch command-line arguments
and environment variables appear in the x86 Linux initial stack. Runtime
options beginning with `--latc-` remain reserved for image inspection and
diagnostics.
`tests/x86-long-tb-exit42.S` contains more than 255 straight-line x86
instructions. It verifies that pretranslation continues at LAT's TB length
limit instead of leaving a missing target in the native image.
`tests/x86-static-hello.S` is a static x86-64 ELF with no interpreter and no
host libraries. Its `_start` writes `Hello, LATC!` with the x86 Linux `write`
syscall and exits with the x86 Linux `exit` syscall. This is the first direct
static-translation test. `tests/x86-glibc-hello.c` is the separate full static
glibc fixture used after the smaller call, helper, IFUNC, and auxv tests pass.
It validates `argc`, `argv`, and `getenv`, then prints `Hello from glibc!` and
exits successfully on the 3A6000 test host. The generated LoongArch shell
currently links the small host runtime dynamically; producing a fully static
LoongArch ELF remains separate work.

The AOT output is a static LoongArch PIE containing the copied LAT runner, the
x86-64 guest, its control-flow graph, and LAT AOT code. Paths not present in the
AOT image use LAT's JIT translator unless strict verification is enabled.

Build and test with:

```sh
make -C tools/latc
make -C tools/latc test
tools/latc/build/latc analyze --json /path/to/static-x86_64-elf
tools/latc/build/latc compile /path/to/static-x86_64-elf \
  -o program.la64 --runner /path/to/loongarch64/latc-runner
```

On a LoongArch build host, generate and embed relocatable LAT AOT code with:

```sh
tools/latc/scripts/compile-aot.sh tools/latc/build/latc \
  /path/to/static/latc-runner x86-program program.la64
```

Set `LATC_STRICT_AOT=1` when testing the output. The runner exits with status
125 before decoding if the runtime translator is entered after embedded AOT
loading.

For dependency-coverage acceptance, use `LATC_STRICT_FILE_AOT=1`. It rejects
JIT generation for code backed by an ELF file while allowing non-file mappings
such as the vDSO. `LATC_STRICT_AOT=1` remains the stronger diagnostic mode that
rejects every runtime translation attempt.

Embedded AOT is installed into `$HOME/.cache/latx` on first use. Later runs
reuse it only when a read-only cache file and its latc marker still match the
recorded SHA-256 digest, inode, size, and modification time. Runtime statistics
include `aot_cache_hit`, `bundle_verify_ns`, `guest_extract_ns`, and
`aot_prepare_ns` so startup costs can be separated from guest execution.
`runtime_tb_gen_attempts` is counted at translator entry; strict mode rejects
the attempt before decoding or generating host code.

An optional binary TB key set selects the translated blocks that belong in the
output. Its fixed header contains `LATTBKS`, the format version, source
SHA-256, record count, and sequence. Each fixed-size record contains an
ELF-relative virtual address and semantic translation flags. It deliberately
contains no execution count. Pass it with
`--tbset FILE`. TB-set compilation decodes and translates the ELF control-flow
graph once. The observed `(RVA, flags)` entries add dynamic targets and
translation variants that static decoding cannot derive. This includes CFG
paths that were not executed during collection, x86-64 lazy PLT binding
entries, and validated fixed-stride code-table targets. It avoids repeated
whole-program translation rounds. A wrong source
digest, an address outside executable sections, an unsupported flag, or a
malformed header or record fails compilation. There is no text profile format
or compatibility parser.

The native translation stage partitions the final TB set at page/TU boundaries
and translates the partitions concurrently. `LATC_AOT_THREADS=N` selects 1 to
32 translation threads; when unset, direct compilation uses up to 8 online
CPUs. `latcd` runs up to 8 ELF compile jobs concurrently by default and divides
the online CPUs among those jobs, so module-level and within-module parallelism
share one CPU budget. Explicit thread counts are honored, including 1 and 2.

An application can also seed the cache before it runs. Start the normal
resident `latcd`, then run:

```sh
latcd --precompile --socket "$XDG_RUNTIME_DIR/latcd.sock" \
  --x86-rootfs /path/to/x86-rootfs [--latc /path/to/latc] [--jobs 8] \
  /usr/bin/python3
```

The final argument is an absolute guest path inside the rootfs. The command
reads `PT_INTERP` and recursively resolves `DT_NEEDED`, using each object's
`RPATH`/`RUNPATH`, the rootfs `ld.so.cache`, and the standard x86-64 library
directories. It does not scan the rootfs or guess libraries loaded later by
`dlopen()`. It first resolves and statically analyzes every discovered ELF. If
any one fails, it submits nothing. Static analysis emits one `CF_PARALLEL` TB
variant per address; single-thread and multi-thread execution use that same
translation. After all analyses succeed, the command submits each
complete TB set to the resident service and waits for `FLUSH_SOURCE` before it
returns. A later runtime submission uses the same union-and-incremental-publish
path, so dynamically discovered TBs extend the static base instead of replacing
it.

Runtime precompilation is opt-in. Set `LATX_AOT_V2_PRECOMPILE=1` together
with `LATX_AOT_V2_CACHE_DIR` and `LATX_AOT_V2_LATCD_SOCKET`. When LAT maps an
x86 ELF that has no compatible cached module, it sends that source FD to the
resident `latcd` and blocks before guest execution. `latcd` derives the guest
path from its configured `--x86-rootfs`, runs the same dependency precompile
described above, and replies only after every accepted module is published.
LAT then loads the newly published module into the current process. The
default remains non-blocking incremental JIT collection when the variable is
unset or is not exactly `1`. A precompile failure is reported and falls back
to JIT unless strict AOT mode separately rejects the miss.

The module tables are emitted as binary data and included directly by the
assembler. They are not formatted as a large C source file for the host C
compiler to parse. `latcd` accepts a cached module as a TB-set hit only when
every requested `(RVA, flags)` pair exists in that module.

For each source ELF, the version 2 `current` manifest names exactly one `.so`,
one canonical `.native` image, and one complete `.tbset`. The first publication
translates the complete set. A later publication subtracts the current module's
actual TB table, translates only keys that the module does not contain, merges
that native image into the canonical image, and links one replacement `.so`.
If the module already contains every newly observed key, `latcd` publishes only
the complete `.tbset` and manifest; it does not invoke translation or linking,
and the `.so` and `.native` remain unchanged. It does not keep a list of
incremental modules. The three
immutable generation files are synchronized before an atomic manifest rename;
only after that commit does `latcd` remove the superseded generation. A failed
compile, merge, link, or pre-manifest publication leaves the prior manifest and
all files it names unchanged. This manifest has no older-format parser.

On LoongArch, validate a copied LAT runner and real x86 guest with:

```sh
make -C tools/latc test-native \
  RUNNER=/path/to/latc-runner X86_GUEST=/path/to/static-x86_64-elf
```

Build a statically linked runner from the matching full LAT checkout on a
LoongArch machine:

```sh
tools/latc/scripts/build-runner.sh /path/to/full/lat /path/to/build-latc-runner
```

The script treats `/path/to/full/lat` as read-only. It creates a disposable
`/path/to/build-latc-runner.source` staging tree and applies the local adapter
there. Delete both build directories after copying out `latx-x86_64`.

`aot-v2-source-map.json` records every canonical AOT v2 integration source and
its generated copy. It covers the main LAT to `tools/latc/lat` copies, the
`tools/latc/aot-v2` runtime files copied into the main runner tree, and the
shared native headers. Generated files marked in `lat-local.json` keep the
minimal imported LAT tree complete; do not edit any mapped target directly.
Regenerate and verify all mapped targets with:

```sh
make -C tools/latc sync-aot-v2-sources
make -C tools/latc check-import
```

`prepare-runner-source.py` reads the same source map and copies its canonical
files into a staging runner. Files in `lat-local.json` without `generated=true`
exist only in the minimal imported tree and remain directly maintained there.
`check-import` prints both the generated path and its canonical source when
they differ.

## Main build and installation

On a LoongArch host, the main Meson build installs one matching AOT v2
toolchain. Configure an explicit prefix, build, and install it as one unit:

```sh
mkdir build-aot-v2 && cd build-aot-v2
../configure --target-list=x86_64-linux-user --enable-latx \
  --optimize-O1 --disable-docs --prefix=/path/to/prefix
ninja
meson install
```

The prefix contains `bin/latx-x86_64`, `bin/latc`, `bin/latcd`,
`lib/liblat-aot-runtime.so.2`, and the compiler helper files below
`libexec/latc`. All four programs and libraries embed the same deterministic
64-character build identity. `latcd` checks the compiler, runner, runtime ABI,
and runtime identity before creating a socket, writing cache files, or running
a compile job. A mismatch is reported and the application can continue with
its normal JIT fallback.

Run the installation-tree integration test with an x86 rootfs and a dynamic
x86 guest. The test does not use binaries or helper scripts from the build
directory:

```sh
make -C tools/latc test-aot-v2-install \
  INSTALL_PREFIX=/path/to/prefix \
  X86_ROOTFS=/path/to/x86-rootfs \
  X86_GUEST=/path/to/x86-rootfs/usr/bin/echo
```

Each `latcd --serve` or `latcd --once` process takes a non-blocking exclusive
lock on `<cache>/.latcd.lock` before it creates a socket, temporary file,
module, profile, or `current` index. A second process using the same cache exits
with `cache is already owned by another latcd`; use a separate cache directory
if both daemons must run. The lock descriptor remains open for the daemon's
whole lifetime. The kernel releases it after either a normal exit or a crash,
so the text left in `.latcd.lock` is diagnostic information, not a PID-file
liveness check. Service statistics include `cache_owner_pid`.

Analyze all twelve SPECint2000 integer executables with:

```sh
make -C tools/latc test-spec-analyze \
  SPECINT_BINDIR=/path/to/specbin/x64_gcc12_2_0
```

Current limitation: generated LoongArch instructions are regenerated when the
output starts unless the bundle was produced by `compile-aot.sh`. An AOT bundle
stores LAT's LoongArch code and relocation records and loads them before guest
execution. Unresolved paths use JIT unless `LATC_STRICT_AOT=1` is set.

`LATC_STATS_OUT=/path/stats.json` records both startup pretranslation and
`runtime_tb_gen_attempts` and `runtime_tb_gen_calls`. The attempt count must be
checked before claiming that a test ran without entering the runtime
translator. `LATC_DISABLE_PRETRANSLATE=1` provides a JIT baseline for the same
bundle. `continuation_tbs` counts TBs added when LAT ends a TB before the end of
its containing CFG block, including instruction-limit and internal translator
splits. `edge_target_tbs` counts executable static-edge targets not represented
by a standalone CFG block. Jump-table case targets must be CFG TB leaders; the
runner does not scan past alignment NOPs to compensate for missing targets.

For AOT v2, the runtime does not increment counters while a TB executes. It
records the exact `(RVA, semantic flags)` key once when a file-backed TB is
created. A background thread submits pending per-source sets every 100 ms;
normal exit and pre-`execve` handling submit any remainder. It never scans the
global JIT TB table. latcd unions keys from all processes; an unchanged set
does not schedule another compile. In `--flush-only` mode it performs no
compilation until `FLUSH_SOURCE` or `FLUSH_ALL`, then compiles each final source
set once. The flush waits until every previously accepted set is published or
an explicit compile error is returned. A successful flush has already merged
and relinked every accepted increment; it never reports success for a pending
fragment.

Profile-module generation is single-pass. The compiler must include every
requested TB and its required direct branch targets in that pass. A missing
static target fails the job and is reported; the driver does not retry with
supplemental TB sets.

All twelve SPECint2000 integer programs pass the official test workloads with
zero main-ELF runtime code generation. Ten pass full strict mode with no JIT at
all. Perlbmk and vortex use two and four guest VDSO/signal-helper TBs
respectively; they pass main-program strict mode with only that explicit system
JIT fallback. See [spec2000/SPEC2000.md](spec2000/SPEC2000.md) for counts and
AOT sizes. The same strict gzip AOT output runs on AOSC Linux and Loongnix Linux
4.19 3A6000 systems.

The official SPECint2000 train workloads are also automated:

```sh
python3 spec2000/prepare-specint-train.py \
  --latc build/latc --runner /path/to/latx-x86_64 \
  --spec-root /path/to/spec2000 --workdir /path/to/train-bundles
python3 spec2000/bench-specint-train.py \
  --runner /path/to/latx-x86_64 --spec-root /path/to/spec2000 \
  --bundle-dir /path/to/train-bundles/bundles \
  --workdir /path/to/train-benchmark --rounds 5
```

All twelve SPECint2000 integer train workloads passed on the AOSC 3A6000 host
on 2026-08-21. Each individual command used a 30-second hard timeout; the
slowest command completed in 16.76 seconds. VPR was checked with the official
numeric tolerances and all other outputs matched byte for byte. Ref inputs have
not been run. A subsequent full run through `myrun1.sh train` regenerated all
twelve ELFs first, then produced twelve official SPEC `.raw` results with
`valid=1`; the slowest whole benchmark was perlbmk at 44.11 seconds under the
60-second per-benchmark limit.

This proves that the main x86 ELF executes from LAT AOT without entering the
runtime translator for the measured workload. It does not yet produce a
standalone LoongArch program without the LAT runtime: the output remains a
static LoongArch runner containing the x86 ELF, CFG, and LAT AOT image.

Current limitations: AOT generation itself must run on a LoongArch build host;
the runner extracts the embedded AOT into `$HOME/.cache/latx` and the x86 guest
into `/tmp`, so both locations must be writable. Guest VDSO and anonymous
signal-helper pages are not yet serialised as AOT segments. Test and train
workloads are validated. Ref profiles still need separate collection because
they may expose additional indirect targets.
