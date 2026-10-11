# AOT v2 M2 product requirements

## Goal

Run a dynamically linked x86-64 application through per-ELF LoongArch AOT v2
modules. The guest dynamic linker remains responsible for x86 PLT/GOT, TLS,
IFUNC, symbol versioning, and relocation semantics. LAT discovers executable
ELF mappings, registers a matching Host module at the actual guest load bias,
and falls back to JIT only for mappings without a usable module.

## Required behaviour

- Accept x86-64 `ET_DYN` main executables as AOT v2 sources.
- Discover the main executable, `PT_INTERP`, and startup DSOs from completed
  executable `PT_LOAD` mappings.
- Register one immutable Host module at one or more guest load biases without
  modifying module text.
- Keep module identity tied to the complete source ELF SHA-256 and LAT codegen
  identity.
- Report AOT lookup, AOT execution, JIT fallback, registration, and rejection
  counts separately for each guest ELF.
- Preserve the existing x86 linux-user syscall and signal paths.
- Preserve static M1 behaviour and its zero-JIT strict mode.

## Acceptance

- A dynamically linked glibc hello prints identical output with ASLR enabled
  and disabled.
- A cold run may use JIT and completes correctly. A warm run uses every
  available main/interpreter/libc module and identifies any remaining JIT by
  guest ELF and guest PC.
- The main executable, interpreter, and libc are registered at their actual
  guest address ranges and resolve the same Host module at different load
  biases when applicable.
- Static hello and all twelve SPECint2000 train programs retain valid output;
  strict static M1 records zero runtime TB generation.

## Exclusions

- `dlopen()` unload/reload and safe module reclamation are M3.
- The asynchronous compiler service and cache publication are M4.
- SMC hardening, fuzzing, signing, and production packaging are M5.
- M2 does not require PGO or broad performance optimisation.

## Result

M2 completed on `3a6000-25g` on 2026-08-26. Dynamic glibc hello passed with a
cold cache, a warm three-module cache, ASLR enabled, and ASLR disabled. The CPU
loop executes AOT Host addresses without compatibility `TranslationBlock`
objects. Static glibc hello, an AOT `SIGFPE` guest-state recovery test, and all
twelve SPECint2000 train workloads passed with zero runtime TB generation.

Dynamic modules still use a dependency-complete subset limited by 256 guest
address slots. Cross-module generated-code caching and unload/reload are not
part of M2.
