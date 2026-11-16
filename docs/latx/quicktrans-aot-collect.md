# Experimental Quicktrans exit-time AOT generation

This opt-in mode records stable executable file mappings while Quicktrans runs,
then generates legacy TU/AOT code in a forked worker at process/thread exit hooks shared with ordinary AOT generation.
It does not load AOT during collection or replace running Quicktrans blocks.

Build with `./latxbuild/build64.sh -c -f`. Collection is disabled by default.
For an isolated test cache, provide a separate HOME directory to the process:

```sh
env HOME=/path/to/test-home LATX_FAST_TRANSLATOR=1 \
  LATX_FAST_TRANSLATOR_STRICT=1 LATX_KZT=0 LATX_AOT=0 LATX_LOAD_AOT=0 \
  LATX_QUICKTRANS_AOT_COLLECT=1 /path/to/latx-x86_64 /path/to/program
```

To verify the generated cache using the same binary's legacy path:

```sh
env HOME=/path/to/test-home LATX_FAST_TRANSLATOR=0 LATX_KZT=0 \
  LATX_AOT=1 LATX_LOAD_AOT=1 LATX_AOT_GENERATE=0 \
  /path/to/latx-x86_64 /path/to/program
```

`LATX_LOAD_AOT` explicitly controls existing-cache loading; its previous default
is unchanged. `LATX_AOT_GENERATE` controls ordinary AOT generation. The separate
experimental collection switch explicitly requests generation even if that
ordinary control is off.

Collection requires strict Quicktrans, hard floating point, runtime AOT loading
off, and Wine/KZT disabled. It records coverage, not execution frequency. No
per-execution counters are inserted. Existing cached startup flags route guest
exit_group through normal cleanup only when collection is requested.

Without a controller, the worker is waited for at exit, so exit can take longer.
Collection uses the ordinary AOT exit hooks: normal final exit, the existing
main-thread exit hook, and default fatal-signal termination (including SIGTERM,
SIGABRT and SIGSEGV). The existing kill/tgkill hooks also snapshot before a guest
sends SIGKILL to itself. External SIGKILL cannot be intercepted. Ignored signals
and application-installed handlers keep their original disposition; abort paths
that bypass the ordinary AOT hook remain unchanged. Runtime TB flushes do not
trigger experimental collection. Normal last-thread exit collects once, after
releasing clone_lock. A snapshot or generation failure does not turn
signal termination into a successful exit. Collection skips
initially writable mappings and removes coverage when permissions change or
mappings are unmapped/remapped. Anonymous/JIT code, SMC, unloaded modules and
previously flushed TBs are not supported coverage sources. A failed exclusive
snapshot or fork can skip generation. Cache compatibility remains subject to the
existing AOT version and source-file checks.

Focused validation covers static and PIE programs, normal and abnormal exits,
disabled mode, mapping invalidation, cache rejection, and selected cross-backend
integer, floating-point, flag and signal-restoration cases. Application evidence
also includes execution inside generated AOT code, rather than load markers
alone. These checks do not establish correctness for every application or all
floating-point and concurrent states. Keep experimental caches isolated.

## Use the generated cache on a later Quicktrans run

Use the same translator and cache HOME, and request loading without collection:

```sh
env HOME=/path/to/test-home LATX_FAST_TRANSLATOR=1 \
  LATX_FAST_TRANSLATOR_STRICT=1 LATX_KZT=0 LATX_AOT=1 LATX_LOAD_AOT=1 \
  LATX_AOT_GENERATE=0 LATX_QUICKTRANS_AOT_COLLECT=0 \
  /path/to/latx-x86_64 /path/to/program
```

Valid cached code uses the legacy TU/AOT backend; uncovered code uses Quicktrans.
Cache creation and a successful second launch alone do not prove AOT execution.
Collection records surviving coverage, not hot-code frequency. Low-frequency
sampling is not enabled: an end-of-run TB table cannot reliably identify older
samples after blocks have been removed or their host addresses reused.

Measure first-run startup, subsequent startup, and complete generation separately.
Delaying generation does not eliminate its CPU, memory or exit-time costs, and
collection still adds mapping and snapshot work. The experimental mode makes no
zero-overhead or general application-speedup guarantee.

## Defer generation until the application tree exits

The optional native `latx-aot-collect` controller delays generation until its
command and all non-generator descendants have exited. Build the controller
explicitly with `ninja -C build64 latx-aot-collect`. It is not installed or enabled
by default. For example:

```sh
env HOME=/path/to/test-home /path/to/latx-aot-collect \
  --report /path/to/result.json -- /path/to/latx-x86_64 /path/to/program
```

The controller sets the strict Quicktrans collection switches for its command.
Guest exec descendants must use the same translator through the configured
runtime/binfmt entry; the controller does not configure that entry. Apply any
application CPU affinity after `--`, so the controller can run separately.

Each eligible exiting process attempts a quiescent snapshot, closes inherited
application descriptors in the worker, and registers it with the controller.
The original process can then exit without waiting for translation. Workers
start only after the application tree exits; the existing concurrent generation,
nonblocking cache locks, merge and publication behavior are retained. The
controller waits for every registered worker and reports application exit,
release and completion times separately. These times include controller startup.
The controller returns the root command's exit status; inspect `failures` and
individual worker status in the report to determine generation success.

A connection closed before READY is recorded separately in
`registration_aborts` with the connecting `peer_pid`, time and the reason
`eof_before_ready`. The application connects before forking its snapshot, so
this PID is not a registered generator PID. Such a disconnect can occur if the
application is terminated during shutdown; EOF alone does not identify a signal
or prove why it disconnected. It does not increment `failures`, but emits an
incomplete-collection warning and sets `collection_incomplete` to true. Failed
registered workers, malformed messages and rejected registrations still count
as failures and also set this flag. Consumers must inspect this flag as well as
`failures`; a zero application exit status or zero failures alone is not a
complete-collection result. A false flag means no such loss was observed by the
controller, not that all application code or all abnormal exits were collected.
The root command's exit-status semantics are unchanged.

The controller uses Linux child-subreaper support, `/proc` and a private Unix
socket. This changes orphan reparenting. Applications that depend on a particular
parent, or leave long-lived descendants, need separate validation; generation
waits for those descendants too. Waiting snapshots retain memory. The default
limit is 64 workers (`--max-workers`, range 1–1024); rejected registrations are
reported and do not represent complete coverage. Loss of the controller cancels
workers that have not been released. Already released workers may finish.

Before translation, workers recheck mapped files' device, inode, size and mtime,
including nanoseconds, and reject a changed snapshot. This is not a content hash
or a guarantee against concurrent writes during translation. Flushed TBs and removed executable mappings remain outside collection.
Exit snapshots remain best-effort: an
in-progress collection or failed snapshot may still prevent a contribution.
Runtime timing can change which processes exit normally; neither worker counts
nor cache-file counts establish equivalent coverage across application runs.
No execution-frequency sampling is enabled by this controller.


### Collection workers preserve each process contribution

After a Quicktrans collection worker is released for generation, it waits for
the per-module writer lock instead of silently skipping a busy module. The
legacy TB-count growth heuristic is not a coverage-inclusion check and is not
used for collection snapshots. Legacy AOT generation retains its previous
nonblocking lock and count heuristic. Collection failures in module admission
or nonempty generation/publication terminate the worker unsuccessfully so the
controller can report the failure. This does not recover processes that exit
before registration, and successful worker exit alone is not proof of complete
application coverage. Lock waiting and the additional generation occur after
application exit; this change makes no startup performance claim.


## Collection report scope

`report_scope` is `observed_collection_protocol`. The controller observes
connections, registered workers, and its child wait statuses; it does not
inventory every translated process or every executed block.

`coverage_status` is `incomplete` when a registration/worker failure is
observed, or the root application dies from a signal without a registered
snapshot. Otherwise it is `not_proven`, never an automatic claim of complete
coverage. A root signal after successful snapshot registration does not itself
mean the snapshot was lost. Unobserved descendant exits remain outside this
report's scope.

All existing fields retain their meanings and values: in particular,
`collection_incomplete=false` means no incompleteness was observed by the old
registration/worker checks, not that every process contributed coverage.
`app_status` and worker statuses retain their actual exit/signal results.
`failures` counts failure events, not unique failed workers.

`registration_aborts[].peer_pid` is the process that connected, which may
differ from a subsequently forked worker. `eof_before_ready` does not establish
a signal or another specific cause. Reports must not infer one without an
associated wait status. These reporting fields do not change signal handling,
snapshot timing, collected blocks, or generation.
