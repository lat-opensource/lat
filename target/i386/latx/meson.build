cs_git = 'CONFIG_LATX_CAPSTONE_GIT' in config_host
cs_latx_debug = 'CONFIG_LATX_DEBUG' in config_host
cs_diet = 'CONFIG_CAPSTONE_DIET' in config_host
cs_op_access = 'CONFIG_LATX_CAPSTONE_OP_ACCESS' in config_host
latx_kzt = 'CONFIG_LATX_KZT' in config_host

capStone_data = configuration_data()
capStone_data.set('CAPSTONE_USE_SYS_DYN_MEM', '1')

capStone_data.set('LA_CAPSTONE', '1')
if cs_diet
  capStone_data.set('CAPSTONE_DIET', '1')
endif
if cs_op_access
  capStone_data.set('LATX_CAPSTONE_OP_ACCESS', '1')
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

latx_string_utils = files('latx-string-utils.c')
latx_runtime = files('latx-runtime.c')

latx_aot_directories = [
  'target/i386/latx/translator',
  'target/i386/latx/ir1',
  'target/i386/latx/ir2',
  'target/i386/latx/optimization',
  'target/i386/latx/sbt',
  'target/i386/latx/include',
]
latx_aot_exclude_directories = [
  'target/i386/latx/sbt/tests',
]
latx_aot_generator = project_source_root / 'scripts/gen-aot-build-id.py'
latx_aot_generator_file = files('../../../scripts/gen-aot-build-id.py')
latx_aot_input_names = run_command(
  python, latx_aot_generator,
  '--root', project_source_root,
  '--print-inputs',
  '--directory', latx_aot_directories[0],
  '--directory', latx_aot_directories[1],
  '--directory', latx_aot_directories[2],
  '--directory', latx_aot_directories[3],
  '--directory', latx_aot_directories[4],
  '--directory', latx_aot_directories[5],
  '--exclude-directory', latx_aot_exclude_directories[0],
  check: true,
).stdout().strip().split('\n')
latx_aot_inputs = []
foreach input_name : latx_aot_input_names
  latx_aot_inputs += files('../../../' + input_name)
endforeach

latx_aot_config_names = [
  'CONFIG_CAPSTONE_DIET',
  'CONFIG_DEBUG_TCG',
  'CONFIG_LATX_AOT',
  'CONFIG_LATX_AVX_OPT',
  'CONFIG_LATX_BNE_B',
  'CONFIG_LATX_CAPSTONE_GIT',
  'CONFIG_LATX_CAPSTONE_OP_ACCESS',
  'CONFIG_LATX_DEBUG',
  'CONFIG_LATX_FAST_JMPCACHE',
  'CONFIG_LATX_FLAG_REDUCTION',
  'CONFIG_LATX_FLAG_REDUCTION_EXTEND',
  'CONFIG_LATX_GLUE_MASK',
  'CONFIG_LATX_HBR',
  'CONFIG_LATX_IMM_REG',
  'CONFIG_LATX_INSTS_PATTERN',
  'CONFIG_LATX_JRRA',
  'CONFIG_LATX_JRRA_STACK',
  'CONFIG_LATX_KZT',
  'CONFIG_LATX_LARGE_CC',
  'CONFIG_LATX_LAZYLINK',
  'CONFIG_LATX_LLSC',
  'CONFIG_LATX_LOW_MEM_0',
  'CONFIG_LATX_LOW_MEM_1',
  'CONFIG_LATX_LOW_MEM_2',
  'CONFIG_LATX_MONITOR_SHARED_MEM',
  'CONFIG_LATX_O0',
  'CONFIG_LATX_O1',
  'CONFIG_LATX_O2',
  'CONFIG_LATX_O3',
  'CONFIG_LATX_OPT_PUSH_POP',
  'CONFIG_LATX_PROFILER',
  'CONFIG_LATX_RADICAL_EFLAGS',
  'CONFIG_LATX_RUNTIME_TRACE_RANGE',
  'CONFIG_LATX_SMC_OPT',
  'CONFIG_LATX_SPLIT_TB',
  'CONFIG_LATX_SSSE3_SSE4',
  'CONFIG_LATX_SYSCALL_TUNNEL',
  'CONFIG_LATX_TBLINK',
  'CONFIG_LATX_TBMINI_ENABLE',
  'CONFIG_LATX_TS',
  'CONFIG_LATX_TU',
  'CONFIG_LATX_TUNNEL_LIB',
  'CONFIG_LATX_XCOMISX_OPT',
  'CONFIG_LOONGARCH_NEW_WORLD',
  'CONFIG_PLUGIN',
  'CONFIG_USER_ONLY',
]
latx_aot_config_args = []
foreach config_name : latx_aot_config_names
  config_value = config_name in config_host ? config_host[config_name] : 'n'
  latx_aot_config_args += ['--config', config_name + '=' + config_value]
endforeach

latx_aot_build_id = custom_target(
  'latx-aot-build-id',
  input: latx_aot_inputs,
  output: 'latx-aot-build-id.h',
  command: [
    python, latx_aot_generator,
    '--root', project_source_root,
    '--output', '@OUTPUT@',
  ] + latx_aot_config_args + [
    '--directory', latx_aot_directories[0],
    '--directory', latx_aot_directories[1],
    '--directory', latx_aot_directories[2],
    '--directory', latx_aot_directories[3],
    '--directory', latx_aot_directories[4],
    '--directory', latx_aot_directories[5],
    '--exclude-directory', latx_aot_exclude_directories[0],
  ],
  depend_files: latx_aot_generator_file,
)
i386_ss.add(when: 'CONFIG_LATX', if_true: latx_aot_build_id)

i386_ss.add(when: 'CONFIG_LATX', if_true: files(
  'error.c',
  'mem.c',
  'latx-config.c',
  'shadow-jmp-cache.c',
  'latx-options.c',
  'latx-perf.c',
  'latx-name-demangling.cpp',
  'latx-special-args.c',
  'latx-signal.c',
) + latx_string_utils + latx_runtime)

lazydis_lib = static_library('lazydis',
                         build_by_default: false,
                         sources: files('latx-disassemble-trace/zydis.c'),
                         include_directories: include_directories('include/', 'include/zydisdir'))

lazydis = declare_dependency(link_with: lazydis_lib,
                         link_args: ['-L' + meson.current_source_dir() + '/exlib', '-lZydis', '-lZycore'],
                         include_directories: include_directories('include/'))

i386_ss.add(when: 'CONFIG_LATX_DEBUG', if_true: lazydis)

i386_ss.add(when: 'CONFIG_LATX_DEBUG', if_true: files(
  'latx-disassemble-trace/latx-disassemble-trace.c',
))

if cs_latx_debug or cs_git
    i386_ss.add(when: 'CONFIG_LATX', if_true: capstone_git)
endif

r = run_command('python3', 'convert.py', check: true)
message('convert.py ->', r.stdout().strip(), r.stderr().strip())

subdir('ir1')
subdir('ir2')
subdir('optimization')
subdir('translator')
subdir('sbt')
if latx_kzt
    subdir('context')
    subdir('wrapper')
endif
