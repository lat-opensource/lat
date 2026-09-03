if 'x86_64-linux-user' in target_dirs and \
   'CONFIG_LATX_AVX_OPT' in config_host
  latx_integration_tests += [{
    'name': 'test-avx-cpu-features',
    'runner': find_program('../../test-avx-cpu-features.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../avx-cpu-features.S'),
    ],
    'timeout': 120,
  }]
endif
