if 'x86_64-linux-user' in target_dirs
  test(
    'test-process-vm-remote-iovec',
    find_program('../../test-process-vm-remote-iovec.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../process-vm-remote-iovec.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
endif
