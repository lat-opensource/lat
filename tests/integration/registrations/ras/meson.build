if config_host.has_key('CONFIG_LATX_DEBUG')
  foreach ras_target : [['i386', '32'], ['x86_64', '64']]
    if ras_target[0] + '-linux-user' in target_dirs
      latx_integration_tests += [{
        'name': 'test-hardware-ras-signal-' + ras_target[0],
        'runner': find_program('../../test-hardware-ras-signal.sh'),
        'args': [
          emulators['latx-' + ras_target[0]],
          files('../../hardware-ras-signal.S'),
          ras_target[1],
        ],
        'timeout': 180,
      }]
      latx_integration_tests += [{
        'name': 'test-hardware-ras-' + ras_target[0],
        'runner': find_program('../../test-hardware-ras.sh'),
        'args': [
          emulators['latx-' + ras_target[0]],
          files('../../hardware-ras.c'),
          ras_target[1],
        ],
        'timeout': 960,
      }]
    endif
  endforeach

  if 'x86_64-linux-user' in target_dirs and config_host.has_key('CONFIG_LATX_KZT')
    latx_integration_tests += [{
      'name': 'test-hardware-ras-kzt',
      'runner': find_program('../../test-hardware-ras-kzt.sh'),
      'args': [emulators['latx-x86_64'], files('../../hardware-ras-kzt.c')],
      'timeout': 240,
    }]
  endif
endif
