if 'x86_64-linux-user' in target_dirs
  latx_integration_tests += [{
    'name': 'test-clone-namespaces',
    'runner': find_program('../../test-clone-namespaces.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../clone-namespaces.S'),
    ],
  }]
  latx_integration_tests += [{
    'name': 'test-setns-ipc-mqueue',
    'runner': find_program('../../test-setns-ipc-mqueue.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('../../setns-ipc-mqueue.S'),
      files('../../exec-with-pidfd.py'),
    ],
  }]
endif
