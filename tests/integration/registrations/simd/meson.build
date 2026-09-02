if 'x86_64-linux-user' in target_dirs
  latx_integration_tests += [{
    'name': 'test-vpmovsx-overwrite',
    'runner': find_program('../../test-vpmovsx-overwrite.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../vpmovsx-overwrite.S'),
    ],
    'timeout': 120,
  }]
endif
