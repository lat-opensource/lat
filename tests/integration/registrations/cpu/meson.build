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
if host_machine.cpu_family() == 'loongarch64' and \
   'CONFIG_LATX_WOW64' in config_host and \
   'x86_64-linux-user' in target_dirs
  foreach case : [
    {'name': 'test-wow64-provider', 'args': []},
    {'name': 'test-wow64-provider-loader', 'args': ['--worker-loader']},
    {'name': 'test-wow64-provider-cache', 'args': ['--cache-pressure']},
  ]
    latx_integration_tests += [{
    'name': case['name'],
    'runner': find_program('../../test-wow64-provider.sh'),
    'args': [
      wow64_provider,
      files('../../wow64-provider.c'),
      project_source_root / 'include',
      files('../../wow64-host-state.S'),
    ] + case['args'],
    'timeout': 120,
  }]
  endforeach
endif
