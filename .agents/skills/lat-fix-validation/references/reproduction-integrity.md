# Reproduction integrity

Use this reference when a fix depends on a non-trivial runtime, an intermittent
failure, a target host, or an original application. A clean RED/GREEN result is
useful only when the experiment identity and the path actually exercised are
known.

## Pin the reproduction identity

Record enough information for another developer to repeat the result:

- source revision, worktree state, and any uncommitted diff;
- build command and configuration, binary path and identity, and relevant
  debug or release flags;
- guest architecture, runtime or loader identity, and required libraries;
- runtime switches, prefix or state directory, translation and AOT caches;
- workload, input, exact command, timeout, expected behavior, and observed
  failure; and
- target environment details that can affect the result.

For a before-and-after comparison, pin both source revisions and binary
identities. Hold the build configuration, runtime, cache policy, workload, and
environment fixed except for the intended code change. If another controlled
variable must differ, record it and limit the conclusion accordingly.

Identify the project source for the expected support claim, such as tracked
product documentation, a compatibility requirement, or an approved task
acceptance criterion. If no source establishes support, describe the run as
exploratory evidence rather than a supported-configuration regression.

## Control the environment and harness

Check the harness before attributing a failure to LAT:

- **Build mode:** Exercise relevant release settings. Under `NDEBUG`, an
  `assert()` expression is removed; never place setup, synchronization, or the
  behavior under test inside it.
- **Loader path:** Record the actual executable and shared objects selected by
  the loader. An unintended `RUNPATH`, test library, or runtime root can bypass
  the path under investigation.
- **Temporary storage:** Confirm that the chosen `TMPDIR` has usable space and
  quota. Compiler, tracing, or runtime temporary-file failures are environment
  evidence, not application regressions.
- **Run isolation:** Use distinct state and cache locations where reuse could
  affect the outcome. Detect leftover translator, application, server, or
  harness processes before and after a run; clean up only processes belonging
  to the scoped reproduction.

Do not repair a harness artifact by changing production behavior. First make
the harness exercise the intended configuration, then reproduce the product
failure again.

## Prove the actual path

Do not infer path execution from configuration or initialization alone. Use an
existing trace, counter, assertion, or temporary sentinel that is unique to the
candidate path. Capture the sentinel with the command and result, and show that
the loaded binary, runtime, libraries, switches, and cache belong to the pinned
reproduction.

For AOT-sensitive paths, artifact creation is not a path sentinel: prove that
the intended current artifact was accepted and loaded. For fallback-sensitive
paths, prove the final host, guest, or mixed endpoint rather than only the
wrapper or dispatcher entry.

Remove temporary instrumentation from the final change unless it is accepted
as maintained diagnostics or regression coverage.

## Keep result namespaces separate

Record each result in its own namespace:

- guest program exit status;
- expected or unexpected guest signal or translated exception;
- host translator or runtime signal;
- child wait status and the identity of the child it describes;
- syscall return value and `errno`;
- timeout, test harness, debugger, or tracing-tool result; and
- shell status, including any shell convention for signal termination.

Equal-looking numbers do not make these results equivalent. Decode wait status
before reporting an exit or signal, identify whether the target, a descendant,
the collector, or the shell failed, and determine whether a visible signal is
expected guest behavior before classifying it as a host crash.

## Close on the original behavior

A focused regression proves only its focused path. Re-run the original
application or full reproduction to its natural terminal state and verify the
expected user-visible output, artifacts, exit behavior, and process cleanup.
The disappearance of the first error, a later failure, a timeout increase, or a
successful launch without terminal output is not original-application success.

If the original run is unavailable, unstable, or unsupported, report that
limit explicitly and carry it as the next validation gate.

## Reproduction record

Use this compact record in evidence or a handoff:

```text
Support source:
Revision and worktree:
Build and binary identity:
Guest and runtime:
Switches and caches:
Workload, command, and timeout:
Environment and harness controls:
Actual-path sentinel:
RED result by namespace:
GREEN result by namespace:
Original terminal behavior:
Uncontrolled differences or remaining gate:
```
