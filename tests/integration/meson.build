latx_integration_tests = []

if host_machine.cpu_family() == 'loongarch64' and \
   'CONFIG_BUILD_LIBLAT' not in config_host
  subdir('registrations/x11-kzt')
  subdir('registrations/sandbox')
  subdir('registrations/process')
  subdir('registrations/namespace')
  subdir('registrations/process-vm')
  subdir('registrations/prctl')
  subdir('registrations/seccomp')
endif

subdir('registrations/liblat')
subdir('registrations')
