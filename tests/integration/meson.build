latx_integration_tests = []

if host_machine.cpu_family() == 'loongarch64'
  subdir('registrations/x11-kzt')
  subdir('registrations/sandbox')
  subdir('registrations/process')
  subdir('registrations/namespace')
  subdir('registrations/process-vm')
  subdir('registrations/prctl')
  subdir('registrations/seccomp')
endif

if host_machine.cpu_family() == 'loongarch64' and \
   'x86_64-linux-user' in target_dirs
  latx_integration_tests += [{
    'name': 'test-pressure-vessel-wrap',
    'runner': find_program('test-pressure-vessel-wrap.sh'),
    'args': [
      emulators['latx-x86_64'],
      files('pressure-vessel-wrap.S'),
      files('pressure-vessel-payload.sh'),
      files('steamwebhelper_sniper_wrap.sh'),
    ],
  }]
endif

subdir('registrations')
