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

Put ordinary unit tests in `tests/unit/meson.build` and environment-dependent
tests in `tests/integration/meson.build`. A test that must use target-specific
build objects may be registered in the top-level `meson.build`, guarded by
`get_option('tests').enabled()`.

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
