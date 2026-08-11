if 'x86_64-linux-user' in target_dirs
  test(
    'test-clone-namespaces',
    find_program('../../test-clone-namespaces.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../clone-namespaces.S'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
  test(
    'test-setns-ipc-mqueue',
    find_program('../../test-setns-ipc-mqueue.sh'),
    args: [
      emulators['latx-x86_64'],
      files('../../setns-ipc-mqueue.S'),
      files('../../exec-with-pidfd.py'),
    ],
    protocol: 'exitcode',
    suite: 'latx-integration',
    timeout: 30,
  )
endif
