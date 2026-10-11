# AOT v2 M3 product requirements

## Goal

Support x86-64 DSOs loaded after process startup. A guest application may use
`dlopen()`, call translated code in the new DSO, pass callbacks across module
boundaries, call `dlclose()`, and load the same or another DSO at a different
guest address without executing stale Host code.

The x86 dynamic linker remains responsible for guest ELF relocation, symbol
versions, interposition, TLS, IFUNC, PLT/GOT, and `LD_PRELOAD`. AOT v2 only
maps a guest executable address to code generated from the same source ELF.

## Required behaviour

- Discover executable mappings created by `dlopen()` after guest startup.
- Register a matching cached AOT v2 module at the completed guest load bias.
- Keep cold-cache execution correct through per-ELF JIT fallback.
- Execute calls and callbacks between the main program, startup DSOs, and
  `dlopen()` DSOs without using the Host dynamic linker for guest symbols.
- Detect executable unmapping and deactivate the corresponding module
  instance before its cached Host addresses can be selected again.
- Allow reload at a different guest address and bind a new instance and
  generation to that address.
- Keep M1 static execution and M2 startup-module execution unchanged.

## Acceptance

- A warm-cache fixture loads a DSO with `dlopen()`, calls it, receives a
  callback, and reports AOT use for both the main executable and loaded DSO.
- Focused fixtures cover startup DSO calls, TLS, IFUNC, symbol interposition,
  symbol versions, and `LD_PRELOAD`.
- Repeated load/call/close/reload never executes an inactive instance or stale
  Host address, including when the guest load bias changes.
- A cold cache completes correctly and reports the exact module-level JIT
  fallback.
- Static hello, dynamic hello, AOT signal recovery, and SPECint2000 train
  remain valid with zero runtime translation in strict static mode.

## Exclusions

- The asynchronous compiler service and automatic cache publication are M4.
- Arbitrary self-modifying code and production SMC hardening are M5.
- M3 does not require SPEC ref or general performance tuning outside the new
  cross-module dispatch cost.
