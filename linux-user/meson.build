linux_user_ss.add(files(
  'elfload.c',
  'exit.c',
  'fd-trans.c',
  'linuxload.c',
  'main.c',
  'mmap.c',
  'safe-syscall.S',
  'signal.c',
  'strace.c',
  'syscall.c',
  'uaccess.c',
  'uname.c',
  'vm86.c'
))
linux_user_ss.add(rt)

linux_user_ss.add(when: 'TARGET_HAS_BFLT', if_true: files('flatload.c'))
linux_user_ss.add(when: 'CONFIG_ARM_COMPATIBLE_SEMIHOSTING', if_true: files('semihost.c'))

syscall_nr_generators = {}

gen_vdso_exe = executable('gen-vdso', 'gen-vdso.c',
                          native: true, build_by_default: false)
gen_vdso = generator(gen_vdso_exe, output: '@BASENAME@.c.inc',
                     arguments: ['-o', '@OUTPUT@', '@EXTRA_ARGS@', '@INPUT@'])

subdir('alpha')
subdir('arm')
subdir('hppa')
subdir('i386')
subdir('m68k')
subdir('microblaze')
subdir('mips64')
subdir('mips')
subdir('ppc')
subdir('s390x')
subdir('sh4')
subdir('sparc64')
subdir('sparc')
subdir('x86_64')
subdir('xtensa')
