if 'x86_64-linux-user' in target_dirs
  latx_integration_tests += [{
    'name': 'test-seccomp-filter-ordering',
    'runner': find_program('../../test-seccomp-filter-ordering.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../seccomp-filter-ordering.S'),
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-seccomp-errno-restart',
    'runner': find_program('../../test-seccomp-errno-restart.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../seccomp-errno-restart.S'),
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-seccomp-virtual-syscall',
    'runner': find_program('../../test-seccomp-virtual-syscall.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../seccomp-virtual-syscall.S'),
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-seccomp-ptrace-trace',
    'runner': find_program('../../test-seccomp-ptrace-trace.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../seccomp-ptrace-trace.S'),
    ],
  }]
endif

if 'i386-linux-user' in target_dirs
  latx_integration_tests += [{
    'name': 'test-seccomp-virtual-syscall-i386',
    'runner': find_program('../../test-seccomp-virtual-syscall.sh'),
    'args': [
      emulators['latx-i386'],
      files('../../seccomp-virtual-syscall-i386.S'),
      'i386',
    ],
  }]
endif
