# AOT v2 M5 design

## Untrusted artifact parsing

The file-descriptor entry point keeps ownership and permission checks, maps the
regular file read-only, and calls a new byte-buffer validator. The byte-buffer
validator performs no I/O and never keeps pointers after it returns. This
separation lets tests feed every truncation and integer boundary directly while
production retains the stronger file checks.

All table arithmetic uses checked ranges before pointer construction. ELF
headers, program headers, section headers, notes, dynamic entries, string
tables, symbols, and relocations are rejected when their entry sizes, counts,
links, offsets, or address ranges are inconsistent. The parser has no input-
dependent allocation and its loops are bounded by tables already proven to fit
inside the supplied buffer.

The deterministic test mutates structural bytes with a fixed seed and tests
all truncation lengths. It runs under ASan and UBSan when the compiler supports
them. A separate `LLVMFuzzerTestOneInput` target uses the same public buffer
entry point for longer libFuzzer runs without adding a production dependency.

## Executable mapping invalidation

Each registered module owns immutable guest source ranges and dispatch entries.
The runner connects those ranges to LAT's existing executable-page and SMC
notifications. Before a guest operation can write, replace, unmap, or remove
execute permission from an overlapping range, the runner marks matching module
ranges unusable and removes their dispatch targets.

Invalidation is monotonic for one module registration: an invalidated target is
never made live in place. A later executable mapping is treated as a new image
and must pass source identity, module validation, and normal registration again.
Threads that raced with invalidation either finish a translation already
entered under LAT's existing execution rules or observe the removed target on
their next dispatch; reclamation must not free module memory while a thread can
still execute it.

Anonymous executable mappings always use JIT. File-backed mappings that are
private but become writable are also invalidated, because copy-on-write can
change their bytes without changing the backing file.

## Signals and unwinding

The generated artifact contains the sorted precise PC map. Loading validates
the table once and registers immutable table bounds; startup does not sort,
copy, or rebuild the map. Signal recovery finds the owning module from the host
PC, performs a bounded lookup in its prebuilt map, and reconstructs the guest
PC and state required by LAT's existing signal path.

Tests cover faults at translation boundaries, inside translated instructions,
inside helpers, nested guest signals, `sigaltstack`, `sigreturn`, module unload,
and reload at a different guest address. When an exact entry is unavailable,
the runner must take the existing safe JIT path or terminate with an explicit
diagnostic; it must not guess a guest PC.

## Measurements

Correctness counters distinguish registered modules, invalidated ranges, AOT
lookups, AOT hits, JIT fallbacks, runtime TB generation, and signal PC-map
lookups. Performance tests run on `3a6000-25g` with fixed CPU placement and the
same inputs for AOT v2, old AOT, LATC M4, and native LoongArch where applicable.
Reported translation efficiency is `native runtime / translated runtime *
100%`, matching the existing LATC performance metric.

Cold measurements start without a usable module. Warm measurements first prove
that the expected cache artifact exists and validates. Every result records
binary SHA-256, runtime SHA-256, cache state, repetition count, median, and raw
samples. A one- or two-point fluctuation is reported but is not treated as a
regression without repeated evidence.
