if 'x86_64-linux-user' in target_dirs
  latx_integration_tests += [{
    'name': 'test-shld-shrd-32-x86_64',
    'runner': find_program('../../test-shld-shrd-32-x86_64.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../shld-shrd-32-x86_64.c'),
    ],
    'timeout': 60,
  }]
endif
