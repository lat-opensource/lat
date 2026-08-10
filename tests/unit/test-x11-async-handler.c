#include "qemu/osdep.h"
#include <glib.h>

#include "target/i386/latx/context/wrappedlibx11_async.h"

#define ASYNC_HANDLER(addr) \
    ((int (*)(void *, void *, char *, int, void *))(uintptr_t)(addr))

static void *bridge_handler(void *handler)
{
    if ((uintptr_t)handler == 0x1000) {
        return (void *)0xa000;
    }
    if ((uintptr_t)handler == 0x2000) {
        return (void *)0xb000;
    }
    g_assert_not_reached();
}

static void test_guest_handlers_are_bridged(void)
{
    latx_XInternalAsync third = {
        .next = NULL,
        .handler = ASYNC_HANDLER(0x9000),
    };
    latx_XInternalAsync second = {
        .next = &third,
        .handler = ASYNC_HANDLER(0x2000),
    };
    latx_XInternalAsync first = {
        .next = &second,
        .handler = ASYNC_HANDLER(0x1000),
    };

    latx_x11_bridge_async_handler_list(&first, 0x8000, bridge_handler);

    g_assert_cmphex((uintptr_t)first.handler, ==, 0xa000);
    g_assert_cmphex((uintptr_t)second.handler, ==, 0xb000);
    g_assert_cmphex((uintptr_t)third.handler, ==, 0x9000);
}

static void test_empty_handler_list_is_accepted(void)
{
    latx_x11_bridge_async_handler_list(NULL, 0x8000, bridge_handler);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/x11/async-handler/bridge-guest-handlers",
                    test_guest_handlers_are_bridged);
    g_test_add_func("/x11/async-handler/empty-list",
                    test_empty_handler_list_is_accepted);

    return g_test_run();
}
