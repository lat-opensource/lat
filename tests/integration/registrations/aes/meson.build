if 'x86_64-linux-user' in target_dirs
  latx_integration_tests += [{
    'name': 'test-aes-vaes-rounds',
    'runner': find_program('../../test-aes-vaes-rounds.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../aes-rounds.S'),
      files('../../aesenc-all-bytes.S'),
      files('../../vaes-xmm-rounds.S'),
      files('../../vaes-ymm-rounds.S'),
    ],
    'timeout': 120,
  }]
endif
