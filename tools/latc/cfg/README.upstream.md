# CFG Dump

`cfg_dump` builds a lightweight intra-procedural CFG for x86-64 ELF
binaries. It was written for comparing SPEC CPU2000 `crafty` binaries, but it
also handles common stripped PIE system binaries.

## Build

```sh
make
```

The default build links against Capstone for optional shadow comparison.
Override `CAPSTONE_CFLAGS` / `CAPSTONE_LIBS` if `pkg-config capstone` is not
available.

## Usage

```sh
./cfg_dump [--shadow-capstone] [binary] [function-name-substring]
```

Default binary:

```sh
./crafty_base.Of.gcc830.dyn
```

Examples:

```sh
./cfg_dump crafty_base.Of.gcc830.dyn main
./cfg_dump crafty_base.Of.gcc830.dyn.strip sub_0000000000401780
./cfg_dump /usr/bin/ls error
./cfg_dump --shadow-capstone crafty_base.Of.gcc830.dyn ValidatePosition
```

## Output

The output starts with a `summary` section:

```text
summary
  binary=crafty_base.Of.gcc830.dyn
  elf_type=EXEC
  linking=static
  function_source=symbols
  functions=1251
  got_symbols=26
  got_external=0
  got_ifunc_irelative=26
  plt_entries=26
  call_modes=direct, plt-ifunc, jump-table, typed-unresolved-indirect
  cfg_check=enabled
  shadow_capstone=enabled
```

For unstripped static SPEC-style binaries, a second early section reports a
best-effort application-code filter:

```text
app_filter_summary
  available=yes
  method=main-contiguous-known-lib-stop
  reason=main-symbol
  range=0x0000000000401780-0x0000000000426920
  functions=109
  precision=heuristic false_positives_allowed
```

Each function then prints basic blocks and edges:

```text
function 0x0000000000401780 size=7222 name=main blocks=190
  bb 0x0000000000401780-0x00000000004017cd term=normal @0x00000000004017c6
    edge fall  0x0000000000401780 -> 0x00000000004017cd
  cfg_check status=ok errors=0 warnings=0 blocks=190 edges=...
```

The run ends with `cfg_check_summary`, `cfg_transfer_summary`, and, when the
application-code filter is available, `app_cfg_check_summary` /
`app_transfer_summary`. The app summaries use the same counters but only for
functions classified as application code. `status=open` means the graph has
known open exits such as unresolved indirect jumps or cross-function tail
jumps.
`status=error` means an internal edge did not close on a basic-block start,
blocks overlap, or block coverage is structurally invalid.
Some x86 code branches into the middle of a decoded instruction by skipping
legacy prefixes, for example entering after `lock` before `cmpxchg`; these are
reported as `prefix_entry_edges` and are not treated as closure errors.

With `--shadow-capstone`, the run also ends with `capstone_shadow_summary`.
It compares every simple decoder instruction length against Capstone at the
same address and reports decode failures or length mismatches.

## Capabilities

- ELF64 little-endian x86-64 input.
- Function discovery from `.symtab` / `.dynsym`.
- Stripped binary fallback via `.eh_frame` FDE ranges.
- Basic block splitting on `jcc`, direct `jmp`, indirect `jmp`, `ret`,
  `int`, and `hlt`.
- Direct branch target recovery for short and near relative branches.
- GCC-style PIC jump table recovery.
- Absolute qword jump table recovery.
- Register-loaded qword function table recovery when entries resolve to known
  function starts.
- Fixed-target register jump recovery for local `lea target; jmp reg`
  patterns.
- Dynamic GOT call/tail-jump symbol annotation.
- Static IFUNC `.plt` / `R_X86_64_IRELATIVE` annotation.
- Direct call target annotation.
- Typed unresolved indirect call/jump annotation.
- Heuristic application-code summary for unstripped static SPEC-style
  binaries.
- CFG consistency checks for coverage, cross-function edges, open indirect
  jumps, and internal edge closure.
- Optional Capstone shadow comparison for decoder length validation.
- Cross-function edge classification for tailcalls, interprocedural edges,
  PLT tailcalls, hot/cold fragments, and GOT/PLT exits. Stripped binaries keep
  address-based classification through `.eh_frame`, but lose `.cold` name
  matching and may report those edges as tailcall/interprocedural instead of
  `cold-fragment`.

## Source Layout

- `src/cfg_dump.c`: CLI, summary, and top-level orchestration.
- `src/common.c`: fatal diagnostics, checked allocation, bounds checks, and
  little-endian/LEB128 readers.
- `src/elf_image.c`: ELF loading, section lookup, and function byte mapping.
- `src/function_symbols.c`: symbol and `.eh_frame` based function discovery.
- `src/cfg_graph.c`: per-function CFG construction and edge/call printing.
- `src/cfg_decoder.c`: lightweight x86-64 instruction decoder.
- `src/got_plt.c`, `src/ijmp_resolve.c`, `src/cfg_check.c`: GOT/PLT,
  jump-table, and CFG integrity analysis.
- `src/app_ownership.c`: best-effort application-vs-library ownership
  filtering for summary counters.

## Limitations

- This is not a full x86 decoder. It decodes enough instruction length and
  operand detail for CFG construction and common jump-table patterns.
- Function pointer jumps and object/vtable dispatch remain
  unresolved unless they match a supported table pattern. The output labels
  their source type, such as `register`, `object-field`, `stack-memory`,
  `indexed-memory`, or `rip-memory`.
- Runtime-dispatch systems are intentionally not forced closed. In SPEC
  CPU2000 this includes Mesa's GL dispatch table through the BSS `CC` context
  pointer, and GAP's BSS method tables such as `EvTab` / `TabSum`, which are
  populated by initialization code rather than stored in the ELF file.
- IFUNC resolver output is CPU-feature dependent. The tool identifies the
  IFUNC call and resolver symbol, not the single runtime implementation.
- External library CFGs are not expanded.
- The application-code filter is intentionally heuristic. It needs a `main`
  symbol, so fully stripped binaries usually report `available=no` until a
  stronger entry-point analysis is added. The current rules handle common
  SPEC C and Fortran layouts, including Fortran `_MOD_` symbols and the
  libgfortran startup helpers around `main`.

See [docs/design.md](docs/design.md) for implementation details.
