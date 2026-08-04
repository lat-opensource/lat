if not get_option('tests').enabled()
  subdir_done()
endif

subdir('unit')

test(
  'test-latu-runtime-manager',
  find_program('runtime/test-latu-runtime-manager.sh'),
  protocol: 'exitcode',
  suite: 'lat-pr-fast',
)

latu_install_depends = []
foreach name, emulator : emulators
  latu_install_depends += emulator
endforeach

test(
  'test-latu-install',
  find_program('runtime/test-latu-install.sh'),
  args: [python.full_path(), meson.source_root() / 'meson' / 'meson.py',
         meson.build_root(), get_option('prefix')],
  depends: latu_install_depends,
  protocol: 'exitcode',
  suite: 'lat-pr-fast',
)

subdir('integration')
