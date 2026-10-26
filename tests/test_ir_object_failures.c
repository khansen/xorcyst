#include "ir/link.h"
#include "ir/program.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define DECLARE(name)                                                                              \
    void test_##name##_allocation_failure(size_t);                                                 \
    size_t test_##name##_allocation_calls(void);                                                   \
    size_t test_##name##_live_allocations(void)
DECLARE(ir);
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
    ir_heap heap = {0};
    ir_unit *u = NULL;
    ir_program *p = NULL;
    ir_link *l = NULL;
    ir_object *o = NULL, *decoded = NULL;
    unsigned char *bytes = NULL;
    const unsigned char *output;
    size_t length = 0;
    ir_id definition, operand, anchor, input;
    ir_location loc = ir_no_location();
    ir_object_error error;
    int ok = 0;
    u = ir_unit_create();
    if (!u)
        goto done;
    loc = (ir_location){ir_file(u, "input.asm"), 1, 1, 1, 8};
    p = ir_program_create(u);
    if (!p)
        goto done;
    definition = ir_definition(u, "Imported", loc);
    operand = ir_reference(u, definition, loc);
    if (!ir_program_instruction(p, IR_NONE, 0xa9, operand, 0, loc))
        goto done;
    anchor = ir_anchor(u, loc);
    if (!ir_program_ram_label(p, anchor, 4, 1, loc) ||
        !ir_program_ram_storage(p, IR_NONE, ir_extent_literal(u, 3, loc), loc))
        goto done;
    o = ir_program_object(p);
    if (!o || !ir_object_encode(o, &bytes, &length))
        goto done;
    decoded = ir_object_decode(bytes, length, &error);
    if (!decoded)
        goto done;
    /* Exercise both first-edit materialization and later table growth while
     * the appended/new blob borrows its source from the same decoded object. */
    if (!ir_object_append_bytes(decoded, 0, ir_object_bytes(decoded, 0, NULL), 1) ||
        ir_object_blob(decoded, ir_object_bytes(decoded, 0, NULL), 1) == IR_NONE)
        goto done;
    ir_object_destroy(decoded);
    decoded = ir_object_decode_owned(bytes, length, &error);
    bytes = NULL;
    if (!decoded)
        goto done;
    /* Root-first ordering forces the translator's temporary stack to grow.
     * Every failed growth must retain its old allocation for cleanup. */
    {
        uint32_t first = ir_object_count(decoded, IR_OBJ_EXPRESSIONS), i;
        uint32_t leaf[6] = {1, 1, 0, 0, 0, 0};
        for (i = 0; i < 40; i++) {
            uint32_t unary[6] = {16, first + i + 1, 0, 0, 0, 0};
            if (ir_object_add(decoded, IR_OBJ_EXPRESSIONS, unary) == IR_NONE) goto done;
        }
        if (ir_object_add(decoded, IR_OBJ_EXPRESSIONS, leaf) == IR_NONE) goto done;
    }
    ir_object_destroy(o);
    o = NULL;
    ir_program_destroy(p);
    p = NULL;
    ir_unit_destroy(u);
    u = NULL;
    l = ir_link_create_heap(pooled ? &heap : NULL);
    if (!l)
        goto done;
    if (pooled) {
        void *copy;
        if (!ir_object_encode(decoded, &bytes, &length)) goto done;
        copy = ir_malloc(&heap, length);
        if (!copy) goto done;
        memcpy(copy, bytes, length);
        ir_object_destroy(decoded);
        decoded = ir_object_decode_heap_owned(&heap, copy, length, &error);
        test_object_release(bytes); bytes = NULL;
        if (!decoded) goto done;
    }
    input = ir_link_add(l, decoded, "consumer.o");
    decoded = NULL;
    if (input == IR_NONE)
        goto done;
    u = ir_unit_create();
    if (!u)
        goto done;
    p = ir_program_create(u);
    if (!p ||
        !ir_program_export(p, "Imported", ir_integer(u, 7, ir_no_location()), ir_no_location()) ||
        !ir_program_raw(p, "\xEA", 1, ir_no_location()))
        goto done;
    o = ir_program_object(p);
    if (!o)
        goto done;
    input = ir_link_add(l, o, "provider.o");
    o = NULL;
    if (input == IR_NONE || !ir_layout_ram_block(ir_link_layout(l), 0x10, 0x100) ||
        !ir_link_append(l, 0) || !ir_link_append(l, 1) || !ir_link_prepare(l) || !ir_link_finish(l))
        goto done;
    output = ir_layout_bytes(ir_link_layout(l), &length);
    assert(length == 3 && !memcmp(output, "\xA9\x07\xEA", 3));
    ok = 1;
done:
    test_object_release(bytes);
    ir_object_destroy(o);
    ir_object_destroy(decoded);
    ir_link_destroy(l);
    ir_program_destroy(p);
    ir_unit_destroy(u);
    assert(!heap.live && !heap.bytes);
    ir_heap_destroy(&heap);
    assert(!test_heap_mappings());
    return ok;
}

int main(void)
{
    void (*failure[])(size_t) = {test_ir_allocation_failure, test_layout_allocation_failure,
                                 test_object_allocation_failure, test_program_allocation_failure,
                                 test_link_allocation_failure};
    size_t (*calls[])(void) = {test_ir_allocation_calls, test_layout_allocation_calls,
                               test_object_allocation_calls, test_program_allocation_calls,
                               test_link_allocation_calls};
    size_t (*live[])(void) = {test_ir_live_allocations, test_layout_live_allocations,
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
