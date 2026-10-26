/* Allocation injection exists only in this test translation unit. */
#include <assert.h>
#include <stdlib.h>

static size_t calls, fail_at, live;
void test_link_allocation_failure(size_t at) { calls = 0; fail_at = at; }
size_t test_link_allocation_calls(void) { return calls; }
size_t test_link_live_allocations(void) { return live; }

static int fails(void) { return ++calls == fail_at; }
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

#define calloc checked_calloc
#define realloc checked_realloc
#define free checked_free
#include "../ir/link.c"
