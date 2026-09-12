# LAT Meson tests

LAT tests are registered with Meson and are configured only when
`--enable-tests` is passed to `configure`.

## Choose a test suite

- `lat-pr-fast`: deterministic, self-contained regression tests that need no
  network access, elevated privileges, namespaces, or other special host
  setup. These tests must be fast enough to run on every pull request.
- `latx-integration`: tests that depend on kernel features, namespaces,
  external programs, special host setup, or substantially longer execution
  time. These tests are not part of the fast pull-request gate.

Do not add a test-specific GitHub Actions step or target. Register the test in
the appropriate suite; the workflow selects the suite automatically.

## Register a test

Put ordinary unit tests in `tests/unit/meson.build`. Register LATX integration
tests in the closest matching domain below
`tests/integration/registrations/`; adding a test to an existing domain must
not modify `tests/integration/meson.build`. Add a root `subdir()` entry only
when introducing a genuinely new test domain. Keep architecture and feature
conditions in the domain registration file, and do not make tests depend on
registration or execution order.

Each LATX integration registration file appends dictionaries containing
`name`, `runner`, and `args` to `latx_integration_tests`. Specify `timeout`
only when the test needs a value other than 30 seconds. Runner and source paths
are relative to the registration file:

```meson
latx_integration_tests += [{
  'name': 'test-example',
  'runner': find_program('../../test-example.sh'),
  'args': [
    emulators['latx-x86_64'],
    files('../../example.S'),
  ],
  'timeout': 120,
}]
```

The central registrar supplies `protocol: 'exitcode'`,
`suite: 'latx-integration'`, and the default timeout. Do not add
`executable()`, `custom_target()`, `generator()`, or `run_command()` merely to
register a script-driven integration test, because normal product builds must
not build test-only fixtures.

A test that must use target-specific build objects may instead be registered
in the top-level `meson.build`, guarded by `get_option('tests').enabled()`.

For example:

```meson
test_program = executable(
  'test-example',
  files('test-example.c'),
  dependencies: glib,
)

test(
  'example',
  test_program,
  suite: 'lat-pr-fast',
  timeout: 30,
)
```

Use `suite: 'latx-integration'` instead when the test needs special host
facilities.

If the test includes QEMU headers that refer to generated QAPI headers, add
`genh` to the executable sources so Meson records the generator dependency:

```meson
test_program = executable(
  'test-example',
  files('test-example.c') + genh,
)
```

If those headers also include TCG trace helpers, add `tcg_trace_genh` as well.
Tests that include generated Linux-user syscall headers must also add the
target's `syscall_nr_generated` sources.

## Run the suites

Configure the build with `--enable-tests` and reuse the same Meson executable
for both configuration and test execution.  For example, configure with
`--meson=meson`, then run:

```sh
meson test \
  -C build64-tests \
  --suite lat-pr-fast \
  --print-errorlogs
```

Meson build data is version-specific.  Do not configure with a system Meson
and then run tests with the bundled `meson/meson.py`, or vice versa.

To run the integration suite, replace `lat-pr-fast` with
`latx-integration`.

Before submitting a new test target, verify both of these:

1. A normal product build without `--enable-tests` does not build the test.
2. A build configured with `--enable-tests` builds and runs the selected suite.

## RCU fork queue regression

`test-rcu-fork-queue` runs on POSIX hosts with `--enable-tests`, including
non-KZT and non-LATX configurations. It covers empty forks, callbacks that
queue another callback, and forks with concurrently published callbacks still
queued. A blocking callback pauses the consumer outside the grace-period lock,
so the ordinary atfork lock path can proceed. The blocker itself has already
been dequeued and is outside the queue-preservation contract.

LATX/KZT configurations also register `test-rcu-fork-queue-held-reader`, which
holds a reader across fork and covers lazy and deferred child startup. This
scenario depends on KZT's atfork path not waiting for the parent's grace period.
Both cases check each callback marked published after `call_rcu1()` returns:
after the child drains, every published callback in its snapshot must have run
exactly once. Producers remain active during the forks.

Run the common case explicitly (the binary is not part of the default build):

```sh
meson test -C build64-tests test-rcu-fork-queue --print-errorlogs
```

With LATX/KZT enabled, also run:

```sh
meson test -C build64-tests test-rcu-fork-queue-held-reader --print-errorlogs
```
