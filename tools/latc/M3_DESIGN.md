# AOT v2 M3 design

## Current state

M2 discovers the main executable, interpreter, and startup libc mappings. It
keeps loaded Host modules for the process lifetime, registers instances by
guest address range, and uses a generation-checked dispatcher cache. Dynamic
mode does not publish AOT targets to LAT's `FastTB` cache because that cache
does not identify the guest module whose address slots are active.

The mmap notification path already observes executable file-backed mappings.
M3 extends this path to track unmapping and instance lifetime rather than
introducing a second loader.

## Loading after startup

When the guest dynamic linker completes the executable `PT_LOAD` mappings for
a DSO, the existing ELF tracker computes its source SHA-256 and guest load
bias. The runner opens `<cache>/<source-sha256>.so`, validates codegen and CPU
identity, and registers an instance for that guest range. Missing or rejected
modules remain local JIT fallbacks for that guest ELF.

The first implementation pins the Host module even after `dlclose()`. Pinning
means its Host text remains mapped, but its guest instance can be inactive and
must not be selected. This separates correctness of stale-address rejection
from later Host mapping reclamation.

## Unmapping and reload

Successful guest `munmap()` and fixed replacement mappings notify the ELF
tracker after the mmap lock is released. If an unmap overlaps an instance's
executable guest range, the registry deactivates that instance and increments
its generation. Per-thread direct-address caches compare the saved generation
before use, so an old entry misses. Any module-aware generated-code cache must
perform the same generation check or be cleared during deactivation.

Reload creates a new instance with the current guest load bias. It may reuse
the pinned immutable Host module when source SHA-256 and codegen identity
match. PC-map signal recovery selects the active instance matching the current
module context; ambiguous inactive instances are never accepted.

## Cross-module execution

Guest PLT/GOT and the x86 dynamic linker choose guest targets. AOT v2 does not
resolve guest symbols with `dlsym()` on the Host. Calls, tail calls, returns,
and callbacks reach the normal indirect dispatcher when a generated-code cache
cannot prove the target module and generation.

The final M3 fast path stores Host address, guest PC, module context, and
generation. Before an indirect jump it must either prove that the required
guest-address slots are already active or enter a small glue path that applies
the target module context. A plain `pc/ptr` `FastTB` entry is insufficient for
dynamic cross-module use.

## Delivery order

1. Add `dlopen()` fixtures and prove post-startup mmap discovery while Host
   modules remain pinned.
2. Add focused guest-linker semantic fixtures.
3. Add unmap notification, registry deactivation, generation invalidation,
   and different-bias reload.
4. Add the module-aware fast path and run full correctness and performance
   regression tests.

## Current result

`WI-2262` proves that the existing deferred mmap notification also covers a
DSO mapped after guest startup. The test DSO carries a generation-only ELF
entry so the existing module exporter can run it independently; a profile adds
the exported function used by the real `dlopen()` call. This does not change
the DSO's guest-linker behaviour.

On `3a6000-25g`, the PIE main, interpreter, libc, and plugin are discovered as
four distinct ELF mappings. The cold-cache run completes with module-local JIT
fallback. Warm-cache runs with ASLR enabled and disabled report AOT lookups for
all four modules, including the post-startup plugin, and allocate no
compatibility `TranslationBlock`. Instance deactivation remains `WI-2264`.

`WI-2263` adds a combined guest-linker fixture. A startup DSO calls a main
callback. A later plugin uses TLS, an IFUNC, default and explicitly versioned
symbols, and an undefined hook that the main executable interposes. A preload
DSO provides a separate marker and a competing hook; the result proves the
main executable retains the guest ELF precedence. All function pointers come
from guest PLT/GOT, `dlsym()`, or `dlvsym()` results.

`WI-2264` notifies AOT v2 after a successful guest `munmap()`. Any overlapping
instance is removed from the registry, its generation is incremented, and the
tracker forgets that source/load-bias pair. The immutable Host module remains
mapped and may back a later instance. Dispatcher cache entries retain the old
generation and therefore cannot pass validation after close.

`WI-2265` keeps LAT's original `FastTB` table unchanged and adds a separate
64-byte AOT v2 table consulted only after a normal indirect lookup misses.
Each entry records the guest PC, Host target, module context, generation
address and value, and a precomputed set of guest-address slots. Generated
code rejects stale generations. If the target module differs, it copies the
precomputed slots, updates the CPU's current context, and jumps directly
without returning to the C dispatcher. Signal recovery reads the same CPU
context, so generated-code module switches remain visible to recovery.

The two new CPU context fields move translated register offsets by 16 bytes.
The code-generation identity is therefore `x64-v3`; runners reject older
`v1` and intermediate `v2` modules instead of executing them with stale
offsets. The runner preparation script copies every affected LAT file from
the extracted source tree so a clean rebuild preserves the local adapter.
