if 'x86_64-linux-user' in target_dirs
  # O3 enables CONFIG_LATX_AVX_OPT through optimize-config.h.
  fpu_extended_env = {}
  if 'CONFIG_LATX_AVX_OPT' in config_host or 'CONFIG_LATX_O3' in config_host
    fpu_extended_env = {
      'LATX_TEST_XSAVE': '1',
      'LATX_TEST_AVX': '1',
    }
  endif

  foreach softfpu : ['1', '2']
    fpu_mode_env = fpu_extended_env + {
      'LATX_SOFTFPU': softfpu,
      'LATX_SOFTFPU_FAST': '0',
    }
    latx_integration_tests += [{
      'name': 'test-mxcsr-cvt-softfpu-' + softfpu,
      'runner': find_program('../../test-mxcsr-cvt.sh'),
      'args': [
        emulators['latx-x86_64'],
        files('../../mxcsr-cvt-guest.S'),
      ],
      'env': fpu_mode_env,
      'timeout': 120,
    }]
    latx_integration_tests += [{
      'name': 'test-fpu-flags-softfpu-' + softfpu,
      'runner': find_program('../../test-fpu-flags.sh'),
      'args': [
        emulators['latx-x86_64'],
        files('../../fpu-flags-guest.S'),
      ],
      'env': fpu_mode_env,
      'timeout': 120,
    }]
    latx_integration_tests += [{
      'name': 'test-fpu-controls-softfpu-' + softfpu,
      'runner': find_program('../../test-fpu-controls.sh'),
      'args': [
        emulators['latx-x86_64'],
        files('../../fpu-controls-guest.S'),
      ],
      'env': fpu_mode_env,
      'timeout': 120,
    }]
    latx_integration_tests += [{
      'name': 'test-x87-pop-softfpu-' + softfpu,
      'runner': find_program('../../test-x87-pop.sh'),
      'args': [
        emulators['latx-x86_64'],
        files('../../x87-pop-guest.S'),
      ],
      'env': fpu_mode_env,
      'timeout': 120,
    }]
    latx_integration_tests += [{
      'name': 'test-fpu-signal-softfpu-' + softfpu,
      'runner': find_program('../../test-fpu-signal.sh'),
      'args': [
        emulators['latx-x86_64'],
        files('../../fpu-signal-guest.S'),
      ],
      'env': fpu_mode_env,
      'timeout': 120,
    }]
  endforeach
endif

foreach arch : ['x86_64', 'i386']
  if arch + '-linux-user' in target_dirs
    latx_integration_tests += [{
      'name': 'test-fpu-mode-boundary-' + arch,
      'runner': find_program('../../test-fpu-mode-boundary.sh'),
      'args': [
        emulators['latx-' + arch],
        files('../../fpu-mode-boundary-guest.S'),
        arch,
      ],
    }]
  endif
endforeach
