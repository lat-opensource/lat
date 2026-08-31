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
  latx_integration_tests += [{
    'name': 'test-gb603-patterns',
    'runner': find_program('../../test-gb603-patterns.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../avx128-shuffle.S'),
      files('../../avx-sum3-pattern.S'),
      files('../../repeat-add.S'),
      files('../../vcomis-jcc.S'),
    ],
    'timeout': 120,
  }]
  latx_integration_tests += [{
    'name': 'test-gb702-fma-forward',
    'runner': find_program('../../test-gb603-patterns.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../scalar-fma-forward.S'),
      files('../../vpshufd-distinct-regs.S'),
    ],
    'timeout': 120,
  }]
endif
