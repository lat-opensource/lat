if ('CONFIG_LATX_DECODE_DEBUG' in config_host)
    i386_ss.add(when: 'CONFIG_LATX', if_true: files(
      'ir1.c'
    ))
endif

i386_ss.add(when: 'CONFIG_LATX', if_true: files(
  'ir1-bd.c'
))
