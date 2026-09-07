if 'x86_64-linux-user' in target_dirs
  # O3 enables CONFIG_LATX_AVX_OPT through optimize-config.h.
  fpu_extended_env = {}
  if 'CONFIG_LATX_AVX_OPT' in config_host or 'CONFIG_LATX_O3' in config_host
    fpu_extended_env = {
      'LATX_TEST_XSAVE': '1',
      'LATX_TEST_AVX': '1',
    }
  endif

  latx_integration_tests += [{
    'name': 'test-mxcsr-cvt',
    'runner': find_program('../../test-mxcsr-cvt.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../mxcsr-cvt-guest.S'),
    ],
    'timeout': 120,
  }]
  latx_integration_tests += [{
    'name': 'test-fpu-flags',
    'runner': find_program('../../test-fpu-flags.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../fpu-flags-guest.S'),
    ],
    'env': fpu_extended_env,
    'timeout': 120,
  }]
  latx_integration_tests += [{
    'name': 'test-fpu-controls',
    'runner': find_program('../../test-fpu-controls.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../fpu-controls-guest.S'),
    ],
    'env': fpu_extended_env,
    'timeout': 120,
  }]
  latx_integration_tests += [{
    'name': 'test-x87-pop',
    'runner': find_program('../../test-x87-pop.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../x87-pop-guest.S'),
    ],
    'timeout': 120,
  }]
endif
