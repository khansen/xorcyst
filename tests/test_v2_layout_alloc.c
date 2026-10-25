/* Allocation injection exists only in this test translation unit. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

static size_t calls, fail_at, live;
void test_layout_allocation_failure(size_t at) { calls = 0; fail_at = at; }
size_t test_layout_allocation_calls(void) { return calls; }
size_t test_layout_live_allocations(void) { return live; }

static int fails(void) { return ++calls == fail_at; }
static void *checked_malloc(size_t size)
{
    void *p = fails() ? NULL : malloc(size);
    if (p) ++live;
    return p;
}
static void *checked_calloc(size_t count, size_t size)
{
    void *p = fails() ? NULL : calloc(count, size);
    if (p) ++live;
    return p;
}
static void *checked_realloc(void *old, size_t size)
{
    int was_null = old == NULL;
    void *p = fails() ? NULL : realloc(old, size);
    if (p && was_null) ++live;
    return p;
}
static void checked_free(void *p)
{
    if (p) { assert(live); --live; }
    free(p);
}
static size_t write_calls, write_failure;
void test_layout_write_failure(size_t at) { write_calls = 0; write_failure = at; }
size_t test_layout_write_calls(void) { return write_calls; }
static size_t checked_fwrite(const void *p, size_t size, size_t count, FILE *stream)
{
    if (++write_calls == write_failure) { errno = EIO; return 0; }
    return fwrite(p, size, count, stream);
}

#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#define free checked_free
#define fwrite checked_fwrite
#include "../v2/layout.c"
