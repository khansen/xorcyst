/* Allocation injection exists only in this test translation unit. */
#include <assert.h>
#include <stdlib.h>

static size_t calls, fail_at, live;
void test_ir_allocation_failure(size_t at) { calls = 0; fail_at = at; }
size_t test_ir_allocation_calls(void) { return calls; }
size_t test_ir_live_allocations(void) { return live; }

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
#include "../ir/value.c"

/* Exercise otherwise unreachable version rollover without 2^64 API calls. */
void test_ir_version_rollover(ir_unit *u, int which)
{
    if (which == 0) u->bindings = UINT64_MAX;
    else if (which == 1) u->layout = UINT64_MAX;
    else { assert(which == 2); u->request = UINT64_MAX; }
}

/* Independent-query oracle: never shipped in the value module. */
void test_ir_clear_memos(ir_unit *u)
{
    size_t i;
    for (i = 0; i < u->node_count; ++i) {
        (*node_memo(u, node_at(u, i), 0)).flags &= (unsigned char)~MEMO_CACHED;
        (*node_memo(u, node_at(u, i), 1)).flags &= (unsigned char)~MEMO_CACHED;
    }
}

size_t test_ir_node_count(ir_unit *u) { return u->node_count; }

/* Pin the contract's no-span-lowering barriers, including cases where crossing
 * a barrier happens to compute the same number but loses the required graph. */
size_t test_ir_span_nodes(ir_unit *u)
{
    size_t i, count = 0;
    for (i = 0; i < u->node_count; ++i) count += (*node_at(u, i)).kind == SPAN;
    return count;
}
