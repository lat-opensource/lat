# Cross-boundary state, ownership, and lifecycle fixes

Read this reference for guest/native, loader, wrapper, callback, registry,
namespace, concurrency, or object-lifecycle defects.

## Draw the actual execution chain

Before changing code, write the stages that the real operation traverses and
locate the first failed stage. Depending on the feature, the chain may include:

```text
process selection
→ object or wrapper eligibility
→ symbol binding
→ relocation or publication
→ bridge or context transition
→ native/guest callback
→ final host, guest, or mixed endpoint
```

An initialization message, provider load, wrapper entry, application success,
or moved error does not prove later stages executed. Independent failures that
share a final exception need separate reproducers and conclusions.

## Define ownership before synchronization

For each shared state item, record:

- authoritative owner and object identity;
- thread-, process-, object-, or namespace-wide sharing scope;
- lock and the exact data it protects;
- publication and visibility point;
- invalidation, unload, or generation transition;
- public and internal operations that belong to the same transaction.

Choose synchronization at the true sharing scope. Increasing a timeout does not
resolve unclear ownership, a missing safe point, or a lifecycle race.

When state can be replaced, unloaded, invalidated, or reused, prove that stale
and in-flight users cannot observe reclaimed storage. Use the current design's
actual lifetime mechanism--for example ownership transfer, reference counts,
generation checks, or a quiescent point--rather than assuming lock release ends
all use. Cover the relevant unload/reload, reuse, and concurrent-callback path.

## Classify state before crossing layers

Do not copy state across guest/host, loader, or thread boundaries because two
structures look similar. Classify every field as one of:

- copy directly;
- relocate or rebase;
- reset for the new owner;
- initialize lazily;
- recreate through a semantic adapter;
- unsupported.

Verify pointer identity, generation, lifetime, and authority for opaque objects,
TLS-like state, callbacks, registries, and loader records.

## Fix the shared boundary, not the triggering application

When an application exposes a shared ABI, context-switch, wrapper, callback, or
namespace defect, fix the shared boundary. Do not add an application whitelist,
hard-coded guest PC, or path-specific bypass.

Derive coverage from the boundary itself: register classes, callee-saved state,
stack and return values, nested callbacks, all dispatching entry points, and
failure/capacity paths as applicable.

## Stage, validate, then publish once

For changes that publish converted pointers, callbacks, registry entries, or
loader state:

1. scan and stage the complete intended update;
2. validate identities, capacities, permissions, and every conversion;
3. publish atomically under the correct lock or transaction;
4. on failure, preserve the previous externally visible state;
5. fail before entering native or irreversible code with partial state.

Do not expose a half-converted shared object and try to repair it after native
code can observe it.

## Separate compatibility fallback from validation

A compatibility runtime may deliberately fail open to an older guest path. A
validation gate for a new component, binding, bridge, callback, or endpoint must
fail closed when that target path is not observed.

Fallback success can show that compatibility was preserved; it cannot show that
the requested cross-boundary feature works.

## Closure evidence

For a cross-boundary fix, report:

- the complete execution chain and first failed stage;
- authority and ownership of every changed state;
- object identity, lock, transaction, publication, and invalidation semantics;
- shared ABI coverage rather than application-only success;
- positive endpoint evidence and a fallback/negative control;
- concurrency, lifecycle, and resource-boundary results when relevant;
- any remaining field or integration gate.

Keep subsystem-specific historical designs out of this general reference unless
they exist in current source and have current regression coverage.
