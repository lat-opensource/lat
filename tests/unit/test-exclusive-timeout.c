#include "qemu/osdep.h"
#include <glib.h>

#include "exec/cpu-common.h"
#include "hw/core/cpu.h"

void qemu_cpu_kick(CPUState *cpu)
{
    (void)cpu;
}

bool qemu_cpu_is_self(CPUState *cpu)
{
    return cpu == current_cpu;
}

static void test_timeout_cancellation_allows_a_retry(void)
{
    CPUState current = { .cpu_index = 0 };
    CPUState blocked = { .cpu_index = 1, .running = true };

    qemu_init_cpu_list();
    cpu_list_add(&current);
    cpu_list_add(&blocked);
    current_cpu = &current;

    g_assert_false(start_exclusive_timeout(0));
    g_assert_cmpuint(current.exclusive_context_count, ==, 0);
    g_assert_false(blocked.has_waiter);

    g_assert_false(start_exclusive_timeout(1));
    g_assert_cmpuint(current.exclusive_context_count, ==, 0);
    g_assert_false(blocked.has_waiter);

    blocked.running = false;
    g_assert_true(start_exclusive_timeout(100));
    g_assert_cmpuint(current.exclusive_context_count, ==, 1);
    end_exclusive();

    g_assert_cmpuint(current.exclusive_context_count, ==, 0);
    cpu_list_remove(&blocked);
    cpu_list_remove(&current);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/cpu/exclusive-timeout/cancel-and-retry",
                    test_timeout_cancellation_allows_a_retry);
    return g_test_run();
}
