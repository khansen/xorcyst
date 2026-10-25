/* Internal, optional link-lifetime allocator. NULL selects the C allocator.
 * A heap must outlive all of its objects; pointers never cross heap domains.
 * Records have stable addresses until freed, and their blocks are reclaimed
 * together. Ordinary allocations support individual free/reallocation. */
#ifndef XORCYST_V2_HEAP_H
#define XORCYST_V2_HEAP_H
#include <stddef.h>
#include <stdlib.h>
typedef struct v2_heap {
    void *small, *records, *large;
    void *available[8];
    size_t live, bytes;
} v2_heap;
void *v2_heap_take(v2_heap *, size_t);
void *v2_heap_record(v2_heap *, size_t);
void *v2_heap_resize(v2_heap *, void *, size_t);
void v2_heap_release(v2_heap *, void *);
void v2_heap_destroy(v2_heap *);
static inline void *v2_malloc(v2_heap *h, size_t n)
{ return h ? v2_heap_take(h, n) : malloc(n); }
static inline void *v2_records(v2_heap *h, size_t n)
{ return h ? v2_heap_record(h, n) : malloc(n); }
static inline void *v2_calloc(v2_heap *h, size_t n, size_t width)
{
    size_t i, bytes;
    unsigned char *p;
    if (!h) return calloc(n, width);
    if (width && n > (size_t)-1 / width) return NULL;
    bytes = n * width;
    p = v2_heap_take(h, bytes);
    if (p) for (i = 0; i < bytes; i++) p[i] = 0;
    return p;
}
static inline void *v2_realloc(v2_heap *h, void *p, size_t n)
{ return h ? v2_heap_resize(h, p, n) : realloc(p, n); }
static inline void v2_free(v2_heap *h, void *p)
{ if (h) v2_heap_release(h, p); else free(p); }
#endif
