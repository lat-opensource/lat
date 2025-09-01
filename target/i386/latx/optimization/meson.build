i386_ss.add(when: 'CONFIG_LATX', if_true: files(
  'opt-jmp.c',
  'ir1-optimization.c',
  'ir2-optimization.c',
  'tunnel-lib.c',
  'syscall-tunnel.c',
  'profile.c',
  'ts.c',
  'imm-cache.c',
  'tu-bd.c',
  'flag-lbt-bd.c',
  'flag-reduction-bd.c',
  'insts-pattern-bd.c',
  'hbr-bd.c'
))
if ('CONFIG_LATX_DECODE_DEBUG' in config_host)
    i386_ss.add(when: 'CONFIG_LATX', if_true: files(
      'tu.c',
      'flag-lbt.c',
      'flag-reduction.c',
      'insts-pattern.c',
      'hbr.c'
    ))
endif