/* Pool small allocations and stable records; promptly unmap large temporaries.
 * This resource belongs to one link. There is no global allocator state. */
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE 1
#endif
#include "heap.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define IR_HEAP_ASAN 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#define IR_HEAP_ASAN 1
#endif
#ifdef IR_HEAP_ASAN
#include <sanitizer/asan_interface.h>
#define POISON(p,n) __asan_poison_memory_region((p),(n))
#define UNPOISON(p,n) __asan_unpoison_memory_region((p),(n))
#else
#define POISON(p,n) ((void)0)
#define UNPOISON(p,n) ((void)0)
#endif
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
typedef union header {
    struct { size_t size, storage; } value;
    union header *next;
    long double alignment;
    void *pointer;
} header;
typedef struct block {
    struct block *next, *previous;
    size_t mapped, used;
    header data[];
} block;
#define RECORD_STORAGE 8u
#define LARGE_STORAGE 9u
#define BLOCK_BYTES ((size_t)262144)
static block *new_block(size_t bytes)
{
    block *b;
    size_t page, total;
    long n = sysconf(_SC_PAGESIZE);
    if (n <= 0 || bytes > SIZE_MAX - sizeof(block)) return NULL;
    page = (size_t)n; total = sizeof(block) + bytes;
    if (total > SIZE_MAX - (page - 1)) return NULL;
    total = (total + page - 1) / page * page;
    b = mmap(NULL, total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (b == MAP_FAILED) return NULL;
    b->next = b->previous = NULL; b->mapped = total; b->used = 0;
    POISON(b->data, total - sizeof(block));
    return b;
}
static header *bump(void **head, size_t bytes)
{
    block *b = *head;
    header *p;
    if (!b || bytes > b->mapped - sizeof(block) - b->used) {
        b = new_block(bytes > BLOCK_BYTES ? bytes : BLOCK_BYTES);
        if (!b) return NULL;
        b->next = *head; *head = b;
    }
    p = (header *)((unsigned char *)b->data + b->used);
    b->used += bytes;
    return p;
}
static void *publish(ir_heap *heap, header *p, size_t n, size_t storage)
{
    UNPOISON(p, sizeof(*p) + n);
    p->value.size = n; p->value.storage = storage;
    ++heap->live; heap->bytes += n;
    return p + 1;
}
void *ir_heap_take(ir_heap *heap, size_t n)
{
    size_t slot = sizeof(header) * 2, k = 0;
    header *p;
    if (n > SIZE_MAX - sizeof(header) || n > SIZE_MAX - heap->bytes || heap->live == SIZE_MAX) return NULL;
    while (k < 8 && slot < sizeof(header) + n) { slot *= 2; ++k; }
    if (k < 8) {
        p = heap->available[k];
        if (p) heap->available[k] = p->next;
        else p = bump(&heap->small, slot);
        if (!p) return NULL;
        return publish(heap, p, n, k);
    } else {
        block *b = new_block(sizeof(header) + n);
        if (!b) return NULL;
        b->next = heap->large;
        if (b->next) b->next->previous = b;
        heap->large = b;
        return publish(heap, b->data, n, LARGE_STORAGE);
    }
}
void *ir_heap_record(ir_heap *heap, size_t n)
{
    size_t bytes;
    header *p;
    if (n > SIZE_MAX - 2 * sizeof(header) || n > SIZE_MAX - heap->bytes || heap->live == SIZE_MAX) return NULL;
    bytes = (sizeof(header) + n + sizeof(header) - 1) / sizeof(header) * sizeof(header);
    p = bump(&heap->records, bytes);
    return p ? publish(heap, p, n, RECORD_STORAGE) : NULL;
}
void ir_heap_release(ir_heap *heap, void *memory)
{
    header *p;
    size_t n, storage;
    if (!memory) return;
    p = (header *)memory - 1; n = p->value.size; storage = p->value.storage;
    assert(heap->live && heap->bytes >= n);
    --heap->live; heap->bytes -= n;
    POISON(memory, n);
    if (storage < 8) {
        p->next = heap->available[storage]; heap->available[storage] = p;
    } else if (storage == LARGE_STORAGE) {
        block *b = (block *)((unsigned char *)p - sizeof(block));
        if (b->previous) b->previous->next = b->next; else heap->large = b->next;
        if (b->next) b->next->previous = b->previous;
        munmap(b, b->mapped);
    } else assert(storage == RECORD_STORAGE);
}
void *ir_heap_resize(ir_heap *heap, void *old, size_t n)
{
    header *p;
    void *next;
    if (!old) return ir_heap_take(heap, n);
    if (!n) { ir_heap_release(heap, old); return NULL; }
    p = (header *)old - 1;
    next = ir_heap_take(heap, n);
    if (!next) return NULL;
    memcpy(next, old, n < p->value.size ? n : p->value.size);
    ir_heap_release(heap, old);
    return next;
}
void ir_heap_destroy(ir_heap *heap)
{
    void *heads[3] = {heap->small, heap->records, heap->large};
    size_t i;
    for (i = 0; i < 3; i++) {
        block *b = heads[i];
        while (b) { block *next = b->next; munmap(b, b->mapped); b = next; }
    }
    memset(heap, 0, sizeof(*heap));
}
