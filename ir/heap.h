/* Internal, optional link-lifetime allocator. NULL selects the C allocator.
 * A heap must outlive all of its objects; pointers never cross heap domains.
 * Records have stable addresses until freed, and their blocks are reclaimed
 * together. Ordinary allocations support individual free/reallocation. */
#ifndef XORCYST_IR_HEAP_H
#define XORCYST_IR_HEAP_H
#include <stddef.h>
#include <stdlib.h>
typedef struct ir_heap {
    void *small, *records, *large;
    void *available[8];
    size_t live, bytes;
} ir_heap;
void *ir_heap_take(ir_heap *, size_t);
void *ir_heap_record(ir_heap *, size_t);
void *ir_heap_resize(ir_heap *, void *, size_t);
void ir_heap_release(ir_heap *, void *);
void ir_heap_destroy(ir_heap *);
static inline void *ir_malloc(ir_heap *h, size_t n)
{ return h ? ir_heap_take(h, n) : malloc(n); }
static inline void *ir_records(ir_heap *h, size_t n)
{ return h ? ir_heap_record(h, n) : malloc(n); }
static inline void *ir_calloc(ir_heap *h, size_t n, size_t width)
{
    size_t i, bytes;
    unsigned char *p;
    if (!h) return calloc(n, width);
    if (width && n > (size_t)-1 / width) return NULL;
    bytes = n * width;
    p = ir_heap_take(h, bytes);
    if (p) for (i = 0; i < bytes; i++) p[i] = 0;
    return p;
}
static inline void *ir_realloc(ir_heap *h, void *p, size_t n)
{ return h ? ir_heap_resize(h, p, n) : realloc(p, n); }
static inline void ir_free(ir_heap *h, void *p)
{ if (h) ir_heap_release(h, p); else free(p); }
#endif
