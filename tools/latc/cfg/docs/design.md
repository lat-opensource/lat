# Design

## Layout

- `src/cfg_dump.c`: command-line parsing, top-level orchestration, and summary
  output.
- `src/common.c`: shared fatal error handling, checked allocation, byte-range
  validation, and little-endian/LEB128 readers.
- `src/elf_image.c`: ELF file loading, section lookup, executable range
  mapping, dynamic/static linking classification, and GOT/PLT/ijmp section
  views.
- `src/function_symbols.c`: function discovery from `.symtab` / `.dynsym` and
  `.eh_frame` FDE fallback, including deduplication by entry address.
- `src/cfg_graph.c`: per-function CFG construction, call/jump annotation,
  edge printing, and handoff to CFG checks.
- `src/cfg_decoder.c`: lightweight x86-64 instruction-length and CFG
  terminator decoder.
- `src/ijmp_resolve.c`: indirect jump recovery for jump-table patterns.
- `src/got_plt.c`: GOT/PLT relocation parsing, symbol lookup, and small
  instruction decoders for GOT calls and GOT tail jumps.
- `src/cfg_check.c`: CFG integrity checks over constructed block and edge
  facts.
- `src/app_ownership.c`: heuristic application-code ownership filter used for
  app-only summaries.
- `src/capstone_shadow.c`: optional Capstone comparison for decoder length
  validation.
- `include/`: public headers shared by the modules.

## Function Discovery

The preferred source is symbol tables:

- `.symtab`
- `.dynsym`

Symbols are deduplicated by function entry address. This removes libc alias
duplicates such as `raise` / `gsignal`.

If a binary has no complete symbol table, the tool parses `.eh_frame` FDEs and
generates names in the form:

```text
sub_0000000000401780
```

For stripped PIE binaries that still have a small `.dynsym`, symbols and
`.eh_frame` are merged.

## Cross-Function Edges

Direct control-flow edges that leave the current symbol range are classified
before CFG checking:

- `tailcall`: a direct `jmp` to another known function entry;
- `plt-tailcall`: a direct `jmp` to a known `.plt` stub;
- `interprocedural`: a direct branch/call target that resolves to another known
  function entry;
- `cold-fragment`: an edge between matching hot/cold split functions, such as
  `foo` and `foo.cold`;
- `external`: a GOT/PLT tail jump.

Only unresolved cross-function edges keep the function in `open` status.
Classified legal cross-function edges are counted separately in
`legal_cross_function_edges`, `tailcall_edges`, `interprocedural_edges`,
`plt_tailcall_edges`, `cold_fragment_edges`, and `external_exit_edges`.

The `.init` and `.fini` sections are added as special synthetic functions when
present. This keeps edges such as `call_fini -> _fini` classified as legal
tailcalls even though `_init` / `_fini` may have zero-sized symbols or no
ordinary `.text` function range.

In stripped binaries the `.cold` suffix is usually gone because `.eh_frame`
fallback names are synthetic (`sub_...`). The same address-based edges are
still treated as legal cross-function edges, but hot/cold split edges degrade to
`tailcall` or `interprocedural` instead of `cold-fragment`.

## Application-Code Filter

Static ELF files do not encode a strict boundary between benchmark code and
statically linked libc/libgcc code. The current filter is intentionally simple
and optimized for unstripped SPEC binaries:

1. Find the `main` symbol.
2. Treat `main` as the beginning of application code.
3. Skip known startup/runtime helpers that may appear immediately after `main`.
4. After the first post-`main` application-looking function cluster begins,
   stop at the first known runtime/library function.
5. Exclude known runtime/library names inside the selected address range.

The runtime/library list intentionally treats libgfortran startup helpers,
glibc startup helpers, CPU feature probes, GOT/PLT support routines, and common
libc functions as non-application code. Fortran module symbols containing
`_MOD_` are explicitly allowed even though they often begin with `__`, because
SPEC Fortran benchmarks use those names for real benchmark functions.

This produces `app_filter_summary` near the top of the log and app-only
`app_cfg_check_summary` / `app_transfer_summary` at the end. It is a convenience
view, not a proof of ownership; false positives are allowed. Fully stripped
binaries normally lack a `main` symbol, so the filter reports
`available=no reason=main-symbol-not-found` and only the global summaries are
authoritative.

## CFG Construction

The scanner decodes x86-64 instruction lengths and records terminators:

- conditional branches: `0x70..0x7f`, `0f 80..8f`
- direct jumps: `eb`, `e9`
- indirect jumps: `ff /4`, `ff /5`
- returns: `c3`, `c2`, `cb`, `ca`
- hard stops: `int`, `iret`, `hlt`

Calls do not split basic blocks, but call annotations are printed inside the
containing block. Direct calls print their target address. IFUNC/GOT calls
print symbol-level annotations. Unresolved indirect calls print a source type.

Instruction-length decoding handles common VEX/EVEX and operand-size cases
well enough to keep control-flow boundaries aligned:

- in 64-bit mode, `c4`/`c5` are treated as VEX and `62` is treated as EVEX;
- `0f 38 f0` (`movbe`) is decoded as a ModR/M instruction without an
  immediate byte;
- accumulator-immediate forms such as `66 3d imm16` honor the operand-size
  prefix.

These details prevent false branch targets from being discovered in the middle
of vector instructions or after an overlong immediate decode.

## Capstone Shadow Compare

When `--shadow-capstone` is enabled, every instruction boundary produced by
`src/cfg_decoder.c` is decoded once with Capstone in x86-64 mode. The tool
compares the simple decoder length with Capstone's length and reports:

- `decode_failures`: Capstone could not decode bytes at a CFG decoder boundary;
- `length_mismatches`: both decoders succeeded but returned different lengths.

The comparison intentionally validates length only. CFG classification and
branch target recovery still come from the local lightweight decoder.

## CFG Checks

`src/cfg_check.c` validates the graph after each function is printed. It checks:

- block ranges stay inside the function and do not overlap;
- block coverage has no gaps inside the declared function range;
- internal edge targets land on a constructed basic-block start;
- conditional and fall-through blocks have their expected outgoing edges;
- unresolved indirect jumps are reported as open exits;
- direct jumps or GOT tail jumps that leave the function are counted as
  cross-function edges.

An x86 branch may legally target the middle of a previously decoded
instruction when the skipped bytes are legacy prefixes. A common glibc pattern
is:

```asm
je     no_lock
.byte  0xf0
no_lock:
cmpxchg qword ptr [rcx], rbx
```

Both entry points share the same opcode bytes except for the skipped prefix.
The checker records this as `prefix_entry_edges` instead of
`unclosed_internal_edges`.

Per-function status values are:

- `ok`: no errors or warnings.
- `open`: structurally valid, but has known open exits such as unresolved
  indirect jumps or cross-function tail jumps.
- `error`: an internal CFG edge does not close on a block start, a block is
  malformed, or block coverage overlaps.

The final `cfg_check_summary` aggregates these counts over all printed
functions.

## Indirect Jumps

`src/ijmp_resolve.c` resolves these supported forms:

```asm
lea    base, [rip+table]
movsxd dst, dword ptr [base+idx*4]
add    dst, base
jmp    dst
```

```asm
lea    base, [rip+table]
movsxd off, dword ptr [base+idx*4]
lea    dst, [base+off]
jmp    dst
```

```asm
lea    base, [rip+table]
jmp    qword ptr [base+idx*8]
```

```asm
lea    base, [rip+table]
mov    dst, qword ptr [base+idx*8+disp]
jmp    dst
```

```asm
lea    dst, [rip+target]
jmp    dst
```

The resolver accepts local targets only when they land inside the current
function and on a decoded instruction boundary. It may also accept
interprocedural qword function tables, but only when the table entry is a known
function entry. This is deliberately stricter than accepting arbitrary
cross-function instruction addresses.

For cross-function qword tables, the resolver requires at least two consecutive
valid table entries before treating the indirect jump as closed. This avoids
misclassifying sparse runtime tables such as GCC's `insn_gen_function`, whose
first entry is valid but many following entries are zero.

Resolver pattern matching also starts only at decoded instruction boundaries.
This avoids false positives from byte sequences inside another instruction,
for example treating the second byte of a REX-prefixed `lea` as a standalone
table-base load.

### Known Remaining Indirect Jump Classes

Some unresolved indirect jumps are intentionally left open because closing them
from the static ELF image would imply data-flow or runtime-state assumptions
that the tool does not currently make.

`mesa_base.Of.gcc830.dyn` uses a Mesa/OpenGL dispatch table. Wrapper functions
load the current context pointer from the BSS global `CC`, then tail-jump
through a slot:

```asm
mov    rdi, qword ptr [rip+CC]
test   rdi, rdi
je     fallback
jmp    qword ptr [rdi+slot]
```

or equivalently:

```asm
mov    rax, qword ptr [rip+CC]
mov    dst, qword ptr [rax+slot]
jmp    dst
```

The actual target depends on the runtime GL context and API dispatch table, so
these edges are dynamic dispatch rather than static jump-table cases. They are
reported as typed unresolved transfers, usually `object-field` or `register`.

`gap_base.Of.gcc830.dyn` uses runtime-initialized method tables such as
`EvTab`, `TabSum`, `TabDiff`, `TabProd`, `TabLt`, and `TabEq`. These symbols
live in BSS, so the ELF file contains no table entries to read. Initialization
code such as `InitEval` fills the tables at runtime before operations dispatch
through them:

```asm
lea    base, [rip+TabSum]
mov    dst, qword ptr [base+idx*8]
jmp    dst
```

Recovering these targets would require simulating or summarizing the table
initializers. The project currently treats them as runtime-initialized tables
and leaves them open.

## Typed Unresolved Indirect Transfers

When an indirect call or jump cannot be resolved to concrete targets, the tool
still records the source operand shape:

```text
call unresolved 0x000000000043ba93 kind=object-field base=r15 disp=56
call unresolved 0x00000000004016b7 kind=stack-memory base=rsp disp=0
ijmp unresolved 0x00000000004262fa kind=register reg=rax
```

Current target kinds:

- `register`: target is held directly in a register.
- `rip-memory`: target is loaded from RIP-relative memory, usually a GOT-like
  slot when no relocation name is available.
- `stack-memory`: target is loaded from `rsp`/`rbp`-relative memory or extended
  stack registers.
- `object-field`: target is loaded from a base register plus displacement,
  commonly an object/vtable or callback field.
- `indexed-memory`: target is loaded from base plus index addressing but did
  not match a supported jump-table pattern.
- `absolute-memory`: target is loaded from an absolute memory form.

## GOT/PLT

`src/got_plt.c` parses relocation tables into:

- external GOT symbols: `GLOB_DAT`, `JUMP_SLOT`, `R_X86_64_64`
- static IFUNC entries: `R_X86_64_IRELATIVE`
- `.plt` stubs pointing at GOT slots

This enables annotations such as:

```text
call extern 0x00000000000031c5 -> fcntl@GOT[0x0000000000027d28]
edge ijmp  0x00000000000031cf -> fflush_unlocked@GOT[...] ; external
call ifunc  0x0000000000402c2b -> memmove@PLT[...] GOT[...]
```

## Summary

The `summary` section reports the binary shape before CFG entries:

- ELF type
- static vs dynamic linking
- function discovery source
- GOT symbol counts
- external vs IFUNC GOT entries
- PLT entry count
- call modes observed by static structure, including typed unresolved
  indirect transfers

`app_filter_summary` reports whether the heuristic application-code filter was
available, the selected range, and the number of functions classified as app
code. The final `cfg_transfer_summary` and `app_transfer_summary` count
unresolved indirect calls, unresolved indirect jumps, recovered jump tables,
and recovered jump-table targets.
