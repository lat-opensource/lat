project('latx', ['c'], meson_version: '>=0.55.0',
        default_options: ['warning_level=1', 'c_std=gnu99', 'cpp_std=gnu++11', 'b_colorout=auto'] +
                         (meson.version().version_compare('>=0.56.0') ? [ 'b_staticpic=false' ] : []),
        version: run_command('head', meson.source_root() / 'VERSION').stdout().strip())

not_found = dependency('', required: false)
if meson.version().version_compare('>=0.56.0')
  keyval = import('keyval')
else
  keyval = import('unstable-keyval')
endif
ss = import('sourceset')
fs = import('fs')

sh = find_program('sh')
cc = meson.get_compiler('c')
config_host = keyval.load(meson.current_build_dir() / 'config-host.mak')
enable_static = 'CONFIG_STATIC' in config_host

latx_version_h = vcs_tag(command : ['git', 'describe'],
                        fallback : 'unknown',
			input : 'linux-user/latx-version.h.in',
                        output :'latx-version.h')
# Allow both shared and static libraries unless --enable-static
static_kwargs = enable_static ? {'static': true} : {}

# Temporary directory used for files created while
# configure runs. Since it is in the build directory
# we can safely blow away any previous version of it
# (and we need not jump through hoops to try to delete
# it when configure exits.)
tmpdir = meson.current_build_dir() / 'meson-private/temp'
qemu_datadir = get_option('datadir') / get_option('qemu_suffix')

config_host_data = configuration_data()
genh = []

target_dirs = config_host['TARGET_DIRS'].split()

python = import('python').find_installation()

supported_oses = ['linux']
supported_cpus = ['x86', 'x86_64', 'mips', 'mips64', 'loongarch64']

cpu = host_machine.cpu_family()

##################
# Compiler flags #
##################

add_global_arguments(config_host['QEMU_CFLAGS'].split(),
                     native: false, language: ['c', 'objc'])
add_global_arguments(config_host['QEMU_CXXFLAGS'].split(),
                     native: false, language: 'cpp')
add_global_link_arguments(config_host['QEMU_LDFLAGS'].split(),
                          native: false, language: ['c', 'cpp', 'objc'])

add_project_arguments('-isystem', meson.current_source_dir() / 'linux-headers',
                      '-isystem', 'linux-headers',
                      language: ['c', 'cpp'])

add_project_arguments('-iquote', '.',
                      '-iquote', meson.current_source_dir(),
                      '-iquote', meson.current_source_dir() / 'include',
                      '-iquote', meson.current_source_dir() / 'disas/libvixl',
                      language: ['c', 'cpp', 'objc'])

link_language = meson.get_external_property('link_language', 'cpp')
add_languages('cpp', required: true, native: false)

###########################################
# Target-specific checks and dependencies #
###########################################

multiprocess_allowed = not get_option('multiprocess').disabled()

m = cc.find_library('m', required: false)
util = cc.find_library('util', required: false)
winmm = []
socket = []
version_res = []
coref = []
iokit = []
emulator_link_args = []
hvf = not_found

accelerators = []
tcg_arch = config_host['ARCH']
if not get_option('tcg').disabled()
  add_project_arguments('-iquote', meson.current_source_dir() / 'tcg' / tcg_arch,
                        language: ['c', 'cpp', 'objc'])

  accelerators += 'CONFIG_TCG'
  config_host += { 'CONFIG_TCG': 'y' }
endif

################
# Dependencies #
################

# The path to glib.h is added to all compilation commands.  This was
# grandfathered in from the QEMU Makefiles.
add_project_arguments(config_host['GLIB_CFLAGS'].split(),
                      native: false, language: ['c', 'cpp', 'objc'])
glib = declare_dependency(compile_args: config_host['GLIB_CFLAGS'].split(),
                          link_args: config_host['GLIB_LIBS'].split())
# override glib dep with the configure results (for subprojects)
meson.override_dependency('glib-2.0', glib)

gnutls = not_found
if 'CONFIG_GNUTLS' in config_host
  gnutls = declare_dependency(compile_args: config_host['GNUTLS_CFLAGS'].split(),
                              link_args: config_host['GNUTLS_LIBS'].split())
endif

libattr_test = '''
  #include <stddef.h>
  #include <sys/types.h>
  #ifdef CONFIG_LIBATTR
  #include <attr/xattr.h>
  #else
  #include <sys/xattr.h>
  #endif
  int main(void) { getxattr(NULL, NULL, NULL, 0); setxattr(NULL, NULL, NULL, 0, 0); return 0; }'''

libattr = not_found
have_old_libattr = false
if not get_option('attr').disabled()
  if cc.links(libattr_test)
    libattr = declare_dependency()
  else
    libattr = cc.find_library('attr', has_headers: ['attr/xattr.h'],
                              required: get_option('attr'),
                              kwargs: static_kwargs)
    if libattr.found() and not \
      cc.links(libattr_test, dependencies: libattr, args: '-DCONFIG_LIBATTR')
      libattr = not_found
      if get_option('attr').enabled()
        error('could not link libattr')
      else
        warning('could not link libattr, disabling')
      endif
    else
      have_old_libattr = libattr.found()
    endif
  endif
endif

rt = cc.find_library('rt', required: false)
libdl = cc.find_library('dl', required: true)
keyutils = dependency('libkeyutils', required: false,
                      method: 'pkg-config', kwargs: static_kwargs)

has_gettid = cc.has_function('gettid')

# Malloc tests

malloc = []
if get_option('malloc') == 'system'
  has_malloc_trim = \
    not get_option('malloc_trim').disabled() and \
    cc.links('''#include <malloc.h>
                int main(void) { malloc_trim(0); return 0; }''')
else
  has_malloc_trim = false
  malloc = cc.find_library(get_option('malloc'), required: true)
endif
if not has_malloc_trim and get_option('malloc_trim').enabled()
  if get_option('malloc') == 'system'
    error('malloc_trim not available on this platform.')
  else
    error('malloc_trim not available with non-libc memory allocator')
  endif
endif

# Check whether the glibc provides statx()

statx_test = '''
  #ifndef _GNU_SOURCE
  #define _GNU_SOURCE
  #endif
  #include <sys/stat.h>
  int main(void) {
    struct statx statxbuf;
    statx(0, "", 0, STATX_BASIC_STATS, &statxbuf);
    return 0;
  }'''

has_statx = cc.links(statx_test)

#################
# config-host.h #
#################

config_host_data.set_quoted('CONFIG_BINDIR', get_option('prefix') / get_option('bindir'))
config_host_data.set_quoted('CONFIG_PREFIX', get_option('prefix'))
config_host_data.set_quoted('CONFIG_QEMU_HELPERDIR', get_option('prefix') / get_option('libexecdir'))
config_host_data.set_quoted('CONFIG_QEMU_LOCALEDIR', get_option('prefix') / get_option('localedir'))
config_host_data.set_quoted('CONFIG_QEMU_LOCALSTATEDIR', get_option('prefix') / get_option('localstatedir'))

config_host_data.set('CONFIG_ATTR', libattr.found())
config_host_data.set('CONFIG_LIBATTR', have_old_libattr)
config_host_data.set('CONFIG_GETTID', has_gettid)
config_host_data.set('CONFIG_MALLOC_TRIM', has_malloc_trim)

config_host_data.set('HAVE_LIBDRM_H', cc.has_header('libdrm/drm.h'))
config_host_data.set('HAVE_DRM_H', cc.has_header('drm/drm.h'))

config_host_data.set('CONFIG_PREADV', cc.has_function('preadv', prefix: '#include <sys/uio.h>'))

ignored = ['CONFIG_QEMU_INTERP_PREFIX'] # actually per-target
strings = ['HOST_DSOSUF', 'CONFIG_IASL']
foreach k, v: config_host
  if ignored.contains(k)
    # do nothing
  elif k == 'ARCH'
    config_host_data.set('HOST_' + v.to_upper(), 1)
  elif strings.contains(k)
    if not k.startswith('CONFIG_')
      k = 'CONFIG_' + k.to_upper()
    endif
    config_host_data.set_quoted(k, v)
  elif k.startswith('CONFIG_') or k.startswith('HAVE_') or k.startswith('HOST_')
    config_host_data.set(k, v == 'y' ? 1 : v)
  endif
endforeach

########################
# Target configuration #
########################

config_all = {}
config_all_disas = {}
config_target_h = {}
config_target_mak = {}

disassemblers = {
  'i386' : ['CONFIG_I386_DIS'],
  'x86_64' : ['CONFIG_I386_DIS'],
  'mips' : ['CONFIG_MIPS_DIS'],
  'loongarch' : ['CONFIG_LOONGARCH_DIS']
}

ignored = [ 'TARGET_XML_FILES', 'TARGET_ABI_DIR', 'TARGET_ARCH' ]

actual_target_dirs = []
foreach target : target_dirs
  config_target = { 'TARGET_NAME': target.split('-')[0] }
  config_target += { 'CONFIG_LINUX_USER': 'y' }
  config_target += {
    'CONFIG_USER_ONLY': 'y',
    'CONFIG_QEMU_INTERP_PREFIX':
      config_host['CONFIG_QEMU_INTERP_PREFIX'].format(config_target['TARGET_NAME'])
  }

  actual_target_dirs += target
  config_target += keyval.load('default-configs/targets' / target + '.mak')
  config_target += { 'TARGET_' + config_target['TARGET_ARCH'].to_upper(): 'y' }

  # Add default keys
  if 'TARGET_BASE_ARCH' not in config_target
    config_target += {'TARGET_BASE_ARCH': config_target['TARGET_ARCH']}
  endif
  if 'TARGET_ABI_DIR' not in config_target
    config_target += {'TARGET_ABI_DIR': config_target['TARGET_ARCH']}
  endif

  foreach k, v: disassemblers
    if config_host['ARCH'].startswith(k) or config_target['TARGET_BASE_ARCH'].startswith(k)
      foreach sym: v
        config_target += { sym: 'y' }
        config_all_disas += { sym: 'y' }
      endforeach
    endif
  endforeach

  config_target_data = configuration_data()
  foreach k, v: config_target
    if not k.startswith('TARGET_') and not k.startswith('CONFIG_')
      # do nothing
    elif ignored.contains(k)
      # do nothing
    elif k == 'TARGET_BASE_ARCH'
      # Note that TARGET_BASE_ARCH ends up in config-target.h but it is
      # not used to select files from sourcesets.
      config_target_data.set('TARGET_' + v.to_upper(), 1)
    elif k == 'TARGET_NAME' or k == 'CONFIG_QEMU_INTERP_PREFIX'
      config_target_data.set_quoted(k, v)
    elif v == 'y'
      config_target_data.set(k, 1)
    else
      config_target_data.set(k, v)
    endif
  endforeach
  config_target_h += {target: configure_file(output: target + '-config-target.h',
                                               configuration: config_target_data)}

  config_target_mak += {target: config_target}
endforeach
target_dirs = actual_target_dirs

# This configuration is used to build files that are shared by
# multiple binaries, and then extracted out of the "common"
# static_library target.
#
# We do not use all_sources()/all_dependencies(), because it would
# build literally all source files, including devices only used by
# targets that are not built for this compilation.  The CONFIG_ALL
# pseudo symbol replaces it.

config_all += config_host
config_all += config_all_disas
config_all += {
  'CONFIG_USER_ONLY': true,
  'CONFIG_ALL': true,
}

#####################
# Generated sources #
#####################

genh += configure_file(output: 'config-host.h', configuration: config_host_data)

hxtool = find_program('scripts/hxtool')
qapi_gen = find_program('scripts/qapi-gen.py')
qapi_gen_depends = [ meson.source_root() / 'scripts/qapi/__init__.py',
                     meson.source_root() / 'scripts/qapi/commands.py',
                     meson.source_root() / 'scripts/qapi/common.py',
                     meson.source_root() / 'scripts/qapi/error.py',
                     meson.source_root() / 'scripts/qapi/events.py',
                     meson.source_root() / 'scripts/qapi/expr.py',
                     meson.source_root() / 'scripts/qapi/gen.py',
                     meson.source_root() / 'scripts/qapi/introspect.py',
                     meson.source_root() / 'scripts/qapi/parser.py',
                     meson.source_root() / 'scripts/qapi/schema.py',
                     meson.source_root() / 'scripts/qapi/source.py',
                     meson.source_root() / 'scripts/qapi/types.py',
                     meson.source_root() / 'scripts/qapi/visit.py',
                     meson.source_root() / 'scripts/qapi/common.py',
                     meson.source_root() / 'scripts/qapi-gen.py'
]

tracetool = [
  python, files('scripts/tracetool.py'),
   '--backend=' + config_host['TRACE_BACKENDS']
]
tracetool_depends = files(
  'scripts/tracetool/backend/log.py',
  'scripts/tracetool/backend/__init__.py',
  'scripts/tracetool/backend/dtrace.py',
  'scripts/tracetool/backend/ftrace.py',
  'scripts/tracetool/backend/simple.py',
  'scripts/tracetool/backend/syslog.py',
  'scripts/tracetool/backend/ust.py',
  'scripts/tracetool/format/tcg_h.py',
  'scripts/tracetool/format/ust_events_c.py',
  'scripts/tracetool/format/ust_events_h.py',
  'scripts/tracetool/format/__init__.py',
  'scripts/tracetool/format/d.py',
  'scripts/tracetool/format/tcg_helper_c.py',
  'scripts/tracetool/format/simpletrace_stap.py',
  'scripts/tracetool/format/c.py',
  'scripts/tracetool/format/h.py',
  'scripts/tracetool/format/tcg_helper_h.py',
  'scripts/tracetool/format/log_stap.py',
  'scripts/tracetool/format/stap.py',
  'scripts/tracetool/format/tcg_helper_wrapper_h.py',
  'scripts/tracetool/__init__.py',
  'scripts/tracetool/transform.py',
  'scripts/tracetool/vcpu.py'
)

###################
# Collect sources #
###################

common_ss = ss.source_set()
crypto_ss = ss.source_set()
linux_user_ss = ss.source_set()
qmp_ss = ss.source_set()
qom_ss = ss.source_set()
specific_ss = ss.source_set()
stub_ss = ss.source_set()
trace_ss = ss.source_set()
user_ss = ss.source_set()
util_ss = ss.source_set()

hw_arch = {}
target_arch = {}

###############
# Trace files #
###############

# TODO: add each directory to the subdirs from its own meson.build, once
# we have those
trace_events_subdirs = [
  'crypto',
  'qapi',
  'qom',
  'util',
]
trace_events_subdirs += [ 'linux-user' ]
trace_events_subdirs += [
  'accel/tcg',
  'hw/core',
  'target/i386',
]

if 'CONFIG_LATX' in config_host
  message(config_host['ARCH'])
  if config_host['ARCH'] in ['loongarch64', 'loongarch']
    trace_events_subdirs += ['target/i386/latx']
  endif
endif

subdir('qapi')
subdir('qobject')
subdir('stubs')
subdir('trace')
subdir('util')
subdir('qom')
subdir('crypto')

stub_ss = stub_ss.apply(config_all, strict: false)

util_ss.add_all(trace_ss)
util_ss = util_ss.apply(config_all, strict: false)
libqemuutil = static_library('qemuutil',
                             sources: util_ss.sources() + stub_ss.sources() + genh,
                             dependencies: [util_ss.dependencies(), m, glib, socket, malloc])
qemuutil = declare_dependency(link_with: libqemuutil,
                              sources: genh + version_res)

decodetree = generator(find_program('scripts/decodetree.py'),
                       output: 'decode-@BASENAME@.c.inc',
                       arguments: ['@INPUT@', '@EXTRA_ARGS@', '-o', '@OUTPUT@'])
subdir('target')

common_ss.add(files('cpus-common.c'))

specific_ss.add(files('cpu.c', 'disas.c', 'gdbstub.c'))
specific_ss.add(when: 'CONFIG_TCG', if_true: files(
  'tcg/optimize.c',
  'tcg/tcg-common.c',
  'tcg/tcg-op-gvec.c',
  'tcg/tcg-op-vec.c',
  'tcg/tcg-op.c',
  'tcg/tcg.c',
))
specific_ss.add(when: 'CONFIG_TCG_INTERPRETER', if_true: files('tcg/tci.c'))

# Work around a gcc bug/misfeature wherein constant propagation looks
# through an alias:
#   https://gcc.gnu.org/bugzilla/show_bug.cgi?id=99696
# to guess that a const variable is always zero.  Without lto, this is
# impossible, as the alias is restricted to page-vary-common.c.  Indeed,
# without lto, not even the alias is required -- we simply use different
# declarations in different compilation units.
pagevary = files('page-vary-common.c')
if get_option('b_lto')
  pagevary_flags = ['-fno-lto']
  if get_option('cfi')
    pagevary_flags += '-fno-sanitize=cfi-icall'
  endif
  pagevary = static_library('page-vary-common', sources: pagevary,
                            c_args: pagevary_flags)
  pagevary = declare_dependency(link_with: pagevary)
endif
common_ss.add(pagevary)
specific_ss.add(files('page-vary.c'))

subdir('disas')
subdir('hw')
subdir('fpu')
subdir('accel')
subdir('plugins')
subdir('linux-user')

linux_user_ss.add(files('gdbstub.c', 'thunk.c'))
specific_ss.add_all(when: 'CONFIG_LINUX_USER', if_true: linux_user_ss)

########################
# Library dependencies #
########################

nm = find_program('nm')
qom_ss = qom_ss.apply(config_host, strict: false)
libqom = static_library('qom', qom_ss.sources() + genh,
                        dependencies: [qom_ss.dependencies()],
                        name_suffix: 'fa')

qom = declare_dependency(link_whole: libqom)

libhwcore = static_library('hwcore', sources: hwcore_files + genh,
                           name_suffix: 'fa',
                           build_by_default: false)
hwcore = declare_dependency(link_whole: libhwcore)
common_ss.add(hwcore)

###########
# Targets #
###########

common_ss.add(qom, qemuutil)

common_ss.add_all(when: 'CONFIG_USER_ONLY', if_true: user_ss)

common_all = common_ss.apply(config_all, strict: false)
common_all = static_library('common',
                            build_by_default: false,
                            sources: common_all.sources() + genh,
                            dependencies: common_all.dependencies(),
                            name_suffix: 'fa')

feature_to_c = find_program('scripts/feature_to_c.sh')

emulators = {}
foreach target : target_dirs
  config_target = config_target_mak[target]
  target_name = config_target['TARGET_NAME']
  arch = config_target['TARGET_BASE_ARCH']
  arch_srcs = [config_target_h[target]]
  arch_deps = []
  c_args = ['-DNEED_CPU_H',
            '-DCONFIG_TARGET="@0@-config-target.h"'.format(target),
            '-DCONFIG_DEVICES="@0@-config-devices.h"'.format(target)]
  link_args = emulator_link_args

  config_target += config_host
  target_inc = [include_directories('target' / config_target['TARGET_BASE_ARCH'])]
    target_inc += include_directories('linux-headers', is_system: true)
    abi = config_target['TARGET_ABI_DIR']
    target_type='user'
    qemu_target_name = 'qemu-' + target_name
      base_dir = 'linux-user'
      target_inc += include_directories('linux-user/host/' / config_host['ARCH'])
    target_inc += include_directories(
      base_dir,
      base_dir / abi,
    )
      dir = base_dir / abi
      arch_srcs += files(dir / 'signal.c', dir / 'cpu_loop.c')
      if config_target.has_key('TARGET_SYSTBL_ABI')
        arch_srcs += \
          syscall_nr_generators[abi].process(base_dir / abi / config_target['TARGET_SYSTBL'],
                                             extra_args : config_target['TARGET_SYSTBL_ABI'])
      endif
    if 'CONFIG_LATX' in config_host
      if config_host['ARCH'] in ['loongarch64', 'loongarch']
        target_inc += include_directories('target/i386/latx/include')
      endif
    endif

  t = target_arch[arch].apply(config_target, strict: false)
  arch_srcs += t.sources()
  arch_deps += t.dependencies()

  target_common = common_ss.apply(config_target, strict: false)
  objects = common_all.extract_objects(target_common.sources())
  deps = target_common.dependencies()

  target_specific = specific_ss.apply(config_target, strict: false)
  arch_srcs += target_specific.sources()
  arch_deps += target_specific.dependencies()

  lib = static_library('qemu-' + target,
                 sources: arch_srcs + genh,
                 dependencies: arch_deps,
                 objects: objects,
                 include_directories: target_inc,
                 c_args: c_args,
                 build_by_default: false,
                 name_suffix: 'fa')

    execs = [{
      'name': 'latx-' + target_name,
      'gui': false,
      'sources': [],
      'dependencies': []
    }]
  foreach exe: execs
    exe_name = exe['name']

    emulator = executable(exe_name, exe['sources'],
               install: true,
               c_args: c_args,
               dependencies: arch_deps + deps + exe['dependencies'],
               objects: lib.extract_all_objects(recursive: true),
               link_language: link_language,
               link_depends: exe.get('link_depends', []),
               link_args: link_args,
               gui_app: exe['gui'])

    emulators += {exe['name']: emulator}

  endforeach
endforeach

# Other build targets

if 'CONFIG_PLUGIN' in config_host
  install_headers('include/qemu/qemu-plugin.h')
endif

subdir('scripts')
subdir('docs')

#########################
# Configuration summary #
#########################

# Directories
summary_info = {}
summary_info += {'Install prefix':    get_option('prefix')}
summary_info += {'binary directory':  get_option('bindir')}
summary_info += {'library directory': get_option('libdir')}
summary_info += {'libexec directory': get_option('libexecdir')}
summary_info += {'include directory': get_option('includedir')}
summary_info += {'config directory':  get_option('sysconfdir')}
summary_info += {'local state directory': get_option('localstatedir')}
summary_info += {'Doc directory':     get_option('docdir')}
summary_info += {'Build directory':   meson.current_build_dir()}
summary_info += {'Source path':       meson.current_source_dir()}
summary_info += {'GIT submodules':    config_host['GIT_SUBMODULES']}
summary(summary_info, bool_yn: true, section: 'Directories')

# Host binaries
summary_info = {}
summary_info += {'git':               config_host['GIT']}
summary_info += {'make':              config_host['MAKE']}
summary_info += {'python':            '@0@ (version: @1@)'.format(python.full_path(), python.language_version())}
summary(summary_info, bool_yn: true, section: 'Host binaries')

# Configurable features
summary_info = {}
summary_info += {'Documentation':     build_docs}
summary_info += {'user-mode emulation': true}
summary_info += {'plugin support':    config_host.has_key('CONFIG_PLUGIN')}
summary_info += {'QOM debugging':     config_host.has_key('CONFIG_QOM_CAST_DEBUG')}
summary(summary_info, bool_yn: true, section: 'Configurable features')

# Compilation information
summary_info = {}
summary_info += {'host CPU':          cpu}
summary_info += {'host endianness':   build_machine.endian()}
summary_info += {'C compiler':        meson.get_compiler('c').cmd_array()[0]}
summary_info += {'Host C compiler':   meson.get_compiler('c', native: true).cmd_array()[0]}
if link_language == 'cpp'
  summary_info += {'C++ compiler':      meson.get_compiler('cpp').cmd_array()[0]}
else
  summary_info += {'C++ compiler':      false}
endif
summary_info += {'ARFLAGS':           config_host['ARFLAGS']}
summary_info += {'CFLAGS':            ' '.join(get_option('c_args')
                                               + ['-O' + get_option('optimization')]
                                               + (get_option('debug') ? ['-g'] : []))}
if link_language == 'cpp'
  summary_info += {'CXXFLAGS':        ' '.join(get_option('cpp_args')
                                               + ['-O' + get_option('optimization')]
                                               + (get_option('debug') ? ['-g'] : []))}
endif
link_args = get_option(link_language + '_link_args')
if link_args.length() > 0
  summary_info += {'LDFLAGS':         ' '.join(link_args)}
endif
summary_info += {'QEMU_CFLAGS':       config_host['QEMU_CFLAGS']}
summary_info += {'QEMU_LDFLAGS':      config_host['QEMU_LDFLAGS']}
summary_info += {'profiler':          config_host.has_key('CONFIG_PROFILER')}
summary_info += {'latx':              config_host.has_key('CONFIG_LATX')}
summary_info += {'new-world':         config_host.has_key('CONFIG_LOONGARCH_NEW_WORLD')}
summary_info += {'link-time optimization (LTO)': get_option('b_lto')}
summary_info += {'PIE':               get_option('b_pie')}
summary_info += {'static build':      config_host.has_key('CONFIG_STATIC')}
summary_info += {'malloc trim support': has_malloc_trim}
summary_info += {'membarrier':        config_host.has_key('CONFIG_MEMBARRIER')}
summary_info += {'preadv support':    config_host_data.get('CONFIG_PREADV')}
summary_info += {'fdatasync':         config_host.has_key('CONFIG_FDATASYNC')}
summary_info += {'madvise':           config_host.has_key('CONFIG_MADVISE')}
summary_info += {'posix_madvise':     config_host.has_key('CONFIG_POSIX_MADVISE')}
summary_info += {'posix_memalign':    config_host.has_key('CONFIG_POSIX_MEMALIGN')}
summary_info += {'debug stack usage': config_host.has_key('CONFIG_DEBUG_STACK_USAGE')}
summary_info += {'mutex debugging':   config_host.has_key('CONFIG_DEBUG_MUTEX')}
summary_info += {'memory allocator':  get_option('malloc')}
summary_info += {'avx2 optimization': config_host.has_key('CONFIG_AVX2_OPT')}
summary_info += {'avx512f optimization': config_host.has_key('CONFIG_AVX512F_OPT')}
summary_info += {'gprof enabled':     config_host.has_key('CONFIG_GPROF')}
summary_info += {'gcov':              get_option('b_coverage')}
summary_info += {'thread sanitizer':  config_host.has_key('CONFIG_TSAN')}
summary_info += {'CFI support':       get_option('cfi')}
if get_option('cfi')
  summary_info += {'CFI debug support': get_option('cfi_debug')}
endif
summary_info += {'strip binaries':    get_option('strip')}

summary(summary_info, bool_yn: true, section: 'Compilation')

# Targets and accelerators
summary_info = {}
summary_info += {'TCG support':       config_all.has_key('CONFIG_TCG')}
if config_all.has_key('CONFIG_TCG')
  if get_option('tcg_interpreter')
    summary_info += {'TCG backend':   'TCI (TCG with bytecode interpreter, experimental and slow)'}
  else
    summary_info += {'TCG backend':   'native (@0@)'.format(cpu)}
  endif
  summary_info += {'TCG debug enabled': config_host.has_key('CONFIG_DEBUG_TCG')}
endif
summary_info += {'target list':       ' '.join(target_dirs)}
summary(summary_info, bool_yn: true, section: 'Targets and accelerators')

# Crypto
summary_info = {}
summary_info += {'TLS priority':      config_host['CONFIG_TLS_PRIORITY']}
summary_info += {'GNUTLS support':    config_host.has_key('CONFIG_GNUTLS')}
# TODO: add back version
summary_info += {'libgcrypt':         config_host.has_key('CONFIG_GCRYPT')}
if config_host.has_key('CONFIG_GCRYPT')
   summary_info += {'  hmac':            config_host.has_key('CONFIG_GCRYPT_HMAC')}
   summary_info += {'  XTS':             not config_host.has_key('CONFIG_QEMU_PRIVATE_XTS')}
endif
# TODO: add back version
summary_info += {'nettle':            config_host.has_key('CONFIG_NETTLE')}
if config_host.has_key('CONFIG_NETTLE')
   summary_info += {'  XTS':             not config_host.has_key('CONFIG_QEMU_PRIVATE_XTS')}
endif
summary_info += {'crypto afalg':      config_host.has_key('CONFIG_AF_ALG')}
summary(summary_info, bool_yn: true, section: 'Crypto')

# Libraries
summary_info = {}
# TODO: add back version
summary_info += {'libtasn1':          config_host.has_key('CONFIG_TASN1')}
summary_info += {'PAM':               config_host.has_key('CONFIG_AUTH_PAM')}
summary_info += {'ATTR/XATTR support': libattr.found()}
summary(summary_info, bool_yn: true, section: 'Dependencies')
