linux_user_ss.add(files(
  'elfload-pagesize.c',
  'elfload.c',
  'exit.c',
  'fd-trans.c',
  'guest-seccomp.c',
  'linuxload.c',
  'main.c',
  'latc-bundle-loader.c',
  'guest-elf-map.c',
  'latcd-client.c',
  'auto-config.c',
  'latcd-protocol.c',
  'tb-key-set.c',
  'mmap.c',
  'safe-syscall.S',
  'signal.c',
  'strace.c',
  'syscall.c',
  'uaccess.c',
  'uname.c',
  'vm86.c',
  'pressure-vessel.c'
))
linux_user_ss.add(rt)
linux_user_ss.add(when: 'TARGET_X86_64',
                  if_true: files('latc-aot-v2-runner.c'),
                  if_false: files('latc-aot-v2-runner-stub.c'))

if 'CONFIG_LATX' in config_host
  linux_user_ss.add(lat_aot_runtime_dep)
endif

linux_user_ss.add(when: 'TARGET_HAS_BFLT', if_true: files('flatload.c'))
linux_user_ss.add(when: 'CONFIG_ARM_COMPATIBLE_SEMIHOSTING', if_true: files('semihost.c'))

syscall_nr_generators = {}

gen_vdso_exe = executable('gen-vdso', 'gen-vdso.c',
                          native: true, build_by_default: false)
gen_vdso = generator(gen_vdso_exe, output: '@BASENAME@.c.inc',
                     arguments: ['-o', '@OUTPUT@', '@EXTRA_ARGS@', '@INPUT@'])

subdir('i386')
subdir('x86_64')
