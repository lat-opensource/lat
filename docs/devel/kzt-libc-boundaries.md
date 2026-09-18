# KZT libc state at explicit native-call boundaries

Guest TLS storage and cross-libc state propagation are separate facilities.
This layer transfers errno/h_errno values and locale category names across
an explicitly selected native-call boundary. It does not transfer locale_t
objects, ctype pointers or allocator ownership between libc instances.

Each Guest context owns its own non-global locale projection objects; they
are not process-wide cache handles. A current locale obtained with
`uselocale(0)` is borrowed from the boundary runtime and remains valid only
until that context is destroyed. Consumers must call `duplocale()` to obtain
an owned copy before modifying it with `newlocale(..., base)` or releasing
it with `freelocale()`. Borrowed projection handles must not be shared with
other threads. Context teardown deselects its active projections before
freeing them in their owning libc. Ordinary consumer-created locale objects
retain their original ownership.

The caller provides the entry state. On return, the callee provides the
state to propagate back. Guest helper calls used by initialization must
not themselves initiate another semantic boundary.

The broker starts inactive. A consumer first initializes semantic state
for the current Guest context and explicitly prepares the process locale
before invoking native code that requires this protocol. Guest TLS must
already be enabled. Merely enabling KZT or optional Guest TLS does not
activate libc state propagation.

Native consumers explicitly call `kzt_libc_semantic_enter_current()`
and `kzt_libc_semantic_leave_current()` around a Guest-to-Host call.
Host-to-Guest calls that participate use
`latx_run_guest_callback_with_libc()`. The ordinary
`latx_run_guest_callback()`, `RunFunctionWithState()` and formatted
callback entries continue to provide TLS without projecting libc state,
even when another consumer has activated the process broker.

Ordinary Guest pthread key/value operations and TLS destructor ownership
belong to the Guest thread runtime, not this broker.

Locale names must be available in both libc installations. Failure to
reconstruct a required locale is an error, not permission to substitute
a pointer or silently select another locale. Resolver state, cancellation
and other private libc caches are not covered by this protocol.

The extended resolver-isolation probe currently fails on the ABI1 profile:
attached threads can receive the same Guest `__res_state()` object.
Neither TLS opt-in nor this broker makes resolver APIs safe on attached
threads. This is a known limitation, retained in local extended validation,
not a successful test or a bidirectional resolver implementation.
