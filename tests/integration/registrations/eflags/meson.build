if 'x86_64-linux-user' in target_dirs
  latx_integration_tests += [{
    'name': 'test-eflags-pattern-consumer',
    'runner': find_program('../../test-eflags-pattern-consumer.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../eflags-pattern-consumer.S'),
    ],
  }, {
    'name': 'test-eflags-cross-boundary-off',
    'runner': find_program('../../test-eflags-cross-boundary.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../eflags-cross-boundary.S'),
      '0',
      '0',
      '1',
    ],
  }, {
    'name': 'test-eflags-cross-boundary-on',
    'runner': find_program('../../test-eflags-cross-boundary.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../eflags-cross-boundary.S'),
      '1',
      '0',
      '1',
    ],
  }]
endif
