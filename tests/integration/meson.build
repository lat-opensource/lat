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

    test(
      'test-x11-async-bridge',
      find_program('test-x11-async-bridge.sh'),
      args: [
        emulators['latx-x86_64'],
        files('x11-async-bridge-guest.c'),
        files('x11-async-bridge-probe.c'),
        files('x11-async-bridge-dummy.c'),
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
    'test-proc-thread-count',
    find_program('test-proc-thread-count.sh'),
    args: [
      emulators['latx-x86_64'],
      files('proc-thread-count.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-syscall-user-dispatch',
    find_program('test-syscall-user-dispatch.sh'),
    args: [
      emulators['latx-x86_64'],
      files('syscall-user-dispatch.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-prctl-x86-abi',
    find_program('test-prctl-x86-abi.sh'),
    args: [
      emulators['latx-x86_64'],
      files('prctl-x86-abi.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-prctl-x86-inherit',
    find_program('test-prctl-x86-inherit.sh'),
    args: [
      emulators['latx-x86_64'],
      files('prctl-x86-inherit.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-prctl-x86-semantics',
    find_program('test-prctl-x86-semantics.sh'),
    args: [
      emulators['latx-x86_64'],
      files('prctl-x86-semantics.c'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 120,
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
    'test-clone-parent-settid',
    find_program('test-clone-parent-settid.sh'),
    args: [
      emulators['latx-x86_64'],
      files('clone-parent-settid.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-guest-fatal-signal-status',
    find_program('test-guest-fatal-signal-status.sh'),
    args: [
      emulators['latx-x86_64'],
      files('guest-fatal-signal-status.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-process-vm-remote-iovec',
    find_program('test-process-vm-remote-iovec.sh'),
    args: [
      emulators['latx-x86_64'],
      files('process-vm-remote-iovec.S'),
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
  test(
    'test-seccomp-filter-ordering',
    find_program('test-seccomp-filter-ordering.sh'),
    args: [
      emulators['latx-x86_64'],
      files('seccomp-filter-ordering.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-seccomp-errno-restart',
    find_program('test-seccomp-errno-restart.sh'),
    args: [
      emulators['latx-x86_64'],
      files('seccomp-errno-restart.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-seccomp-virtual-syscall',
    find_program('test-seccomp-virtual-syscall.sh'),
    args: [
      emulators['latx-x86_64'],
      files('seccomp-virtual-syscall.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-seccomp-ptrace-trace',
    find_program('test-seccomp-ptrace-trace.sh'),
    args: [
      emulators['latx-x86_64'],
      files('seccomp-ptrace-trace.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
endif

if host_machine.cpu_family() == 'loongarch64' and \
   'i386-linux-user' in target_dirs
  test(
    'test-seccomp-virtual-syscall-i386',
    find_program('test-seccomp-virtual-syscall.sh'),
    args: [
      emulators['latx-i386'],
      files('seccomp-virtual-syscall-i386.S'),
      'i386',
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
endif
