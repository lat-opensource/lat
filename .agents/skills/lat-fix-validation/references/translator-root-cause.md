# Translator root-cause workflow

Read this reference when a crash, hang, wrong result, or compatibility failure
may originate between x86 guest decode and LoongArch host execution. For the
maintained stage and source map, also read
[`docs/devel/latx/debugging-model.rst`](../../../../docs/devel/latx/debugging-model.rst).

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

## Select the observation boundary

Use that maintained model for the current pipeline, debug-option build
conditions, dump boundaries, GDB anchors, generated-code inspection, and
watchpoint guidance.

Choose the smallest observation that separates the current hypotheses:

- compare raw/optimized IR or generated code at the suspected translation stage;
- capture the runtime TB and endpoint when generated code may differ from what
  actually executes;
- find the writer when initially correct code or state later changes; or
- classify the signal and execution boundary before blaming translation.

A dump, log, dispatcher breakpoint, or changed memory value proves only its own
observation point. Confirm that the selected binary exposes the requested debug
control, account for controls that alter execution, and prove the required
runtime endpoint separately.

## Return to the core workflow

Once the first divergence is confirmed, return to the main Skill for task-mode
authorization, the smallest owning-layer change, RED/GREEN, original-reproduction
closure, evidence level, and terminal state. Read the invariant checklist before
changing translator state or layout, and the regression reference before choosing
coverage or a synthetic seam.

Do not import historical runtime options, fixed addresses, offsets, register
assumptions, source line numbers, or host-specific commands. An IR difference,
changed path, or memory difference routes the next experiment; it does not name
the root cause without a confirmed writer, transition, or violated invariant.
