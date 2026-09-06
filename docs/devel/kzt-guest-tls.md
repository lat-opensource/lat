# Optional Guest TLS for KZT native-thread callbacks

KZT can receive Guest callbacks on pthreads created by a native library.
Such a thread has not executed the Guest pthread creation path. The optional
Guest TLS runtime provides a Guest CPU, stack, TCB and DTV for that thread.

## Activation

The feature is disabled by default. Select it at process startup:

```sh
LATX_KZT=1 LATX_KZT_GUEST_TLS=1 latx-x86_64 program
```

The equivalent command-line option is `-latx-kzt-guest-tls 1`.
Only `0` and `1` are accepted. Enabling this option without an effective
KZT library group does not activate the feature. Configuration is fixed
before Guest execution and must not be changed after threads are attached.

With the option disabled, no Host-thread template is created, automatic
attachment is disabled, and the added loader transaction and TLS refresh
locks are bypassed. Existing library registration remains available.
Guest pthreads continue to own their Guest libc TLS.

## Ownership and lifetime

An immutable template is captured at the Guest program entry point. An
unattached Host thread copies this template, creates its own Guest stack
and constructs Guest TLS from validated live loader information. It never
copies a concurrently executing parent's CPU state or entire pthread
descriptor.

The loader observer reads Guest r_debug/link_map and ELF metadata. Static
TLS placement, dynamic module IDs, object identity and DTV generation must
agree before Guest code executes. An unchanged loader epoch allows a
callback to reuse its TLS without another link_map walk.

Callback execution protects the attached thread's DTV from concurrent
replacement. Loader changes and fork coordinate with this protection.
Internal initialization calls use an explicit no-refresh entry point to
avoid recursively entering the initializer.

Guest pthread keys and values stay in Guest libc. Optional key wrappers
record destructors while forwarding key operations to Guest libc. Attached
thread exit runs recorded TSD and C++ TLS destructors, releases retained
DSOs and handles non-PI robust mutex owner death before releasing TLS.
The standard Guest pthread exit path retains its own destructor ownership.

If Guest code running on an attached thread creates a Guest pthread, CPU
cloning clears the managed stack, TLS allocation, parent snapshot and
destructor-state pointers. Guest libc supplies the child's own TLS through
the normal clone path. The child must not inherit the attached parent's
DTV ownership or execution lock.

## Scope

This feature does not enable bidirectional errno or locale synchronization.
The separate libc boundary facility requires explicit activation.
Constructing a usable Guest libc thread state still initializes that
thread's Guest locale/ctype data.

The implementation targets x86-64 glibc Guest TLS. Other libc layouts,
additional loader namespaces, PI robust futexes and arbitrary non-local
exits across a native callback require separate validation.

This does not implement all private glibc pthread initialization. In
particular, the extended ABI1 resolver probe currently observes shared
`__res_state()` backing storage on attached threads. Resolver APIs are
outside the supported attached-thread profile until that bootstrap is
implemented and validated.

The Guest loader must expose the TLS allocation helpers and a loaded
`dlinfo` provider for module IDs that cannot be established from relocation
evidence. On systems with a separate Guest libdl, the caller must load/link
that library before attachment. Missing helpers reject attachment; the
runtime does not infer private link_map offsets or guess module IDs.

## Tests

`test-kzt-guest-tls-policy` exercises the opt-in policy.
`test-kzt-public-loader-observer` exercises live ELF validation.
`test-kzt-guest-tls-epoch` exercises snapshot reuse.
`test-kzt-guest-tls-opt-in` uses a native probe library to check existing
thread callbacks with the option unset/zero and isolated Host-thread TLS
with the option enabled, including Guest TSD destruction.

With a debug-symbol LATX build, set `LATX_KZT_BOOTSTRAP_GDB=gdb` when
running `test-kzt-guest-tls-opt-in.sh` to additionally check the actual
Guest TLS allocation helper entry. The diagnostic checks that every
observed attached TCB already contains its real thread ID before the
loader helper executes, and fails if that entry is never reached.
The second diagnostic injects one transient busy loader snapshot before
Guest TLS allocation and verifies that attachment retries and executes the
callback. Attachment retries release the failed attempt's resources and
locks before waiting; other initialization failures are not retried.

These test binaries are not part of the default product build.
Target tests require a LoongArch host and matching Guest compilation tools.
SKIP is not a successful runtime result.
