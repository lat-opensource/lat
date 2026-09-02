# LATX debugging model

Use this model to locate the first stage at which a failing run differs from a
working run. A difference routes the next experiment; it is not, by itself,
proof of root cause.

## Translation and execution stages

```text
guest bytes and architectural state
  -> decode into IR1
  -> optimize IR1
  -> translate IR1 into IR2
  -> optimize and resolve IR2
  -> assemble LoongArch host code
  -> dispatch and execute that host code
  -> produce guest-visible state, syscalls, signals, and output
```

For the ordinary TB translation path, the current source maps these stages as
follows:

| Stage | Current source anchors | State to compare |
|---|---|---|
| Guest bytes -> decoded IR1 | `target/i386/latx/translator/translate.c`: `get_ir1_list()`, `ir1_disasm()`; called through `tr_disasm()` | Guest PC, instruction bytes, decoded opcode, operands, length, TB boundary |
| Decoded IR1 -> optimized IR1 | `target/i386/latx/latx-config.c`: `target_latx_host()`; `target/i386/latx/optimization/ir1-optimization.c`: `ir1_optimization()` | Instruction order, pattern annotations, flag requirements, successors |
| Optimized IR1 -> IR2 | `target/i386/latx/translator/translate.c`: `tr_ir2_generate()`, `ir1_translate()` | IR2 sequence, operands, labels, guest-instruction association |
| IR2 -> resolved IR2 | `target/i386/latx/translator/translate.c`: `tr_translate_tb()`, `label_dispose()`; `target/i386/latx/optimization/ir2-optimization.c`: `tr_ir2_optimize()` | Removed or inserted operations, resolved labels/data, branch targets |
| Resolved IR2 -> host code | `target/i386/latx/translator/translate.c`: `tr_ir2_assemble()` | Emitted 32-bit LoongArch words, `tb->tc.ptr`, and the returned code size; compare `tb->tc.size` after the caller finalizes the TB |
| Host code -> actual execution | `accel/tcg/cpu-exec.c`: `tb_find()`, `cpu_loop_exec_tb()`, `tcg_qemu_tb_exec()` and translated direct paths | Selected TB, actual host PC, registers, memory, signal context, exit route |

On the current source, an ordinary cache miss in `tb_find()` calls
`tb_gen_code()`, which reaches `target_latx_host()` through
`accel/tcg/translate-all.c`. AOT generation enters `translate_lib()`; its
`translate_seg()` selects `translate_by_tu()` only when `CONFIG_LATX_TU` is
built, and otherwise selects `translate_by_tb()`. `LATX_TU` is not a runtime
option, and the currently defined `tu_gen_code()` has no tracked caller.
Recheck this call graph when the source changes.

## What the built-in dumps show

The command-line and environment options for `LATX_DUMP` and `LATX_SHOW_TB` are
registered when either `CONFIG_LATX_DEBUG` or `CONFIG_DEBUG_TCG` is enabled,
but the detailed IR1, IR2, and host-code dump bodies require
`CONFIG_LATX_DEBUG`. Use a LATX debug build and confirm the expected output
before relying on the dump.

`LATX_DUMP` is a five-character bitmap in this order:

```text
function, IR1, IR2, host, profile
```

For example, `LATX_DUMP=11110` enables function, IR1, IR2, and host-code output
without profiler output. Choose the smallest useful bitmap because an unbounded
dump can perturb timing and produce very large logs.

`LATX_SHOW_TB=<guest-pc>` enables function, IR1, IR2, and host output while a
matching TB is translated. The option is parsed in
`target/i386/latx/latx-options.c` and applied in `tr_translate_tb()`; verify that
the selected debug binary exposes it before relying on it. It does not force an
AOT- or cache-loaded TB to be translated, and its parser prints a diagnostic to
stdout, so separate diagnostic output from application-output evidence.

The placement of the current dump calls matters:

- The IR1 dump is emitted from `tr_ir2_generate()`, after
  `ir1_optimization()`. It represents optimized IR1, not the raw decoder
  result.
- The IR2 dump is emitted before `tr_ir2_optimize()`.
- The host dump is emitted by `tr_ir2_assemble()` after IR2 optimization and
  label resolution.

To compare raw and optimized IR1, observe immediately before and after
`ir1_optimization()` with GDB or narrowly scoped temporary instrumentation. To
compare IR2 before and after optimization, observe around `tr_ir2_optimize()`;
`LAT_LOG=lat_ir2_sched` can provide pass-specific evidence when that pass logs
the relevant operation.

## Logging and runtime observation

`LAT_LOG` uses the current log categories registered in `util/log.c`. Useful
categories include `exec`, `lat_syscall`, `lat_ir2_sched`, `lat_eflags`,
`lat_aot`, and `lat_log_smc`. Select only categories that answer a specific
hypothesis.

`nochain` is an active diagnostic control, not a passive log category: it
disables TB chaining. Use it to obtain a complete dispatcher trace, but do not
treat that run as proof of the normal linked path; restore chaining for final
validation.

Logging proves that the logging point ran. It does not prove that every direct
translated transition, callback, fallback, or final native endpoint ran. For a
path claim, combine logs with a breakpoint, generated-code inspection, or an
observable endpoint.

Use GDB symbols and runtime values rather than copied addresses or structure
offsets:

- filter translation breakpoints with the current `tb->pc`;
- capture `tb->tc.ptr` and `tb->tc.size` at runtime;
- inspect the host instruction at the real faulting PC;
- compare guest architectural state with the source-defined register mapping;
- use `ptype /o`, `offsetof`, and current headers when a field offset is needed;
- break at the actual wrapper, helper, syscall, signal, or callback endpoint
  whose execution must be proved.

Do not assume that a dispatcher breakpoint sees every TB. Directly linked or
mode-specific transitions can bypass a dispatcher observation point. When that
matters, break at the captured generated-code address or another required
endpoint.

## Detecting post-generation corruption

If the host dump and the bytes immediately after assembly are correct but the
executed bytes are not, capture the runtime address from `tb->tc.ptr` and set a
hardware watchpoint on the smallest changed word. A watchpoint hit identifies
the writer and stack that changed the code.

Hardware watchpoint availability and capacity are target- and debugger-specific.
If unavailable, compare code bytes at explicit lifecycle boundaries and narrow
the interval. Never hard-code a historical code-buffer address or a source-line
offset into a maintained procedure.

## Interpreting the first divergence

Use the earliest confirmed difference to choose the next observation:

- Different guest bytes or runtime inputs: stabilize the loader, guest root,
  application state, cache, and test harness before blaming translation.
- Same bytes but different decoded IR1: inspect decode mode, instruction
  boundaries, and operand construction.
- Same decoded IR1 but different optimized IR1: inspect the responsible IR1
  pass and the invariant that permits the transformation.
- Correct optimized IR1 but wrong IR2: inspect the opcode translator, operand
  conversion, register allocation, and flag semantics.
- Correct IR2 but wrong emitted word or target: inspect optimization, label/data
  resolution, and assembly.
- Correct emitted code but different bytes at execution: inspect linking,
  relocation, cache invalidation, SMC handling, and code-buffer mutation.
- Correct executed instruction but wrong effect: inspect incoming state,
  register/context mapping, memory ownership, signals, callbacks, and guest
  semantics.

These are investigation routes, not automatic classifications. Confirm the
writer, state transition, or violated invariant before naming the root cause.

## Unsupported legacy assumptions

- The current tracked source does not expose `LATX_DUMP_PC`; do not use it.
- Do not prescribe historical `LATX_TU` or “TU-JIT” runtime commands. Confirm
  supported options in the current binary and source before constructing a
  comparison.
- Do not copy fixed GDB offsets, register names, code-buffer addresses, source
  line numbers, test paths, or host names from an old case study.
- A mode change, a disabled optimization, a longer timeout, or a moved error is
  a diagnostic observation, not a root-cause fix.
