# LAT AOT v2 design

## 1. Goal

AOT v2 translates each x86-64 ELF image into a loadable LoongArch64 ELF
artifact. The source image may be the main executable, the x86 dynamic linker,
or a shared object loaded at startup or by `dlopen()`.

The first target is a dynamically linked x86 hello-world program. LAT must run
it with ASLR enabled while loading separate AOT artifacts for the main image,
the x86 dynamic linker, libc, and any other executed shared object. A missing
artifact or missing TB must fall back to LAT JIT without changing guest-visible
dynamic-linker behaviour.

AOT v2 does not initially turn an x86 shared library into a native LoongArch
library that an arbitrary LoongArch program can link against. The x86 dynamic
linker remains responsible for the guest ABI. The host dynamic linker only
loads translated code and the LAT runtime ABI.

## 2. Design decisions

### 2.1 One artifact type for executables and shared objects

Every translated image is a LoongArch64 `ET_DYN` file with no `PT_INTERP`.
It exports one versioned descriptor, `lat_aot_module_v2`, and has one host
dependency, `liblat-aot-runtime.so.2`. Hidden visibility is used for all
translated TB symbols and implementation details.

The main x86 executable uses the same module format as a shared object. A thin
LoongArch PIE launcher may later provide a directly executable file, but it
must load the main AOT module through the same runtime API. It must not create
a second translation format.

### 2.2 Keep guest and host dynamic linking separate

The x86 dynamic linker continues to process `DT_NEEDED`, symbol versions,
PLT/GOT relocations, TLS, IFUNC resolvers, `LD_PRELOAD`, audit modules, and
`dlopen()` ordering. It maps the original x86 ELF segments into guest address
space exactly as LAT does today.

The LoongArch dynamic linker resolves only the AOT artifact's references to
`liblat-aot-runtime.so.2`. It does not resolve x86 symbols. Calls through an x86
PLT or function pointer still read guest memory and dispatch by guest PC.

This separation is required for correctness. Mapping x86 symbols directly to
LoongArch ELF symbols would require a complete ABI bridge for calling
conventions, TLS, symbol interposition, IFUNC, callbacks, exceptions, and
unloading before a normal dynamically linked application could run.

### 2.3 Load on a cache hit, compile outside the application

The runtime checks for an AOT artifact at these points:

1. After the main x86 executable and `PT_INTERP` identities are known.
2. After a file-backed executable mapping identifies an ELF module. This also
   covers libraries loaded by the x86 dynamic linker and by `dlopen()`.
3. Before JIT translation of a missing TB, to handle mappings whose executable
   permission was added later with `mprotect()`.

On a cache hit, the runtime loads and registers the artifact synchronously. On
a miss, execution continues through LAT JIT and a request is sent to an
external compiler service. Compilation must not run while the mmap lock is
held and must not fork a multithreaded translated process.

Normal cold-cache execution never waits for compilation. It starts through JIT
and submits the main executable, interpreter, and every valid file-backed x86
ELF executable mapping to a separate per-user `latcd` process. Synchronous
compilation is available only through an explicit preparation command. The
service accepts a read-only file descriptor, computes the identity itself,
compiles into a temporary file, validates it, then publishes an immutable
artifact and atomically updates a small `current` index.

## 3. Artifact layout

The artifact is a normal LoongArch64 shared object plus LAT-specific metadata:

| ELF content | Purpose |
| --- | --- |
| `.text.lat.tu` | Position-independent LoongArch translated TUs |
| `.rodata.lat.tb` | Sorted `(guest RVA, flags) -> host text RVA` table |
| `.rodata.lat.range` | Source executable ranges expressed as guest RVAs |
| `.rodata.lat.guest` | Compile-assigned guest-address slots loaded relative to `$fp` |
| `.rodata.lat.map` | Host-PC to guest-PC map for signals and diagnostics |
| `.note.lat.aot` | Format version, source identity, LAT codegen ABI, options, and required CPU features |
| `.data.rel.ro.lat.module` | `LatAotModuleV2` exported descriptor |
| `.dynsym` / `.gnu.version*` | The descriptor and versioned runtime imports |

The descriptor contains offsets and sizes, not build-process pointers:

```c
typedef struct LatAotModuleV2 {
    uint32_t abi_version;
    uint32_t struct_size;
    uint64_t flags;
    uint8_t source_sha256[32];
    uint8_t codegen_id[32];
    const LatAotTbV2 *tb_begin;
    const LatAotTbV2 *tb_end;
    const LatAotPcMapV2 *pc_map_begin;
    const LatAotPcMapV2 *pc_map_end;
    const LatAotGuestSlotV2 *guest_slot_begin;
    const LatAotGuestSlotV2 *guest_slot_end;
} LatAotModuleV2;
```

The exact C layout must be moved into a small standalone ABI header and covered
by size, offset, endianness, and malformed-file tests before code generation is
connected.

## 4. Address model and relocation

All guest addresses in an artifact are module-relative virtual addresses
(RVAs). Runtime registration supplies the actual x86 load bias. This is
required for PIE, shared objects, and ASLR.

Relocations are divided into four classes:

1. Host runtime calls use normal LoongArch ELF relocations against versioned
   `liblat-aot-runtime.so.2` symbols.
2. Intra-module TB edges resolve to local hidden symbols or local text RVAs.
3. Cross-module and unresolved guest targets call the global LAT dispatcher.
4. Guest data addresses use `guest_load_bias + guest_rva`.

AOT v2 never patches `.text`. For the first implementation, the compiler
assigns each referenced guest RVA a context slot. The runtime writes
`guest_load_bias + guest_rva` to that per-instance slot, and generated code
loads the value with one `$fp`-relative `ld.d`. The artifact's executable pages
remain read-only and shareable from the moment the host dynamic linker maps
them. The direct-offset form supports 256 slots. Two-level tables support
65,536 addresses with two loads. Larger modules use three loads and a
three-level table with up to 16,777,216 addresses.

Cross-module direct linking is deliberately deferred. The initial runtime
always uses indirect dispatch across module boundaries. Later it may patch
links after both modules are registered, but must undo or invalidate them on
`dlclose()` and executable `munmap()`.

## 5. Runtime registry

`liblat-aot-runtime.so.2` owns a process-wide registry. A shared
`LatAotModuleInstance` records:

- source identity and AOT descriptor;
- x86 load bias and executable guest ranges;
- host text range and PC map;
- registration generation and mapping reference count;
- TB index;
- JIT fallback state.

Each translated thread has a separate `LatAotExecutionContext` for each active
module instance. `$fp` continues to point at entry zero of the fixed-size jump
cache, so the existing indirect-dispatch assembly does not change. A fixed
prefix immediately before `$fp` holds the module-instance data and the first
guest-address slots; generated code reaches these fields with negative
offsets. Multiple guest instances at different load biases may share one
artifact and its read-only `.text`.

Lookup takes `(guest_pc, translation_flags)` and first finds the owning guest
range, then performs a module-local TB lookup. Writers publish immutable range
index snapshots under a lock; dispatch and signal handlers read the current
snapshot without locking. Old snapshots are reclaimed after a thread epoch.
Each instance has atomic `active` and `generation` fields.

The AOT ELF never contains `TranslationBlock` objects, and loading a module
does not create and register one LAT/QEMU TB object for every descriptor entry.
The M1 adapter first performs the existing JIT TB lookup, then queries the AOT
module registry on a miss. Because the current `cpu_tb_exec()` function still
accepts a `TranslationBlock *`, M1 lazily creates one thread-local compatibility
object for an AOT TB that is actually selected. This object points at the
module's shared read-only `.text`; it is not inserted into the guest qht, TCG
Host-PC tree, page invalidation lists, or direct-link lists. The selected object
is placed in LAT's fast indirect jump cache and QEMU's per-CPU TB cache, so a
repeated indirect target jumps directly to module text. Production code should
accept either a JIT TB or `LatAotTargetV2` and remove this compatibility object.

M1 publishes all supported native-image TBs. Direct intra-module exits are
resolved while linking the AOT ELF; indirect exits use the existing LAT jump
cache and return to the normal dispatcher only on a miss.

The first implementation pins Host AOT modules until process exit. Guest
`dlclose()` deactivates the instance, advances its generation, and invalidates
data caches without unmapping Host code. Reloading the same x86 ELF creates a
new instance and may reuse the same artifact at a different load bias.

## 6. Module discovery and identity

The existing executable-file mmap hook in `linux-user/mmap.c` is the natural
discovery point. It already records file path, offset, guest range, and invokes
AOT recovery. AOT v2 should replace path-only matching with a `LatElfMapping`
record built from the open file descriptor:

- device and inode for grouping mappings during one load;
- ELF type, machine, program headers, and executable `PT_LOAD` ranges;
- file offset to guest-address correspondence;
- GNU build ID when present;
- SHA-256 over the complete source file as the authoritative identity.

Paths are diagnostic only. Cache validity must not depend on pathname or mtime.
The cache key includes the source SHA-256, AOT ABI version, LAT codegen ID,
translation options, required LoongArch ISA features, page-size assumptions,
signal-map format, and profile digest. LBT and LSX are mandatory. LASX is an
optional separate CPU variant selected only on a compatible target.

Suggested cache path:

```text
~/.cache/latx/aot-v2/<codegen-id>/<source-sha256>/<variant>.so
```

The artifact note repeats all key fields. The loader rejects a mismatch before
`dlopen()`. Cache files are owned by the user, never writable by other users,
opened with no-follow checks, and validated again through the opened file
descriptor to avoid path replacement races. Per-user artifacts do not require
a digital signature, but the loader applies a strict ELF allowlist: no
constructors, no extra dependencies or exports, no W+X segment, and only
approved relocation types and targets.

## 7. End-to-end sequence

### 7.1 Process entry

The user starts the original x86 ELF, not the AOT artifact. Linux either
redirects an x86-64 ELF to the LoongArch LAT executable through `binfmt_misc`,
or the user invokes LAT explicitly:

```text
execve(x86-program)
  -> binfmt_misc starts /usr/bin/latx with the x86 path
```

```sh
latx ./x86-program arg1
```

The kernel therefore creates a native LoongArch LAT process. LAT remains
responsible for the x86 process ABI, system calls, signals, and guest address
space. An AOT v2 module is an `ET_DYN` object loaded by LAT; it is not directly
passed to `execve()`.

LAT then performs the existing linux-user ELF setup:

1. Validate and map the x86 main executable's `PT_LOAD` segments.
2. Read `PT_INTERP` and map the specified x86 dynamic linker.
3. Allocate the x86 stack and write `argc`, `argv`, `envp`, and auxv.
4. Set `AT_ENTRY` to the x86 main executable entry and `AT_BASE` to the x86
   dynamic linker's load address.
5. Set the initial x86 RIP to the x86 dynamic linker entry.

The original x86 mappings remain present even when AOT coverage is complete.
They contain guest data, ELF headers, GOT/PLT, TLS templates, unwind data, and
bytes that the application may inspect.

### 7.2 Initial AOT lookup

Before entering the x86 dynamic linker, LAT identifies the main executable and
`PT_INTERP` by opened file descriptor and source SHA-256. For each image it
looks up a matching AOT v2 artifact.

On a cache hit, LAT performs the following steps:

1. Validate `.note.lat.aot` against the opened source ELF and runtime ABI.
2. Load the LoongArch artifact with `dlopen()` using local symbol visibility.
3. Resolve and validate the versioned `lat_aot_module_v2` descriptor.
4. Supply the x86 load bias and executable guest ranges to
   `lat_aot_register_v2()`.
5. Apply any permitted guest-base fixups, publish the TB index atomically, and
   make the module available to dispatch.

On a cache miss or validation failure, LAT enters the guest through JIT and
records the exact reason. A compiler request may be sent to `latcd`, but the
failure must not prevent the x86 process from starting.

### 7.3 Running the x86 dynamic linker

Execution starts at the x86 `PT_INTERP`, such as
`/lib64/ld-linux-x86-64.so.2`, rather than at the application's `main`.
Translated x86 `ld.so` reads the main executable's dynamic section and maps
its `DT_NEEDED` dependencies through x86 `mmap` system calls.

Those calls pass through LAT's `target_mmap()` implementation. After a
file-backed executable `PT_LOAD` mapping succeeds, the AOT v2 hook groups the
mappings by source ELF, computes the module load bias, and performs the same
cache lookup and registration used for the main executable. This discovers
libc, libm, other startup libraries, and later libraries loaded by `dlopen()`.

The hook must not compile while the mmap lock is held. A hit may load an
already validated artifact after leaving the critical section. A miss queues a
request and allows the x86 dynamic linker to continue with JIT code.

The x86 dynamic linker remains responsible for guest `DT_NEEDED` ordering,
symbol lookup and versions, GOT/PLT relocations, TLS, IFUNC, `LD_PRELOAD`, and
`dlopen()`. The LoongArch dynamic linker resolves only the AOT artifact's LAT
runtime imports.

### 7.4 Transfer to the application

After completing guest relocations and initializers, the x86 dynamic linker
transfers control to the x86 executable entry recorded in `AT_ENTRY`. Every
subsequent control transfer uses the same dispatch rule:

```text
guest RIP
  -> find the registered module whose guest ranges contain RIP
  -> guest RVA = RIP - module load bias
  -> look up (guest RVA, translation flags) in that module's TB table
  -> execute the LoongArch AOT TB when present
  -> otherwise enter LAT JIT
```

Calls through an x86 PLT entry or function pointer still read the target from
guest memory. If the target belongs to another module, global dispatch selects
that module's AOT TB. Cross-module direct patching is not required for the
initial implementation.

### 7.5 Cold and warm cache behaviour

A cold-cache launch has no usable AOT artifacts. LAT starts immediately with
JIT and asks `latcd` to compile the main executable, interpreter, and every
valid file-backed x86 ELF as soon as an executable `PT_LOAD` is mapped.
Validated output is published atomically for a later load. Anonymous executable
memory and guest JIT pages stay on LAT JIT and are not submitted as ELF modules.

A warm-cache launch validates and loads the main executable and interpreter
artifacts before guest entry, then loads library artifacts as the x86 dynamic
linker maps them. Missing or previously unseen paths continue through JIT. A
warm run can claim full file-backed AOT coverage only when runtime statistics
show that every executed file-backed guest TB came from a registered AOT v2
module.

### 7.6 Complete startup overview

```text
exec x86 program
  -> binfmt_misc or explicit latx starts the LoongArch LAT runtime
  -> LAT maps the x86 main image and x86 PT_INTERP
  -> LAT constructs the x86 stack and auxv
  -> identify the main image and interpreter by opened fd and SHA-256
  -> cache hit: dlopen and register matching AOT modules with x86 load biases
  -> cache miss: keep JIT available and queue latcd requests
  -> enter the translated x86 dynamic linker
       -> x86 ld.so mmaps libc or another DSO
       -> LAT mmap hook groups executable PT_LOAD mappings
       -> cache hit: dlopen and register the DSO AOT module
       -> cache miss: continue with JIT and queue a latcd request
       -> x86 ld.so resolves guest GOT/PLT, TLS, symbols, and IFUNC
  -> x86 ld.so transfers control to the x86 application entry
  -> dispatch chooses an AOT TB when present, otherwise a JIT TB
```

The x86 image remains mapped even when all its current TBs have AOT code. Guest
data, ELF headers, GOT/PLT, TLS images, unwind data, and self-inspection still
refer to the original mapping.

## 8. Compiler changes

`latc` gains a module command instead of extending the static-shell command:

```text
latc compile-module X86_ELF -o MODULE.so --tbset FILE
latc inspect-module MODULE.so
latc verify-module MODULE.so X86_ELF
```

The implementation should reuse:

- `tools/latc/cfg` for ELF executable ranges, CFG, and jump tables;
- the copied LAT decoder, IR, TU optimizer, and code generator;
- the native exporter's stable TB and relocation classification;
- the current runtime's x86 state ABI, syscall handling, and dispatch code;
- LAT's existing executable mmap discovery and per-segment AOT recovery timing.

`FILE` is the binary `LATTBKS` format. Its header binds the set to one source
SHA-256 and its fixed records contain only ELF-relative `RVA` and semantic
flags. It contains no execution count and has no compatibility parser. The
runtime records a key when a file-backed JIT TB is created, then submits the
deduplicated set at exit. It does not scan the global JIT TB table.

The static native-image container remains a test vehicle during migration. The
new ELF writer should consume a versioned module intermediate representation,
not parse `.aot2` internals in shell scripts. A small writer based on libelf or
the project's accepted ELF library is preferred over manually concatenating
ELF structures. The linker may be used initially to turn generated assembly,
metadata objects, and a version script into `ET_DYN`.

## 9. Failure rules

An AOT artifact is optional. Failure to find, open, or validate it falls back
to JIT and records a reason. A loaded artifact must never be partially used:
registration is atomic after all validation and guest fixups succeed.

The runtime rejects an artifact for any of these reasons:

- source hash, machine, ELF class, or executable ranges differ;
- AOT ABI or LAT codegen ID differs;
- required LBT, LSX, LASX, page size, or runtime helper version is unavailable;
- TB table, relocation table, PC map, or ELF section ranges are malformed;
- a relocation points outside its permitted text or guest range;
- W+X would be required;
- an executable guest mapping changed after registration.

Writable executable guest code, JIT-generated guest code, and self-modifying
code stay on LAT JIT until page-versioned invalidation is designed. AOT pages
must participate in the same invalidation mechanism as current LAT AOT.

## 10. Implementation milestones

### M0: Freeze the ABI and build a loadable fixture

- Add the standalone AOT v2 ABI header and ELF note definition.
- Build a hand-written `ET_DYN` fixture exporting `lat_aot_module_v2`.
- Load it with `dlopen()`, validate it, register one synthetic TB, and dispatch
  to it.
- Add malformed ELF, ABI mismatch, source mismatch, and W^X tests.

Exit criterion: the fixture is a normal LoongArch shared object and dispatches
without copying the descriptor or using LAT internal structs.

Status: implemented. The M0 fixture has the required named sections, one
versioned dependency on `liblat-aot-runtime.so.2`, no `PT_INTERP`, RPATH,
RUNPATH, constructor, text relocation, or W+X segment. The pre-loader validates
identity, CPU features, dynamic symbols, dependencies, relocation types and
targets before `dlopen()`. The 3A6000 test registers the same artifact at two
guest load biases, dispatches its synthetic TB, and receives `42`.

### M1: Package current translated code as a module

- Add `latc compile-module` and `inspect-module`.
- Convert runtime calls into host ELF relocations and guest addresses into
  `$fp`-relative module-context loads without modifying `.text`.
- Translate the existing static assembly hello into an AOT module and run it
  through the LAT runtime registry, not the standalone native shell.

Exit criterion: ASLR on/off runs produce identical output, zero runtime TB
generation for the fixture, and the artifact passes `readelf`, `dlopen()`, and
format validation.

Status: complete for the static M1 target. `latc compile-module` invokes the
existing LAT exporter and module linker for an x86 ELF, while
`latc inspect-module` validates and reports the resulting artifact without
executing it. The lower-level `latc emit-aot-v2` and
`link-aot-v2-module.sh` consume the current `.latnative` intermediate, replace
guest absolute loads with `$fp` slots, replace the syscall helper address with
a module-local PC-relative trampoline, and produce a byte-identical ET_DYN on
repeated builds. On 3A6000, real translated TBs from `x86-exit42`, no-libc
hello, static glibc hello, and all twelve SPECint2000 programs execute from the
loaded module. The module TB count and PC-map count must exactly match the
native image before execution.

The disposable dynamic LAT runner now loads the same module before guest entry.
Its normal TB lookup falls through to the AOT registry, and the syscall
trampoline calls `helper_raise_syscall`; the existing x86 linux-user CPU loop
and `linux-user/syscall.c` print `Hello, LATC!` and execute `exit(0)`. With bundle
pretranslation disabled, the recorded `runtime_tb_gen_attempts` and
`runtime_tb_gen_calls` are both zero. Repeating the run with
`setarch loongarch64 -R` disables ASLR and produces byte-identical output with
the same zero-translation counters.

The native v2 exporter decodes LAT's existing per-instruction search data into
stable Host offsets and guest PCs before clearing the old packed data from the
copied executable buffer. The packager emits the fully covered subset as
`.rodata.lat.map`; the loader validates every range. LAT's Host signal path
searches this map and reuses `restore_state_to_opc()` to recover the guest PC
and mapped registers. The module declares `LAT_AOT_MODULE_PRECISE_PC_MAP` and
does not carry `LAT_AOT_MODULE_M1_TEST_ONLY`.

The x86-64 guest vDSO is disabled while an M1 AOT v2 module is selected. Static
glibc then uses syscall instructions from the main ELF, which continue through
LAT's existing `linux-user/syscall.c`; runner-provided vDSO code never requires
runtime translation.

On `3a6000-25g` on 2026-08-26, all twelve official SPECint2000 train workloads
passed under a 60-second per-benchmark limit. Every run recorded zero
`runtime_tb_gen_attempts` and zero `runtime_tb_gen_calls`. Ref inputs were not
run.

The copied LAT code generator currently emits LASX vector-state save and load
instructions, so the M1 artifact correctly declares `LAT_AOT_FEATURE_LASX` and
the test runner is limited to LASX-capable 3A6000 systems. A production
LSX-only variant and runtime HWCAP-based variant selection are still required.

### M2: Dynamically linked hello world

- Change execution to accept `LatAotTargetV2` directly, remove the temporary
  `TranslationBlock`, and preserve the full-TB behaviour proven by static M1.
- Discover and register the main executable and `PT_INTERP` before guest entry.
- Load libc and other startup modules from the executable mmap hook.
- Keep guest PLT/GOT, TLS, and IFUNC execution under the x86 dynamic linker.
- Permit per-module JIT fallback and record exact AOT/JIT TB counts.

Exit criterion: a dynamically linked x86 hello runs with ASLR enabled in cold
and warm cache cases; warm cache reports every executed file-backed TB as AOT.

As of 2026-08-26, the main PIE, guest `ld.so`, and guest `libc.so.6` can each be
compiled into a separate AOT v2 ELF and loaded from a cache named
`<source-sha256>.so`. The dynamic glibc hello passes with ASLR enabled and
disabled. A cold cache uses JIT; a warm cache registers all three modules and
reports `aot_lookups` and `jit_fallbacks` for each guest ELF range.

The CPU loop now receives a `LatcAotV2Target` containing the Host code address
and executes it directly. AOT hits do not allocate, publish, or register a
compatibility `TranslationBlock`. A separate per-thread address cache avoids
repeating registry searches and validates the module generation before reuse.
For the static single-module path, the runner publishes the Host address and
guest PC directly to LAT's existing `FastTB` cache. This preserves indirect
TB-to-TB execution without constructing a `TranslationBlock`; `164.gzip`
measured 9.94 seconds versus 9.93 seconds for the removed proxy path on the
same host and input.

Synchronous Host exceptions now recognize AOT module text before entering
LAT's normal signal exit path. `cpu_restore_state()` resolves the generated PC
map and restores guest state without a runtime TB. A static x86 regression
test raises `SIGFPE` inside AOT code, changes guest `RIP` in its handler, and
exits with zero runtime translation.

PIE modules select a two-level or three-level guest-address table according to
their address count. Explicit TB targets and Host-code fall-through successors
must all remain present; unsupported relocations still exclude the dependent
TB rather than executing an invalid partial graph.

Cache-directory mode does not publish module targets to the shared `FastTB`
cache. A direct cross-module jump could otherwise enter code while the guest
address slots still belong to the previous module. Dynamic execution instead
uses the direct address cache in the dispatcher and applies guest slots when
the selected module changes. A module-aware generated-code cache remains M3
work. The counters measure address resolutions and registry-to-JIT fallbacks;
they are not instruction counts.

### M3: `dlopen()`, unloading, and cross-module behaviour

- Test a program that calls a function from a startup DSO and another loaded
  by `dlopen()`.
- Test symbol interposition, symbol versions, TLS, IFUNC, callbacks, and
  `LD_PRELOAD` one at a time.
- Pin modules initially, then add safe reclamation and cache invalidation for
  real unload/reload at a different guest base.

Exit criterion: repeated load, call, unload, and reload does not execute stale
host code or resolve a guest symbol through the host linker.

### M4: On-demand compiler service

- Add the per-user `latcd` request, SHA-256 deduplication, priority queue,
  resource limits, atomic publication, and negative-cache handling.
- Submit the main executable and interpreter at startup and every other valid
  x86 ELF when an executable `PT_LOAD` is mapped.
- Never block mmap locks or translated threads on compilation.

Exit criterion: a cold run completes through JIT while producing valid cache
artifacts; the next run consumes them; concurrent processes publish one valid
artifact without races.

### M5: Production correctness

- Harden precise signal recovery and unwinding across helpers and faults.
- Integrate SMC and executable mapping invalidation.
- Treat cached AOT ELF files as untrusted input and fuzz notes, tables,
  symbols, and relocations through a byte-buffer validator.
- Measure startup time, RSS, shared text, AOT coverage, JIT fallback, and
  steady-state translation efficiency on `3a6000-25g`.

The detailed M5 requirements, implementation rules, and acceptance commands
are maintained in `M5_PRD.md`, `M5_DESIGN.md`, and `M5_TEST_PLAN.md`.
- Fuzz artifact parsing and relocation validation.
- Measure startup, RSS, shared text pages, translation coverage, and steady
  performance on real dynamically linked applications.

## 11. Current implementation boundary

M0 through M3 are complete. Static executables, dynamically linked startup
images, `dlopen()` modules, cross-module calls, callbacks, unload/reload, and
module-generation-aware fast dispatch have focused coverage. The AOT v2 runner
still treats a cache miss as JIT-only for the lifetime of the process.

M4 adds the external per-user compiler service described in
`M4_DESIGN.md`. The one-request publication core and resident service are
complete, including FD transfer, stable source snapshots, validation, atomic
publication, queueing, deduplication, resource limits, and negative caching.
Nonblocking runner submission is also complete for static, startup dynamic,
and later DSO mappings. M4 final regression passed M1-M3, signal recovery,
dynamic symbol semantics, compiler-service concurrency and failure handling,
and a fresh SPECint train 12/12 generation and run. M5 production hardening
remains.

## 12. Rosetta comparison

Apple publicly describes Rosetta 2 as using both JIT and AOT, looking up an
AOT artifact for an executable image through a service, binding that artifact
to the source image's identity, and mapping a cached executable-like object
when available. AOT v2 adopts those general properties.

It does not copy the macOS kernel handoff, Mach-O format, code-signing system,
or `dyld` integration. On Linux, LAT remains the process runtime, x86 `ld.so`
remains the guest dynamic linker, and the generated artifact is a LoongArch
ELF loaded by the host dynamic linker.

Reference: [Apple Platform Security: Rosetta 2 on a Mac with Apple silicon](https://support.apple.com/zh-cn/guide/security/secebb113be1/web).

## 13. Confirmed operating rules

- The original x86 ELF is the only user-visible launch target. `binfmt_misc`
  or explicit `latx` starts LAT; AOT modules are internal cache artifacts.
- One source x86 ELF has one exact merged TB-key set and one atomic manifest.
  The manifest names exactly one immutable AOT module, its canonical native
  image, and the complete published TB-key set. Missing keys use JIT and are
  added to the same set for the next publication.
- AOT and JIT use the same register and CPU-state ABI. Dispatch may return
  either kind of TB and validates cached targets with module or TCG generation.
- A TB key contains guest RVA, x86 code mode, parallel-safe variant, and stable
  semantic translation flags. It does not expose raw internal `cflags` as a
  permanent file ABI.
- Runtime imports come only from versioned `liblat-aot-runtime.so.2`. Generated
  code additionally requires an exact `codegen-id`; a LAT codegen change may
  regenerate the cache.
- Module-internal edges may be direct. Cross-module edges use dispatch and a
  writable per-instance inline cache; they never patch shared `.text`.
- Newly compiled artifacts do not replace code in a running instance. They are
  used by the next process or next module load.
- Any covered guest code page becoming writable, being replaced, or changing
  content deactivates the whole module instance for the first implementation.
- Real translated modules require stable instruction-level Host-PC to guest-PC
  recovery records. AOT signal recovery redirects to a fixed exit trampoline
  and never unlinks or patches AOT code.
- AOT syscalls use the existing LAT `linux-user/syscall.c`; the runtime library
  does not carry a second x86 syscall implementation.
- A successful `fork()` keeps the parent's AOT state unchanged, but the child
  marks inherited AOT state unusable before returning to guest code. The child
  clears its current module and jump cache, stops module discovery and signal
  PC-map lookup, closes the inherited compiler-service connection, and runs
  JIT-only until `execve()`. `execve()` replaces the host process and therefore
  starts with a newly initialized registry.
- TB-key updates are merged by source. `--flush-only` separates collection
  from compilation: `FLUSH_ALL` snapshots the final union and queues each ELF
  once. Default incremental mode uses a 100 ms quiet period and a 500 ms
  maximum wait before compiling newly added keys.
- Offline precompilation starts from one absolute guest ELF path and follows
  only `PT_INTERP` plus recursive `DT_NEEDED` inside the selected x86 rootfs.
  It resolves and statically analyzes the complete dependency set before the
  first request is sent. Each static CFG block produces one `CF_PARALLEL` key;
  that conservative translation is also used by single-thread execution. The
  client then uses the existing `SUBMIT_KEYS` and
  `FLUSH_SOURCE` requests for every ELF; no separate cache or protocol exists.
  Libraries loaded later with `dlopen()` are added by the same runtime
  incremental path.
- Runtime precompilation is disabled by default. With
  `LATX_AOT_V2_PRECOMPILE=1`, the loader sends one source FD when it first maps
  an ELF without a compatible module and waits for `latcd`. The daemon maps
  the FD back into its configured x86 rootfs, performs the same full dependency
  analysis, publishes through the normal TB-set queue, and acknowledges only
  after publication. The waiting LAT process retries module registration
  before it executes guest code. Failure leaves JIT available unless strict
  AOT is also requested.
- The compile delta is the accepted key union minus the TB table in the current
  module, not minus the previously observed TB-key set. If static CFG already
  put all newly observed keys in the module, `latcd` atomically publishes the
  new complete TB-key set and manifest without translation or relinking.
- The per-user compiler defaults to the online logical CPU count capped at
  eight workers. Different source ELFs compile in parallel; one source has at
  most one active compiler job.
- Cache cleanup uses a capacity limit and LRU policy. It preserves the current
  generation while a replacement is being built and removes the superseded
  generation only after the replacement manifest is visible.
- Given identical source, codegen, CPU variant, options, and TB-key-set digest,
  compilation must produce a byte-identical ELF without timestamps, temporary
  paths, process addresses, or random build IDs.

## 14. Current state names and ownership

- **source** is the complete x86 ELF identified by SHA-256. A pathname is only
  diagnostic and never identifies cached code.
- **module** is the one immutable AOT ELF selected for a source, codegen
  identity and exact TB-key-set digest. Host module text stays mapped until
  process exit.
  A module that omits a requested TB variant is rejected before publication.
  Complex-application acceptance counts every executed file-backed ELF,
  including missing modules, and requires 100% AOT lookup coverage for each
  selected application.
  Coverage and end-to-end performance are separate gates: partial AOT cannot
  pass only because it happens to improve elapsed time.
- **instance** is one guest mapping of a module at a specific load bias. It owns
  the guest executable ranges, host text/PC map and dispatch context used by
  normal lookup and signal recovery.
- **generation** starts at one for a registered instance. Deactivation removes
  the instance from the published range snapshot, clears `active`, increments
  generation, and invalidates dispatch entries. A cached target is usable only
  while its saved instance, generation and translation flags still match.
- **TB-key set** is the per-source set of observed guest RVA and semantic flag
  pairs. It contains no execution count. `latcd` serializes updates for one
  source, translates the first immutable snapshot, then translates and merges
  only keys absent from the current module. The complete set digest names each
  published generation.
- **current** is `<source-sha>.current`, a version 2 JSON index naming one
  immutable module, one canonical native image, one complete TB-key set, and
  their source/codegen identity. No earlier manifest format is accepted.
  `latcd` writes it to a private
  temporary file, changes it to read-only, calls `fsync()`, atomically renames
  it over the old index, then synchronizes the cache directory. The cache owner
  lock prevents a second latcd from publishing concurrently.
- **JIT fallback** means normal LAT translation after no active, matching AOT
  target can be proven. A cache miss never makes an existing process adopt a
  newly compiled module; later processes or later module loads can select it.
