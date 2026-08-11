if 'x86_64-linux-user' in target_dirs
  latx_integration_tests += [{
    'name': 'test-process-vm-remote-iovec',
    'runner': find_program('../../test-process-vm-remote-iovec.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../process-vm-remote-iovec.S'),
    ],
  }]
endif
