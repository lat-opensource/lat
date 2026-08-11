if 'x86_64-linux-user' in target_dirs
  test(
    'test-proc-readdir',
    find_program('../../test-proc-readdir.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../proc-readdir.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-rcu-thread-visibility',
    find_program('../../test-rcu-thread-visibility.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../rcu-thread-visibility.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-proc-thread-count',
    find_program('../../test-proc-thread-count.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../proc-thread-count.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-clone-parent-settid',
    find_program('../../test-clone-parent-settid.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../clone-parent-settid.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-guest-fatal-signal-status',
    find_program('../../test-guest-fatal-signal-status.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../guest-fatal-signal-status.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
endif
