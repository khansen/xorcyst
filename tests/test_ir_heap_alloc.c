/* Only this translation unit injects mapping failures. */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include "ir/heap.h"
static size_t calls, fail_at, live;
void test_heap_failure(size_t at) { calls = 0; fail_at = at; }
size_t test_heap_calls(void) { return calls; }
size_t test_heap_mappings(void) { return live; }
static void *checked_mmap(void *p, size_t n, int protection, int flags, int fd, off_t offset)
{
    void *r;
    if (!calls)
    {
        const char *at = getenv("XASM_TEST_LINK_HEAP_FAIL");
        if (at) fail_at = (size_t)strtoull(at, NULL, 10);
    }
    if (++calls == fail_at) return MAP_FAILED;
    r = mmap(p, n, protection, flags, fd, offset);
    if (r != MAP_FAILED) ++live;
    return r;
}
static int checked_munmap(void *p, size_t n)
{
    int result = munmap(p, n);
    assert(result == 0 && live);
    --live;
    return result;
}
#define mmap checked_mmap
#define munmap checked_munmap
#define ir_heap_destroy destroy_heap_storage
#include "../ir/heap.c"
#undef ir_heap_destroy
void ir_heap_destroy(ir_heap *heap)
{
    /* Bulk arena reclamation must not hide a missed object/buffer release. */
    assert(!heap->live && !heap->bytes);
    destroy_heap_storage(heap);
}
