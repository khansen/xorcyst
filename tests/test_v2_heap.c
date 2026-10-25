#include "v2/heap.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
void test_heap_failure(size_t);
size_t test_heap_calls(void);
size_t test_heap_mappings(void);
static int run(void)
{
    v2_heap h[2] = {{0}, {0}};
    unsigned char *p[128] = {0};
    size_t n[128] = {0}, i, j;
    int ok = 0;
    /* All size classes, page/block boundaries, and records larger than a
     * block. Independent heaps are interleaved and destroyed separately. */
    for (i = 0; i < 128; i++) {
        n[i] = i < 32 ? i : i < 64 ? (size_t)1 << (i % 20) : i * 73;
        p[i] = i % 5 ? v2_malloc(&h[i % 2], n[i]) : v2_records(&h[i % 2], n[i]);
        if (!p[i]) goto done;
        assert((uintptr_t)p[i] % sizeof(void *) == 0);
        if (n[i]) memset(p[i], (int)i, n[i]);
    }
    for (i = 0; i < 128; i++) {
        size_t next_size = i % 2 ? n[i] / 2 + 1 : n[i] * 2 + 1;
        unsigned char *next = v2_realloc(&h[i % 2], p[i], next_size);
        /* Failed realloc preserves ownership and contents. */
        if (!next) {
            for (j = 0; j < n[i]; j++) assert(p[i][j] == (unsigned char)i);
            goto done;
        }
        p[i] = next;
        for (j = 0; j < (n[i] < next_size ? n[i] : next_size); j++)
            assert(p[i][j] == (unsigned char)i);
        n[i] = next_size;
    }
    assert(!v2_malloc(&h[0], SIZE_MAX));
    assert(!v2_records(&h[0], SIZE_MAX));
    assert(!v2_calloc(&h[0], SIZE_MAX, 2));
    assert(!v2_realloc(&h[0], p[0], SIZE_MAX));
    ok = 1;
done:
    for (i = 0; i < 128; i++) v2_free(&h[i % 2], p[i]);
    assert(!h[0].live && !h[0].bytes && !h[1].live && !h[1].bytes);
    v2_heap_destroy(&h[0]); v2_heap_destroy(&h[1]);
    assert(!test_heap_mappings());
    return ok;
}
int main(void)
{
    size_t i, count;
    test_heap_failure(0);
    assert(run()); count = test_heap_calls();
    for (i = 1; i <= count; i++) { test_heap_failure(i); assert(!run()); }
    /* Individually freed slots are reused; they do not inflate a live budget. */
    {
        v2_heap h = {0}; void *first = NULL; size_t calls;
        test_heap_failure(0);
        for (i = 0; i < 10000; i++) {
            void *p = v2_calloc(&h, 17, 1);
            assert(p && !memcmp(p, "\0\0\0\0", 4));
            if (!i) first = p; else assert(p == first);
            v2_free(&h, p); assert(!h.live && !h.bytes);
        }
        calls = test_heap_calls(); assert(calls == 1);
        v2_heap_destroy(&h); assert(!test_heap_mappings());
    }
    printf("%zu mapping failures preserved ownership\n", count);
    return 0;
}
