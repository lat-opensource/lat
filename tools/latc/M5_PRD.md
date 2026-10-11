# AOT v2 M5: production correctness and hardening

## Goal

Make AOT v2 safe for long-running, dynamically linked programs whose
executable mappings, signals, and loaded objects change while the process is
running. Treat every cached AOT ELF as untrusted input before `dlopen()`.

## Required behaviour

- Malformed, truncated, or adversarial AOT ELF files must be rejected without
  an out-of-bounds access, integer overflow, excessive allocation, or hang.
- Replacing, unmapping, or making guest executable bytes writable must prevent
  later dispatch through stale AOT translations for the affected guest range.
- Restoring executable permission may use AOT only after the mapped bytes still
  match the compiled source identity and the module is registered again.
- Synchronous signals and helper faults must recover the exact guest PC from
  the AOT PC map. Guest signal delivery and return must preserve x86 state.
- Host unwinding and diagnostics must identify the owning AOT module and guest
  PC without scanning or rebuilding PC maps at process startup.
- Static hello, dynamic hello, `dlopen`/`dlclose`, SPECint2000 train, and M1-M4
  failure behaviour must remain correct.

## Out of scope

- Signed artifact distribution or a system-wide privileged cache.
- Translating anonymous guest JIT code ahead of time.
- SPEC ref workloads.
- Replacing LAT's existing guest SMC machinery; M5 connects AOT registration
  and dispatch to it.

## Milestones

1. Extract an in-memory ELF validator and run deterministic mutation tests plus
   sanitizer-backed fuzzing against notes, tables, symbols, and relocations.
2. Invalidate AOT dispatch for `mmap`, `munmap`, `mprotect`, and guest writes
   that can change executable bytes.
3. Cover precise signal recovery, signal return, helper faults, and module
   unload/reload with exact guest-PC assertions.
4. Measure startup time, RSS, shared executable pages, AOT coverage, JIT
   fallback, and steady-state efficiency on `3a6000-25g`, then run all M1-M4
   regressions and SPECint2000 train 12/12.

## Exit criteria

- The parser fuzz target has no sanitizer finding in the recorded bounded run,
  and all discovered failures are fixed regression inputs.
- No test can execute a stale AOT translation after executable guest bytes or
  mapping permissions change.
- Signal tests recover the expected x86 PC and register state from AOT code.
- Every production measurement states the exact binary digest, host, command,
  repetitions, and raw counters.
- SPECint2000 train passes 12/12 with a 60-second limit per benchmark; ref is
  not run.
