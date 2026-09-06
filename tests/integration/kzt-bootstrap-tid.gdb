# SPDX-License-Identifier: GPL-2.0-or-later
# Explicit diagnostic regression. Requires a LATX binary with debug symbols.
# Invoke with the existing native-thread opt-in fixture and its environment.
set pagination off
set confirm off
set print thread-events off
set $bootstrap_hits = 0
break RunFunctionWithStateInternal if fnc == guest_allocate_tls_init
commands
  silent
  set $env = (CPUX86State *)lsenv->cpu_state
  set $snapshot = (kzt_guest_parent_tls_snapshot_t *)$env->kzt_guest_tls_parent_snapshot
  set $task = (TaskState *)((CPUState *)thread_cpu)->opaque
  set $tid = *(unsigned int *)($env->segs[4].base + $snapshot->tid_offset)
  if $tid != $task->ts_tid
    printf "FAIL: Guest loader entered with tid=%u, expected=%u\n", $tid, $task->ts_tid
    quit 1
  end
  set $bootstrap_hits = $bootstrap_hits + 1
  continue
end
run
if $bootstrap_hits == 0
  echo FAIL: Guest loader bootstrap breakpoint was not reached\n
  quit 1
end
if $_exitcode != 0
  echo FAIL: native-thread fixture failed\n
  quit 1
end
printf "PASS: %d Guest loader entries had initialized TIDs\n", $bootstrap_hits
