/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <pthread.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>
typedef unsigned (*write_callback)(void *, const unsigned char *, unsigned);
extern unsigned cairo_surface_write_to_png_stream(void *, write_callback, void *);
struct request { void *surface; write_callback callback; void *closure; unsigned result; };
static void *write_thread(void *opaque)
{
    struct request *request = opaque;
    fprintf(stderr, "NATIVE_CAIRO_THREAD=%ld\n", syscall(SYS_gettid));
    request->result = cairo_surface_write_to_png_stream(request->surface,
                                                       request->callback, request->closure);
    return NULL;
}
unsigned latx_test_cairo_thread_write(void *surface, write_callback callback, void *closure)
{
    struct request request = { surface, callback, closure, 1 };
    pthread_t thread;
    if (pthread_create(&thread, NULL, write_thread, &request)) return 1;
    if (pthread_join(thread, NULL)) return 1;
    return request.result;
}
