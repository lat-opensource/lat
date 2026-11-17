# Wine WoW64 execution library

`liblatx-wow64.so` embeds the LATX i386 execution engine in a native LoongArch
process. Wine owns the PE loader, Windows services, guest address space and
callbacks. The library executes guest instructions until a service request,
exception or stop, then returns the i386 context to Wine. It does not require
an EC ABI or implement x86-64 Windows execution.

## Build

On a LoongArch Linux host with the normal LAT build dependencies:

```sh
mkdir build-wow64
cd build-wow64
../configure --target-list=x86_64-linux-user --enable-latx --optimize-O1 \
    --enable-wow64 --enable-tests --disable-docs --disable-werror \
    --disable-pie --meson=meson
ninja liblatx-wow64.so
meson test --suite lat-pr-fast --print-errorlogs
meson test test-wow64-provider test-wow64-provider-loader \
    test-wow64-provider-cache --print-errorlogs
LATX_SOFTFPU=2 meson test test-wow64-provider --print-errorlogs
```

The x86_64 Linux-user target supplies the existing engine and can execute in
32-bit protected mode. The public interface supports i386 only. The option is
off by default and requires a non-static build. The shared library and
`latx-wow64.h` are installed by the normal install target.

## Host contract

Load the library with `RTLD_NOW | RTLD_LOCAL | RTLD_NODELETE` and obtain its
versioned function table with `lat_wow64_get_api(LAT_WOW64_ABI_VERSION)`.
The library retains process-wide translation state, signal handlers and an RCU
worker. It must remain loaded until process exit, including after
`process_destroy`; there is no engine shutdown or unload operation.

Create a process and a thread with initialized size and ABI-version fields.
The initial context generation must be zero. A zero maximum entry depth selects
the default of eight. Contexts use the architectural 512-byte FXSAVE format,
including logical ST(0)..ST(7) order and the eight i386 XMM registers. FS/GS bases
are explicit. Initialize control words and MXCSR before execution.

Guest addresses identify host mappings directly; the host must reserve them in
the 32-bit address space and provide the backing memory and protections.
Register mappings through `memory_event` before executing them. Notify changes
through MAP, PROTECT, UNMAP, WRITE and FLUSH. These notifications serialize backend
publication. Executable changes interrupt active guests and wait for their entry
calls to finish before returning. The host must prevent execution of reclaimed
memory until notification completes; notifications do not allocate or free host
mappings. An entry interrupted by such a change can return STALE_MAPPING and an
updated context; reenter after the update completes. Completed services, faults
and breakpoints retain their exit reason when a mapping changes concurrently;
the host must still handle those exits.

Each thread can execute on only one host thread at a time. Different guest
threads execute concurrently. Keep thread/process handles alive while calls use
them. Destroy a thread only after its entry has returned and pending replies
have been completed. CPU-list readers and delayed reclamation are handled by
existing QEMU RCU mechanisms, without waiting for unrelated long-running guests.
Loading the library on a host worker thread that subsequently exits is supported.

`enter` returns service numbers and exception details instead of performing
Linux guest system calls. For exits carrying REPLY_REQUIRED, send exactly one
reply with the returned sequence before reentry. Use CONTINUE after a handled
service, or update the context and use the appropriate advertised reply action.
Explicit entry and redirect PCs must fit in 32 bits; only entry accepts
`LAT_WOW64_ENTER_CURRENT_PC` as a sentinel. Effective i386 addresses wrap at
32 bits. Executing a mapping without EXEC returns a page-fault exit with the
instruction-fetch error bit, including when an instruction crosses a page.
A configured `int3` address is the host's optional return sentinel; other `int3`
instructions produce breakpoint exits.

Only synchronous SIGSEGV, SIGBUS, SIGILL and SIGFPE faults are intercepted for
translated execution and protected code pages. Other host signals retain their
handlers. The engine also uses SIGRTMIN+1 with its existing fork-unlink packet
format to interrupt linked translations, chaining unrelated deliveries to the
previous host handler. Do not replace these handlers after initialization.

Hardware debug breakpoints are not implemented: enabling DR7 bits 0..7 is rejected
by thread creation and context updates. General debugger support, live guest
register capture from a suspended host signal handler, AOT loading and engine
unloading are outside this interface. Context functions are normal synchronized
calls, not async-signal-safe operations. AVX/YMM state is not part of this ABI.

## Validation

The fast suite includes the platform-independent core test: context generations,
thread state, reply sequences, concurrent memory publication and active-entry
barriers. The LoongArch integration test loads the actual shared library and
executes i386 code. It checks system-call exits, FS reloads, x87/SSE conversion,
precise access faults, concurrent execution, independent CPU creation/destruction,
host control-signal delivery, interruption for unrelated code-page updates,
return sentinels and self-modifying code.

Wine integration is a separate consumer. It needs native Loong64 ntdll, wow64,
wow64cpu and wow64win plus matching i386 Windows DLLs. Its CPU adapter converts
Windows contexts and routes service requests to WoW64. Mixing DLLs from different
Wine revisions can change service numbers or Unix-call tables and is not a valid
integration test.

Native integration also checks callee-saved host floating-point registers,
forwarded-signal syscall restart, worker-thread library loading and progress
through translation-cache exhaustion. `LATX_SOFTFPU` and `LATX_SOFTFPU_FAST` are
read once at first process creation, using the existing LATX option handlers.
Other Linux launcher options and per-application configuration are not parsed.
The default x87 implementation uses native double precision; use
`LATX_SOFTFPU=2` for architectural 80-bit precision. The test includes a precision
sentinel and pending x87/SSE exception checks.

Unmasked native floating-point faults preserve their x87 or SSE trap and status
when returned to Wine. This does not provide independent x87/SSE exception
controls: the existing engine shares native FCSR exception enables. In particular,
CW=0x037f with MXCSR=0x1d80 currently fails to raise an SSE divide fault. Tests of
SSE fault delivery enable divide in both controls to isolate delivery from that
known limitation. Mixed-control floating-point semantics need separate work.
