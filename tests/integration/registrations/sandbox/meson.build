if 'x86_64-linux-user' in target_dirs
  latx_integration_tests += [{
    'name': 'test-cef-userns-nested',
    'runner': find_program('../../test-cef-userns-nested.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../cef-userns-nested.S'),
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-cef-userns-exec',
    'runner': find_program('../../test-cef-userns-exec.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../cef-userns-exec.S'),
    ],
  }]
endif
