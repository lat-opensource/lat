if 'x86_64-linux-user' in target_dirs
  latx_integration_tests += [{
    'name': 'test-proc-fd-closefrom-x86_64',
    'runner': find_program('../../test-proc-fd-closefrom.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../proc-fd-closefrom.c'),
      'x86_64',
    ],
  }]
endif

if 'i386-linux-user' in target_dirs
  latx_integration_tests += [{
    'name': 'test-proc-fd-closefrom-i386',
    'runner': find_program('../../test-proc-fd-closefrom.sh'),
    'args': [
      emulators['latx-i386'],
      files('../../proc-fd-closefrom.c'),
      'i386',
    ],
  }]
endif

foreach arch : ['x86_64', 'i386']
  if arch + '-linux-user' in target_dirs
    latx_integration_tests += [{
      'name': 'test-proc-fd-thread-race-' + arch,
      'runner': find_program('../../test-proc-fd-thread-race.sh'),
      'args': [
        emulators['latx-' + arch],
        files('../../proc-fd-thread-race.c'),
        arch,
      ],
      'timeout': 120,
    }, {
      'name': 'test-fd-conversion-scope-' + arch,
      'runner': find_program('../../test-fd-conversion-scope.sh'),
      'args': [
        emulators['latx-' + arch],
        files('../../fd-conversion-scope.c'),
        arch,
      ],
    }, {
      'name': 'test-close-linger-progress-' + arch,
      'runner': find_program('../../test-close-linger-progress.sh'),
      'args': [
        emulators['latx-' + arch],
        files('../../close-linger-progress.c'),
        arch,
      ],
      'timeout': 120,
    }, {
      'name': 'test-procfs-fstat-provider-race-' + arch,
      'runner': find_program('../../test-procfs-fstat-provider-race.sh'),
      'args': [
        emulators['latx-' + arch],
        files('../../procfs-fstat-provider-race.c'),
        arch,
      ],
      'timeout': 60,
    }, {
      'name': 'test-procfs-path-snapshot-' + arch,
      'runner': find_program('../../test-procfs-path-snapshot.sh'),
      'args': [
        emulators['latx-' + arch],
        files('../../procfs-path-snapshot.c'),
        arch,
      ],
      'timeout': 90,
    }, {
      'name': 'test-procfs-mapped-path-' + arch,
      'runner': find_program('../../test-procfs-mapped-path.sh'),
      'args': [
        emulators['latx-' + arch],
        files('../../procfs-mapped-path.c'),
        arch,
      ],
      'timeout': 90,
    }]
  endif
endforeach

if 'x86_64-linux-user' in target_dirs
  latx_integration_tests += [{
    'name': 'test-statx-fork-race',
    'runner': find_program('../../test-statx-fork-race.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../statx-fork-race.c'),
    ],
    'timeout': 90,
  }]
  latx_integration_tests += [{
    'name': 'test-signal-return-bridge',
    'runner': find_program('../../test-signal-return-bridge.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../signal-return-bridge.S'),
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-async-sigill-x86_64',
    'runner': find_program('../../test-async-sigill.py'),
    'args': [
      emulators['latx-x86_64'],
      files('../../async-sigill.S'),
      'x86_64',
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-x87-signal-mode',
    'runner': find_program('../../test-x87-signal-mode.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../x87-signal-mode.S'),
    ],
    'timeout': 120,
  }]
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
  latx_integration_tests += [{
    'name': 'test-softfpu-helper-store-gpr-x86_64',
    'runner': find_program('../../test-softfpu-helper-store-gpr-x86_64.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../softfpu-helper-store-gpr-x86_64.S'),
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-softfpu-shared-region-gpr-x86_64',
    'runner': find_program('../../test-softfpu-shared-region-gpr-x86_64.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../softfpu-shared-region-gpr-x86_64.S'),
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-softfpu-region-planner-x86_64',
    'runner': find_program('../../test-softfpu-region-planner-x86_64.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../softfpu-region-planner-x86_64.S'),
    ],
  }]
  if 'CONFIG_LATX_AVX_OPT' in config_host
    latx_integration_tests += [{
      'name': 'test-softfpu-xgetbv-gpr-x86_64',
      'runner': find_program('../../test-softfpu-xgetbv-gpr-x86_64.sh'),
      'args': [
        emulators['latx-x86_64'],
        files('../../softfpu-xgetbv-gpr-x86_64.S'),
      ],
    }]
  endif
  if 'CONFIG_LATX_KZT' in config_host
    latx_integration_tests += [{
      'name': 'test-int3-sigtrap-rip',
      'runner': find_program('../../test-int3-sigtrap-rip.sh'),
      'args': [
        emulators['latx-x86_64'],
        files('../../int3-sigtrap-rip.S'),
      ],
    }]
  endif
endif

if 'i386-linux-user' in target_dirs
  latx_integration_tests += [{
    'name': 'test-futex-private-leader-exit-i386',
    'runner': find_program('../../test-futex-private-leader-exit.py'),
    'args': [
      emulators['latx-i386'],
      files('../../futex-private-leader-exit-i386.c'),
      files('../../futex-private-leader-exit-shim.c'),
    ],
    'timeout': 45,
  }]
  latx_integration_tests += [{
    'name': 'test-async-sigill-i386',
    'runner': find_program('../../test-async-sigill.py'),
    'args': [
      emulators['latx-i386'],
      files('../../async-sigill.S'),
      'i386',
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-futex-private-truncate-i386',
    'runner': find_program('../../test-futex-private-truncate-i386.sh'),
    'args': [
      emulators['latx-i386'],
      files('../../futex-private-truncate-i386.c'),
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-softfpu-control-rounding-i386',
    'runner': find_program('../../test-softfpu-control-rounding-i386.sh'),
    'args': [
      emulators['latx-i386'],
      files('../../softfpu-control-rounding-i386.S'),
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-softfpu-eflags-region-i386',
    'runner': find_program('../../test-softfpu-eflags-region-i386.sh'),
    'args': [
      emulators['latx-i386'],
      files('../../softfpu-eflags-region-i386.S'),
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-latx-16k-page-permission-i386',
    'runner': find_program('../../test-latx-16k-page-permission-i386.sh'),
    'args': [
      emulators['latx-i386'],
      files('../../latx-16k-page-permission-i386.S'),
    ],
  }]
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
