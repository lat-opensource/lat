# Registration interface: name, runner, args, and optional non-default timeout.
foreach test_def : latx_integration_tests
  test(
    test_def['name'],
    test_def['runner'],
    args: test_def['args'],
    env: test_def.get('env', {}),
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: test_def.get('timeout', 30),
  )
endforeach
