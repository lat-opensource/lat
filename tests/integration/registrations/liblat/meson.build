if 'CONFIG_BUILD_LIBLAT' in config_host and host_machine.cpu_family() == 'loongarch64'
  foreach mode : ['callbacks', 'bad-arguments', 'repeat', 'host-map',
                  'image-collision', 'bootstrap-status', 'native-noreplace',
                  'native-fixed', 'native-unmap', 'native-remap',
                  'native-shm', 'native-shmdt']
    latx_integration_tests += [{
      'name': 'liblat-' + mode,
      'runner': find_program('../../liblat/run.sh'),
      'args': [liblat, mode],
      'timeout': 120,
    }]
  endforeach
endif
