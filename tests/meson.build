if not get_option('tests').enabled()
  subdir_done()
endif

subdir('unit')

if host_machine.system() == 'linux'
  test(
    'test-latu-runtime-manager',
    find_program('runtime/test-latu-runtime-manager.sh'),
    args: [latu_runtime_manager.full_path()],
    depends: latu_runtime_manager,
    protocol: 'exitcode',
    suite: 'lat-pr-fast',
  )
endif

subdir('integration')
