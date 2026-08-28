if 'x86_64-linux-user' in target_dirs
  latx_integration_tests += [{
    'name': 'test-avx-ymmh-zero',
    'runner': find_program('../../test-avx-ymmh-zero.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../vex128-ymmh-zero.S'),
    ],
    'timeout': 120,
  }]
endif
