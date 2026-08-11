if 'x86_64-linux-user' in target_dirs
  test(
    'test-syscall-user-dispatch',
    find_program('../../test-syscall-user-dispatch.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../syscall-user-dispatch.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-prctl-x86-abi',
    find_program('../../test-prctl-x86-abi.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../prctl-x86-abi.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-prctl-x86-inherit',
    find_program('../../test-prctl-x86-inherit.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../prctl-x86-inherit.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-prctl-x86-semantics',
    find_program('../../test-prctl-x86-semantics.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../prctl-x86-semantics.c'),
      files('../../prctl-native-env-helper.c'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 120,
  )
  test(
    'test-prctl-x86-privileged',
    find_program('../../test-prctl-x86-privileged.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../prctl-x86-semantics.c'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 120,
  )
  test(
    'test-prctl-x86-exec-race',
    find_program('../../test-prctl-x86-exec-race.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../prctl-x86-semantics.c'),
      files('../../prctl-native-env-helper.c'),
      files('../../prctl-exec-race-shim.c'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 45,
  )
endif

if 'i386-linux-user' in target_dirs
  test(
    'test-prctl-i386-abi',
    find_program('../../test-prctl-i386-abi.sh'),
    args: [
      emulators['latx-i386'],
      files('../../prctl-i386-abi.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-prctl-i386-exec-race',
    find_program('../../test-prctl-i386-exec-race.sh'),
    args: [
      emulators['latx-i386'],
      files('../../prctl-i386-exec-race.S'),
      files('../../prctl-native-env-helper.c'),
      files('../../prctl-exec-race-shim.c'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 45,
  )
  test(
    'test-prctl-i386-mremap',
    find_program('../../test-prctl-i386-mremap.sh'),
    args: [
      emulators['latx-i386'],
      files('../../prctl-i386-mremap.c'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
endif
