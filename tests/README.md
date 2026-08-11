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

Configure the build with `--enable-tests`, then run:

```sh
python3 -B meson/meson.py test \
  -C build64-tests \
  --suite lat-pr-fast \
  --print-errorlogs
```

To run the integration suite, replace `lat-pr-fast` with
`latx-integration`.

Before submitting a new test target, verify both of these:

1. A normal product build without `--enable-tests` does not build the test.
2. A build configured with `--enable-tests` builds and runs the selected suite.
