#include "v2/link.h"
#include "v2/program.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define DECLARE(name)                                                                              \
    void test_##name##_allocation_failure(size_t);                                                 \
    size_t test_##name##_allocation_calls(void);                                                   \
    size_t test_##name##_live_allocations(void)
DECLARE(v2);
DECLARE(layout);
DECLARE(object);
DECLARE(program);
DECLARE(link);
void test_object_release(void *);

void test_heap_failure(size_t);
size_t test_heap_calls(void);
size_t test_heap_mappings(void);

static int run(int pooled)
{
    v2_heap heap = {0};
    v2_unit *u = NULL;
    v2_program *p = NULL;
    v2_link *l = NULL;
    v2_object *o = NULL, *decoded = NULL;
    unsigned char *bytes = NULL;
    const unsigned char *output;
    size_t length = 0;
    v2_id definition, operand, anchor, input;
    v2_location loc = v2_no_location();
    v2_object_error error;
    int ok = 0;
    u = v2_unit_create();
    if (!u)
        goto done;
    loc = (v2_location){v2_file(u, "input.asm"), 1, 1, 1, 8};
    p = v2_program_create(u);
    if (!p)
        goto done;
    definition = v2_definition(u, "Imported", loc);
    operand = v2_reference(u, definition, loc);
    if (!v2_program_instruction(p, V2_NONE, 0xa9, operand, 0, loc))
        goto done;
    anchor = v2_anchor(u, loc);
    if (!v2_program_ram_label(p, anchor, 4, 1, loc) ||
        !v2_program_ram_storage(p, V2_NONE, v2_extent_literal(u, 3, loc), loc))
        goto done;
    o = v2_program_object(p);
    if (!o || !v2_object_encode(o, &bytes, &length))
        goto done;
    decoded = v2_object_decode(bytes, length, &error);
    if (!decoded)
        goto done;
    /* Exercise both first-edit materialization and later table growth while
     * the appended/new blob borrows its source from the same decoded object. */
    if (!v2_object_append_bytes(decoded, 0, v2_object_bytes(decoded, 0, NULL), 1) ||
        v2_object_blob(decoded, v2_object_bytes(decoded, 0, NULL), 1) == V2_NONE)
        goto done;
    v2_object_destroy(decoded);
    decoded = v2_object_decode_owned(bytes, length, &error);
    bytes = NULL;
    if (!decoded)
        goto done;
    /* Root-first ordering forces the translator's temporary stack to grow.
     * Every failed growth must retain its old allocation for cleanup. */
    {
        uint32_t first = v2_object_count(decoded, V2_OBJ_EXPRESSIONS), i;
        uint32_t leaf[6] = {1, 1, 0, 0, 0, 0};
        for (i = 0; i < 40; i++) {
            uint32_t unary[6] = {16, first + i + 1, 0, 0, 0, 0};
            if (v2_object_add(decoded, V2_OBJ_EXPRESSIONS, unary) == V2_NONE) goto done;
        }
        if (v2_object_add(decoded, V2_OBJ_EXPRESSIONS, leaf) == V2_NONE) goto done;
    }
    v2_object_destroy(o);
    o = NULL;
    v2_program_destroy(p);
    p = NULL;
    v2_unit_destroy(u);
    u = NULL;
    l = v2_link_create_heap(pooled ? &heap : NULL);
    if (!l)
        goto done;
    if (pooled) {
        void *copy;
        if (!v2_object_encode(decoded, &bytes, &length)) goto done;
        copy = v2_malloc(&heap, length);
        if (!copy) goto done;
        memcpy(copy, bytes, length);
        v2_object_destroy(decoded);
        decoded = v2_object_decode_heap_owned(&heap, copy, length, &error);
        test_object_release(bytes); bytes = NULL;
        if (!decoded) goto done;
    }
    input = v2_link_add(l, decoded, "consumer.o");
    decoded = NULL;
    if (input == V2_NONE)
        goto done;
    u = v2_unit_create();
    if (!u)
        goto done;
    p = v2_program_create(u);
    if (!p ||
        !v2_program_export(p, "Imported", v2_integer(u, 7, v2_no_location()), v2_no_location()) ||
        !v2_program_raw(p, "\xEA", 1, v2_no_location()))
        goto done;
    o = v2_program_object(p);
    if (!o)
        goto done;
    input = v2_link_add(l, o, "provider.o");
    o = NULL;
    if (input == V2_NONE || !v2_layout_ram_block(v2_link_layout(l), 0x10, 0x100) ||
        !v2_link_append(l, 0) || !v2_link_append(l, 1) || !v2_link_prepare(l) || !v2_link_finish(l))
        goto done;
    output = v2_layout_bytes(v2_link_layout(l), &length);
    assert(length == 3 && !memcmp(output, "\xA9\x07\xEA", 3));
    ok = 1;
done:
    test_object_release(bytes);
    v2_object_destroy(o);
    v2_object_destroy(decoded);
    v2_link_destroy(l);
    v2_program_destroy(p);
    v2_unit_destroy(u);
    assert(!heap.live && !heap.bytes);
    v2_heap_destroy(&heap);
    assert(!test_heap_mappings());
    return ok;
}

int main(void)
{
    void (*failure[])(size_t) = {test_v2_allocation_failure, test_layout_allocation_failure,
                                 test_object_allocation_failure, test_program_allocation_failure,
                                 test_link_allocation_failure};
    size_t (*calls[])(void) = {test_v2_allocation_calls, test_layout_allocation_calls,
                               test_object_allocation_calls, test_program_allocation_calls,
                               test_link_allocation_calls};
    size_t (*live[])(void) = {test_v2_live_allocations, test_layout_live_allocations,
                              test_object_live_allocations, test_program_live_allocations,
                              test_link_live_allocations};
    size_t counts[5], i, j, k;
    for (i = 0; i < 5; ++i)
        failure[i](0);
    assert(run(0));
    for (i = 0; i < 5; ++i)
    {
        counts[i] = calls[i]();
        assert(!live[i]());
    }
    for (i = 0; i < 5; ++i)
    {
        for (j = 1; j <= counts[i]; ++j)
        {
            for (k = 0; k < 5; ++k)
                failure[k](k == i ? j : 0);
            assert(!run(0));
            for (k = 0; k < 5; ++k)
                assert(!live[k]());
        }
        printf("%zu:%zu allocation failures preserved ownership\n", i, counts[i]);
    }
    for (i = 0; i < 5; ++i) failure[i](0);
    test_heap_failure(0); assert(run(1));
    j = test_heap_calls();
    for (i = 1; i <= j; i++) {
        test_heap_failure(i); assert(!run(1));
        for (k = 0; k < 5; k++) assert(!live[k]());
    }
    printf("%zu pooled-link mapping failures preserved ownership\n", j);
    return 0;
}
