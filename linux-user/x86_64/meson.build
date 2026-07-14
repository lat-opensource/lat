syscall_nr_generators += {
  'x86_64': generator(sh,
                      arguments: [ meson.current_source_dir() / 'syscallhdr.sh',
                                   '@INPUT@', '@OUTPUT0@', '@OUTPUT1@',
                                   '@OUTPUT2@', '@EXTRA_ARGS@' ],
                      output: [ '@BASENAME@_nr.h',
                                '@BASENAME@_to_32_map.h',
                                '@BASENAME@_target_32_nr.h' ])
}

vdso_inc = gen_vdso.process('vdso.so')

linux_user_ss.add(when: 'TARGET_X86_64', if_true: vdso_inc)
