/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef void cairo_surface_t;
typedef unsigned int (*write_callback)(void *, const unsigned char *, unsigned);
extern cairo_surface_t *cairo_image_surface_create(int, int, int);
extern unsigned cairo_surface_status(cairo_surface_t *);
extern unsigned cairo_surface_write_to_png_stream(cairo_surface_t *, write_callback, void *);
extern void cairo_surface_destroy(cairo_surface_t *);

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL:%d: %s\n", __LINE__, #c); return 1; } } while (0)
static unsigned hits, nested_hits;
static size_t bytes;
static unsigned char signature[8];
static int guest_error, nested;
static cairo_surface_t *inner;

static unsigned inner_write(void *unused, const unsigned char *data, unsigned len)
{
    (void)unused; (void)data; (void)len;
    nested_hits++;
    return 0;
}
static unsigned write_data(void *unused, const unsigned char *data, unsigned len)
{
    (void)unused;
    hits++;
    if (guest_error) return 11; /* CAIRO_STATUS_WRITE_ERROR */
    if (nested && hits == 1 &&
        cairo_surface_write_to_png_stream(inner, inner_write, NULL) != 0) return 11;
    for (unsigned i = 0; i < len && bytes + i < sizeof(signature); i++) {
        signature[bytes + i] = data[i];
    }
    bytes += len;
    return 0;
}
int main(int argc, char **argv)
{
    CHECK(argc >= 2);
    const char *mode = argv[1];
    int failed_prepare = !strcmp(mode, "prepare-fail");
    unsigned iterations = argc > 2 ? strtoul(argv[2], NULL, 10) : 1;
    guest_error = !strcmp(mode, "guest-error");
    nested = !strcmp(mode, "nested");
    cairo_surface_t *surface = cairo_image_surface_create(0, 8, 8);
    CHECK(surface && cairo_surface_status(surface) == 0);
    if (nested) {
        inner = cairo_image_surface_create(0, 2, 2);
        CHECK(inner && cairo_surface_status(inner) == 0);
    }
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    unsigned status = 0;
    for (unsigned i = 0; i < iterations; i++) {
        status = cairo_surface_write_to_png_stream(surface, write_data, NULL);
        if (failed_prepare || guest_error || status) break;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    fprintf(stderr, "RESULT mode=%s status=%u hits=%u bytes=%zu nested=%u ns=%lld\n",
            mode, status, hits, bytes, nested_hits,
            (long long)(end.tv_sec - start.tv_sec) * 1000000000 + end.tv_nsec - start.tv_nsec);
    cairo_surface_destroy(surface);
    if (inner) cairo_surface_destroy(inner);
    if (failed_prepare) {
        CHECK(status != 0 && hits == 0 && bytes == 0);
    } else if (guest_error) {
        CHECK(status == 11 && hits == 1 && bytes == 0);
    } else {
        static const unsigned char png[8] = { 137,80,78,71,13,10,26,10 };
        CHECK(status == 0 && hits > 0 && bytes > 8 && !memcmp(signature, png, 8));
        CHECK(!nested || nested_hits > 0);
    }
    puts("PASS: Cairo callback execution/result boundary");
    return 0;
}
