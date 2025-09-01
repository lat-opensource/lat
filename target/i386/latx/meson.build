cs_git = 'CONFIG_LATX_CAPSTONE_GIT' in config_host
cs_latx_debug = 'CONFIG_LATX_DEBUG' in config_host
cs_diet = 'CONFIG_CAPSTONE_DIET' in config_host
latx_kzt = 'CONFIG_LATX_KZT' in config_host

if ('CONFIG_LATX_DECODE_DEBUG' in config_host)
    capStone_data = configuration_data()
    capStone_data.set('CAPSTONE_USE_SYS_DYN_MEM', '1')

    capStone_data.set('LA_CAPSTONE', '1')
    if cs_diet
      capStone_data.set('CAPSTONE_DIET', '1')
    endif


    if cs_latx_debug or cs_git
        capstone_git_files = files('capstone_git/cs.c')
        capstone_git_files += files('capstone_git/MCInst.c')
        capstone_git_files += files('capstone_git/MCInstrDesc.c')
        capstone_git_files += files('capstone_git/MCRegisterInfo.c')
        capstone_git_files += files('capstone_git/MCInstPrinter.c')
        capstone_git_files += files('capstone_git/Mapping.c')
        capstone_git_files += files('capstone_git/SStream.c')
        capstone_git_files += files('capstone_git/utils.c')
        capstone_git_files += files('latx-disassemble-trace/capstone_git.c')
        capstone_git_files += files('capstone_git/arch/X86/X86Disassembler.c')
        capstone_git_files += files('capstone_git/arch/X86/X86DisassemblerDecoder.c')
        capstone_git_files += files('capstone_git/arch/X86/X86InstPrinterCommon.c')
        capstone_git_files += files('capstone_git/arch/X86/X86ATTInstPrinter.c')
        capstone_git_files += files('capstone_git/arch/X86/X86IntelInstPrinter.c')
        capstone_git_files += files('capstone_git/arch/X86/X86Mapping.c')
        capstone_git_files += files('capstone_git/arch/X86/X86Module.c')

        capstone_git_incdir_library = include_directories('capstone_git/include','include/')
        capstone_git_incdir_dependency = include_directories('capstone_git/include/capstone')
    endif


    capStone_data.set('CAPSTONE_HAS_X86', 1)

    configure_file(output: 'capStone-defs.h', configuration: capStone_data)

    capStone_cargs = [
      # FIXME: There does not seem to be a way to completely replace the c_args
      # that come from add_project_arguments() -- we can only add to them.
      # So: disable all warnings with a big hammer.
      '-Wno-error', '-w',

      # Include all configuration defines via a header file, which will wind up
      # as a dependency on the object file, and thus changes here will result
      # in a rebuild.
      '-include', 'capStone-defs.h'
    ]

    if cs_latx_debug or cs_git
        libcapstone_git_lib = static_library('capstone_git',
                                    build_by_default: false,
                                    sources: capstone_git_files,
                                    c_args: capStone_cargs,
                                    include_directories: capstone_git_incdir_library)
        capstone_git = declare_dependency(link_with: libcapstone_git_lib,
                                      include_directories: capstone_git_incdir_dependency)
    endif
    if cs_latx_debug or cs_git
        i386_ss.add(when: 'CONFIG_LATX', if_true: capstone_git)
    endif
    subdir('translator')

    laxed_lib = static_library('laxed',
                         build_by_default: false,
                         sources: files('latx-disassemble-trace/xed.c'),
                         include_directories: include_directories('include/'))

    laxed = declare_dependency(link_with: laxed_lib,
                            link_args: ['-L' + meson.current_source_dir() + '/exlib', '-lxed'],
                            include_directories: include_directories('include/'))

    lazydis_lib = static_library('lazydis',
                            build_by_default: false,
                            sources: files('latx-disassemble-trace/zydis.c'),
                            include_directories: include_directories('include/', 'include/zydisdir'))

    lazydis = declare_dependency(link_with: lazydis_lib,
                            link_args: ['-L' + meson.current_source_dir() + '/exlib', '-lZydis', '-lZycore'],
                            include_directories: include_directories('include/'))

    i386_ss.add(when: 'CONFIG_LATX_DEBUG', if_true: laxed)
    i386_ss.add(when: 'CONFIG_LATX_DEBUG', if_true: lazydis)

    i386_ss.add(when: 'CONFIG_LATX_DEBUG', if_true: files(
    'latx-disassemble-trace/latx-disassemble-trace.c',
    ))
endif

i386_ss.add(when: 'CONFIG_LATX', if_true: files(
  'error.c',
  'mem.c',
  'latx-config.c',
  'latx-options.c',
  'latx-perf.c',
  'latx-name-demangling.cpp',
  'latx-special-args.c',
  'latx-signal.c',
))

bddisasm_lib = static_library('bddisasm',
        build_by_default: true,
        sources: files('bddisasm/bddisasm.c'),
        include_directories: include_directories('include/', 'include/bddisasm'))

if ('CONFIG_BDDISASM_NO_MNEMONIC' in config_host)
    bddisasm_noprint = declare_dependency(
        link_args: [meson.current_source_dir() + '/exlib/libbddisasm.a']
    )
    bddisasm = declare_dependency(link_with: bddisasm_lib,
        link_with: bddisasm_lib,
        dependencies: bddisasm_noprint,
        include_directories: include_directories('include/'))
else
    bddisasm_print = declare_dependency(
        link_args: [meson.current_source_dir() + '/exlib/libbddisasm-print.a']
    )
    bddisasm = declare_dependency(link_with: bddisasm_lib,
        link_with: bddisasm_lib,
        dependencies: bddisasm_print,
        include_directories: include_directories('include/'))
endif

i386_ss.add(bddisasm)

r = run_command('python3', 'convert.py', check: true)
message('convert.py ->', r.stdout().strip(), r.stderr().strip())

subdir('diStorm')
subdir('ir1')
subdir('ir2')
subdir('optimization')
subdir('translator-bd')
subdir('sbt')
if latx_kzt
    subdir('context')
    subdir('wrapper')
endif
