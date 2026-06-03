libvixl_ss = ss.source_set()
subdir('libvixl')

common_ss.add(when: 'CONFIG_I386_DIS', if_true: files('i386.c'))
common_ss.add(when: 'CONFIG_LOONGARCH_DIS', if_true: files('loongarch.c'))
