test_elfload_pagesize = executable(
  'test-elfload-pagesize',
  files('test-elfload-pagesize.c'),
  '../../linux-user/elfload-pagesize.c',
  dependencies: glib,
)

test(
  'test-elfload-pagesize',
  test_elfload_pagesize,
  suite: 'lat-pr-fast',
)

test_exclusive_timeout = executable(
  'test-exclusive-timeout',
  files('test-exclusive-timeout.c'),
  '../../cpus-common.c',
  include_directories: include_directories('../..'),
  dependencies: [glib, qemuutil, qom],
)

test(
  'test-exclusive-timeout',
  test_exclusive_timeout,
  suite: 'lat-pr-fast',
)
