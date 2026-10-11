# AOT v2 M6 design

## Signal and module lifetime

Use a deterministic two-thread handshake. The execution thread stops inside a translated fault
or PC-map lookup window; the lifecycle thread removes or invalidates the module before the signal
thread resumes. A generation token ties the recovered PC to the registration that supplied the map.
Diagnostics expose module source digest, guest range, generation, and resolved guest PC.

Signal fixtures preserve and compare the complete supported x86 signal frame: GPRs, RFLAGS,
signal mask, and the vector state represented by the current LAT signal ABI. Unsupported host
toolchain features are reported as skipped fields, not silently counted as covered.

## Safe reclamation

Replace permanent retired lists with a reader-aware lifetime scheme. Dispatch and signal readers
enter a read-side critical section without taking the mapping writer mutex. Writers atomically
publish immutable snapshots and retire old snapshots, instances, statistics, and loaded modules.
Objects are freed only after all readers that could have observed the old generation have exited.

Add live/retired object counters and a test-only drain operation. Stress tests assert that counts
return to a bounded steady state after repeated registration and invalidation.

## Mapping revalidation and platform selection

Record enough file-backed mapping identity to re-inspect an image after successful RX restoration
or mremap. Re-registration creates a new instance and repeats source SHA-256 and module validation;
an old inactive instance is never reactivated. Delay invalidation state changes until the guest
mapping operation is known to succeed where LAT ordering permits it.

Derive available AOT features from LoongArch HWCAP. Prefer a LASX artifact when supported and fall
back to an LSX artifact when available. Reject a module requiring an unavailable feature.

## Cache service

latcd accepts bounded worker count and cache-size settings. Publication remains atomic. Eviction
uses least-recently-used validated modules, never temporary files or active compilations. Cache
loading uses immutable publication permissions and rejects externally supplied writable artifacts
unless an immutable copy is created and validated before loading.

## Validation and measurements

The fast loop is a focused local lifecycle/signal test. Strong validation rebuilds the runner on
`3a6000-25g`, runs cold/warm dynamic tests, then executes a multi-hour or high-cycle stress test.
Performance scripts pin CPU placement, record governor/frequency state, use at least five samples,
and retain raw outputs for AOT v2, old AOT, M4, and native LoongArch where available.
