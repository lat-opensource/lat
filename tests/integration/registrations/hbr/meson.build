if 'x86_64-linux-user' in target_dirs
  foreach hbr_fixture : [
    ['hbr-avx-shift-fixture', 'hbr-avx-shift-fixture.c'],
    ['hbr-edge-fixture', 'hbr-edge-fixture.c'],
    ['hbr-ghbr-div-fixture', 'hbr-ghbr-div-fixture.c'],
    ['hbr-known-zero64-fixture', 'hbr-known-zero64-fixture.c'],
    ['hbr-round-fixture', 'hbr-round-fixture.c'],
    ['hbr-zero-semantics-fixture', 'hbr-zero-semantics-fixture.c'],
  ]
    latx_integration_tests += [{
      'name': 'test-latx-' + hbr_fixture[0],
      'runner': find_program('../../test-latx-hbr-fixture.sh'),
      'args': [
        emulators['latx-x86_64'],
        project_source_root / 'target/i386/latx/optimization/tests' / hbr_fixture[1],
      ],
      'timeout': 120,
    }]
  endforeach
endif
