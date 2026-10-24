#include "v2/layout.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static v2_unit *u;
static v2_layout *l;
static v2_location loc;
static int expect_no_spans;
extern size_t test_v2_span_nodes(v2_unit *);
static v2_id num(int32_t n) { return v2_integer(u, n, loc); }
static v2_id anchor(void) { return v2_anchor(u, loc); }
static v2_id address(v2_id a) { return v2_address(u, a, loc); }
static v2_id op(v2_operator o, v2_id a, v2_id b) { return v2_binary(u, o, a, b, loc); }
static v2_id sub(v2_id a, int32_t n) { return op(V2_SUBTRACT, a, num(n)); }
static void mark(v2_id a) { assert(v2_layout_mark(l, a, loc) != V2_NONE); }
static void ins(unsigned char code, v2_id operand) { assert(v2_layout_instruction(l, V2_NONE, code, operand, 0, 0, loc) != V2_NONE); }
static void nop(void) { ins(0xea, V2_NONE); }
static void data(v2_id e, unsigned w) { assert(v2_layout_data(l, V2_NONE, e, w, 1, NULL, loc) != V2_NONE); }
static v2_id literal(uint64_t n) { return v2_extent_literal(u, n, loc); }
static v2_id storage(v2_id count, uint32_t width, uint32_t used, int field)
{
    v2_id e = v2_extent_count(u, count, field ? 0 : 1, INT32_MAX, loc);
    e = v2_extent_repeat(u, e, literal(width), loc);
    if (field) e = v2_extent_remainder(u, e, literal(used), loc);
    return v2_extent_bound(u, e, field ? 0 : 1, 65535, loc);
}
static void begin(int link)
{
    u = v2_unit_create(); assert(u);
    loc.file = v2_file(u, "fixture.asm"); loc.line = 7; loc.column = 3;
    loc.end_line = 7; loc.end_column = 9;
    l = v2_layout_create(u, link ? V2_LINK_LAYOUT : V2_BINARY_LAYOUT); assert(l);
}
static void finish(void)
{
    size_t count, size, i;
    const unsigned char *bytes;
    const v2_layout_fact *facts;
    int ok = v2_layout_run(l);
    if (expect_no_spans) assert(test_v2_span_nodes(u) == 0);
    bytes = v2_layout_bytes(l, &size); facts = v2_layout_facts(l, &count);
    if (!ok) {
        v2_layout_diagnostic d = v2_layout_error_at(l);
        assert(!bytes && !size && !facts && !count);
        printf("ERROR %d %d %u\n", d.code, d.value_error.code, d.location.line);
    } else {
        size_t end = 0;
        for (i = 0; i < count; ++i) {
            assert(facts[i].output_offset == end); end += facts[i].length;
        }
        assert(end == size);
        for (i = 0; i < size; ++i) printf("%02X", bytes[i]);
        printf("\n");
        for (i = 0; i < count; ++i) if (facts[i].has_operand)
            printf("%u:%u:%u:%02X:%" PRId32 ":%u ", facts[i].address,
                   facts[i].output_offset, facts[i].length, facts[i].opcode,
                   facts[i].operand, facts[i].warning);
        printf("\n");
    }
    v2_layout_destroy(l); v2_unit_destroy(u);
}
static void controls(int which)
{
    v2_id a = anchor(), b = anchor();
    int i;
    if (which == 0) {
        v2_layout_ram_block(l, 0x10, 0x100);
        v2_layout_ram(l, a, literal(1), 0, 1, loc);
        v2_layout_origin(l, 0x8000, loc); mark(b);
        for (i = 0; i < 43; ++i) ins(0xad, address(a));
        ins(0xd0, address(b));
    } else if (which == 1) { ins(0xad, sub(address(a), 3)); mark(a); nop(); }
    else if (which == 2) { ins(0xad, num(16)); ins(0xad, sub(address(a), 6)); mark(a); nop(); }
    else if (which == 3) {
        v2_layout_origin(l, 0xfa, loc); ins(0xad, num(16)); ins(0xad, address(a)); nop(); mark(a); ins(0x60, V2_NONE);
    } else if (which == 4) {
        v2_layout_origin(l, 0xf6, loc); ins(0xad, address(a)); ins(0xad, address(b)); mark(b);
        for (i = 0; i < 4; ++i) nop();
        mark(a); ins(0x60, V2_NONE);
    } else if (which == 8) {
        mark(b); nop(); ins(0xad, address(b));
        ins(0xad, op(V2_SUBTRACT, num(263), op(V2_SUBTRACT, address(a), address(b))));
        nop(); mark(a); ins(0x60, V2_NONE);
    } else if (which == 9) {
        v2_id tail = anchor();
        mark(b); ins(0xad, op(V2_SUBTRACT, address(tail), address(b)));
        mark(tail); ins(0xad, op(V2_SUBTRACT, num(261), address(a)));
        mark(a); ins(0x60, V2_NONE);
    } else if (which == 7) {
        mark(b); nop(); ins(0xad, op(V2_SUBTRACT, num(262), address(a)));
        ins(0xad, address(b)); mark(a); ins(0x60, V2_NONE);
    } else {
        mark(b); nop(); ins(0xad, address(b));
        ins(0xad, op(V2_SUBTRACT, num(which == 5 ? 263 : 262), address(a)));
        nop(); mark(a); ins(0x60, V2_NONE);
    }
}
static void spans(int which)
{
    v2_id start = anchor(), head = anchor(), tail = anchor();
    v2_id difference = op(V2_SUBTRACT, address(tail), address(which == 1 ? start : head));
    if (which == 3) {
        v2_layout_storage(l, V2_NONE, storage(address(tail), 1, 0, 0), loc);
        v2_layout_origin(l, 5, loc); mark(tail); ins(0x60, V2_NONE); return;
    }
    if (which == 4) {
        v2_layout_storage(l, V2_NONE, storage(address(tail), 1, 0, 0), loc);
        mark(tail); ins(0x60, V2_NONE); return;
    }
    if (which == 5) {
        v2_layout_origin(l, 3, loc);
        v2_layout_storage(l, head, storage(sub(address(head), 2), 1, 0, 0), loc);
        data(address(head), 1); return;
    }
    mark(start);
    v2_layout_storage(l, V2_NONE, storage(difference, 1, 0, 0), loc);
    if (which == 2) v2_layout_origin(l, 0x8000, loc);
    mark(head); nop(); nop(); mark(tail); ins(0x60, V2_NONE);
}
static void extents(void)
{
    v2_id bad = v2_extent_count(u, num(-1), 0, INT32_MAX, loc);
    v2_result r;
    r = v2_evaluate(u, v2_extent_repeat(u, literal(0), bad, loc));
    assert(r.state == V2_KNOWN && r.value.kind == V2_EXTENT && r.value.as.extent == 0);
    r = v2_evaluate(u, v2_extent_sum(u, literal(100), bad, loc));
    assert(r.state == V2_INVALID && r.diagnostic.code == V2_ERROR_EXTENT);
    r = v2_evaluate(u, v2_extent_sum(u, literal(UINT64_MAX), literal(1), loc));
    assert(r.state == V2_INVALID);
    r = v2_evaluate(u, v2_extent_repeat(u, literal(UINT64_MAX), literal(2), loc));
    assert(r.state == V2_INVALID);
    r = v2_evaluate(u, v2_extent_integer(u, literal(UINT64_C(0x100000001)), loc));
    assert(r.state == V2_KNOWN && r.value.kind == V2_INTEGER && r.value.as.integer == 1);
    r = v2_evaluate(u, v2_extent_remainder(u, literal(1), literal(2), loc)); assert(r.state == V2_INVALID);
    r = v2_evaluate(u, storage(num(0), 1, 0, 0)); assert(r.state == V2_INVALID);
    r = v2_evaluate(u, storage(num(0), 1, 0, 1)); assert(r.state == V2_KNOWN && r.value.as.extent == 0);
    r = v2_evaluate(u, storage(num(65535), 1, 0, 0)); assert(r.state == V2_KNOWN);
    r = v2_evaluate(u, storage(num(65536), 1, 0, 0)); assert(r.state == V2_INVALID);
    r = v2_evaluate(u, storage(num(INT32_MAX), INT32_MAX, 0, 0)); assert(r.state == V2_INVALID);
    {
        v2_id a = anchor(), ref = address(a);
        v2_extent_anchor(u, a, op(V2_ADD, ref, num(1)));
        r = v2_evaluate_extent(u, ref); assert(r.state == V2_INVALID && r.diagnostic.code == V2_ERROR_CYCLE);
        v2_place(u, a, 1, 7);
        r = v2_evaluate(u, ref); assert(r.state == V2_KNOWN && r.value.as.integer == 7);
        r = v2_evaluate_extent(u, ref); assert(r.state == V2_INVALID && r.diagnostic.code == V2_ERROR_CYCLE);
    }
    v2_layout_storage(l, V2_NONE, v2_extent_sum(u, storage(num(40000), 1, 0, 0), storage(num(40000), 1, 0, 0), loc), loc);
}
static void dependency_modes(void)
{
    v2_id a = anchor(), ref = address(a), missing = v2_definition(u, "Missing", loc);
    v2_id other = v2_definition(u, "Other", loc), root;
    v2_result r;
    v2_extent_anchor(u, a, v2_reference(u, missing, loc));
    v2_place(u, a, 1, 9);
    root = op(V2_ADD, ref, v2_reference(u, other, loc));
    r = v2_evaluate_extent(u, root); assert(r.state == V2_DEFERRED && r.dependency_count == 2);
    r = v2_evaluate(u, ref); assert(r.state == V2_KNOWN && r.value.as.integer == 9);
    r = v2_evaluate_extent(u, root); assert(r.state == V2_DEFERRED && r.dependency_count == 2);
    v2_bind(u, missing, num(7));
    v2_place(u, a, 0, 0);
    r = v2_evaluate(u, root); assert(r.state == V2_DEFERRED && r.dependency_count == 2);
    r = v2_evaluate_extent(u, ref); assert(r.state == V2_KNOWN && r.value.as.integer == 7);
    r = v2_evaluate(u, root); assert(r.state == V2_DEFERRED && r.dependency_count == 2);
}
static void prefix_edges(void)
{
    v2_id a = anchor(), b = anchor(), ref = address(a), root = op(V2_ADD, ref, num(1));
    v2_result r;
    assert(v2_place(u, a, 1, 9));
    r = v2_evaluate_extent(u, root); assert(r.state == V2_KNOWN && r.value.as.integer == 10);
    assert(v2_extent_anchor(u, a, address(b)));
    r = v2_evaluate_extent(u, root); assert(r.state == V2_DEFERRED && r.dependency_count == 1);
    assert(v2_place(u, b, 1, 20));
    r = v2_evaluate_extent(u, root); assert(r.state == V2_KNOWN && r.value.as.integer == 21);
    assert(v2_place(u, b, 1, 30));
    r = v2_evaluate_extent(u, root); assert(r.state == V2_KNOWN && r.value.as.integer == 31);
    r = v2_evaluate(u, root); assert(r.state == V2_KNOWN && r.value.as.integer == 10);
}
static void span_barriers(int which)
{
    v2_id head = anchor(), tail = anchor(), difference = op(V2_SUBTRACT, address(tail), address(head));
    expect_no_spans = 1;
    if (which == 1) v2_layout_bank(l, 4, 4, loc); else v2_layout_origin(l, 4, loc);
    mark(head); nop();
    if (which == 0) v2_layout_origin(l, 16, loc);
    else if (which == 1) v2_layout_bank(l, 16, 4, loc);
    else v2_layout_pad(l, V2_PAD_ORIGIN, 16, loc);
    mark(tail); nop(); data(difference, 1);
}
static void strings(void)
{
    unsigned char map[256];
    v2_id text = v2_definition(u, "Text", loc), pc = anchor();
    unsigned i;
    for (i = 0; i < 256; ++i) map[i] = (unsigned char)i;
    map['H'] = 1; map['I'] = 0;
    v2_layout_origin(l, 0x8000, loc);
    v2_layout_data(l, V2_NONE, v2_reference(u, text, loc), 1, 1, map, loc);
    map['H'] = 2;
    v2_layout_data(l, V2_NONE, num('H'), 1, 1, map, loc);
    v2_layout_data(l, pc, address(pc), 2, 0, NULL, loc);
    data(v2_string(u, "AB", 2, loc), 4);
    data(v2_string(u, NULL, 0, loc), 1);
    data(v2_string(u, "A\0B", 3, loc), 1);
    v2_bind(u, text, v2_string(u, "HI", 2, loc));
}
static void ram_fixture(int which)
{
    static const unsigned sizes[] = {1,2,1,2,1,2,4,1,2,1,1};
    v2_id a[11];
    unsigned i;
    v2_layout_ram_block(l, 0x10, 0x100);
    if (which == 4) {
        for (i = 0; i < 11; ++i) a[i] = anchor();
        for (i = 0; i < 11; ++i) { unsigned j = (i + 5) % 11;
            v2_layout_ram(l, a[j], literal(sizes[j]), 0, 0, loc); }
        for (i = 0; i < 11; ++i) data(address(a[(i + 5) % 11]), 2);
    } else if (which == 0) {
        for (i = 0; i < 11; ++i) { a[i] = anchor(); v2_layout_ram(l, a[i], literal(sizes[i]), 0, 0, loc); }
        for (i = 0; i < 11; ++i) data(address(a[i]), 1);
    } else if (which == 1) {
        for (i = 0; i < 3; ++i) { a[i] = anchor(); v2_layout_ram(l, a[i], literal(i == 0 ? 0 : 1), 0, 0, loc); }
        for (i = 0; i < 3; ++i) data(address(a[i]), 1);
    } else if (which == 2) {
        a[0] = anchor(); a[1] = anchor();
        v2_layout_ram(l, a[0], literal(4), 0, 0, loc); v2_layout_ram(l, a[1], literal(1), 0, 0, loc);
        data(address(a[0]), 1); data(address(a[1]), 1);
    } else {
        a[0] = anchor(); a[1] = anchor(); a[2] = anchor();
        v2_layout_ram(l, a[0], literal(1), 0, 0, loc);
        v2_layout_ram(l, a[1], storage(sub(address(a[0]), 15), 1, 0, 0), 0, 0, loc);
        v2_layout_ram(l, a[2], literal(1), 0, 0, loc);
        data(address(a[2]), 1);
    }
}
static void banks(void)
{
    v2_id a = anchor(), b = anchor(), c = anchor();
    mark(a); data(v2_layout_bank_expression(l, a, loc), 1);
    v2_layout_bank(l, 0x8000, 8, loc); mark(b); data(address(b), 2);
    data(v2_layout_bank_expression(l, b, loc), 1);
    v2_layout_pad(l, V2_PAD_ORIGIN, 0x8005, loc); data(num(0x55), 1);
    v2_layout_bank(l, 0x8000, 4, loc); mark(c); data(v2_layout_bank_expression(l, c, loc), 1);
    v2_layout_pad(l, V2_PAD_BANK_OFFSET, 3, loc); data(num(0xaa), 1);
}
static void work(unsigned n)
{
    unsigned i;
    v2_id end = anchor(), shared = address(end);
    for (i = 0; i < 200; ++i) shared = op(V2_ADD, shared, num(0));
    for (i = 0; i < n; ++i) { mark(anchor()); ins(0xad, shared); }
    mark(end); ins(0x60, V2_NONE);
    assert(v2_layout_run(l));
    { v2_layout_work w = v2_layout_work_done(l);
      assert(w.walks <= w.candidates + 3);
      printf("%" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",
             w.walks, w.fragment_visits, w.narrowings, v2_work_done(u).evaluated_nodes); }
    v2_layout_destroy(l); v2_unit_destroy(u);
}
static void cascade(unsigned n)
{
    unsigned i;
    v2_id end = anchor();
    for (i = 0; i < n; ++i)
        ins(0xad, op(V2_ADD, address(end), num(255 - (int32_t)(3 * n) + (int32_t)i)));
    mark(end);
    assert(v2_layout_run(l));
    { size_t size; const unsigned char *bytes = v2_layout_bytes(l, &size);
      v2_layout_work w = v2_layout_work_done(l);
      assert(size == 2 * n && w.walks == n + 2 && w.narrowings == n);
      for (i = 0; i < n; ++i) { assert(bytes[2*i] == 0xa5); assert(bytes[2*i+1] == 255 - n + i); }
      printf("%" PRIu64 " %" PRIu64 "\n", w.walks, w.fragment_visits); }
    v2_layout_destroy(l); v2_unit_destroy(u);
}
static void cache_work(unsigned n)
{
    v2_id *anchors = malloc(n * sizeof(*anchors));
    v2_id shared = num(0);
    unsigned i;
    assert(anchors);
    for (i = 0; i < n; ++i) { anchors[i] = anchor(); shared = op(V2_ADD, shared, address(anchors[i])); }
    shared = v2_unary(u, V2_LOW, shared, loc);
    for (i = 0; i < n; ++i) { mark(anchors[i]); ins(0xad, shared); }
    assert(v2_layout_run(l));
    { v2_work w = v2_work_done(u); v2_layout_work t = v2_layout_work_done(l);
      printf("%" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",
             t.walks, w.evaluated_nodes, w.invalidation_edges, w.dependency_visits); }
    free(anchors); v2_layout_destroy(l); v2_unit_destroy(u);
}
static void alignment(void)
{
    v2_id a = anchor(), b = anchor(), c = anchor();
    v2_layout_ram_block(l, 0x11, 0x40);
    v2_layout_ram(l, a, literal(2), 4, 0, loc);
    v2_layout_ram(l, b, literal(3), 0, 0, loc);
    v2_layout_ram(l, c, literal(1), 0, 1, loc);
    data(address(a), 1); data(address(b), 1); data(address(c), 1);
}
static void limits(int which)
{
    if (which == 0) { v2_layout_origin(l, 0xffff, loc); nop(); data(num(1), 1); }
    else if (which == 1) { v2_layout_bank(l, 0x8000, 1, loc); nop(); nop(); }
    else if (which == 2) { nop(); v2_layout_pad(l, V2_PAD_ORIGIN, 0, loc); }
    else if (which == 3) { v2_layout_storage(l, V2_NONE, literal(UINT64_MAX), loc); }
    else if (which == 4) {
        v2_id a = anchor(); v2_layout_ram_block(l, 0x100, 0x200);
        v2_layout_ram(l, a, literal(1), 0, 1, loc);
    } else if (which == 5) {
        v2_id a = anchor(); v2_layout_ram_block(l, 0x10, 0x11);
        v2_layout_ram(l, a, literal(1), 0, 0, loc); data(address(a), 1);
    } else if (which == 6) {
        v2_id a = anchor(); v2_layout_ram_block(l, 0x10, 0x11);
        v2_layout_ram(l, a, literal(2), 0, 0, loc);
    } else if (which == 7) { v2_id a = anchor(); mark(a); mark(a); }
    else if (which == 8) { v2_id a = anchor(); mark(a); v2_layout_ram(l, a, literal(0), 0, 0, loc); }
    else if (which == 9) { v2_id a = v2_definition(u, "Missing", loc); data(v2_reference(u, a, loc), 1); }
    else if (which == 10) {
        v2_id a = anchor();
        data(op(V2_ADD, v2_string(u, "A", 1, loc), address(a)), 1); mark(a);
    } else if (which == 11) {
        v2_id a = anchor();
        v2_layout_instruction(l, V2_NONE, 0xad, address(a), 1, 0, loc); mark(a); nop();
    } else if (which == 17) {
        v2_location bad = loc; bad.file = 99;
        v2_layout_origin(l, 0x10000, bad);
        assert(v2_layout_error_at(l).location.file == V2_NONE);
    } else if (which == 16) {
        v2_id head = anchor(), tail = anchor(), first = anchor(), next = anchor();
        mark(head); ins(0xad, num(16)); mark(tail);
        v2_layout_ram_block(l, 0x10, 0x100);
        v2_layout_ram(l, first, storage(op(V2_SUBTRACT, address(tail), address(head)), 1, 0, 0), 0, 0, loc);
        v2_layout_ram(l, next, literal(1), 0, 0, loc);
        data(address(first), 1); data(address(next), 1);
    } else if (which == 15) {
        v2_id later = anchor(), first = anchor(), next = anchor();
        v2_layout_ram_origin(l, 0x10, loc);
        v2_layout_ram(l, first, storage(address(later), 1, 0, 0), 0, 0, loc);
        v2_layout_ram(l, next, literal(1), 0, 0, loc);
        nop(); nop(); mark(later); data(address(next), 1);
    } else if (which == 14) {
        v2_id here = anchor(), pc = anchor();
        v2_layout_origin(l, 0xffff, loc); nop(); mark(here);
        data(address(here), 4); v2_layout_data(l, pc, address(pc), 4, 0, NULL, loc);
    } else if (which == 13) {
        v2_id code = anchor(), var = anchor();
        nop(); mark(code); nop();
        v2_layout_ram_origin(l, 0x10, loc);
        v2_layout_ram(l, var, storage(address(code), 1, 0, 0), 0, 0, loc);
        data(address(var), 1);
    } else if (which == 12) {
        v2_layout_instruction(l, V2_NONE, 0xa9, num(255), 0, 1, loc);
    }
}
extern void test_v2_allocation_failure(size_t);
extern size_t test_v2_allocation_calls(void);
extern size_t test_v2_live_allocations(void);
extern void test_v2_version_rollover(v2_unit *, int);
static void mixed_error(void)
{
    v2_id a = anchor(), ref = address(a);
    v2_id left = op(V2_DIVIDE, num(1), ref);
    v2_id right = v2_unary(u, V2_NEGATE, v2_string(u, "s", 1, loc), loc);
    v2_id root = op(V2_ADD, left, right);
    v2_result r = v2_evaluate(u, root);
    assert(r.state == V2_INVALID && r.diagnostic.code == V2_ERROR_TYPE);
    r = v2_evaluate_extent(u, ref); assert(r.state == V2_DEFERRED);
    assert(v2_place(u, a, 1, 0));
    r = v2_evaluate(u, root);
    assert(r.state == V2_INVALID && r.diagnostic.code == V2_ERROR_DIV_ZERO);
    {
        v2_id d1 = v2_definition(u, "Cycle1", loc), d2 = v2_definition(u, "Cycle2", loc);
        v2_id r1 = v2_reference(u, d1, loc), r2;
        v2_location second = loc; second.line = second.end_line = 8;
        r2 = v2_reference(u, d2, second);
        assert(v2_bind(u, d1, r2) && v2_bind(u, d2, r1));
        r = v2_evaluate(u, r1); assert(r.state == V2_INVALID && r.diagnostic.location.line == 7);
        r = v2_evaluate(u, r2); assert(r.state == V2_INVALID && r.diagnostic.location.line == 8);
        test_v2_version_rollover(u, 2);
        r = v2_evaluate(u, r1); assert(r.state == V2_INVALID && r.diagnostic.location.line == 7);
        test_v2_version_rollover(u, 2);
        r = v2_evaluate(u, r2); assert(r.state == V2_INVALID && r.diagnostic.location.line == 8);
    }
}
static void concat_reuse(int which)
{
    if (which == 0) {
        v2_id head = anchor(), tail = anchor();
        data(op(V2_ADD, v2_string(u, "AB", 2, loc), v2_string(u, "CD", 2, loc)), 1);
        v2_layout_storage(l, V2_NONE, storage(op(V2_SUBTRACT, address(tail), address(head)), 1, 0, 0), loc);
        mark(head); nop(); nop(); mark(tail); ins(0x60, V2_NONE);
    } else {
        size_t size = 8u << 20, live, calls;
        unsigned char *bytes = malloc(size);
        v2_id a = anchor(), count = v2_extent_count(u, address(a), 0, 100, loc);
        v2_id zero = v2_extent_integer(u, v2_extent_repeat(u, literal(0), count, loc), loc), root;
        const unsigned char *saved = NULL;
        v2_result r; int i;
        assert(bytes); memset(bytes, 'x', size);
        root = op(V2_ADD, v2_string(u, bytes, size, loc), zero); free(bytes);
        r = v2_evaluate(u, root); assert(r.state == V2_KNOWN);
        saved = r.value.as.string.bytes;
        /* Warm both traversal modes and the invalidation queue before counting. */
        v2_evaluate_extent(u, count); assert(v2_place(u, a, 1, 0));
        v2_evaluate_extent(u, root); calls = test_v2_allocation_calls();
        live = test_v2_live_allocations();
        for (i = 1; i <= 40; ++i) {
            v2_evaluate_extent(u, count); assert(v2_place(u, a, 1, i));
            r = i % 2 ? v2_evaluate(u, root) : v2_evaluate_extent(u, root);
            assert(r.state == V2_KNOWN && r.value.kind == V2_STRING);
            assert(r.value.as.string.bytes == saved && r.value.as.string.length == size + 1);
            assert(saved[0] == 'x' && saved[size-1] == 'x' && saved[size] == '0');
            assert(test_v2_live_allocations() == live && test_v2_allocation_calls() == calls);
        }
    }
}
static void ram_failed_split(int which)
{
    v2_id a = anchor(), b = anchor();
    if (which == 3) {
        assert(v2_layout_ram_block(l, 0xf8, 0x200));
        assert(v2_layout_ram_block(l, 0, 0x10));
        assert(v2_layout_ram(l, a, literal(1), 4, 1, loc));
        assert(v2_layout_ram(l, b, literal(16), 0, 0, loc));
    } else if (which == 2) {
        assert(v2_layout_ram_block(l, 0x11, 0x20));
        assert(v2_layout_ram_block(l, 0x43, 0x80));
        assert(v2_layout_ram(l, a, literal(0), 4, 0, loc));
        assert(v2_layout_ram(l, b, literal(1), 0, 0, loc));
    } else {
        assert(v2_layout_ram_block(l, 0x310, 0x490));
        assert(v2_layout_ram_block(l, 0x800, 0xa00));
        if (!which) assert(v2_layout_ram_block(l, 0x1000, 0x1200));
        assert(v2_layout_ram(l, a, literal(256), 8, 0, loc));
        assert(v2_layout_ram(l, b, literal(0x120), 0, 0, loc));
    }
    data(address(a), 2); data(address(b), 2);
}
extern void test_layout_allocation_failure(size_t);
extern size_t test_layout_allocation_calls(void);
extern size_t test_layout_live_allocations(void);
static const char *allocation_copy;
static int allocation_sample(void)
{
    v2_unit *values = v2_unit_create();
    v2_layout *layout;
    v2_location at = v2_no_location();
    v2_id a, count, span;
    unsigned char map[256];
    int ok;
    unsigned i;
    if (!values) return 0;
    for (i = 0; i < 256; ++i) map[i] = (unsigned char)i;
    layout = v2_layout_create(values, V2_LINK_LAYOUT);
    if (!layout) { v2_unit_destroy(values); return 0; }
    a = v2_anchor(values, at);
    count = v2_integer(values, 2, at);
    span = v2_extent_count(values, count, 1, 65535, at);
    v2_layout_ram_block(layout, 0x11, 0x100);
    v2_layout_ram(layout, a, span, 4, 1, at);
    v2_layout_bank(layout, 0x8000, 2000, at);
    v2_layout_copy(layout, allocation_copy, at);
    for (i = 0; i < 40; ++i) {
        v2_id pc = v2_anchor(values, at), def = v2_definition(values, "value", at);
        v2_id expression = v2_reference(values, def, at);
        v2_layout_data(layout, pc, expression, 1, 1, map, at);
        v2_bind(values, def, v2_binary(values, V2_ADD,
            v2_string(values, "hel", 3, at), v2_string(values, "lo", 2, at), at));
        v2_layout_instruction(layout, V2_NONE, 0xad, v2_address(values, a, at), 0, 0, at);
        v2_layout_storage(layout, V2_NONE, span, at);
        v2_layout_raw(layout, "raw", 3, at);
    }
    {
        v2_id head = v2_anchor(values, at), tail = v2_anchor(values, at);
        v2_id difference = v2_binary(values, V2_SUBTRACT, v2_address(values, tail, at), v2_address(values, head, at), at);
        v2_layout_storage(layout, V2_NONE, v2_extent_count(values, difference, 1, 65535, at), at);
        v2_layout_mark(layout, head, at);
        v2_layout_raw(layout, "xx", 2, at);
        v2_layout_mark(layout, tail, at);
    }
    ok = v2_layout_run(layout);
    if (!ok) {
        size_t n;
        assert(!v2_layout_bytes(layout, &n) && n == 0);
        assert(!v2_layout_facts(layout, &n) && n == 0);
    }
    v2_layout_destroy(layout); v2_unit_destroy(values);
    assert(!test_v2_live_allocations() && !test_layout_live_allocations());
    return ok;
}
static void allocations(void)
{
    size_t value_calls, layout_calls, i;
    test_v2_allocation_failure(0); test_layout_allocation_failure(0);
    assert(allocation_sample());
    value_calls = test_v2_allocation_calls(); layout_calls = test_layout_allocation_calls();
    for (i = 1; i <= value_calls; ++i) {
        test_v2_allocation_failure(i); test_layout_allocation_failure(0);
        assert(!allocation_sample());
    }
    for (i = 1; i <= layout_calls; ++i) {
        test_v2_allocation_failure(0); test_layout_allocation_failure(i);
        assert(!allocation_sample());
    }
    printf("%zu %zu\n", value_calls, layout_calls);
}
extern void test_layout_cache(void);
int main(int argc, char **argv)
{
    int which;
    assert(argc >= 3);
    if (!strcmp(argv[1], "cache-differential")) {
        test_layout_cache(); assert(!test_v2_live_allocations()); return 0;
    }
    if (!strcmp(argv[1], "allocations")) { assert(argc == 5); allocation_copy = argv[4]; allocations(); return 0; }
    begin(atoi(argv[2])); which = argc > 3 ? atoi(argv[3]) : 0;
    if (!strcmp(argv[1], "controls")) controls(which);
    else if (!strcmp(argv[1], "spans")) spans(which);
    else if (!strcmp(argv[1], "extents")) extents();
    else if (!strcmp(argv[1], "dependency-modes")) dependency_modes();
    else if (!strcmp(argv[1], "mixed-error")) mixed_error();
    else if (!strcmp(argv[1], "prefix-edges")) prefix_edges();
    else if (!strcmp(argv[1], "span-barriers")) span_barriers(which);
    else if (!strcmp(argv[1], "early-truncation")) {
        v2_layout_instruction(l, V2_NONE, 0xa9, num(0x1ff), 0, which, loc);
    }
    else if (!strcmp(argv[1], "concat-reuse")) concat_reuse(which);
    else if (!strcmp(argv[1], "ram-failed-split")) ram_failed_split(which);
    else if (!strcmp(argv[1], "strings")) strings();
    else if (!strcmp(argv[1], "ram")) ram_fixture(which);
    else if (!strcmp(argv[1], "banks")) banks();
    else if (!strcmp(argv[1], "alignment")) alignment();
    else if (!strcmp(argv[1], "limits")) limits(which);
    else if (!strcmp(argv[1], "cascade")) { cascade((unsigned)which); return 0; }
    else if (!strcmp(argv[1], "cache-work")) { cache_work((unsigned)which); return 0; }
    else if (!strcmp(argv[1], "work")) { work((unsigned)which); return 0; }
    else if (!strcmp(argv[1], "opcode")) {
        int width; assert(argc == 5); width = atoi(argv[4]);
        v2_id operand = width == 1 ? V2_NONE : num(width == 3 ? 0x1234 :
            (((unsigned)which & 0x1fu) == 0x10u ? 129 : 127));
        v2_layout_instruction(l, V2_NONE, (unsigned char)which, operand, 1, 0, loc);
    }
    else if (!strcmp(argv[1], "range")) {
        assert(argc == 6);
        if (which == 0) data(num((int32_t)strtoll(argv[4], NULL, 0)), (unsigned)atoi(argv[5]));
        else ins((unsigned char)which, num((int32_t)strtoll(argv[4], NULL, 0)));
    } else if (!strcmp(argv[1], "copy")) {
        FILE *f;
        assert(argc == 5); v2_layout_copy(l, argv[4], loc);
        if (which) { f = fopen(argv[4], "wb"); assert(f);
            if (which == 1) fputs("changed", f);
            else if (which == 2) fwrite("NEQ\x1a", 1, 4, f);
            else fputc('N', f);
            fclose(f); }
    } else assert(0);
    finish(); assert(!test_v2_live_allocations()); return 0;
}
