foreach arch : ['x86_64', 'i386']
  if arch + '-linux-user' in target_dirs
    latx_integration_tests += [{
      'name': 'test-xlat-segment-' + arch,
      'runner': find_program('../../test-xlat-segment.sh'),
      'args': [
        emulators['latx-' + arch],
        files('../../xlat-segment.c'),
        arch,
      ],
      'timeout': 60,
    }]
  endif
endforeach
