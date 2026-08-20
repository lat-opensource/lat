if 'x86_64-linux-user' in target_dirs
  latx_integration_tests += [{
    'name': 'test-proc-readdir',
    'runner': find_program('../../test-proc-readdir.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../proc-readdir.S'),
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-rcu-thread-visibility',
    'runner': find_program('../../test-rcu-thread-visibility.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../rcu-thread-visibility.S'),
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-proc-thread-count',
    'runner': find_program('../../test-proc-thread-count.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../proc-thread-count.S'),
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-clone-parent-settid',
    'runner': find_program('../../test-clone-parent-settid.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../clone-parent-settid.S'),
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-guest-fatal-signal-status',
    'runner': find_program('../../test-guest-fatal-signal-status.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../guest-fatal-signal-status.S'),
    ],
  }]
endif

if 'i386-linux-user' in target_dirs
  latx_integration_tests += [{
    'name': 'test-fd-transform-race-i386',
    'runner': find_program('../../test-fd-transform-race-i386.sh'),
    'args': [
      emulators['latx-i386'],
      files('../../fd-transform-race-i386.S'),
      files('../../fd-transform-race-shim.c'),
    ],
  }]
endif
