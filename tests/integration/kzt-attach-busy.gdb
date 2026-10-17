# SPDX-License-Identifier: GPL-2.0-or-later
# Requires a LATX debug-symbol build and the native-thread opt-in fixture.
set pagination off
set confirm off
set print thread-events off
set $injected = 0
set $mode = 0
break kzt_collect_guest_tls_objects if lsenv != 0 \
    && ($mode == 2 || $injected == 0) \
    && ((CPUX86State *)lsenv->cpu_state)->kzt_guest_tls_parent_snapshot != 0 \
    && ((CPUX86State *)lsenv->cpu_state)->kzt_guest_tls_allocation == 0
commands
 silent
 set $injected = $injected + 1
 if $mode == 1
  return (int)-1
 else
  return (int)1
 end
 continue
end

# One transient busy result must not drop a valid callback.
run
if $injected != 1 || $_exitcode != 0
 echo FAIL: attached callback did not recover from transient loader busy\n
 quit 1
end
echo PASS: attached callback retried transient loader busy\n

# A permanent error must not be retried into apparent success.
set $injected = 0
set $mode = 1
run
if $injected != 1 || $_exitcode == 0
 echo FAIL: permanent attachment failure was ignored\n
 quit 1
end
echo PASS: permanent attachment failure remains an error\n

# The fixture starts two native threads. Each has a bounded retry budget.
set $injected = 0
set $mode = 2
run
if $injected < 2 || $injected > 64 || $_exitcode == 0
 echo FAIL: persistent loader busy was ignored or exceeded the retry budget\n
 quit 1
end
echo PASS: persistent loader busy is bounded\n
