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

python3 = find_program('python3', required: false)
if python3.found()
  latx_python_tests = [
    ['test-latx-ghbr-audit', '../scripts/test_latx_ghbr_audit.py'],
    ['test-latx-ghbr-conditional', '../scripts/test_latx_ghbr_conditional.py'],
    ['test-latx-ghbr-implicit', '../scripts/test_latx_ghbr_implicit.py'],
    ['test-latx-ghbr-stats', '../scripts/test_latx_ghbr_stats.py'],
    ['test-latx-ir2-zero-chain', '../scripts/test_latx_ir2_zero_chain.py'],
  ]
  foreach test_def : latx_python_tests
    test(
      test_def[0],
      python3,
      args: [files(test_def[1])],
      protocol: 'exitcode',
      suite: 'lat-pr-fast',
    )
  endforeach
endif

subdir('integration')
