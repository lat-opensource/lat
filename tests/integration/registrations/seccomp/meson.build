if 'x86_64-linux-user' in target_dirs
  test(
    'test-seccomp-filter-ordering',
    find_program('../../test-seccomp-filter-ordering.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../seccomp-filter-ordering.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-seccomp-errno-restart',
    find_program('../../test-seccomp-errno-restart.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../seccomp-errno-restart.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-seccomp-virtual-syscall',
    find_program('../../test-seccomp-virtual-syscall.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../seccomp-virtual-syscall.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-seccomp-ptrace-trace',
    find_program('../../test-seccomp-ptrace-trace.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../seccomp-ptrace-trace.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
endif

if 'i386-linux-user' in target_dirs
  test(
    'test-seccomp-virtual-syscall-i386',
    find_program('../../test-seccomp-virtual-syscall.sh'),
    args: [
      emulators['latx-i386'],
      files('../../seccomp-virtual-syscall-i386.S'),
      'i386',
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
endif
