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

if 'CONFIG_LATX' in config_host
  test_util_path = executable(
    'test-util-path',
    files('test-util-path.c'),
    dependencies: [glib, qemuutil],
    build_by_default: false,
  )

  test(
    'test-util-path',
    test_util_path,
    suite: 'lat-pr-fast',
  )
endif

if 'CONFIG_LATX' in config_host
  test_callback_args = executable(
    'test-callback-args',
    files(
      'test-callback-args.c',
      '../../target/i386/latx/context/callback-args.c',
    ),
    include_directories: include_directories('../..'),
    build_by_default: false,
  )

  test(
    'test-callback-args',
    test_callback_args,
    suite: 'lat-pr-fast',
  )
endif

test(
  'test-x11-synchronize-callback',
  python,
  args: [
    files('check-x11-synchronize-callback.py'),
    project_source_root / 'target/i386/latx/context/wrappedlibx11.c',
  ],
  suite: 'lat-pr-fast',
)

test(
  'test-xcb-image-wrappers',
  python,
  args: [
    files('check-xcb-image-wrappers.py'),
    project_source_root / 'target/i386/latx/include/wrappedlibxcbimage_private.h',
    project_source_root / 'target/i386/latx/include/wrapper.h',
    project_source_root / 'target/i386/latx/context/wrapper.c',
  ],
  suite: 'lat-pr-fast',
)

test(
  'test-x11-async-wrapper-routing',
  python,
  args: [
    files('check-x11-async-wrapper.py'),
    project_source_root / 'target/i386/latx/context/wrappedlibx11.c',
    project_source_root / 'target/i386/latx/include/wrappedlibx11_private.h',
    project_source_root / 'target/i386/latx/include/generated/wrappedlibx11types.h',
  ],
  suite: 'lat-pr-fast',
)

test_rcu_lazy_thread = executable(
  'test-rcu-lazy-thread',
  files('test-rcu-lazy-thread.c'),
  include_directories: include_directories('../..'),
  dependencies: [glib, qemuutil],
)

test(
  'test-rcu-lazy-thread',
  test_rcu_lazy_thread,
  suite: 'lat-pr-fast',
)

if 'CONFIG_LATX' in config_host
  test_x11_async_handler = executable(
    'test-x11-async-handler',
    files('test-x11-async-handler.c'),
    include_directories: include_directories('../..'),
    dependencies: dependency('threads'),
    build_by_default: false,
  )

  test(
    'test-x11-async-handler',
    test_x11_async_handler,
    suite: 'lat-pr-fast',
  )
endif

test_kzt_public_loader_observer = executable(
  'test-kzt-public-loader-observer',
  files(
    'kzt/test_kzt_public_loader_observer.c',
    '../../target/i386/latx/context/kzt_public_loader_observer.c',
  ),
  include_directories: include_directories(
    '../../target/i386/latx/include',
  ),
)

test(
  'test-kzt-public-loader-observer',
  test_kzt_public_loader_observer,
  suite: 'lat-pr-fast',
)

test_kzt_relocation_transaction = executable(
  'test-kzt-relocation-transaction',
  files(
    'kzt/test_kzt_relocation_transaction.c',
    '../../target/i386/latx/context/kzt_relocation_transaction.c',
  ),
  include_directories: include_directories(
    '../../target/i386/latx/include',
  ),
)

test(
  'test-kzt-relocation-transaction',
  test_kzt_relocation_transaction,
  suite: 'lat-pr-fast',
)

test_kzt_address_policy = executable(
  'test-kzt-address-policy',
  files('kzt/test_kzt_address_policy.c'),
  include_directories: include_directories(
    '../../target/i386/latx/include',
  ),
  build_by_default: false,
)

test(
  'test-kzt-address-policy',
  test_kzt_address_policy,
  suite: 'lat-pr-fast',
)
