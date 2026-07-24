if host_machine.cpu_family() == 'loongarch64' and \
   'x86_64-linux-user' in target_dirs
  test(
    'test-cef-userns-nested',
    find_program('test-cef-userns-nested.sh'),
    args: [
      emulators['latx-x86_64'],
      files('cef-userns-nested.S'),
    ],
    protocol: 'exitcode',
    timeout: 30,
  )
  test(
    'test-cef-userns-exec',
    find_program('test-cef-userns-exec.sh'),
    args: [
      emulators['latx-x86_64'],
      files('cef-userns-exec.S'),
    ],
    protocol: 'exitcode',
    timeout: 30,
  )
endif
