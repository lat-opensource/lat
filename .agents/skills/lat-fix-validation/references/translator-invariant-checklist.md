# Translator invariant and fanout checklist

Read this reference before changing translator metadata, generated-code layout,
register/state handling, optimization passes, or compile-time path selection.

## Path symmetry

- Find every current path that implements the same semantic operation: ordinary
  JIT, AOT, build-gated variants, guest widths, linking and fallback paths.
- For a new feature or guard, inspect equivalent paths and state explicitly why
  each is updated or intentionally unaffected.
- Do not copy a historical mode matrix; derive current paths from source and the
  built binary.

## Storage aliasing and sentinel values

- Inspect unions, overlays, bitfields, and reused buffers before changing one
  member. Record byte overlap and every writer that can affect another view.
- Distinguish valid zero from uninitialized zero, absent, reset, and invalid.
- Check width conversions and arithmetic on sentinel values before narrowing or
  storing them in smaller fields.
- Do not assume code-buffer or temporary structures are zero-initialized; verify
  current allocation and reset behavior.

## Mutation fanout

For every changed field or emitted-code location, search all readers, writers,
copies, save/restore operations, and lifecycle transitions. Ask whether the
change invalidates:

- branch offsets, labels, relocation records, or compacted code;
- metadata such as code pointer, code size, jump, link, or successor state;
- caches and lookup identities;
- recovery, unlink/relink, SMC, or invalidation behavior;
- retry state shared across translation passes;
- serialized AOT format or artifact identity.

If a value is initially correct and later wrong, find the real writer with a
watchpoint or boundary comparison instead of listing mutation sites by inspection.

## Register and ABI state

- Resolve register mappings, reserved registers, stack layout, and callee-saved
  obligations from current headers and generated code.
- Across guest/native transitions, verify every state class that remains live,
  including GPR, vector/FPU, flags, stack, return values, and nested callbacks as
  relevant.
- Never import fixed offsets or register assumptions from a historical case
  without proving them against the current build.

## Compile-time visibility

- Compare declarations, definitions, call sites, and tests under the same
  `#ifdef` and feature gates.
- Reproduce release-only behavior with release and `NDEBUG` settings; never put a
  required action inside an `assert()` expression.
- Build affected configurations rather than assuming code compiled in one mode
  exists in another.

## Optimization boundaries

- Preserve the semantic precondition that permits an optimization.
- A mode switch or disabled pass can isolate the failing domain but is not proof
  that the pass is the root cause.
- Final validation must restore the original supported configuration unless an
  explicitly accepted fail-closed fallback is the product decision.

## Required review output

Record the invariant, affected current paths, shared storage or lifecycle,
fanout search, sentinel/width checks, ABI obligations, compile configurations,
and the regression cases that cover them.
