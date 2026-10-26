#include "ir/layout.h"
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ir_unit *u;
static ir_layout *l;
static ir_location loc;
static int expect_no_spans;
extern size_t test_ir_span_nodes(ir_unit *);
void test_layout_write_failure(size_t);
size_t test_layout_write_calls(void);
static ir_id num(int32_t n) { return ir_integer(u, n, loc); }
static ir_id anchor(void) { return ir_anchor(u, loc); }
static ir_id address(ir_id a) { return ir_address(u, a, loc); }
static ir_id op(ir_operator o, ir_id a, ir_id b) { return ir_binary(u, o, a, b, loc); }
static ir_id sub(ir_id a, int32_t n) { return op(IR_SUBTRACT, a, num(n)); }
static void mark(ir_id a) { assert(ir_layout_mark(l, a, loc) != IR_NONE); }
static void ins(unsigned char code, ir_id operand) { assert(ir_layout_instruction(l, IR_NONE, code, operand, 0, 0, loc) != IR_NONE); }
static void nop(void) { ins(0xea, IR_NONE); }
static void data(ir_id e, unsigned w) { assert(ir_layout_data(l, IR_NONE, e, w, 1, NULL, loc) != IR_NONE); }
static ir_id literal(uint64_t n) { return ir_extent_literal(u, n, loc); }
static ir_id storage(ir_id count, uint32_t width, uint32_t used, int field)
{
    ir_id e = ir_extent_count(u, count, field ? 0 : 1, INT32_MAX, loc);
    e = ir_extent_repeat(u, e, literal(width), loc);
    if (field) e = ir_extent_remainder(u, e, literal(used), loc);
    return ir_extent_bound(u, e, field ? 0 : 1, 65535, loc);
}
static void begin(int link)
{
    u = ir_unit_create(); assert(u);
    loc.file = ir_file(u, "fixture.asm"); loc.line = 7; loc.column = 3;
    loc.end_line = 7; loc.end_column = 9;
    l = ir_layout_create(u, link ? IR_LINK_LAYOUT : IR_BINARY_LAYOUT); assert(l);
}
static void finish(void)
{
    size_t count, size, i;
    const unsigned char *bytes;
    const ir_layout_fact *facts;
    int ok = ir_layout_run(l);
    if (expect_no_spans) assert(test_ir_span_nodes(u) == 0);
    bytes = ir_layout_bytes(l, &size); facts = ir_layout_facts(l, &count);
    if (!ok) {
        ir_layout_diagnostic d = ir_layout_error_at(l);
        assert(!bytes && !size && !facts && !count);
        printf("ERROR %d %d %u\n", d.code, d.value_error.code, d.location.line);
    } else {
        size_t end = 0;
        for (i = 0; i < count; ++i) {
            assert(facts[i].output_offset == end);
            if (facts[i].kind != IR_FACT_RAM) end += facts[i].length;
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
    ir_layout_destroy(l); ir_unit_destroy(u);
}
static void ram_analysis_facts(void)
{
    unsigned mode;
    for (mode = 0; mode < 4; ++mode) {
        ir_layout_fact f;
        ir_id a, b, reset, slot;
        size_t size;
        unsigned char actual[4], expected[] = {0xea, 0xa5, 0x20, 0x60};
        FILE *stream;
        begin(0);
        assert(ir_layout_keep_facts(l, !(mode & 1)));
        assert(ir_layout_keep_bytes(l, !(mode & 2)));
        ir_layout_origin(l, 0x8000, loc); nop();
        reset = (ir_id)ir_layout_fragment_count(l);
        assert(ir_layout_ram_origin(l, 0x20, loc));
        a = anchor(); b = anchor();
        slot = (ir_id)ir_layout_fragment_count(l);
        assert(ir_layout_ram(l, a, literal(3), 0, 0, loc));
        ins(0xad, address(a));
        assert(ir_layout_ram(l, b, literal(2), 0, 0, loc));
        ins(0x60, IR_NONE);
        assert(!ir_layout_fact_at(l, slot, &f));
        assert(ir_layout_run(l));
        assert(ir_layout_fact_at(l, reset, &f));
        assert(f.kind == IR_FACT_RAM && f.address == 0x20 && f.length == 0 && f.output_offset == 1);
        assert(ir_layout_fact_at(l, slot, &f));
        assert(f.kind == IR_FACT_RAM && f.address == 0x20 && f.length == 3 && f.output_offset == 1);
        assert(ir_layout_fact_at(l, slot + 2, &f));
        assert(f.kind == IR_FACT_RAM && f.address == 0x23 && f.length == 2 && f.output_offset == 3);
        assert(ir_place(u, a, 1, 99)); /* Published facts own the final snapshot. */
        assert(ir_layout_fact_at(l, slot, &f) && f.address == 0x20 && f.length == 3);
        if (!(mode & 2)) {
            const unsigned char *bytes = ir_layout_bytes(l, &size);
            assert(size == sizeof(expected) && !memcmp(bytes, expected, size));
        }
        stream = tmpfile(); assert(stream);
        assert(ir_layout_write_fragments(l, 0, ir_layout_fragment_count(l), stream));
        assert(ftell(stream) == sizeof(expected));
        rewind(stream);
        assert(fread(actual, 1, sizeof(actual), stream) == sizeof(actual));
        assert(!memcmp(actual, expected, sizeof(actual)));
        fclose(stream);
        ir_layout_destroy(l); ir_unit_destroy(u);
    }
    puts("RAM analysis facts: OK");
}
static void compact_facts(void)
{
    ir_layout *pair[2];
    ir_unit *units[2];
    size_t counts[2], lengths[2], i;
    const unsigned char *bytes[2];
    const ir_layout_fact *facts;
    ir_layout_fact a, b;
    unsigned mode;
    for (mode = 0; mode < 2; mode++) {
        ir_id start, end;
        unsigned char raw[] = {1,2,3};
        begin(1); pair[mode] = l; units[mode] = u;
        assert(ir_layout_keep_facts(l, !mode));
        assert(!ir_layout_fact_at(l, 0, &a));
        start = anchor(); end = anchor();
        ir_layout_bank(l, 0x8000, 32, loc); mark(start);
        ir_layout_raw(l, raw, sizeof(raw), loc);
        ins(0xad, address(end));
        data(num(300), 1); /* warning and full projected operand survive */
        data(ir_string(u, "AB", 2, loc), 2);
        mark(end); nop(); ins(0xd0, address(start));
        /* Bank-end padding advances output without advancing the CPU PC.
         * Origins restart the affine address run while retaining that offset. */
        ir_id second = ir_layout_bank(l, 0xc000, 16, loc);
        assert(second != IR_NONE);
        ir_layout_raw(l, "Z", 1, loc);
        ir_id reset = ir_layout_origin(l, 0xd000, loc);
        data(num(0x1234), 2);
        assert(ir_layout_run(l));
        assert(ir_layout_fact_at(l, second - 1, &a));
        assert(a.address == 0x800e && a.output_offset == 14 && a.length == 18 && a.bank == 0);
        assert(ir_layout_fact_at(l, second, &a));
        assert(a.address == 0xc000 && a.output_offset == 32 && !a.length && a.bank == 1);
        assert(ir_layout_fact_at(l, reset, &a));
        assert(a.address == 0xd000 && a.output_offset == 33 && !a.length && a.bank == 1);
        assert(ir_layout_fact_at(l, reset + 1, &a));
        assert(a.address == 0xd000 && a.output_offset == 33 && a.length == 2 && a.operand == 0x1234);
        assert(ir_layout_fact_at(l, reset + 2, &a));
        assert(a.address == 0xd002 && a.output_offset == 35 && a.length == 13 && a.bank == 1);
        bytes[mode] = ir_layout_bytes(l, &lengths[mode]);
        counts[mode] = ir_layout_fact_count(l);
        assert(!ir_layout_fact_at(l, (ir_id)counts[mode], &a));
        assert(!ir_layout_fact_at(l, 0, NULL));
    }
    assert(counts[0] == counts[1] && lengths[0] == lengths[1]);
    assert(!memcmp(bytes[0], bytes[1], lengths[0]));
    facts = ir_layout_facts(pair[0], &i); assert(facts && i == counts[0]);
    assert(!ir_layout_facts(pair[1], &i) && !i);
    for (i = 0; i < counts[0]; i++) {
        assert(ir_layout_fact_at(pair[0], (ir_id)i, &a));
        assert(ir_layout_fact_at(pair[1], (ir_id)i, &b));
        assert(a.kind == b.kind && a.address == b.address && a.output_offset == b.output_offset &&
               a.length == b.length && a.bank == b.bank && a.operand == b.operand &&
               a.opcode == b.opcode && a.has_operand == b.has_operand && a.warning == b.warning);
        assert(a.location.file == b.location.file && a.location.line == b.location.line &&
               a.location.column == b.location.column && a.location.end_line == b.location.end_line &&
               a.location.end_column == b.location.end_column);
    }
    for (mode = 0; mode < 2; mode++) { ir_layout_destroy(pair[mode]); ir_unit_destroy(units[mode]); }
    puts("compact facts: OK");
}
static void streamed_bytes(void)
{
    unsigned mode;
    unsigned char reference[32768], actual[32768], text[2053], map[256];
    size_t i;
    memset(text, 'A', sizeof(text));
    for (i = 0; i < sizeof(map); ++i) map[i] = (unsigned char)i;
    map['A'] = 9;
    for (mode = 0; mode < 4; ++mode) {
        FILE *stream = tmpfile();
        size_t count, length;
        const unsigned char *bytes;
        ir_id split, target;
        ir_layout_fact fact;
        assert(stream);
        begin(1);
        assert(ir_layout_keep_facts(l, !(mode & 1)));
        assert(ir_layout_keep_bytes(l, !(mode & 2)));
        assert(!ir_layout_write_fragments(l, 0, 0, stream));
        ir_layout_bank(l, 0x8000, sizeof(reference), loc);
        ir_layout_raw(l, "ABC", 3, loc);
        assert(ir_layout_data(l, IR_NONE, ir_string(u, text, sizeof(text), loc), 2, 1, NULL, loc) != IR_NONE);
        assert(ir_layout_data(l, IR_NONE, ir_string(u, text, sizeof(text), loc), 1, 1, map, loc) != IR_NONE);
        ir_layout_data(l, IR_NONE, ir_string(u, "", 0, loc), 4, 1, NULL, loc);
        split = ir_layout_mark(l, anchor(), loc);
        target = anchor();
        ins(0xad, address(target));
        data(num(-1), 4);
        ir_layout_storage(l, IR_NONE, ir_extent_literal(u, 23, loc), loc);
        mark(target); ins(0xd0, address(target));
        assert(ir_layout_run(l));
        count = ir_layout_fact_count(l);
        bytes = ir_layout_bytes(l, &length);
        if (mode & 2) assert(!bytes && !length);
        else {
            assert(bytes && length == sizeof(reference));
            if (!mode) memcpy(reference, bytes, length);
            else assert(!memcmp(reference, bytes, length));
        }
        assert(ir_layout_fact_at(l, split, &fact) && fact.output_offset == 6162);
        /* The writer consumes the final snapshot, never the edited graph. */
        assert(ir_place(u, target, 1, 1));
        assert(!ir_layout_write_fragments(l, count, count + 1, stream));
        assert(!ir_layout_write_fragments(l, split, split - 1, stream));
        assert(!ir_layout_write_fragments(l, 0, count, NULL));
        assert(ir_layout_write_fragments(l, 0, split, stream));
        assert(ftell(stream) == 6162);
        assert(ir_layout_write_fragments(l, split, count, stream));
        assert(ftell(stream) == (long)sizeof(reference));
        rewind(stream);
        assert(fread(actual, 1, sizeof(actual), stream) == sizeof(actual));
        assert(!memcmp(actual, reference, sizeof(actual)));
        assert(fgetc(stream) == EOF && !ferror(stream));
        assert(!fclose(stream));
        /* Exercise short writes in both a partial final buffer and a whole
         * bank, then retry from the same immutable snapshot. */
        for (i = 0; i < 2; i++) {
            size_t end = i ? count : split, writes, at;
            stream = tmpfile(); assert(stream);
            test_layout_write_failure(0);
            assert(ir_layout_write_fragments(l, 0, end, stream));
            writes = test_layout_write_calls(); assert(writes);
            assert(!fclose(stream));
            for (at = 1; at <= writes; at++) {
                stream = tmpfile(); assert(stream);
                test_layout_write_failure(at);
                assert(!ir_layout_write_fragments(l, 0, end, stream));
                assert(!fclose(stream));
            }
            test_layout_write_failure(0);
        }
        ir_layout_destroy(l); ir_unit_destroy(u);
    }
    assert(!memcmp(reference, "ABC", 3));
    for (i = 3; i < 4109; i += 2) assert(reference[i] == 65 && reference[i + 1] == 0);
    for (i = 4109; i < 6162; ++i) assert(reference[i] == 9);
    assert(reference[6162] == 0xad && reference[6192] == 0xd0 && reference[6193] == 0xfe);
    puts("streamed bytes: OK");
}
static void controls(int which)
{
    ir_id a = anchor(), b = anchor();
    int i;
    if (which == 0) {
        ir_layout_ram_block(l, 0x10, 0x100);
        ir_layout_ram(l, a, literal(1), 0, 1, loc);
        ir_layout_origin(l, 0x8000, loc); mark(b);
        for (i = 0; i < 43; ++i) ins(0xad, address(a));
        ins(0xd0, address(b));
    } else if (which == 1) { ins(0xad, sub(address(a), 3)); mark(a); nop(); }
    else if (which == 2) { ins(0xad, num(16)); ins(0xad, sub(address(a), 6)); mark(a); nop(); }
    else if (which == 3) {
        ir_layout_origin(l, 0xfa, loc); ins(0xad, num(16)); ins(0xad, address(a)); nop(); mark(a); ins(0x60, IR_NONE);
    } else if (which == 4) {
        ir_layout_origin(l, 0xf6, loc); ins(0xad, address(a)); ins(0xad, address(b)); mark(b);
        for (i = 0; i < 4; ++i) nop();
        mark(a); ins(0x60, IR_NONE);
    } else if (which == 8) {
        mark(b); nop(); ins(0xad, address(b));
        ins(0xad, op(IR_SUBTRACT, num(263), op(IR_SUBTRACT, address(a), address(b))));
        nop(); mark(a); ins(0x60, IR_NONE);
    } else if (which == 9) {
        ir_id tail = anchor();
        mark(b); ins(0xad, op(IR_SUBTRACT, address(tail), address(b)));
        mark(tail); ins(0xad, op(IR_SUBTRACT, num(261), address(a)));
        mark(a); ins(0x60, IR_NONE);
    } else if (which == 7) {
        mark(b); nop(); ins(0xad, op(IR_SUBTRACT, num(262), address(a)));
        ins(0xad, address(b)); mark(a); ins(0x60, IR_NONE);
    } else {
        mark(b); nop(); ins(0xad, address(b));
        ins(0xad, op(IR_SUBTRACT, num(which == 5 ? 263 : 262), address(a)));
        nop(); mark(a); ins(0x60, IR_NONE);
    }
}
static void spans(int which)
{
    ir_id start = anchor(), head = anchor(), tail = anchor();
    ir_id difference = op(IR_SUBTRACT, address(tail), address(which == 1 ? start : head));
    if (which == 3) {
        ir_layout_storage(l, IR_NONE, storage(address(tail), 1, 0, 0), loc);
        ir_layout_origin(l, 5, loc); mark(tail); ins(0x60, IR_NONE); return;
    }
    if (which == 4) {
        ir_layout_storage(l, IR_NONE, storage(address(tail), 1, 0, 0), loc);
        mark(tail); ins(0x60, IR_NONE); return;
    }
    if (which == 5) {
        ir_layout_origin(l, 3, loc);
        ir_layout_storage(l, head, storage(sub(address(head), 2), 1, 0, 0), loc);
        data(address(head), 1); return;
    }
    mark(start);
    ir_layout_storage(l, IR_NONE, storage(difference, 1, 0, 0), loc);
    if (which == 2) ir_layout_origin(l, 0x8000, loc);
    mark(head); nop(); nop(); mark(tail); ins(0x60, IR_NONE);
}
static void extents(void)
{
    ir_id bad = ir_extent_count(u, num(-1), 0, INT32_MAX, loc);
    ir_result r;
    r = ir_evaluate(u, ir_extent_repeat(u, literal(0), bad, loc));
    assert(r.state == IR_KNOWN && r.value.kind == IR_EXTENT && r.value.as.extent == 0);
    r = ir_evaluate(u, ir_extent_sum(u, literal(100), bad, loc));
    assert(r.state == IR_INVALID && r.diagnostic.code == IR_ERROR_EXTENT);
    r = ir_evaluate(u, ir_extent_sum(u, literal(UINT64_MAX), literal(1), loc));
    assert(r.state == IR_INVALID);
    r = ir_evaluate(u, ir_extent_repeat(u, literal(UINT64_MAX), literal(2), loc));
    assert(r.state == IR_INVALID);
    r = ir_evaluate(u, ir_extent_integer(u, literal(UINT64_C(0x100000001)), loc));
    assert(r.state == IR_KNOWN && r.value.kind == IR_INTEGER && r.value.as.integer == 1);
    r = ir_evaluate(u, ir_extent_remainder(u, literal(1), literal(2), loc)); assert(r.state == IR_INVALID);
    r = ir_evaluate(u, storage(num(0), 1, 0, 0)); assert(r.state == IR_INVALID);
    r = ir_evaluate(u, storage(num(0), 1, 0, 1)); assert(r.state == IR_KNOWN && r.value.as.extent == 0);
    r = ir_evaluate(u, storage(num(65535), 1, 0, 0)); assert(r.state == IR_KNOWN);
    r = ir_evaluate(u, storage(num(65536), 1, 0, 0)); assert(r.state == IR_INVALID);
    r = ir_evaluate(u, storage(num(INT32_MAX), INT32_MAX, 0, 0)); assert(r.state == IR_INVALID);
    {
        ir_id a = anchor(), ref = address(a);
        ir_extent_anchor(u, a, op(IR_ADD, ref, num(1)));
        r = ir_evaluate_extent(u, ref); assert(r.state == IR_INVALID && r.diagnostic.code == IR_ERROR_CYCLE);
        ir_place(u, a, 1, 7);
        r = ir_evaluate(u, ref); assert(r.state == IR_KNOWN && r.value.as.integer == 7);
        r = ir_evaluate_extent(u, ref); assert(r.state == IR_INVALID && r.diagnostic.code == IR_ERROR_CYCLE);
    }
    ir_layout_storage(l, IR_NONE, ir_extent_sum(u, storage(num(40000), 1, 0, 0), storage(num(40000), 1, 0, 0), loc), loc);
}
static void dependency_modes(void)
{
    ir_id a = anchor(), ref = address(a), missing = ir_definition(u, "Missing", loc);
    ir_id other = ir_definition(u, "Other", loc), root;
    ir_result r;
    ir_extent_anchor(u, a, ir_reference(u, missing, loc));
    ir_place(u, a, 1, 9);
    root = op(IR_ADD, ref, ir_reference(u, other, loc));
    r = ir_evaluate_extent(u, root); assert(r.state == IR_DEFERRED && r.dependency_count == 2);
    r = ir_evaluate(u, ref); assert(r.state == IR_KNOWN && r.value.as.integer == 9);
    r = ir_evaluate_extent(u, root); assert(r.state == IR_DEFERRED && r.dependency_count == 2);
    ir_bind(u, missing, num(7));
    ir_place(u, a, 0, 0);
    r = ir_evaluate(u, root); assert(r.state == IR_DEFERRED && r.dependency_count == 2);
    r = ir_evaluate_extent(u, ref); assert(r.state == IR_KNOWN && r.value.as.integer == 7);
    r = ir_evaluate(u, root); assert(r.state == IR_DEFERRED && r.dependency_count == 2);
}
static void prefix_edges(void)
{
    ir_id a = anchor(), b = anchor(), ref = address(a), root = op(IR_ADD, ref, num(1));
    ir_result r;
    assert(ir_place(u, a, 1, 9));
    r = ir_evaluate_extent(u, root); assert(r.state == IR_KNOWN && r.value.as.integer == 10);
    assert(ir_extent_anchor(u, a, address(b)));
    r = ir_evaluate_extent(u, root); assert(r.state == IR_DEFERRED && r.dependency_count == 1);
    assert(ir_place(u, b, 1, 20));
    r = ir_evaluate_extent(u, root); assert(r.state == IR_KNOWN && r.value.as.integer == 21);
    assert(ir_place(u, b, 1, 30));
    r = ir_evaluate_extent(u, root); assert(r.state == IR_KNOWN && r.value.as.integer == 31);
    r = ir_evaluate(u, root); assert(r.state == IR_KNOWN && r.value.as.integer == 10);
}
static void span_barriers(int which)
{
    ir_id head = anchor(), tail = anchor(), difference = op(IR_SUBTRACT, address(tail), address(head));
    expect_no_spans = 1;
    if (which == 1) ir_layout_bank(l, 4, 4, loc); else ir_layout_origin(l, 4, loc);
    mark(head); nop();
    if (which == 0) ir_layout_origin(l, 16, loc);
    else if (which == 1) ir_layout_bank(l, 16, 4, loc);
    else ir_layout_pad(l, IR_PAD_ORIGIN, 16, loc);
    mark(tail); nop(); data(difference, 1);
}
static void strings(void)
{
    unsigned char map[256];
    ir_id text = ir_definition(u, "Text", loc), pc = anchor();
    unsigned i;
    for (i = 0; i < 256; ++i) map[i] = (unsigned char)i;
    map['H'] = 1; map['I'] = 0;
    ir_layout_origin(l, 0x8000, loc);
    ir_layout_data(l, IR_NONE, ir_reference(u, text, loc), 1, 1, map, loc);
    map['H'] = 2;
    ir_layout_data(l, IR_NONE, num('H'), 1, 1, map, loc);
    ir_layout_data(l, pc, address(pc), 2, 0, NULL, loc);
    data(ir_string(u, "AB", 2, loc), 4);
    data(ir_string(u, NULL, 0, loc), 1);
    data(ir_string(u, "A\0B", 3, loc), 1);
    ir_bind(u, text, ir_string(u, "HI", 2, loc));
}
static void ram_fixture(int which)
{
    static const unsigned sizes[] = {1,2,1,2,1,2,4,1,2,1,1};
    ir_id a[11];
    unsigned i;
    ir_layout_ram_block(l, 0x10, 0x100);
    if (which == 4) {
        for (i = 0; i < 11; ++i) a[i] = anchor();
        for (i = 0; i < 11; ++i) { unsigned j = (i + 5) % 11;
            ir_layout_ram(l, a[j], literal(sizes[j]), 0, 0, loc); }
        for (i = 0; i < 11; ++i) data(address(a[(i + 5) % 11]), 2);
    } else if (which == 0) {
        for (i = 0; i < 11; ++i) { a[i] = anchor(); ir_layout_ram(l, a[i], literal(sizes[i]), 0, 0, loc); }
        for (i = 0; i < 11; ++i) data(address(a[i]), 1);
    } else if (which == 1) {
        for (i = 0; i < 3; ++i) { a[i] = anchor(); ir_layout_ram(l, a[i], literal(i == 0 ? 0 : 1), 0, 0, loc); }
        for (i = 0; i < 3; ++i) data(address(a[i]), 1);
    } else if (which == 2) {
        a[0] = anchor(); a[1] = anchor();
        ir_layout_ram(l, a[0], literal(4), 0, 0, loc); ir_layout_ram(l, a[1], literal(1), 0, 0, loc);
        data(address(a[0]), 1); data(address(a[1]), 1);
    } else {
        a[0] = anchor(); a[1] = anchor(); a[2] = anchor();
        ir_layout_ram(l, a[0], literal(1), 0, 0, loc);
        ir_layout_ram(l, a[1], storage(sub(address(a[0]), 15), 1, 0, 0), 0, 0, loc);
        ir_layout_ram(l, a[2], literal(1), 0, 0, loc);
        data(address(a[2]), 1);
    }
}
static void banks(void)
{
    ir_id a = anchor(), b = anchor(), c = anchor();
    mark(a); data(ir_layout_bank_expression(l, a, loc), 1);
    ir_layout_bank(l, 0x8000, 8, loc); mark(b); data(address(b), 2);
    data(ir_layout_bank_expression(l, b, loc), 1);
    ir_layout_pad(l, IR_PAD_ORIGIN, 0x8005, loc); data(num(0x55), 1);
    ir_layout_bank(l, 0x8000, 4, loc); mark(c); data(ir_layout_bank_expression(l, c, loc), 1);
    ir_layout_pad(l, IR_PAD_BANK_OFFSET, 3, loc); data(num(0xaa), 1);
}
static void work(unsigned n)
{
    unsigned i;
    ir_id end = anchor(), shared = address(end);
    for (i = 0; i < 200; ++i) shared = op(IR_ADD, shared, num(0));
    for (i = 0; i < n; ++i) { mark(anchor()); ins(0xad, shared); }
    mark(end); ins(0x60, IR_NONE);
    assert(ir_layout_run(l));
    { ir_layout_work w = ir_layout_work_done(l);
      assert(w.walks <= w.candidates + 3);
      printf("%" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",
             w.walks, w.fragment_visits, w.narrowings, ir_work_done(u).evaluated_nodes); }
    ir_layout_destroy(l); ir_unit_destroy(u);
}
static void cascade(unsigned n)
{
    unsigned i;
    ir_id end = anchor();
    for (i = 0; i < n; ++i)
        ins(0xad, op(IR_ADD, address(end), num(255 - (int32_t)(3 * n) + (int32_t)i)));
    mark(end);
    assert(ir_layout_run(l));
    { size_t size; const unsigned char *bytes = ir_layout_bytes(l, &size);
      ir_layout_work w = ir_layout_work_done(l);
      assert(size == 2 * n && w.walks == n + 2 && w.narrowings == n);
      for (i = 0; i < n; ++i) { assert(bytes[2*i] == 0xa5); assert(bytes[2*i+1] == 255 - n + i); }
      printf("%" PRIu64 " %" PRIu64 "\n", w.walks, w.fragment_visits); }
    ir_layout_destroy(l); ir_unit_destroy(u);
}
static void cache_work(unsigned n)
{
    ir_id *anchors = malloc(n * sizeof(*anchors));
    ir_id shared = num(0);
    unsigned i;
    assert(anchors);
    for (i = 0; i < n; ++i) { anchors[i] = anchor(); shared = op(IR_ADD, shared, address(anchors[i])); }
    shared = ir_unary(u, IR_LOW, shared, loc);
    for (i = 0; i < n; ++i) { mark(anchors[i]); ins(0xad, shared); }
    assert(ir_layout_run(l));
    { ir_work w = ir_work_done(u); ir_layout_work t = ir_layout_work_done(l);
      printf("%" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",
             t.walks, w.evaluated_nodes, w.invalidation_edges, w.dependency_visits); }
    free(anchors); ir_layout_destroy(l); ir_unit_destroy(u);
}
static void alignment(void)
{
    ir_id a = anchor(), b = anchor(), c = anchor();
    ir_layout_ram_block(l, 0x11, 0x40);
    ir_layout_ram(l, a, literal(2), 4, 0, loc);
    ir_layout_ram(l, b, literal(3), 0, 0, loc);
    ir_layout_ram(l, c, literal(1), 0, 1, loc);
    data(address(a), 1); data(address(b), 1); data(address(c), 1);
}
static void limits(int which)
{
    if (which == 0) { ir_layout_origin(l, 0xffff, loc); nop(); data(num(1), 1); }
    else if (which == 1) { ir_layout_bank(l, 0x8000, 1, loc); nop(); nop(); }
    else if (which == 2) { nop(); ir_layout_pad(l, IR_PAD_ORIGIN, 0, loc); }
    else if (which == 3) { ir_layout_storage(l, IR_NONE, literal(UINT64_MAX), loc); }
    else if (which == 4) {
        ir_id a = anchor(); ir_layout_ram_block(l, 0x100, 0x200);
        ir_layout_ram(l, a, literal(1), 0, 1, loc);
    } else if (which == 5) {
        ir_id a = anchor(); ir_layout_ram_block(l, 0x10, 0x11);
        ir_layout_ram(l, a, literal(1), 0, 0, loc); data(address(a), 1);
    } else if (which == 6) {
        ir_id a = anchor(); ir_layout_ram_block(l, 0x10, 0x11);
        ir_layout_ram(l, a, literal(2), 0, 0, loc);
    } else if (which == 7) { ir_id a = anchor(); mark(a); mark(a); }
    else if (which == 8) { ir_id a = anchor(); mark(a); ir_layout_ram(l, a, literal(0), 0, 0, loc); }
    else if (which == 9) { ir_id a = ir_definition(u, "Missing", loc); data(ir_reference(u, a, loc), 1); }
    else if (which == 10) {
        ir_id a = anchor();
        data(op(IR_ADD, ir_string(u, "A", 1, loc), address(a)), 1); mark(a);
    } else if (which == 11) {
        ir_id a = anchor();
        ir_layout_instruction(l, IR_NONE, 0xad, address(a), 1, 0, loc); mark(a); nop();
    } else if (which == 18) {
        nop();
        loc.line = loc.end_line = 9;
        ir_layout_instruction(l, IR_NONE, 0xa9, num(256), 0, 0, loc);
    } else if (which == 19) {
        ir_location bad = loc;
        nop();
        bad.file = 99;
        ir_layout_instruction(l, IR_NONE, 0xea, IR_NONE, 0, 0, bad);
    } else if (which == 17) {
        ir_location bad = loc; bad.file = 99;
        ir_layout_origin(l, 0x10000, bad);
        assert(ir_layout_error_at(l).location.file == IR_NONE);
    } else if (which == 16) {
        ir_id head = anchor(), tail = anchor(), first = anchor(), next = anchor();
        mark(head); ins(0xad, num(16)); mark(tail);
        ir_layout_ram_block(l, 0x10, 0x100);
        ir_layout_ram(l, first, storage(op(IR_SUBTRACT, address(tail), address(head)), 1, 0, 0), 0, 0, loc);
        ir_layout_ram(l, next, literal(1), 0, 0, loc);
        data(address(first), 1); data(address(next), 1);
    } else if (which == 15) {
        ir_id later = anchor(), first = anchor(), next = anchor();
        ir_layout_ram_origin(l, 0x10, loc);
        ir_layout_ram(l, first, storage(address(later), 1, 0, 0), 0, 0, loc);
        ir_layout_ram(l, next, literal(1), 0, 0, loc);
        nop(); nop(); mark(later); data(address(next), 1);
    } else if (which == 14) {
        ir_id here = anchor(), pc = anchor();
        ir_layout_origin(l, 0xffff, loc); nop(); mark(here);
        data(address(here), 4); ir_layout_data(l, pc, address(pc), 4, 0, NULL, loc);
    } else if (which == 13) {
        ir_id code = anchor(), var = anchor();
        nop(); mark(code); nop();
        ir_layout_ram_origin(l, 0x10, loc);
        ir_layout_ram(l, var, storage(address(code), 1, 0, 0), 0, 0, loc);
        data(address(var), 1);
    } else if (which == 12) {
        ir_layout_instruction(l, IR_NONE, 0xa9, num(255), 0, 1, loc);
    }
}
extern void test_ir_allocation_failure(size_t);
extern size_t test_ir_allocation_calls(void);
extern size_t test_ir_live_allocations(void);
extern void test_ir_version_rollover(ir_unit *, int);
static void mixed_error(void)
{
    ir_id a = anchor(), ref = address(a);
    ir_id left = op(IR_DIVIDE, num(1), ref);
    ir_id right = ir_unary(u, IR_NEGATE, ir_string(u, "s", 1, loc), loc);
    ir_id root = op(IR_ADD, left, right);
    ir_result r = ir_evaluate(u, root);
    assert(r.state == IR_INVALID && r.diagnostic.code == IR_ERROR_TYPE);
    r = ir_evaluate_extent(u, ref); assert(r.state == IR_DEFERRED);
    assert(ir_place(u, a, 1, 0));
    r = ir_evaluate(u, root);
    assert(r.state == IR_INVALID && r.diagnostic.code == IR_ERROR_DIV_ZERO);
    {
        ir_id d1 = ir_definition(u, "Cycle1", loc), d2 = ir_definition(u, "Cycle2", loc);
        ir_id r1 = ir_reference(u, d1, loc), r2;
        ir_location second = loc; second.line = second.end_line = 8;
        r2 = ir_reference(u, d2, second);
        assert(ir_bind(u, d1, r2) && ir_bind(u, d2, r1));
        r = ir_evaluate(u, r1); assert(r.state == IR_INVALID && r.diagnostic.location.line == 7);
        r = ir_evaluate(u, r2); assert(r.state == IR_INVALID && r.diagnostic.location.line == 8);
        test_ir_version_rollover(u, 2);
        r = ir_evaluate(u, r1); assert(r.state == IR_INVALID && r.diagnostic.location.line == 7);
        test_ir_version_rollover(u, 2);
        r = ir_evaluate(u, r2); assert(r.state == IR_INVALID && r.diagnostic.location.line == 8);
    }
}
static void concat_reuse(int which)
{
    if (which == 0) {
        ir_id head = anchor(), tail = anchor();
        data(op(IR_ADD, ir_string(u, "AB", 2, loc), ir_string(u, "CD", 2, loc)), 1);
        ir_layout_storage(l, IR_NONE, storage(op(IR_SUBTRACT, address(tail), address(head)), 1, 0, 0), loc);
        mark(head); nop(); nop(); mark(tail); ins(0x60, IR_NONE);
    } else {
        size_t size = 8u << 20, live, calls;
        unsigned char *bytes = malloc(size);
        ir_id a = anchor(), count = ir_extent_count(u, address(a), 0, 100, loc);
        ir_id zero = ir_extent_integer(u, ir_extent_repeat(u, literal(0), count, loc), loc), root;
        const unsigned char *saved = NULL;
        ir_result r; int i;
        assert(bytes); memset(bytes, 'x', size);
        root = op(IR_ADD, ir_string(u, bytes, size, loc), zero); free(bytes);
        r = ir_evaluate(u, root); assert(r.state == IR_KNOWN);
        saved = r.value.as.string.bytes;
        /* Warm both traversal modes and the invalidation queue before counting. */
        ir_evaluate_extent(u, count); assert(ir_place(u, a, 1, 0));
        ir_evaluate_extent(u, root); calls = test_ir_allocation_calls();
        live = test_ir_live_allocations();
        for (i = 1; i <= 40; ++i) {
            ir_evaluate_extent(u, count); assert(ir_place(u, a, 1, i));
            r = i % 2 ? ir_evaluate(u, root) : ir_evaluate_extent(u, root);
            assert(r.state == IR_KNOWN && r.value.kind == IR_STRING);
            assert(r.value.as.string.bytes == saved && r.value.as.string.length == size + 1);
            assert(saved[0] == 'x' && saved[size-1] == 'x' && saved[size] == '0');
            assert(test_ir_live_allocations() == live && test_ir_allocation_calls() == calls);
        }
    }
}
static void ram_failed_split(int which)
{
    ir_id a = anchor(), b = anchor();
    if (which == 3) {
        assert(ir_layout_ram_block(l, 0xf8, 0x200));
        assert(ir_layout_ram_block(l, 0, 0x10));
        assert(ir_layout_ram(l, a, literal(1), 4, 1, loc));
        assert(ir_layout_ram(l, b, literal(16), 0, 0, loc));
    } else if (which == 2) {
        assert(ir_layout_ram_block(l, 0x11, 0x20));
        assert(ir_layout_ram_block(l, 0x43, 0x80));
        assert(ir_layout_ram(l, a, literal(0), 4, 0, loc));
        assert(ir_layout_ram(l, b, literal(1), 0, 0, loc));
    } else {
        assert(ir_layout_ram_block(l, 0x310, 0x490));
        assert(ir_layout_ram_block(l, 0x800, 0xa00));
        if (!which) assert(ir_layout_ram_block(l, 0x1000, 0x1200));
        assert(ir_layout_ram(l, a, literal(256), 8, 0, loc));
        assert(ir_layout_ram(l, b, literal(0x120), 0, 0, loc));
    }
    data(address(a), 2); data(address(b), 2);
}
extern void test_layout_allocation_failure(size_t);
extern size_t test_layout_allocation_calls(void);
extern size_t test_layout_live_allocations(void);
static const char *allocation_copy;
static int allocation_sample(void)
{
    ir_unit *values = ir_unit_create();
    ir_layout *layout;
    ir_location at = ir_no_location();
    ir_id a, count, span;
    unsigned char map[256];
    int ok;
    unsigned i;
    if (!values) return 0;
    for (i = 0; i < 256; ++i) map[i] = (unsigned char)i;
    layout = ir_layout_create(values, IR_LINK_LAYOUT);
    if (!layout) { ir_unit_destroy(values); return 0; }
    a = ir_anchor(values, at);
    count = ir_integer(values, 2, at);
    span = ir_extent_count(values, count, 1, 65535, at);
    ir_layout_ram_block(layout, 0x11, 0x100);
    ir_layout_ram(layout, a, span, 4, 1, at);
    ir_layout_bank(layout, 0x8000, 2000, at);
    ir_layout_copy(layout, allocation_copy, at);
    for (i = 0; i < 40; ++i) {
        ir_id pc = ir_anchor(values, at), def = ir_definition(values, "value", at);
        ir_id expression = ir_reference(values, def, at);
        ir_layout_data(layout, pc, expression, 1, 1, map, at);
        ir_bind(values, def, ir_binary(values, IR_ADD,
            ir_string(values, "hel", 3, at), ir_string(values, "lo", 2, at), at));
        ir_layout_instruction(layout, IR_NONE, 0xad, ir_address(values, a, at), 0, 0, at);
        ir_layout_storage(layout, IR_NONE, span, at);
        ir_layout_raw(layout, "raw", 3, at);
    }
    {
        ir_id head = ir_anchor(values, at), tail = ir_anchor(values, at);
        ir_id difference = ir_binary(values, IR_SUBTRACT, ir_address(values, tail, at), ir_address(values, head, at), at);
        ir_layout_storage(layout, IR_NONE, ir_extent_count(values, difference, 1, 65535, at), at);
        ir_layout_mark(layout, head, at);
        ir_layout_raw(layout, "xx", 2, at);
        ir_layout_mark(layout, tail, at);
    }
    ir_reserve_expressions(values, 1000);
    ok = ir_layout_run(layout);
    if (!ok) {
        size_t n;
        assert(!ir_layout_bytes(layout, &n) && n == 0);
        assert(!ir_layout_facts(layout, &n) && n == 0);
    }
    ir_layout_destroy(layout); ir_unit_destroy(values);
    assert(!test_ir_live_allocations() && !test_layout_live_allocations());
    return ok;
}
static void allocations(void)
{
    size_t value_calls, layout_calls, i;
    test_ir_allocation_failure(0); test_layout_allocation_failure(0);
    assert(allocation_sample());
    value_calls = test_ir_allocation_calls(); layout_calls = test_layout_allocation_calls();
    for (i = 1; i <= value_calls; ++i) {
        test_ir_allocation_failure(i); test_layout_allocation_failure(0);
        assert(!allocation_sample());
    }
    for (i = 1; i <= layout_calls; ++i) {
        test_ir_allocation_failure(0); test_layout_allocation_failure(i);
        assert(!allocation_sample());
    }
    printf("%zu %zu\n", value_calls, layout_calls);
}
static ir_id rooted_span_value(void *context, ir_id left, ir_id right, ir_location at)
{
    unsigned *calls = context;
    (void)left; (void)right;
    ++*calls;
    return ir_integer(u, 12, at);
}
static void rooted_spans(void)
{
    ir_id a = anchor(), b = anchor(), c = anchor(), d = anchor(), pc = anchor();
    ir_id selected = op(IR_SUBTRACT, address(a), address(b));
    ir_id ordinary = op(IR_SUBTRACT, address(c), address(d));
    ir_id definition = ir_definition(u, "extent alias", loc);
    ir_id root;
    unsigned calls = 0;
    assert(ir_bind(u, definition, selected));
    assert(ir_extent_anchor(u, pc, ir_reference(u, definition, loc)));
    root = ir_extent_count(u, address(pc), 1, 100, loc);
    assert(ir_lower_extent_spans(u, &root, 1, rooted_span_value, &calls));
    assert(calls == 1 && test_ir_span_nodes(u) == 1);
    assert(ir_place(u, a, 1, 10) && ir_place(u, b, 1, 2));
    assert(ir_place(u, c, 1, 20) && ir_place(u, d, 1, 4));
    assert(ir_reserve_expressions(u, 1000));
    assert(ir_evaluate(u, selected).value.as.integer == 8);
    assert(ir_evaluate_extent(u, root).value.as.extent == 12);
    assert(ir_evaluate_extent(u, ordinary).value.as.integer == 16);
    assert(ir_lower_extent_spans(u, NULL, 0, rooted_span_value, &calls));
    assert(calls == 1);
    root = IR_NONE;
    assert(!ir_lower_extent_spans(u, &root, 1, rooted_span_value, &calls));
    assert(ir_unit_error(u).code == IR_ERROR_REFERENCE);
    ir_layout_destroy(l); ir_unit_destroy(u);
    assert(!test_ir_live_allocations());
    u = ir_unit_create();
    assert(u && !ir_reserve_expressions(u, SIZE_MAX));
    assert(ir_unit_error(u).code == IR_ERROR_MEMORY);
    ir_unit_destroy(u);
    assert(!test_ir_live_allocations());
    puts("rooted spans: OK");
}
extern void test_layout_cache(void);
int main(int argc, char **argv)
{
    int which;
    assert(argc >= 3);
    if (!strcmp(argv[1], "cache-differential")) {
        test_layout_cache(); assert(!test_ir_live_allocations()); return 0;
    }
    if (!strcmp(argv[1], "allocations")) { assert(argc == 5); allocation_copy = argv[4]; allocations(); return 0; }
    if (!strcmp(argv[1], "compact-facts")) { compact_facts(); assert(!test_ir_live_allocations()); return 0; }
    if (!strcmp(argv[1], "ram-analysis")) { ram_analysis_facts(); assert(!test_ir_live_allocations()); return 0; }
    if (!strcmp(argv[1], "streamed-bytes")) { streamed_bytes(); assert(!test_ir_live_allocations()); return 0; }
    begin(atoi(argv[2])); which = argc > 3 ? atoi(argv[3]) : 0;
    if (!strcmp(argv[1], "rooted-spans")) { rooted_spans(); return 0; }
    if (!strcmp(argv[1], "controls")) controls(which);
    else if (!strcmp(argv[1], "spans")) spans(which);
    else if (!strcmp(argv[1], "extents")) extents();
    else if (!strcmp(argv[1], "dependency-modes")) dependency_modes();
    else if (!strcmp(argv[1], "mixed-error")) mixed_error();
    else if (!strcmp(argv[1], "prefix-edges")) prefix_edges();
    else if (!strcmp(argv[1], "span-barriers")) span_barriers(which);
    else if (!strcmp(argv[1], "early-truncation")) {
        ir_layout_instruction(l, IR_NONE, 0xa9, num(0x1ff), 0, which, loc);
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
        ir_id operand = width == 1 ? IR_NONE : num(width == 3 ? 0x1234 :
            (((unsigned)which & 0x1fu) == 0x10u ? 129 : 127));
        ir_layout_instruction(l, IR_NONE, (unsigned char)which, operand, 1, 0, loc);
    }
    else if (!strcmp(argv[1], "literal") || !strcmp(argv[1], "expression-literal")) {
        int32_t value;
        assert(argc == 7);
        value = (int32_t)strtoll(argv[4], NULL, 0);
        if (!strcmp(argv[1], "literal"))
            ir_layout_literal_instruction(l, IR_NONE, (unsigned char)which, value,
                                          atoi(argv[5]), atoi(argv[6]), loc);
        else
            ir_layout_instruction(l, IR_NONE, (unsigned char)which, num(value),
                                  atoi(argv[5]), atoi(argv[6]), loc);
    }
    else if (!strcmp(argv[1], "range")) {
        assert(argc == 6);
        if (which == 0) data(num((int32_t)strtoll(argv[4], NULL, 0)), (unsigned)atoi(argv[5]));
        else ins((unsigned char)which, num((int32_t)strtoll(argv[4], NULL, 0)));
    } else if (!strcmp(argv[1], "copy")) {
        FILE *f;
        assert(argc == 5); ir_layout_copy(l, argv[4], loc);
        if (which) { f = fopen(argv[4], "wb"); assert(f);
            if (which == 1) fputs("changed", f);
            else if (which == 2) fwrite("NEQ\x1a", 1, 4, f);
            else fputc('N', f);
            fclose(f); }
    } else assert(0);
    finish(); assert(!test_ir_live_allocations()); return 0;
}
