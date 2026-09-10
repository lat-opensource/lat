if 'x86_64-linux-user' in target_dirs and \
   'CONFIG_LATX_KZT' in config_host
  latx_integration_tests += [{
    'name': 'test-x11-xcb-wrapper-abi',
    'runner': find_program('../../test-x11-xcb-wrapper-abi.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../x11-xcb-wrapper-abi-guest.c'),
      files('../../x11-xcb-wrapper-abi-probe.c'),
      files('../../x11-xcb-wrapper-abi-dummy.c'),
    ],
    'timeout': 120,
  }]

  latx_integration_tests += [{
    'name': 'test-x11-async-bridge',
    'runner': find_program('../../test-x11-async-bridge.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../x11-async-bridge-guest.c'),
      files('../../x11-async-bridge-probe.c'),
      files('../../x11-async-bridge-dummy.c'),
    ],
    'timeout': 120,
  }]


  latx_integration_tests += [{
    'name': 'test-kzt-guest-tls-opt-in',
    'runner': find_program('../../test-kzt-guest-tls-opt-in.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../kzt-guest-tls-opt-in.c'),
      files('../../x11-async-bridge-dummy.c'),
      files('../../kzt-guest-tls-opt-in-plugin.c'),
    ],
    'timeout': 120,
  }]

  latx_integration_tests += [{
    'name': 'test-kzt-pthread-tsd-alias',
    'runner': find_program('../../test-kzt-pthread-tsd-alias.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../kzt-pthread-tsd-alias-guest.c'),
    ],
    'timeout': 120,
  }]

  latx_integration_tests += [{
    'name': 'test-kzt-attached-robust',
    'runner': find_program('../../test-kzt-attached-robust.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../kzt-attached-robust-guest.c'),
      files('../../kzt-attached-robust-probe.c'),
      files('../../x11-async-bridge-dummy.c'),
    ],
    'timeout': 120,
  }]

  latx_integration_tests += [{
    'name': 'test-kzt-cxx-tls-lifetime',
    'runner': find_program('../../test-kzt-cxx-tls-lifetime.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../kzt-cxx-tls-lifetime-guest.c'),
      files('../../kzt-cxx-tls-lifetime-plugin.cpp'),
      files('../../kzt-cxx-tls-lifetime-destructor.cpp'),
      files('../../kzt-cxx-tls-lifetime-probe.c'),
      files('../../x11-async-bridge-dummy.c'),
    ],
    'timeout': 120,
  }]

  latx_integration_tests += [{
    'name': 'test-kzt-tls-dlopen-stress',
    'runner': find_program('../../test-kzt-tls-dlopen-stress.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../kzt-tls-dlopen-stress-guest.c'),
      files('../../kzt-host-thread-tls-guest-plugin.c'),
      files('../../kzt-host-thread-tls-guest-plugin-b.c'),
      files('../../kzt-tls-dlopen-stress-ie-plugin.c'),
      files('../../kzt-tls-dlopen-stress-probe.c'),
      files('../../x11-async-bridge-dummy.c'),
    ],
    'timeout': 180,
  }]

  latx_integration_tests += [{
    'name': 'test-kzt-libc-boundary',
    'runner': find_program('../../test-kzt-libc-boundary.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../kzt-libc-boundary.c'),
      files('../../x11-async-bridge-dummy.c'),
    ],
    'timeout': 120,
  }]

  latx_integration_tests += [{
    'name': 'test-kzt-host-thread-tls',
    'runner': find_program('../../test-kzt-host-thread-tls.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../kzt-host-thread-tls-guest.c'),
      files('../../kzt-host-thread-tls-guest-plugin.c'),
      files('../../kzt-host-thread-tls-probe.c'),
      files('../../x11-async-bridge-dummy.c'),
      files('../../kzt-host-thread-tls-ie-plugin.c'),
      files('../../kzt-host-thread-cxx-tls-plugin.cpp'),
      files('../../kzt-host-thread-tls-guest-plugin-b.c'),
      files('../../kzt-host-thread-tls-tlsdesc-plugin.c'),
    ],
    'timeout': 120,
  }]

endif
