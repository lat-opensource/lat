if host_machine.cpu_family() == 'loongarch64' and \
   'x86_64-linux-user' in target_dirs
  if 'CONFIG_LATX_KZT' in config_host
    test(
      'test-x11-xcb-wrapper-abi',
      find_program('test-x11-xcb-wrapper-abi.sh'),
      args: [
        emulators['latx-x86_64'],
        files('x11-xcb-wrapper-abi-guest.c'),
        files('x11-xcb-wrapper-abi-probe.c'),
        files('x11-xcb-wrapper-abi-dummy.c'),
      ],
      protocol: 'exitcode',
      suite: 'latx-integration',
      timeout: 120,
    )
  endif

  test(
    'test-cef-userns-nested',
    find_program('test-cef-userns-nested.sh'),
    args: [
      emulators['latx-x86_64'],
      files('cef-userns-nested.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-cef-userns-exec',
    find_program('test-cef-userns-exec.sh'),
    args: [
      emulators['latx-x86_64'],
      files('cef-userns-exec.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-proc-readdir',
    find_program('test-proc-readdir.sh'),
    args: [
      emulators['latx-x86_64'],
      files('proc-readdir.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-rcu-thread-visibility',
    find_program('test-rcu-thread-visibility.sh'),
    args: [
      emulators['latx-x86_64'],
      files('rcu-thread-visibility.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-clone-namespaces',
    find_program('test-clone-namespaces.sh'),
    args: [
      emulators['latx-x86_64'],
      files('clone-namespaces.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-setns-ipc-mqueue',
    find_program('test-setns-ipc-mqueue.sh'),
    args: [
      emulators['latx-x86_64'],
      files('setns-ipc-mqueue.S'),
      files('exec-with-pidfd.py'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
endif
