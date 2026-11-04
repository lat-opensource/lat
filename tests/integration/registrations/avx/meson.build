if 'x86_64-linux-user' in target_dirs
  if 'CONFIG_LATX_AVX_OPT' in config_host
    latx_integration_tests += [{
      'name': 'test-f16c-memory-boundary',
      'runner': find_program('../../test-avx-regressions.py'),
      'args': [emulators['latx-x86_64'], files('../../f16c-memory-boundary.asm')],
      'timeout': 120,
    }]
    latx_integration_tests += [{
      'name': 'test-f16c-mxcsr',
      'runner': find_program('../../test-avx-regressions.py'),
      'args': [emulators['latx-x86_64'], files('../../f16c-mxcsr.asm')],
      'timeout': 120,
    }]
    latx_integration_tests += [{
      'name': 'test-f16c-upper-zero',
      'runner': find_program('../../test-avx-regressions.py'),
      'args': [emulators['latx-x86_64'], files('../../f16c-upper-zero.asm')],
      'timeout': 120,
    }]
  endif
  latx_integration_tests += [{
    'name': 'test-instptn-signal-recovery',
    'runner': find_program('../../test-avx-regressions.py'),
    'args': [emulators['latx-x86_64'],
             files('../../instptn-signal-recovery.S')],
    'env': {'LATX_SOFTFPU': '0'},
    'timeout': 120,
  }]
  latx_integration_tests += [{
    'name': 'test-ymmh-conditional-save',
    'runner': find_program('../../test-avx-regressions.py'),
    'args': [emulators['latx-x86_64'], files('../../ymmh-conditional-save.asm')],
    'env': {'LATX_SOFTFPU': '0'},
    'timeout': 120,
  }]
  latx_integration_tests += [{
    'name': 'test-sse41-ymmh-preserve',
    'runner': find_program('../../test-avx-regressions.py'),
    'args': [emulators['latx-x86_64'], files('../../sse41-ymmh-preserve.asm')],
    'timeout': 120,
  }]
  latx_integration_tests += [{
    'name': 'test-xsave64-roundtrip',
    'runner': find_program('../../test-avx-regressions.py'),
    'args': [emulators['latx-x86_64'], files('../../xsave64-roundtrip.S')],
    'timeout': 120,
  }]
  latx_integration_tests += [{
    'name': 'test-avx-compare-jcc',
    'runner': find_program('../../test-avx-regressions.py'),
    'args': [emulators['latx-x86_64'], files('../../vcomis-jcc.S')],
    'timeout': 120,
  }]
  latx_integration_tests += [{
    'name': 'test-vcomis-flag-reduction',
    'runner': find_program('../../test-avx-regressions.py'),
    'args': [emulators['latx-x86_64'], files('../../vcomis-flag-reduction.asm')],
    'env': {'LATX_SOFTFPU': '0'},
    'timeout': 180,
  }]
  latx_integration_tests += [{
    'name': 'test-avx-test-flags',
    'runner': find_program('../../test-avx-regressions.py'),
    'args': [emulators['latx-x86_64'], files('../../avx-test-flags.asm')],
    'env': {'LATX_SOFTFPU': '0'},
    'timeout': 180,
  }]
  latx_integration_tests += [{
    'name': 'test-avx-test-jcc',
    'runner': find_program('../../test-avx-regressions.py'),
    'args': [emulators['latx-x86_64'], files('../../avx-test-jcc.asm')],
    'env': {'LATX_SOFTFPU': '0'},
    'timeout': 180,
  }]
  latx_integration_tests += [{
    'name': 'test-xcomis-patterns',
    'runner': find_program('../../test-avx-regressions.py'),
    'args': [emulators['latx-x86_64'], files('../../xcomis-patterns.asm')],
    'timeout': 180,
  }]
  latx_integration_tests += [{
    'name': 'test-ymmh-fused-jcc',
    'runner': find_program('../../test-avx-regressions.py'),
    'args': [emulators['latx-x86_64'], files('../../ymmh-fused-jcc.asm')],
    'timeout': 180,
  }]
  latx_integration_tests += [{
    'name': 'test-vzeroall-pending',
    'runner': find_program('../../test-avx-regressions.py'),
    'args': [emulators['latx-x86_64'], files('../../vzeroall-pending.asm')],
    'timeout': 120,
  }]
  latx_integration_tests += [{
    'name': 'test-ymmh-tb-fallthrough',
    'runner': find_program('../../test-avx-regressions.py'),
    'args': [emulators['latx-x86_64'], files('../../ymmh-tb-fallthrough.S')],
    'timeout': 120,
  }]
  latx_integration_tests += [{
    'name': 'test-avx-single-instruction',
    'runner': find_program('../../test-avx-regressions.py'),
    'args': [
      emulators['latx-x86_64'],
      files('../../avx-single-instruction-boundaries.asm'),
      files('../../avx-scalar-double-alias.asm'),
      files('../../vaddsubpd-lanes.asm'),
      files('../../vbroadcastsd-memory.asm'),
      files('../../vinsertf128-alias.asm'),
      files('../../vinsertf128-high-pending.asm'),
      files('../../vmovsd-alias.asm'),
      files('../../vperm2x128-immediate.asm'),
      files('../../vpermilpd-ymm-immediate.asm'),
      files('../../vpermilpd-immediate.asm'),
      files('../../vmovd-xmm-zero.asm'),
      files('../../vmovss-memory-zero.asm'),
      files('../../avx128-shuffle.S'),
      files('../../avx-ymm-writeback.S'),
      files('../../vpshufd-distinct-regs.S'),
      files('../../vroundps-imm0b.S'),
      files('../../vroundsd-fixed.asm'),
      files('../../vroundss-fixed.asm'),
      files('../../vcvtss2sd-alias.asm'),
      files('../../vcvtdq2pd-ymm-alias.asm'),
      files('../../vshufpd-interleave.asm'),
      files('../../avx-regression-extract-alias.S'),
      files('../../avx-regression-narrow-state.S'),
      files('../../avx-regression-fma-alias.S'),
      files('../../avx-regression-vround-alias.S'),
      files('../../avx-regression-vround-boundaries.S'),
      files('../../vcvtps2dq-alias.c'),
      files('../../vpackus-fast.c'),
      files('../../vpmulhrsw-fast.c'),
      files('../../vpshufb-fast.c'),
    ],
    'timeout': 300,
  }]
  test(
    'test-redundant-xvldi',
    find_program('python3'),
    args: files('../../../latx/test-redundant-xvldi.py'),
    suite: 'latx-integration',
  )
  foreach cache_option : ['0000', '1100', '1101', '1110', '1111']
    latx_integration_tests += [{
      'name': 'test-base-displacement-' + cache_option,
      'runner': find_program('../../test-avx-regressions.py'),
      'args': [emulators['latx-x86_64'],
               files('../../base-displacement.asm')],
      'env': {'LATX_IMM_REG': cache_option, 'LATX_SOFTFPU': '0'},
      'timeout': 120,
    }]
    test(
      'test-rip-relative-cache-' + cache_option,
      find_program('../../test-avx-regressions.py'),
      args: [emulators['latx-x86_64'],
             files('../../rip-relative-cache.asm')],
      env: ['LATX_IMM_REG=' + cache_option, 'LATX_AVX_PREBUILT='],
      suite: 'latx-integration',
      timeout: 120,
    )
  endforeach
endif

if 'i386-linux-user' in target_dirs
  if 'CONFIG_LATX_AVX_OPT' in config_host
    latx_integration_tests += [{
      'name': 'test-f16c-mxcsr-i386',
      'runner': find_program('../../test-avx-regressions.py'),
      'args': ['--guest-bits', '32', emulators['latx-i386'],
               files('../../f16c-mxcsr.asm')],
      'timeout': 120,
    }]
    latx_integration_tests += [{
      'name': 'test-f16c-upper-zero-i386',
      'runner': find_program('../../test-avx-regressions.py'),
      'args': ['--guest-bits', '32', emulators['latx-i386'],
               files('../../f16c-upper-zero.asm')],
      'timeout': 120,
    }]
  endif
  latx_integration_tests += [{
    'name': 'test-instptn-signal-recovery-i386',
    'runner': find_program('../../test-avx-regressions.py'),
    'args': ['--guest-bits', '32', emulators['latx-i386'],
             files('../../instptn-signal-recovery-i386.S')],
    'env': {'LATX_SOFTFPU': '0'},
    'timeout': 120,
  }]
  latx_integration_tests += [{
    'name': 'test-ymmh-conditional-save-i386',
    'runner': find_program('../../test-avx-regressions.py'),
    'args': ['--guest-bits', '32', emulators['latx-i386'],
             files('../../ymmh-conditional-save.asm')],
    'env': {'LATX_SOFTFPU': '0'},
    'timeout': 120,
  }]
endif
