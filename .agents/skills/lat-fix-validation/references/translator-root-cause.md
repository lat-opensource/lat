# Translator root-cause workflow

Read this reference when a crash, hang, wrong result, or compatibility failure
may originate between x86 guest decode and LoongArch host execution. For the
maintained stage and source map, also read
[`docs/devel/latx/debugging-model.md`](../../../../docs/devel/latx/debugging-model.md).

## Establish a comparable pair

Define one working observation and one failing observation. A useful pair may
be two revisions, two inputs, debug and release builds, a native x86 reference
and LATX, or one currently supported runtime option changed in isolation.

For both sides, record the source revision, worktree state, build configuration,
binary identity, guest runtime/root, application or fixture identity, command,
environment, cache, target host, output, signal, and exit state that can affect
the result.

Change one variable at a time. Rebuild or isolate caches when code generation
can be reused across builds. If no known-good LAT revision exists, construct a
reference from architectural semantics, native x86 behavior, or a minimized
self-checking fixture.

Do not use a mode difference as the repair. It only identifies a branch in the
investigation unless the task separately defines and accepts a fail-closed
product fallback.

## Confirm that the target path ran

Before inspecting translation, rule out a failure that occurred earlier:

- verify the intended binary and guest program were executed;
- verify loader dependencies, guest root, prefix, input, and environment;
- verify the test actually performs its setup and assertions in the selected
  build configuration;
- identify the target process when wrappers, launchers, children, or timeouts
  are involved;
- separate shell status, process exit, signal, errno, and tool status.

An absent final result, a timeout, or an error moving later is not proof that the
translator path ran correctly.

## Locate the first divergence

Compare the pair in pipeline order:

1. guest bytes and incoming architectural state;
2. decoded IR1;
3. optimized IR1;
4. generated and optimized IR2;
5. assembled LoongArch host code;
6. the instructions and state that actually execute;
7. the guest-visible memory, syscall, signal, callback, and output effect.

Stop at the earliest confirmed difference and design the next experiment around
that boundary. Do not jump from a final exception directly to the subsystem
named in its message: loader, TLS, callback, signal, and guest-application stages
can collapse into one final symptom.

For each candidate cause, write an observation that would distinguish it from
the alternatives. Prefer one experiment that eliminates several hypotheses over
several edits that each test a guess.

## Use the current debug interfaces

### `LATX_DUMP` and `LATX_SHOW_TB`

The current debug build supports a five-bit `LATX_DUMP` bitmap ordered as
function, IR1, IR2, host, and profile. A focused example is:

```sh
LATX_DUMP=11110 <translator> <guest> 2>dump.log
```

When the guest PC is known, prefer the current targeted option:

```sh
LATX_SHOW_TB=0x<guest-pc> <translator> <guest> 2>tb.log
```

First confirm that a LATX debug build exposes the expected dump bodies. Keep the
original instruction order when comparing dumps; normalize only unstable
addresses or presentation noise. `LATX_SHOW_TB` applies only when the matching
TB is translated, and its parser prints a diagnostic to stdout; it neither
forces an AOT- or cache-loaded TB to be translated nor preserves pristine
application output.

Know the observation boundary:

- dumped IR1 is already IR1-optimized;
- dumped IR2 precedes `tr_ir2_optimize()`;
- dumped host words follow IR2 optimization and label resolution.

Use GDB or narrow temporary instrumentation around the relevant function when a
pre/post view is required. The current tracked source has no `LATX_DUMP_PC`
option; do not copy that command from the imported `lat-debug` material.

### `LAT_LOG`

Choose current log categories from `util/log.c`. Examples include `exec`,
`lat_syscall`, `lat_ir2_sched`, `lat_eflags`, `lat_aot`, and `lat_log_smc`.

`nochain` is an active diagnostic control, not a passive log category: it
disables TB chaining. It can provide a complete dispatcher trace, but that run
does not prove the normal linked path; restore chaining for final validation.

Use logs to answer a named question, such as whether an AOT artifact was loaded,
an IR2 operation was changed, or a syscall path was entered. A log line proves
only its observation point; it does not prove the final translated, wrapper, or
native endpoint.

### GDB and generated code

Use symbol and data breakpoints, not historical line numbers or fixed offsets.
A typical investigation shape is:

1. break on `target_latx_host()` or `tr_translate_tb()` for the target
   `tb->pc`;
2. observe before and after `ir1_optimization()` when decode versus IR1
   optimization is in question;
3. observe before and after `tr_ir2_optimize()` when the generated IR2 is
   correct but the optimized sequence is suspect;
4. capture `tb->tc.ptr` and inspect it after `tr_ir2_assemble()`;
5. break at the captured generated-code address or the real faulting host PC to
   inspect the state that actually executes;
6. break at the required helper, syscall, wrapper, callback, or signal endpoint
   to prove the full route.

Resolve structure layouts and register mappings from the current headers and
debug symbols. Direct translated links can bypass dispatcher breakpoints, so a
dispatcher trace alone is not complete execution evidence.

### Hardware watchpoints

If generated code is initially correct but later changes, set a watchpoint on
the smallest affected word at the runtime address captured from `tb->tc.ptr`.
The writer and backtrace are stronger evidence than a list of possible mutation
sites.

If the target cannot provide a hardware watchpoint, compare bytes at successive
lifecycle boundaries. Do not retain instrumentation that changes code layout as
the only proof of a layout-sensitive failure.

## Turn the divergence into a fix

Name the violated invariant in general guest/host terms. Repair the shared
translator or runtime boundary rather than adding an application name, guest PC,
benchmark address, or machine path.

Treat disabled features and forced TB boundaries as probes. Re-enable the
original supported configuration for final validation unless an explicitly
accepted fail-closed fallback is the product decision.

Add the smallest deterministic regression that exercises the real seam. Show
that it fails without the fix and passes with it, then rerun the original
reproduction. A synthetic GREEN proves only the synthetic seam; retain the real
application or target-host gate when it has not passed.

Report separately:

- the first confirmed divergence and evidence;
- the root cause and violated invariant;
- the change and focused RED/GREEN;
- the original reproduction result;
- configurations and paths actually exercised;
- alternatives ruled out and hypotheses still open;
- the exact remaining validation gate.

## Current-source guardrails

- Do not use absent `LATX_DUMP_PC` commands.
- Do not prescribe historical `LATX_TU` or “TU-JIT” commands; discover supported
  controls from the current source and binary.
- Do not copy fixed code-buffer addresses, structure offsets, GDB register names,
  source line numbers, test paths, or host-specific commands from old cases.
- Do not infer root cause solely from IR2 differences, a changed execution path,
  or a memory difference. Each is a routing observation that still requires a
  confirmed writer, state transition, or violated invariant.
- Do not call source inspection, compilation, CI, or a related regression
  target-runtime proof.
