/* Allocation injection exists only in this test translation unit. */
#include <assert.h>
#include <stdlib.h>

static size_t calls, fail_at, live;
void test_v2_allocation_failure(size_t at) { calls = 0; fail_at = at; }
size_t test_v2_allocation_calls(void) { return calls; }
size_t test_v2_live_allocations(void) { return live; }

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

#define malloc checked_malloc
#define calloc checked_calloc
#define realloc checked_realloc
#define free checked_free
#include "../v2/value.c"

/* Exercise otherwise unreachable version rollover without 2^64 API calls. */
void test_v2_version_rollover(v2_unit *u, int which)
{
    if (which == 0) u->bindings = UINT64_MAX;
    else if (which == 1) u->layout = UINT64_MAX;
    else { assert(which == 2); u->request = UINT64_MAX; }
}

/* Independent-query oracle: never shipped in the value module. */
void test_v2_clear_memos(v2_unit *u)
{
    size_t i;
    for (i = 0; i < u->node_count; ++i)
        u->nodes[i].memo[0].cached = u->nodes[i].memo[1].cached = 0;
}

size_t test_v2_node_count(v2_unit *u) { return u->node_count; }

/* Pin the contract's no-span-lowering barriers, including cases where crossing
 * a barrier happens to compute the same number but loses the required graph. */
size_t test_v2_span_nodes(v2_unit *u)
{
    size_t i, count = 0;
    for (i = 0; i < u->node_count; ++i) count += u->nodes[i].kind == SPAN;
    return count;
}
