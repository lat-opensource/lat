# Regression surface and risk-driven validation matrix

Read this reference when choosing regression coverage or deciding how much of the
LAT configuration matrix a fix must exercise.

## Derive coverage from the invariant

Do not stop at the triggering application or first failing function. Identify:

- the semantic invariant restored by the fix;
- every entry point and caller that can exercise it;
- compile-time and runtime branches that implement the same behavior;
- fallback, error, and negative paths;
- boundary values, resource exhaustion, and malformed inputs;
- previous fixes whose protection overlaps the changed path.

A current application passing is one data point. The regression surface must
cover the shared behavior that made the application fail.

## Choose a durable test seam

Prefer a deterministic, self-contained seam that preserves the relevant guest
instruction, ABI, loader, callback, threading, or lifecycle topology.

When a durable seam exists:

1. prove RED without the fix;
2. prove GREEN with the fix;
3. cover fallback or negative behavior;
4. retain or recreate regression coverage for earlier fixes on the same path.

Do not redesign production code or add substantial Meson/test-fixture churn only
to satisfy a formal “one test per fix” rule. For a narrow compile-compatibility
fix with no stable seam, focused source and compile evidence may be the correct
boundary. State that target and runtime behavior were not validated.

Test-only programs and fixtures must remain outside normal product builds and be
registered according to `tests/README.md`.

## Build only the relevant matrix

Derive the matrix from current source and the identified risk. Possible axes
include, only when relevant:

- x86_64 versus i386 guest width;
- debug, release, `NDEBUG`, sanitizer, and optimization builds;
- current runtime switches and compile-time feature gates;
- JIT versus AOT, cold generation versus hot loading, and isolated cache states;
- KZT disabled versus the specific wrapper/binding path under test;
- guest runtime, ABI, or library-family differences;
- nested callback, concurrency, fork, loader, or object-lifecycle transitions;
- alignment, page, descriptor, capacity, and resource-exhaustion boundaries.

Do not copy a historical TU/AOT/KZT/ABI matrix without proving those paths still
exist and are affected in current source.

## Require path sentinels

Every matrix result needs an observable sentinel that proves the intended path
ran. Suitable evidence can include:

- a generated-code or disassembly difference tied to the candidate binary;
- a wrapper, binding, bridge, native, guest, or callback endpoint observation;
- an AOT load observation that excludes stale cache and JIT fallback;
- a signal context, state transition, or counter specific to the target path.

A pass or failure that never reached the target path is harness or environment
evidence, not fix evidence.

## Keep synthetic and field gates separate

When the real application is unavailable, build a self-contained RED/GREEN seam
and use it to validate the covered mechanism. Do not rename that result as field
or product completion.

Before closing the original report:

- run the exact original command, input, runtime, and path;
- verify expected output, visible state, child processes, and final termination;
- separate any later independent error from the original fixed failure;
- keep the field gate open when the original application was not retested.

For an intermittent failure, choose and record a candidate repetition count or
soak duration, exposure conditions, acceptance threshold, and early-stop rule
before candidate runs. Justify them from the observed RED rate, consequence of
failure, and required support claim; there is no universal safe repetition
count. A run below that criterion is non-recurrence evidence, not closure.

## Validation order

Use the smallest evidence that can fail quickly, then widen deliberately:

1. focused RED/GREEN seam;
2. affected-object or focused build;
3. relevant fallback and matrix cases;
4. complete `lat-pr-fast` before a pull request;
5. affected `latx-integration` where the environment exists;
6. exact LoongArch and real-application path required by the claim.

Record every unexecuted layer and its reason rather than treating it as an
implicit pass.
