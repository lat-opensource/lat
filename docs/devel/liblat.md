# Embedded liblat runtime

The shared library runs trusted x86-64 Linux Guest code inside a LoongArch
Linux application. It requires KZT and a matching x86-64 runtime prefix.
It does not isolate Guest code from the embedding process.

## Build and test

Run `latxbuild/buildliblat.sh -c` to configure and build the shared library.
Use `-c -O 0`, `-c -O 2`, or `-c -O 3` for other optimization levels and
`-c -d` for a debug build. `LIBLAT_BUILD_DIR` selects the build directory.
An incremental invocation requires an existing shared-library configuration.
The configure interface is `--enable-latx --enable-kzt --enable-liblat
--target-list=x86_64-linux-user`. Static linking is unsupported.

Configure with `--enable-tests` to register the `liblat-*` integration tests.
They require `LATX_X86_64_SYSROOT` and either `LATX_X86_64_CC` or prebuilt
fixtures in `LIBLAT_GUEST_ARTIFACT_DIR`. Prebuilt fixtures must match the
checked-in sources; record compiler, link inputs, and binary hashes alongside
results. Run `meson test -C BUILD --suite latx-integration liblat-*`.

## Lifetime and errors

Include `latx/liblat.h`. Call `lat_init(false, 2, argv, query, offsets)`, where
`argv` contains a program name and the path to an x86-64 bootstrap ELF.
The bootstrap loads the Guest runtime and exits with status zero. Set
`LAT_LD_PREFIX` before initialization. The callbacks must remain valid for the
runtime lifetime. This profile does not support Host-dispatched signals;
passing `true` returns `-ENOTSUP`.

There is one runtime per process. Basic argument and ELF header validation
returns negative errno values before consuming that runtime. Once execution
starts, subsequent initialization with valid arguments returns `-EALREADY`, including after a late
initialization failure. A nonzero bootstrap exit is an initialization error.
Loader failures and fatal Guest execution errors can terminate the embedding
process; a negative return is not a guarantee of recoverability for arbitrary
Guest failures.

Join or otherwise quiesce all runtime callers before `lat_end`. It drains
Guest exit callbacks in reverse registration order, including callbacks
registered during finalization. Reentrant finalization does not run a record
twice. It does not destroy the runtime, restore process signals, or permit
reinitialization. The shared object is linked with `NODELETE`; unloading and
reloading it does not create a new runtime. Do not unload Guest modules while
another thread is executing their code. Native `fork` followed by continued
use of the inherited runtime is unsupported; use a fresh process via `exec`.

## Boundaries

The loader entry points retain the existing NBL ABI. `is_lat_method` is a
compatibility alias for `is_lat_symbol`. Native threads entering Guest code
receive independent CPU and Guest TLS state. Nested callbacks save boundary
records dynamically rather than relying on a fixed nesting limit.

The shared build reserves only addresses it owns. Guest fixed mapping and
unmapping operations must not replace unrelated Host mappings. The embedding
application must likewise not replace or unmap runtime-owned mappings, close
runtime-owned descriptors, or change process-wide runtime state concurrently.
Host and Guest libc objects such as `localeconv()` results are distinct;
compare their contents, not their addresses. JVM adapters are a separate
product integration and are not required by the shared library.
