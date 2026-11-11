# Cross-page TU remains opt-in through AOT=3 inside these runtime runners.
foreach tu_target : [['i386', '32'], ['x86_64', '64']]
  if tu_target[0] + '-linux-user' in target_dirs
    tu_cases = ['boundary', 'mode-cache', 'hash-cache', 'lifecycle', 'signal-fault']
    if tu_target[0] == 'i386'
      tu_cases += ['late-thunk']
    endif
    foreach tu_case : tu_cases
      latx_integration_tests += [{
        'name': 'test-tu-' + tu_case + '-' + tu_target[0],
        'runner': find_program('../../../latx/test-tu-runtime.sh'),
        'args': [emulators['latx-' + tu_target[0]], tu_target[1], tu_case],
        'timeout': 360,
      }]
    endforeach
  endif
endforeach
