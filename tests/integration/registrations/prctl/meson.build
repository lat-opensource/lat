if 'x86_64-linux-user' in target_dirs
  latx_integration_tests += [{
    'name': 'test-syscall-user-dispatch',
    'runner': find_program('../../test-syscall-user-dispatch.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../syscall-user-dispatch.S'),
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-prctl-x86-abi',
    'runner': find_program('../../test-prctl-x86-abi.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../prctl-x86-abi.S'),
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-prctl-x86-inherit',
    'runner': find_program('../../test-prctl-x86-inherit.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../prctl-x86-inherit.S'),
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-prctl-x86-semantics',
    'runner': find_program('../../test-prctl-x86-semantics.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../prctl-x86-semantics.c'),
      files('../../prctl-native-env-helper.c'),
    ],
    'timeout': 120,
  }]
  latx_integration_tests += [{
    'name': 'test-prctl-x86-privileged',
    'runner': find_program('../../test-prctl-x86-privileged.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../prctl-x86-semantics.c'),
    ],
    'timeout': 120,
  }]
  latx_integration_tests += [{
    'name': 'test-prctl-x86-exec-race',
    'runner': find_program('../../test-prctl-x86-exec-race.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../prctl-x86-semantics.c'),
      files('../../prctl-native-env-helper.c'),
      files('../../prctl-exec-race-shim.c'),
    ],
    'timeout': 45,
  }]
endif

if 'i386-linux-user' in target_dirs
  latx_integration_tests += [{
    'name': 'test-prctl-i386-abi',
    'runner': find_program('../../test-prctl-i386-abi.sh'),
    'args': [
      emulators['latx-i386'],
      files('../../prctl-i386-abi.S'),
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-prctl-i386-exec-race',
    'runner': find_program('../../test-prctl-i386-exec-race.sh'),
    'args': [
      emulators['latx-i386'],
      files('../../prctl-i386-exec-race.S'),
      files('../../prctl-native-env-helper.c'),
      files('../../prctl-exec-race-shim.c'),
    ],
    'timeout': 45,
  }]
  latx_integration_tests += [{
    'name': 'test-prctl-i386-mremap',
    'runner': find_program('../../test-prctl-i386-mremap.sh'),
    'args': [
      emulators['latx-i386'],
      files('../../prctl-i386-mremap.c'),
    ],
  }]
endif
