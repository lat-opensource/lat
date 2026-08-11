if 'x86_64-linux-user' in target_dirs and \
   'CONFIG_LATX_KZT' in config_host
  test(
    'test-x11-xcb-wrapper-abi',
    find_program('../../test-x11-xcb-wrapper-abi.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../x11-xcb-wrapper-abi-guest.c'),
      files('../../x11-xcb-wrapper-abi-probe.c'),
      files('../../x11-xcb-wrapper-abi-dummy.c'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 120,
  )

  test(
    'test-x11-async-bridge',
    find_program('../../test-x11-async-bridge.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../x11-async-bridge-guest.c'),
      files('../../x11-async-bridge-probe.c'),
      files('../../x11-async-bridge-dummy.c'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 120,
  )
endif
