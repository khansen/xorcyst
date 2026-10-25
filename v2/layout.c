#include "heap.h"
#include "layout.h"
#include "graph.h"
#include "../opcode.h"
#include "../expr_int.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { ORIGIN, BANK, MARK, RAW, COPY, DATA, INSTRUCTION, STORAGE, PAD, RAM, RAM_ORIGIN, BANK_END } kind;
typedef struct {
    v2_id loc;
    v2_id anchor, expression, extent;
    uint32_t run;
    unsigned kind:4, opcode:8, narrow:8, width:3;
    unsigned strings:1, mapped:1, early:1, literal_operand:1, string_value:1, has_operand:1, warning:1;
    v2_id payload; /* auxiliary record, or an instruction width anchor */
} fragment;
typedef union {
    struct { unsigned char *bytes; char *path; } raw;
    /* During preparation only the length is needed; after validation the
     * fragment holds its final length and this slot retains string bytes. */
    struct { union { const unsigned char *bytes; size_t length; } value; unsigned char *map; } data;
    struct { uint32_t argument, origin; v2_pad_kind pad; } placement;
} fragment_payload;

typedef struct {
    uint32_t address, output_offset, length;
    int32_t bank, operand;
} placement;
typedef struct { v2_id first; int32_t bank; uint32_t address, output_offset; } checkpoint;
typedef struct { v2_id anchor, extent; unsigned align, zp; uint32_t size, key; v2_location loc; } ram;
typedef struct { uint32_t start, end; } block;
typedef struct { v2_id anchor, definition; v2_location loc; } bank_request;
typedef struct { v2_id anchor, fragment; } anchor_position;
struct v2_layout {
    v2_heap *heap;
    v2_unit *unit;
    v2_layout_mode mode;
    fragment **fragments;
    fragment_payload **payloads;
    size_t payload_count, payload_capacity;
    unsigned char **raw_blocks;
    size_t raw_count, raw_capacity, raw_used, raw_size;
    ram *ram;
    block *blocks;
    bank_request *banks;
    anchor_position *positions;
    v2_id *tree;
    v2_id fixed_extents[256];
    size_t tree_base, position_count;
    size_t count, capacity, ram_count, ram_capacity, block_count, block_capacity;
    size_t bank_count, bank_capacity;
    checkpoint *checkpoints;
    size_t checkpoint_count;
    v2_layout_fact *facts;
    unsigned keep_facts, keep_bytes;
    unsigned char *output;
    size_t output_size;
    unsigned started, ready, bank_calls;
    v2_layout_diagnostic error;
    v2_layout_work work;
};

#define FRAGMENT_SHIFT 9u
#define FRAGMENT_BLOCK (1u << FRAGMENT_SHIFT)
static fragment *fragment_at(const v2_layout *l, size_t id)
{ return &l->fragments[id >> FRAGMENT_SHIFT][id & (FRAGMENT_BLOCK - 1)]; }

/* Origins and bank transitions delimit affine address runs. Retain one
 * checkpoint per transition, rather than an address/offset/bank tuple for
 * every byte fragment. After validation, a fragment's no-longer-needed
 * extent ID and anchor ID hold its final offset and numeric operand; run
 * holds its length once prefix lowering has finished. */
static const checkpoint *checkpoint_at(const v2_layout *l, size_t fragment_id)
{
    size_t lo = 0, hi = l->checkpoint_count;
    while (lo + 1 < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (l->checkpoints[mid].first <= fragment_id) lo = mid;
        else hi = mid;
    }
    return &l->checkpoints[lo];
}
static placement final_placement(const v2_layout *l, size_t i)
{
    placement p;
    if (l->facts) {
        const v2_layout_fact *f = &l->facts[i];
        p = (placement){f->address, f->output_offset, f->length, f->bank, f->operand};
    } else {
        const fragment *f = fragment_at(l, i);
        const checkpoint *c = checkpoint_at(l, i);
        p.address = c->address + (f->extent - c->output_offset);
        p.output_offset = f->extent; p.length = f->run; p.bank = c->bank;
        p.operand = f->has_operand ? expr_i32(f->anchor) : 0;
    }
    return p;
}
static fragment_payload *extra(const v2_layout *l, const fragment *f)
{ return &l->payloads[f->payload >> FRAGMENT_SHIFT][f->payload & (FRAGMENT_BLOCK - 1)]; }

static int layout_fail(v2_layout *l, v2_layout_error code, v2_location loc)
{
    l->ready = 0;
    if (loc.file != V2_NONE && (!v2_file_name(l->unit, loc.file) ||
        !loc.line || !loc.column || !loc.end_column || loc.end_line < loc.line ||
        (loc.end_line == loc.line && loc.end_column < loc.column)))
        loc = v2_no_location();
    if (l->error.code == V2_LAYOUT_OK) { l->error.code = code; l->error.location = loc; }
    return 0;
}
static int unit_ok(v2_layout *l)
{
    v2_diagnostic d = v2_unit_error(l->unit);
    if (!d.code) return 1;
    if (!l->error.code) { l->error.value_error = d; layout_fail(l, V2_LAYOUT_VALUE, d.location); }
    return 0;
}
static void *reserve(v2_layout *l, void *p, size_t *capacity, size_t count, size_t width)
{
    size_t n = *capacity ? *capacity : 16;
    void *next;
    if (l->error.code || count <= *capacity) return p;
    if (count > UINT32_MAX || count > SIZE_MAX / width) goto failed;
    while (n < count) {
        if (n > UINT32_MAX / 2 || n > SIZE_MAX / width / 2) { n = count; break; }
        n *= 2;
    }
    next = v2_realloc(l->heap, p, n * width);
    if (!next) goto failed;
    *capacity = n; return next;
failed:
    layout_fail(l, V2_LAYOUT_MEMORY, v2_no_location()); return p;
}
/* Raw runs often contain only a few bytes. Store their owned payloads in
 * slabs rather than a separate malloc allocation for each source statement. */
static unsigned char *raw_storage(v2_layout *l, size_t size, v2_location loc)
{
    unsigned char *p;
    if (!size) return NULL;
    if (size > l->raw_size - l->raw_used) {
        size_t n = size > 4096 ? size : 4096;
        l->raw_blocks = reserve(l, l->raw_blocks, &l->raw_capacity, l->raw_count + 1, sizeof(*l->raw_blocks));
        if (l->error.code) return NULL;
        p = v2_records(l->heap, n);
        if (!p) { layout_fail(l, V2_LAYOUT_MEMORY, loc); return NULL; }
        l->raw_blocks[l->raw_count++] = p; l->raw_used = 0; l->raw_size = n;
    }
    p = l->raw_blocks[l->raw_count - 1] + l->raw_used; l->raw_used += size;
    return p;
}
static int building(v2_layout *l, v2_location loc)
{
    /* A layout is a single construction/run, including failed runs. */
    if (l->started) return layout_fail(l, V2_LAYOUT_INPUT, loc);
    if (loc.file != V2_NONE && (!v2_file_name(l->unit, loc.file) ||
        !loc.line || !loc.column || !loc.end_column || loc.end_line < loc.line ||
        (loc.end_line == loc.line && loc.end_column < loc.column)))
        return layout_fail(l, V2_LAYOUT_INPUT, v2_no_location());
    return !l->error.code && unit_ok(l);
}
/* These are synthetic fixed widths, not source expressions. The fragment
 * retains its own source span and all range checks use that span. */
static v2_id fixed_extent(v2_layout *l, uint64_t size, v2_location loc)
{
    if (size >= 256) return v2_extent_literal(l->unit, size, loc);
    if (l->fixed_extents[size] == V2_NONE)
        l->fixed_extents[size] = v2_extent_literal(l->unit, size, v2_no_location());
    return l->fixed_extents[size];
}
static fragment fresh(v2_layout *l, kind k, v2_location loc)
{
    fragment f;
    memset(&f, 0, sizeof(f));
    f.kind = k; f.loc = v2_graph_location_id(l->unit, loc); f.anchor = f.expression = f.extent = V2_NONE;
    f.narrow = 0xff;
    f.payload = V2_NONE;
    if (k == RAW || k == COPY || k == DATA || k == ORIGIN || k == BANK || k == PAD || k == RAM || k == RAM_ORIGIN) {
        size_t block = l->payload_count >> FRAGMENT_SHIFT, old = l->payload_capacity;
        l->payloads = reserve(l, l->payloads, &l->payload_capacity, block + 1, sizeof(*l->payloads));
        if (old < l->payload_capacity) memset(l->payloads + old, 0, (l->payload_capacity - old) * sizeof(*l->payloads));
        if (l->error.code) return f;
        if (!l->payloads[block] && !(l->payloads[block] = v2_records(l->heap, FRAGMENT_BLOCK * sizeof(fragment_payload)))) {
            layout_fail(l, V2_LAYOUT_MEMORY, loc); return f;
        }
        f.payload = (v2_id)l->payload_count++;
        memset(extra(l, &f), 0, sizeof(fragment_payload));
    }

    return f;
}
static v2_id add(v2_layout *l, fragment f)
{
    if (!building(l, v2_graph_location(l->unit, f.loc))) goto failed;
    if (f.anchor != V2_NONE) v2_anchor_value(l->unit, f.anchor, NULL, NULL, v2_graph_location(l->unit, f.loc));
    if (f.extent == V2_NONE) f.extent = fixed_extent(l, 0, v2_graph_location(l->unit, f.loc));
    {
        size_t old = l->capacity, block = l->count >> FRAGMENT_SHIFT;
        l->fragments = reserve(l, l->fragments, &l->capacity, block + 1, sizeof(*l->fragments));
        if (old < l->capacity) memset(l->fragments + old, 0, (l->capacity - old) * sizeof(*l->fragments));
        if (l->error.code || !unit_ok(l)) goto failed;
        if (!l->fragments[block]) {
            l->fragments[block] = v2_records(l->heap, FRAGMENT_BLOCK * sizeof(fragment));
            if (!l->fragments[block]) { layout_fail(l, V2_LAYOUT_MEMORY, v2_graph_location(l->unit, f.loc)); goto failed; }
        }
    }
    (*fragment_at(l, l->count)) = f;
    return (v2_id)l->count++;
failed:
    if (f.payload != V2_NONE && f.kind == COPY) { v2_free(l->heap, extra(l, &f)->raw.bytes); v2_free(l->heap, extra(l, &f)->raw.path); }
    if (f.payload != V2_NONE && f.kind == DATA) v2_free(l->heap, extra(l, &f)->data.map);
    return V2_NONE;
}
v2_layout *v2_layout_create(v2_unit *u, v2_layout_mode mode)
{
    v2_layout *l;
    if (!u || (mode != V2_BINARY_LAYOUT && mode != V2_LINK_LAYOUT)) return NULL;
    l = v2_calloc(v2_graph_heap(u), 1, sizeof(*l));
    if (l) {
        size_t i;
        l->heap = v2_graph_heap(u); l->unit = u; l->mode = mode; l->keep_facts = l->keep_bytes = 1; l->error.location = v2_no_location();
        for (i = 0; i < 256; ++i) l->fixed_extents[i] = V2_NONE;
    }
    return l;
}
void v2_layout_destroy(v2_layout *l)
{
    size_t i;
    if (!l) return;
    for (i = 0; i < l->count; ++i) {
        fragment *f = fragment_at(l, i);
        if (f->kind == COPY) { v2_free(l->heap, extra(l, f)->raw.bytes); v2_free(l->heap, extra(l, f)->raw.path); }
        if (f->kind == DATA) v2_free(l->heap, extra(l, f)->data.map);
    }
    for (i = 0; i < l->raw_count; i++) v2_free(l->heap, l->raw_blocks[i]);
    for (i = 0; i < l->capacity; i++) v2_free(l->heap, l->fragments[i]);
    for (i = 0; i < l->payload_capacity; i++) v2_free(l->heap, l->payloads[i]);
    v2_free(l->heap, l->raw_blocks);
    v2_free(l->heap, l->fragments); v2_free(l->heap, l->payloads); v2_free(l->heap, l->ram); v2_free(l->heap, l->blocks); v2_free(l->heap, l->banks);
    v2_free(l->heap, l->positions); v2_free(l->heap, l->tree); v2_free(l->heap, l->facts); v2_free(l->heap, l->checkpoints); v2_free(l->heap, l->output); v2_free(l->heap, l);
}
v2_layout_diagnostic v2_layout_error_at(const v2_layout *l) { return l->error; }
v2_layout_work v2_layout_work_done(const v2_layout *l) { return l->work; }
const char *v2_layout_error_message(v2_layout_error e)
{
    switch (e) {
    case V2_LAYOUT_OK: return "no error";
    case V2_LAYOUT_MEMORY: return "out of memory laying out unit";
    case V2_LAYOUT_INPUT: return "invalid layout input";
    case V2_LAYOUT_VALUE: return "invalid layout expression";
    case V2_LAYOUT_UNRESOLVED: return "layout value remains unresolved";
    case V2_LAYOUT_RANGE: return "operand out of range";
    case V2_LAYOUT_RAM: return "RAM placement failed";
    case V2_LAYOUT_OVERFLOW: return "layout exceeds address, bank or output limit";
    case V2_LAYOUT_COPY_CHANGED: return "copy input changed after capture";
    case V2_LAYOUT_UNSTABLE: return "layout did not stabilize";
    }
    return "invalid layout diagnostic";
}
v2_id v2_layout_origin(v2_layout *l, uint32_t origin, v2_location loc)
{
    fragment f = fresh(l, ORIGIN, loc);
    if (l->error.code) return V2_NONE;
    extra(l, &f)->placement.origin = origin;
    if (origin > 65535) layout_fail(l, V2_LAYOUT_INPUT, loc);
    return add(l, f);
}
v2_id v2_layout_bank(v2_layout *l, uint32_t origin, uint32_t size, v2_location loc)
{
    fragment f = fresh(l, BANK, loc);
    if (l->error.code) return V2_NONE;
    extra(l, &f)->placement.origin = origin; extra(l, &f)->placement.argument = size;
    v2_id id;
    if (origin > 65535 || !size) layout_fail(l, V2_LAYOUT_INPUT, loc);
    if (l->bank_calls && add(l, fresh(l, BANK_END, loc)) == V2_NONE) return V2_NONE;
    id = add(l, f);
    if (id != V2_NONE) ++l->bank_calls;
    return id;
}
v2_id v2_layout_mark(v2_layout *l, v2_id anchor, v2_location loc)
{
    fragment f = fresh(l, MARK, loc); f.anchor = anchor;
    if (anchor == V2_NONE) layout_fail(l, V2_LAYOUT_INPUT, loc);
    return add(l, f);
}
v2_id v2_layout_raw_anchored(v2_layout *l, v2_id anchor, const void *bytes, size_t size, v2_location loc)
{
    fragment f = fresh(l, RAW, loc);
    f.anchor = anchor;
    if (l->error.code) return V2_NONE;
    if ((size && !bytes) || size > UINT32_MAX) {
        layout_fail(l, V2_LAYOUT_INPUT, loc);
        return V2_NONE;
    }
    if (!building(l, loc)) return V2_NONE;
    if (size) {
        extra(l, &f)->raw.bytes = raw_storage(l, size, loc);
        if (!extra(l, &f)->raw.bytes) { layout_fail(l, V2_LAYOUT_MEMORY, loc); return V2_NONE; }
        memcpy(extra(l, &f)->raw.bytes, bytes, size);
    }
    f.expression = (uint32_t)size; f.extent = fixed_extent(l, size, loc);
    return add(l, f);
}
v2_id v2_layout_raw(v2_layout *l, const void *bytes, size_t size, v2_location loc)
{ return v2_layout_raw_anchored(l, V2_NONE, bytes, size, loc); }
v2_id v2_layout_copy(v2_layout *l, const char *path, v2_location loc)
{
    FILE *file;
    long length;
    fragment f = fresh(l, COPY, loc);
    if (l->error.code) return V2_NONE;
    if (!building(l, loc)) return V2_NONE;
    if (!path || !(file = fopen(path, "rb"))) { layout_fail(l, V2_LAYOUT_INPUT, loc); return V2_NONE; }
    if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 ||
        (uint64_t)length > UINT32_MAX || fseek(file, 0, SEEK_SET)) {
        fclose(file); layout_fail(l, V2_LAYOUT_INPUT, loc); return V2_NONE;
    }
    f.expression = (uint32_t)length;
    extra(l, &f)->raw.path = v2_malloc(l->heap, strlen(path) + 1);
    extra(l, &f)->raw.bytes = f.expression ? v2_malloc(l->heap, f.expression) : NULL;
    if (!extra(l, &f)->raw.path || (f.expression && !extra(l, &f)->raw.bytes)) layout_fail(l, V2_LAYOUT_MEMORY, loc);
    else {
        strcpy(extra(l, &f)->raw.path, path);
        if (f.expression && fread(extra(l, &f)->raw.bytes, 1, f.expression, file) != f.expression) layout_fail(l, V2_LAYOUT_INPUT, loc);
    }
    if (fclose(file)) layout_fail(l, V2_LAYOUT_INPUT, loc);
    f.extent = v2_extent_literal(l->unit, f.expression, loc);
    return add(l, f);
}
v2_id v2_layout_data(v2_layout *l, v2_id pc, v2_id expr, unsigned width,
                      int strings, const unsigned char *map, v2_location loc)
{
    fragment f = fresh(l, DATA, loc);
    if (l->error.code) return V2_NONE;
    if ((width != 1 && width != 2 && width != 4) || (map && width != 1))
        layout_fail(l, V2_LAYOUT_INPUT, loc);
    if (!building(l, loc)) return V2_NONE;
    f.anchor = pc; f.expression = expr; f.width = width & 7u; f.strings = strings != 0;
    if (map) {
        extra(l, &f)->data.map = v2_malloc(l->heap, 256);
        if (!extra(l, &f)->data.map) { layout_fail(l, V2_LAYOUT_MEMORY, loc); return V2_NONE; }
        memcpy(extra(l, &f)->data.map, map, 256); f.mapped = 1;
    }
    f.extent = fixed_extent(l, width, loc);
    return add(l, f);
}
static v2_id instruction(v2_layout *l, v2_id pc, unsigned char opcode,
                        v2_id operand, const int32_t *literal, int wide, int early, v2_location loc)
{
    fragment f = fresh(l, INSTRUCTION, loc);
    instr_mnemonic mnemonic;
    unsigned width = (unsigned)opcode_length(opcode);
    if (!opcode_mnemonic(opcode, &mnemonic) ||
        (width > 1) != (operand != V2_NONE || literal != NULL))
        layout_fail(l, V2_LAYOUT_INPUT, loc);
    if (!building(l, loc)) return V2_NONE;
    f.anchor = pc; f.opcode = opcode; f.expression = operand; f.early = early != 0;
    if (!wide) f.narrow = opcode_zp_equiv(opcode, opcode_addressing_mode(opcode));
    if (literal) {
        f.literal_operand = 1;
        f.expression = (uint32_t)*literal;
        if (*literal < 0 || *literal > 255) f.narrow = 0xff;
    }
    /* The initial width is fixed until preparation has bound banks and RAM.
     * Only surviving layout-dependent candidates need length anchors. */
    f.extent = fixed_extent(l, width, loc);
    return add(l, f);
}
v2_id v2_layout_instruction(v2_layout *l, v2_id pc, unsigned char opcode,
                            v2_id operand, int wide, int early, v2_location loc)
{ return instruction(l, pc, opcode, operand, NULL, wide, early, loc); }

v2_id v2_layout_literal_instruction(v2_layout *l, v2_id pc, unsigned char opcode,
                                    int32_t operand, int wide, int early, v2_location loc)
{ return instruction(l, pc, opcode, V2_NONE, &operand, wide, early, loc); }
v2_id v2_layout_storage(v2_layout *l, v2_id pc, v2_id extent, v2_location loc)
{
    fragment f = fresh(l, STORAGE, loc); f.anchor = pc; f.extent = extent;
    if (extent == V2_NONE) layout_fail(l, V2_LAYOUT_INPUT, loc);
    return add(l, f);
}
v2_id v2_layout_pad(v2_layout *l, v2_pad_kind kind, uint32_t value, v2_location loc)
{
    fragment f = fresh(l, PAD, loc);
    if (l->error.code) return V2_NONE;
    extra(l, &f)->placement.pad = kind; extra(l, &f)->placement.argument = value;
    if (kind < V2_PAD_SIZE || kind > V2_PAD_BANK_OFFSET) layout_fail(l, V2_LAYOUT_INPUT, loc);
    if (kind == V2_PAD_SIZE) f.extent = v2_extent_literal(l->unit, value, loc);
    return add(l, f);
}
int v2_layout_ram_block(v2_layout *l, uint32_t start, uint32_t end)
{
    block b;
    if (!building(l, v2_no_location())) return 0;
    if (start >= end || end > 65536) return layout_fail(l, V2_LAYOUT_INPUT, v2_no_location());
    b.start = start; b.end = end;
    l->blocks = reserve(l, l->blocks, &l->block_capacity, l->block_count + 1, sizeof(b));
    if (l->error.code) return 0;
    l->blocks[l->block_count++] = b; return 1;
}
int v2_layout_ram(v2_layout *l, v2_id anchor, v2_id extent, unsigned align,
                   int zp, v2_location loc)
{
    ram r;
    if (!building(l, loc)) return 0;
    if (align > 15 || anchor == V2_NONE || extent == V2_NONE) return layout_fail(l, V2_LAYOUT_INPUT, loc);
    memset(&r, 0, sizeof(r)); r.anchor = anchor; r.extent = extent;
    r.align = align; r.zp = zp != 0; r.loc = loc;
    v2_anchor_value(l->unit, anchor, NULL, NULL, loc);
    r.extent = v2_extent_bound(l->unit, extent, 0, UINT32_MAX, loc);
    if (!unit_ok(l)) return 0;
    l->ram = reserve(l, l->ram, &l->ram_capacity, l->ram_count + 1, sizeof(r));
    if (l->error.code) return 0;
    { fragment f = fresh(l, RAM, loc);
      if (l->error.code) return 0;
    extra(l, &f)->placement.argument = (uint32_t)l->ram_count;
      if (add(l, f) == V2_NONE) return 0; }
    l->ram[l->ram_count++] = r; return 1;
}
v2_id v2_layout_bank_expression(v2_layout *l, v2_id anchor, v2_location loc)
{
    bank_request b;
    if (!building(l, loc)) return V2_NONE;
    if (l->mode != V2_LINK_LAYOUT || anchor == V2_NONE) {
        layout_fail(l, V2_LAYOUT_INPUT, loc); return V2_NONE;
    }
    b.anchor = anchor; b.loc = loc; b.definition = v2_definition(l->unit, "bank", loc);
    l->banks = reserve(l, l->banks, &l->bank_capacity, l->bank_count + 1, sizeof(b));
    if (l->error.code) return V2_NONE;
    l->banks[l->bank_count++] = b;
    return v2_reference(l->unit, b.definition, loc);
}

static int evaluated(v2_layout *l, v2_id expr, v2_location loc, v2_value *value)
{
    v2_result r = v2_evaluate(l->unit, expr);
    if (r.state == V2_KNOWN) { *value = r.value; return 1; }
    if (r.state == V2_INVALID) {
        if (!l->error.code) l->error.value_error = r.diagnostic;
        layout_fail(l, V2_LAYOUT_VALUE, r.diagnostic.location); return 0;
    }
    layout_fail(l, V2_LAYOUT_UNRESOLVED, loc); return 0;
}
static int extent(v2_layout *l, v2_id expr, v2_location loc, uint64_t *size)
{
    v2_result r = v2_evaluate_extent(l->unit, expr);
    if (r.state == V2_INVALID) {
        l->error.value_error = r.diagnostic;
        layout_fail(l, V2_LAYOUT_VALUE, r.diagnostic.location); return 0;
    }
    if (r.state == V2_DEFERRED) { layout_fail(l, V2_LAYOUT_UNRESOLVED, loc); return 0; }
    if (r.value.kind != V2_EXTENT) { layout_fail(l, V2_LAYOUT_INPUT, loc); return 0; }
    *size = r.value.as.extent; return 1;
}

/* Tail recursion elimination keeps C stack bounded by log N while preserving
 * exactly the v1 last-pivot permutation, including ties. */
static void ram_sort(ram *a, size_t lo, size_t hi)
{
    while (hi > lo + 1) {
        size_t i = lo, j;
        ram t, pivot = a[hi - 1];
        for (j = lo; j + 1 < hi; ++j) if (a[j].key >= pivot.key) {
            t = a[i]; a[i++] = a[j]; a[j] = t;
        }
        a[hi - 1] = a[i]; a[i] = pivot;
        if (i - lo < hi - i - 1) { ram_sort(a, lo, i); lo = i + 1; }
        else { ram_sort(a, i + 1, hi); hi = i; }
    }
}
static int ram_size(v2_layout *l, ram *r)
{
    uint64_t size;
    if (!extent(l, r->extent, r->loc, &size)) return 0;
    if (size > (l->mode == V2_LINK_LAYOUT ? UINT64_C(65536) : UINT32_MAX))
        return layout_fail(l, V2_LAYOUT_RAM, r->loc);
    r->size = (uint32_t)size;
    r->key = (r->zp << 31) | ((r->align ? r->align : 1) << 24) | (0x10000u - r->size);
    return 1;
}
static int allocate_ram(v2_layout *l)
{
    size_t i, j;
    if (l->mode != V2_LINK_LAYOUT) return 1;
    {
        for (i = 0; i < l->ram_count; ++i) if (!ram_size(l, &l->ram[i])) return 0;
        /* v1 calculates data sizes but skips placement without ram{} blocks.
         * Leave anchors unresolved: an actual address use still fails. Check
         * this before allocation, which can consume all supplied blocks. */
        if (!l->block_count) return 1;
        ram_sort(l->ram, 0, l->ram_count);
    }
    for (i = 0; i < l->ram_count; ++i) {
        ram *r = &l->ram[i];
        int placed = 0;
        for (j = 0; j < l->block_count; ++j) {
            uint32_t start = l->blocks[j].start, end = l->blocks[j].end;
            uint32_t mask = r->align ? (1u << r->align) - 1 : 0;
            uint32_t aligned = (start + mask) & ~mask;
            if ((r->zp && start >= 256) || r->size > end - start) continue;
            if (aligned != start) {
                /* v1 keeps this split even if the aligned remainder cannot
                 * fit the object. Recheck that remainder on the next iteration.
                 * An alignment point at the exclusive end is not a block. */
                if (aligned < end) {
                    l->blocks = reserve(l, l->blocks, &l->block_capacity,
                                        l->block_count + 1, sizeof(block));
                    if (l->error.code) return 0;
                    memmove(l->blocks + j + 1, l->blocks + j,
                            (l->block_count - j) * sizeof(block));
                    ++l->block_count;
                    l->blocks[j].end = aligned;
                    l->blocks[j + 1].start = aligned;
                }
                continue;
            }
            l->blocks[j].start = aligned + r->size;
            if (l->blocks[j].start == end) {
                memmove(l->blocks + j, l->blocks + j + 1,
                        (l->block_count - j - 1) * sizeof(block));
                --l->block_count;
            }
            if (!v2_place(l->unit, r->anchor, 1, (int32_t)aligned)) return unit_ok(l);
            placed = 1; break;
        }
        if (!placed)
            return layout_fail(l, V2_LAYOUT_RAM, r->loc);
    }
    return 1;
}
/* Binary data placement is a separate source-order cursor. Alignment and
 * zeropage annotations are linker constraints in v1, not binary padding. */
static int binary_ram(v2_layout *l, ram *r, uint32_t *pc, int partial)
{
    v2_result result;
    if (!v2_place(l->unit, r->anchor, 1, (int32_t)*pc)) return unit_ok(l);
    result = v2_evaluate_extent(l->unit, r->extent);
    if (partial && result.state == V2_DEFERRED) return 0;
    if (!ram_size(l, r)) return 0;
    if (r->size > UINT32_MAX - *pc) return layout_fail(l, V2_LAYOUT_OVERFLOW, r->loc);
    *pc += r->size;
    return 1;
}
int v2_layout_ram_origin(v2_layout *l, uint32_t origin, v2_location loc)
{
    fragment f = fresh(l, RAM_ORIGIN, loc);
    if (l->error.code) return 0;
    extra(l, &f)->placement.origin = origin;
    if (l->mode != V2_BINARY_LAYOUT || origin > 65535) return layout_fail(l, V2_LAYOUT_INPUT, loc);
    return add(l, f) != V2_NONE;
}

static int position_compare(const void *a, const void *b)
{
    v2_id x = ((const anchor_position *)a)->anchor, y = ((const anchor_position *)b)->anchor;
    return (x > y) - (x < y);
}
static v2_id find_position(v2_layout *l, v2_id anchor)
{
    anchor_position key, *found;
    key.anchor = anchor; key.fragment = 0;
    found = bsearch(&key, l->positions, l->position_count, sizeof(key), position_compare);
    return found && found->fragment < l->count ? found->fragment : V2_NONE;
}
static int span_tree(v2_layout *l)
{
    size_t i;
    v2_id zero;
    if (l->tree) return 1;
    zero = v2_extent_literal(l->unit, 0, v2_no_location());
    l->tree_base = 1;
    while (l->tree_base < l->count) {
        if (l->tree_base > SIZE_MAX / 4 / sizeof(v2_id)) return layout_fail(l, V2_LAYOUT_MEMORY, v2_no_location());
        l->tree_base *= 2;
    }
    l->tree = v2_malloc(l->heap, l->tree_base * 2 * sizeof(*l->tree));
    if (!l->tree) return layout_fail(l, V2_LAYOUT_MEMORY, v2_no_location());
    for (i = 0; i < l->tree_base; ++i)
        l->tree[l->tree_base + i] = i < l->count ? fragment_at(l, i)->extent : zero;
    for (i = l->tree_base; --i;)
        l->tree[i] = v2_extent_sum(l->unit, l->tree[2 * i], l->tree[2 * i + 1], v2_no_location());
    return unit_ok(l);
}

/* Segment-tree range sums share extents across overlapping anchor differences,
 * rather than cloning whole spans. Barriers are checked before querying. */
static v2_id lower_span(void *context, v2_id left, v2_id right, v2_location loc)
{
    v2_layout *l = context;
    v2_id a = find_position(l, left), b = find_position(l, right), sum = V2_NONE;
    size_t start, end;
    int negative;
    if (a == V2_NONE || b == V2_NONE || fragment_at(l, a)->run != fragment_at(l, b)->run) return V2_NONE;
    if (!span_tree(l)) return V2_NONE;
    negative = a < b;
    start = (negative ? a : b) + l->tree_base;
    end = (negative ? b : a) + l->tree_base;
    while (start < end) {
        if (start & 1) {
            sum = sum == V2_NONE ? l->tree[start] : v2_extent_sum(l->unit, sum, l->tree[start], loc);
            ++start;
        }
        if (end & 1) {
            --end;
            sum = sum == V2_NONE ? l->tree[end] : v2_extent_sum(l->unit, sum, l->tree[end], loc);
        }
        start /= 2; end /= 2;
    }
    if (sum == V2_NONE) sum = v2_extent_literal(l->unit, 0, loc);
    sum = v2_extent_integer(l->unit, sum, loc);
    return negative ? v2_unary(l->unit, V2_NEGATE, sum, loc) : sum;
}
static int narrow(v2_layout *l, fragment *f)
{
    v2_result r;
    int32_t value;
    if (f->narrow == 0xff) return 0;
    if (f->literal_operand) value = expr_i32(f->expression);
    else {
        r = v2_evaluate(l->unit, f->expression);
        if (r.state != V2_KNOWN || r.value.kind != V2_INTEGER) return 0;
        value = r.value.as.integer;
    }
    if (value < 0 || value > 255) return 0;
    f->opcode = f->narrow; f->narrow = 0xff;
    if (f->payload != V2_NONE)
        v2_place(l->unit, f->payload, 1, 2);
    else
        f->extent = fixed_extent(l, 2, v2_graph_location(l->unit, f->loc));
    ++l->work.narrowings; return 1;
}
/* Prefixes are checked extent DAGs, not provisional integer PCs. Only extent
 * evaluation follows them; instruction decisions still see placed addresses.
 * This resolves acyclic forward counts without a second retry loop. */
/* Keep fixed stretches as a checked literal offset instead of allocating two
 * prefix nodes for every byte-emitting fragment. Materialize only at anchors
 * and variable extents; optional widths remain graph dependencies. */
typedef struct { v2_id base; uint64_t fixed; } prefix;
static v2_id prefix_value(v2_layout *l, prefix p, v2_location loc)
{
    v2_id fixed;
    if (!p.fixed && p.base != V2_NONE) return p.base;
    fixed = v2_extent_literal(l->unit, p.fixed, loc);
    return p.base == V2_NONE ? fixed : v2_extent_sum(l->unit, p.base, fixed, loc);
}
static void prefix_fixed(v2_layout *l, prefix *p, uint64_t size, v2_location loc)
{
    if (size > UINT64_MAX - p->fixed) {
        p->base = prefix_value(l, *p, loc);
        p->fixed = 0;
    }
    p->fixed += size;
}
static void prefix_extent(v2_layout *l, prefix *p, v2_id extent, v2_location loc)
{
    p->base = v2_extent_sum(l->unit, prefix_value(l, *p, loc), extent, loc);
    p->fixed = 0;
}
static int prefixes(v2_layout *l)
{
    size_t i;
    prefix pc = { V2_NONE, 0 }, used = { V2_NONE, 0 };
    prefix ram_pc = { V2_NONE, l->block_count ? l->blocks[0].start : 0 };
    for (i = 0; i < l->count; ++i) {
        fragment *f = fragment_at(l, i);
        uint64_t fixed = 0;
        int constant = 1;
        if (f->kind == ORIGIN || f->kind == BANK) pc = (prefix){ V2_NONE, extra(l, f)->placement.origin };
        if (f->kind == BANK) used = (prefix){ V2_NONE, 0 };
        if (f->anchor != V2_NONE)
            v2_extent_anchor(l->unit, f->anchor,
                v2_extent_integer(l->unit, prefix_value(l, pc, v2_graph_location(l->unit, f->loc)), v2_graph_location(l->unit, f->loc)));
        if (f->kind == RAM_ORIGIN) ram_pc = (prefix){ V2_NONE, extra(l, f)->placement.origin };
        if (f->kind == RAM && l->mode == V2_BINARY_LAYOUT) {
            ram *r = &l->ram[extra(l, f)->placement.argument];
            v2_extent_anchor(l->unit, r->anchor,
                v2_extent_integer(l->unit, prefix_value(l, ram_pc, r->loc), r->loc));
            prefix_extent(l, &ram_pc, r->extent, r->loc);
        }
        if (f->kind == PAD && extra(l, f)->placement.pad != V2_PAD_SIZE)
            f->extent = v2_extent_remainder(l->unit, v2_extent_literal(l->unit, extra(l, f)->placement.argument, v2_graph_location(l->unit, f->loc)),
                prefix_value(l, extra(l, f)->placement.pad == V2_PAD_ORIGIN ? pc : used, v2_graph_location(l->unit, f->loc)), v2_graph_location(l->unit, f->loc));
        if (f->kind == BANK_END || f->kind == BANK || f->kind == ORIGIN ||
            f->kind == MARK || f->kind == RAM || f->kind == RAM_ORIGIN) continue;
        if (f->kind == RAW || f->kind == COPY) fixed = f->expression;
        else if (f->kind == DATA) fixed = extra(l, f)->data.value.length;
        else if (f->kind == INSTRUCTION && f->narrow == 0xff) fixed = (unsigned)opcode_length(f->opcode);
        else if (f->kind == PAD && extra(l, f)->placement.pad == V2_PAD_SIZE) fixed = extra(l, f)->placement.argument;
        else constant = 0;
        if (constant) {
            prefix_fixed(l, &pc, fixed, v2_graph_location(l->unit, f->loc));
            prefix_fixed(l, &used, fixed, v2_graph_location(l->unit, f->loc));
        } else {
            prefix_extent(l, &pc, f->extent, v2_graph_location(l->unit, f->loc));
            prefix_extent(l, &used, f->extent, v2_graph_location(l->unit, f->loc));
        }
    }
    return unit_ok(l);
}

static int prepare(v2_layout *l)
{
    size_t i, positions;
    int ram_placed = 0, needs_prefix = 0;
    uint32_t run = 0;
    int32_t bank = -1;
    if (l->count > UINT32_MAX - l->ram_count ||
        l->count + l->ram_count > SIZE_MAX / sizeof(*l->positions))
        return layout_fail(l, V2_LAYOUT_MEMORY, v2_no_location());
    positions = l->ram_count;
    for (i = 0; i < l->count; ++i)
        if (fragment_at(l, i)->anchor != V2_NONE) ++positions;
    l->position_count = 0;
    l->checkpoint_count = l->count ? 1 : 0;
    for (i = 1; i < l->count; ++i)
        if (fragment_at(l, i)->kind == ORIGIN || fragment_at(l, i)->kind == BANK ||
            fragment_at(l, i - 1)->kind == BANK_END) ++l->checkpoint_count;
    l->checkpoints = l->checkpoint_count ? v2_calloc(l->heap, l->checkpoint_count, sizeof(*l->checkpoints)) : NULL;
    size_t checkpoint_index = 0;
    l->positions = positions ? v2_malloc(l->heap, positions * sizeof(*l->positions)) : NULL;
    if ((l->checkpoint_count && !l->checkpoints) || (positions && !l->positions)) return layout_fail(l, V2_LAYOUT_MEMORY, v2_no_location());
    for (i = 0; i < l->count; ++i) {
        fragment *f = fragment_at(l, i);
        if (f->kind == ORIGIN || f->kind == BANK || f->kind == BANK_END || (f->kind == PAD && extra(l, f)->placement.pad != V2_PAD_SIZE)) ++run;
        f->run = run;
        if (f->kind == BANK) ++bank;
        if (i == 0 || f->kind == ORIGIN || f->kind == BANK ||
            fragment_at(l, i - 1)->kind == BANK_END) {
            l->checkpoints[checkpoint_index].first = (v2_id)i;
            l->checkpoints[checkpoint_index++].bank = bank;
        }
        if (f->anchor != V2_NONE) {
            anchor_position p; p.anchor = f->anchor; p.fragment = (v2_id)i;
            if (!l->positions || l->position_count >= positions) return layout_fail(l, V2_LAYOUT_INPUT, v2_graph_location(l->unit, f->loc));
            l->positions[l->position_count++] = p;
            int known;
            if (!v2_anchor_value(l->unit, f->anchor, &known, NULL, v2_graph_location(l->unit, f->loc)) || known)
                return layout_fail(l, V2_LAYOUT_INPUT, v2_graph_location(l->unit, f->loc));
        }
        /* Dynamic pads end a common-prefix run on both sides. */
        if (f->kind == PAD && extra(l, f)->placement.pad != V2_PAD_SIZE) ++run;
    }
    for (i = 0; i < l->ram_count; ++i) {
        int known;
        anchor_position p;
        if (!v2_anchor_value(l->unit, l->ram[i].anchor, &known, NULL, l->ram[i].loc) || known)
            return layout_fail(l, V2_LAYOUT_INPUT, l->ram[i].loc);
        p.anchor = l->ram[i].anchor; p.fragment = (v2_id)(l->count + i);
        if (!l->positions || l->position_count >= positions) return layout_fail(l, V2_LAYOUT_INPUT, l->ram[i].loc);
        l->positions[l->position_count++] = p;
    }
    if (l->position_count) qsort(l->positions, l->position_count, sizeof(*l->positions), position_compare);
    for (i = 1; i < l->position_count; ++i) if (l->positions[i - 1].anchor == l->positions[i].anchor) {
        v2_id f = l->positions[i].fragment;
        return layout_fail(l, V2_LAYOUT_INPUT, f < l->count ? v2_graph_location(l->unit, fragment_at(l, f)->loc) : l->ram[f - l->count].loc);
    }
    for (i = 0; i < l->bank_count; ++i) {
        bank_request *r = &l->banks[i];
        v2_id f = find_position(l, r->anchor);
        if (f == V2_NONE || f >= l->count) return layout_fail(l, V2_LAYOUT_INPUT, r->loc);
        v2_bind(l->unit, r->definition, v2_integer(l->unit, checkpoint_at(l, f)->bank, r->loc));
    }
    for (i = 0; i < l->count; ++i) {
        fragment *f = fragment_at(l, i);
        if (f->kind == DATA) {
            v2_result r = v2_evaluate(l->unit, f->expression);
            uint64_t size = f->width;
            if (r.state == V2_INVALID) {
                l->error.value_error = r.diagnostic;
                layout_fail(l, V2_LAYOUT_VALUE, r.diagnostic.location); return 0;
            }
            if (r.state == V2_KNOWN && r.value.kind == V2_STRING) {
                if (!f->strings) return layout_fail(l, V2_LAYOUT_INPUT, v2_graph_location(l->unit, f->loc));
                size *= r.value.as.string.length;
            } else if (r.state == V2_DEFERRED) {
                size_t d;
                for (d = 0; d < r.dependency_count; ++d)
                    if (r.dependencies[d].kind != V2_DEP_ANCHOR)
                        return layout_fail(l, V2_LAYOUT_UNRESOLVED, v2_graph_location(l->unit, f->loc));
            }
            extra(l, f)->data.value.length = (size_t)size;
            f->extent = fixed_extent(l, size, v2_graph_location(l->unit, f->loc));
        }
    }
    /* Link RAM placement precedes code relaxation. Resolve independent RAM
     * now, so its instruction operands do not acquire unused width graphs. */
    if (l->mode == V2_LINK_LAYOUT) {
        int known = 1;
        for (i = 0; i < l->ram_count && known; ++i)
            known = v2_evaluate_extent(l->unit, l->ram[i].extent).state == V2_KNOWN;
        if (known) {
            if (!allocate_ram(l)) return 0;
            ram_placed = 1;
        }
    }
    /* Literal/constant widths also feed RAM size spans. */
    for (i = 0; i < l->count; ++i) if (fragment_at(l, i)->kind == INSTRUCTION)
        narrow(l, fragment_at(l, i));
    /* Width anchors exist only when an extent graph can depend on widths.
     * Otherwise each walk reads the opcode directly and narrowing updates the
     * fragment's shared literal extent. */
    for (i = 0; i < l->count && !needs_prefix; ++i) {
        fragment *f = fragment_at(l, i);
        if (f->kind == STORAGE)
            needs_prefix = v2_evaluate_extent(l->unit, f->extent).state != V2_KNOWN;
    }
    for (i = 0; i < l->ram_count && !needs_prefix; ++i)
        needs_prefix = v2_evaluate_extent(l->unit, l->ram[i].extent).state != V2_KNOWN;
    if (needs_prefix) for (i = 0; i < l->count; ++i) {
        fragment *f = fragment_at(l, i);
        if (f->kind == INSTRUCTION && f->narrow != 0xff) {
            f->payload = v2_anchor(l->unit, v2_graph_location(l->unit, f->loc));
            v2_place(l->unit, f->payload, 1, (int32_t)opcode_length(f->opcode));
            f->extent = v2_extent_count(l->unit, v2_address(l->unit, f->payload, v2_graph_location(l->unit, f->loc)), 1, 3, v2_graph_location(l->unit, f->loc));
        }
    }
    {
        if (needs_prefix) {
            if (!prefixes(l)) return 0;
            {
                size_t count = l->count + l->ram_count;
                v2_id *roots = count ? v2_malloc(l->heap, count * sizeof(*roots)) : NULL;
                int lowered;
                if (count && !roots) return layout_fail(l, V2_LAYOUT_MEMORY, v2_no_location());
                for (i = 0; i < l->count; ++i) roots[i] = fragment_at(l, i)->extent;
                for (i = 0; i < l->ram_count; ++i) roots[l->count + i] = l->ram[i].extent;
                lowered = v2_lower_extent_spans(l->unit, roots, count, lower_span, l);
                v2_free(l->heap, roots);
                if (!lowered || !unit_ok(l) || l->error.code) return 0;
            }
        }
    }

    if (!ram_placed && !allocate_ram(l)) return 0;
    if (l->mode == V2_BINARY_LAYOUT) {
        uint32_t pc = l->block_count ? l->blocks[0].start : 0;
        int partial = 1;
        for (i = 0; i < l->count; ++i) {
            fragment *f = fragment_at(l, i);
            if (f->kind == RAM_ORIGIN) { pc = extra(l, f)->placement.origin; partial = 1; }
            else if (f->kind == RAM && partial) partial = binary_ram(l, &l->ram[extra(l, f)->placement.argument], &pc, 1);
            if (l->error.code) return 0;
        }
    }
    for (i = 0; i < l->count; ++i) if (fragment_at(l, i)->kind == INSTRUCTION) {
        if (l->ram_count || l->tree) narrow(l, fragment_at(l, i));
        if (fragment_at(l, i)->narrow != 0xff) ++l->work.candidates;
    }
    return unit_ok(l);
}

/* Update facts in walk order. After the unchanged walk, validation checks
 * every checked extent against the size actually used here before encoding. */
static int walk(v2_layout *l, int *shortened)
{
    uint64_t pc = 0, output = 0, bank_start = 0, bank_size = 0;
    size_t i;
    uint32_t ram_pc = l->block_count ? l->blocks[0].start : 0;
    size_t checkpoint_index = 0;
    ++l->work.walks; *shortened = 0;
    for (i = 0; i < l->count; ++i) {
        fragment *f = fragment_at(l, i);
        uint64_t size = 0;
        ++l->work.fragment_visits;
        if (l->mode == V2_BINARY_LAYOUT) {
            if (f->kind == RAM_ORIGIN) ram_pc = extra(l, f)->placement.origin;
            else if (f->kind == RAM && !binary_ram(l, &l->ram[extra(l, f)->placement.argument], &ram_pc, 0)) return 0;
        }
        if (f->kind == BANK_END && bank_size) {
            if (output - bank_start > bank_size) return layout_fail(l, V2_LAYOUT_OVERFLOW, v2_graph_location(l->unit, f->loc));
            size = bank_size - (output - bank_start);
        }
        if (f->kind == ORIGIN || f->kind == BANK) pc = extra(l, f)->placement.origin;
        if (f->kind == BANK) { bank_start = output; bank_size = extra(l, f)->placement.argument; }
        if (f->anchor != V2_NONE && !v2_place(l->unit, f->anchor, 1, expr_i32((uint32_t)pc))) return unit_ok(l);
        if (checkpoint_index < l->checkpoint_count && l->checkpoints[checkpoint_index].first == i) {
            l->checkpoints[checkpoint_index].address = (uint32_t)pc;
            l->checkpoints[checkpoint_index++].output_offset = (uint32_t)output;
        }
        if (f->kind == INSTRUCTION) {
            *shortened |= narrow(l, f);
            size = (unsigned)opcode_length(f->opcode);
        } else if (f->kind == PAD && extra(l, f)->placement.pad != V2_PAD_SIZE) {
            uint64_t at = extra(l, f)->placement.pad == V2_PAD_ORIGIN ? pc : output - bank_start;
            if (extra(l, f)->placement.argument < at) return layout_fail(l, V2_LAYOUT_OVERFLOW, v2_graph_location(l->unit, f->loc));
            size = extra(l, f)->placement.argument - at;
        } else if (f->kind != BANK && f->kind != BANK_END && !extent(l, f->extent, v2_graph_location(l->unit, f->loc), &size)) return 0;
        if (size > UINT32_MAX - output) return layout_fail(l, V2_LAYOUT_OVERFLOW, v2_graph_location(l->unit, f->loc));
        f->run = (uint32_t)size;
        output += size;
        if (f->kind != BANK && f->kind != BANK_END) pc += size;
        /* Raw script copies and script padding may form CHR banks larger than
         * the CPU address space. The 64K check belongs to linked code. */
        if (pc > UINT32_MAX || (l->mode == V2_LINK_LAYOUT && pc > 65536 &&
            (f->kind == INSTRUCTION || f->kind == RAW || f->kind == DATA || f->kind == STORAGE))) return layout_fail(l, V2_LAYOUT_OVERFLOW, v2_graph_location(l->unit, f->loc));
        if (bank_size && output - bank_start > bank_size) return layout_fail(l, V2_LAYOUT_OVERFLOW, v2_graph_location(l->unit, f->loc));
    }
    if (output > UINT32_MAX || output > SIZE_MAX) return layout_fail(l, V2_LAYOUT_OVERFLOW, v2_no_location());
    l->output_size = (size_t)output;
    return 1;
}
static int validate(v2_layout *l)
{
    size_t i;
    uint32_t pc = 0, output = 0;
    for (i = 0; i < l->ram_count; ++i) {
        uint64_t size;
        if (!extent(l, l->ram[i].extent, l->ram[i].loc, &size)) return 0;
        if (size != l->ram[i].size) return layout_fail(l, V2_LAYOUT_UNSTABLE, l->ram[i].loc);
    }
    for (i = 0; i < l->count; ++i) {
        fragment *f = fragment_at(l, i);
        uint32_t address;
        if (f->kind == ORIGIN || f->kind == BANK) pc = extra(l, f)->placement.origin;
        address = pc;
        if (f->kind != BANK && f->kind != BANK_END) pc += f->run;
        int32_t value;
        v2_value operand;
        int warn = 0;
        if (f->kind == STORAGE || f->kind == DATA) {
            uint64_t size;
            if (!extent(l, f->extent, v2_graph_location(l->unit, f->loc), &size)) return 0;
            if (size != f->run) return layout_fail(l, V2_LAYOUT_UNSTABLE, v2_graph_location(l->unit, f->loc));
        }
        /* All width constraints have now consumed the original extent ID. */
        f->extent = output; output += f->run;
        if (f->kind != DATA && f->kind != INSTRUCTION) continue;
        if (f->expression == V2_NONE && !f->literal_operand) continue;
        if (f->literal_operand) { operand.kind = V2_INTEGER; operand.as.integer = expr_i32(f->expression); }
        else if (!evaluated(l, f->expression, v2_graph_location(l->unit, f->loc), &operand)) return 0;
        if (f->kind == DATA) {
            f->string_value = operand.kind == V2_STRING;
            if (f->string_value) extra(l, f)->data.value.bytes = operand.as.string.bytes;
        }
        if (operand.kind == V2_STRING && f->kind == DATA && f->strings) {
            if ((uint64_t)f->width * operand.as.string.length != f->run)
                return layout_fail(l, V2_LAYOUT_UNSTABLE, v2_graph_location(l->unit, f->loc));
            continue;
        }
        if (operand.kind != V2_INTEGER) return layout_fail(l, V2_LAYOUT_RANGE, v2_graph_location(l->unit, f->loc));
        value = operand.as.integer;
        f->has_operand = 1; f->anchor = (uint32_t)value;
        if (f->kind == DATA) {
            warn = (f->width == 1 && (value < -128 || value > 255)) ||
                   (f->width == 2 && (value < -32768 || value > 65535));
        } else {
            addressing_mode mode = opcode_addressing_mode(f->opcode);
            if (mode == RELATIVE_MODE) {
                int64_t displacement = (int64_t)value - ((int64_t)address + 2);
                if (displacement < -128 || displacement > 127) return layout_fail(l, V2_LAYOUT_RANGE, v2_graph_location(l->unit, f->loc));
            } else if (!f->early) {
                if (mode == ZEROPAGE_MODE || mode == ZEROPAGE_X_MODE || mode == ZEROPAGE_Y_MODE) {
                    if (value < 0 || value > 255) return layout_fail(l, V2_LAYOUT_RANGE, v2_graph_location(l->unit, f->loc));
                } else if (f->run == 2) {
                    if (value < -128 || value > 255) return layout_fail(l, V2_LAYOUT_RANGE, v2_graph_location(l->unit, f->loc));
                } else if (f->run == 3 && (value < -32768 || value > 65535)) {
                    if (l->mode == V2_LINK_LAYOUT) return layout_fail(l, V2_LAYOUT_RANGE, v2_graph_location(l->unit, f->loc));
                    warn = 1;
                }
            }
        }
        f->warning = warn != 0;
    }
    return 1;
}
static int check_copies(v2_layout *l)
{
    size_t i;
    for (i = 0; i < l->count; ++i) if (fragment_at(l, i)->kind == COPY) {
        fragment *f = fragment_at(l, i);
        unsigned char buf[4096];
        size_t at = 0, n;
        int ok = 1;
        FILE *input = fopen(extra(l, f)->raw.path, "rb");
        if (!input) return layout_fail(l, V2_LAYOUT_COPY_CHANGED, v2_graph_location(l->unit, f->loc));
        for (;;) {
            n = fread(buf, 1, sizeof(buf), input);
            if (!n) break;
            if (n > f->expression - at || memcmp(buf, extra(l, f)->raw.bytes + at, n)) { ok = 0; break; }
            at += n;
            if (n < sizeof(buf)) break;
        }
        if (ferror(input) || at != f->expression) ok = 0;
        if (fclose(input)) ok = 0;
        if (!ok) return layout_fail(l, V2_LAYOUT_COPY_CHANGED, v2_graph_location(l->unit, f->loc));
    }
    return 1;
}
static void put(unsigned char *to, uint32_t value, unsigned width)
{
    unsigned i;
    for (i = 0; i < width; ++i) { to[i] = (unsigned char)value; value >>= 8; }
}
typedef int (*byte_sink)(void *, const unsigned char *, size_t);
static int emit_fragments(const v2_layout *l, size_t first, size_t end,
                          byte_sink sink, void *context)
{
    static const unsigned char zero[4096];
    unsigned char buffer[4096];
    size_t i, j;
    for (i = first; i < end; ++i) {
        const fragment *f = fragment_at(l, i);
        placement fact = final_placement(l, i);
        if (!fact.length) continue;
        if (fact.output_offset > l->output_size || fact.length > l->output_size - fact.output_offset)
            return 0;
        if (f->kind == RAW || f->kind == COPY) {
            if (f->expression != fact.length ||
                !sink(context, extra(l, f)->raw.bytes, fact.length)) return 0;
        } else if (f->kind == INSTRUCTION) {
            uint32_t operand = (uint32_t)fact.operand;
            if (fact.length > 3) return 0;
            buffer[0] = f->opcode;
            if (opcode_addressing_mode(f->opcode) == RELATIVE_MODE) operand -= fact.address + 2;
            if (fact.length > 1) put(buffer + 1, operand, fact.length - 1);
            if (!sink(context, buffer, fact.length)) return 0;
        } else if (f->kind == DATA) {
            if (!f->string_value) {
                if (fact.length != f->width || f->width > 4) return 0;
                put(buffer, (uint32_t)fact.operand, f->width);
                if (!sink(context, buffer, f->width)) return 0;
            } else {
                size_t at = 0;
                for (j = 0; j < f->run / f->width; ++j) {
                    unsigned char c = extra(l, f)->data.value.bytes[j];
                    put(buffer + at, f->mapped ? extra(l, f)->data.map[c] : c, f->width);
                    at += f->width;
                    if (at == sizeof(buffer)) {
                        if (!sink(context, buffer, at)) return 0;
                        at = 0;
                    }
                }
                if (at && !sink(context, buffer, at)) return 0;
            }
        } else {
            size_t left = fact.length;
            while (left) {
                size_t n = left < sizeof(zero) ? left : sizeof(zero);
                if (!sink(context, zero, n)) return 0;
                left -= n;
            }
        }
    }
    return 1;
}
typedef struct { unsigned char *next; size_t left; } byte_buffer;
static int buffer_sink(void *context, const unsigned char *bytes, size_t n)
{
    byte_buffer *b = context;
    if (n > b->left) return 0;
    if (n) { memcpy(b->next, bytes, n); b->next += n; b->left -= n; }
    return 1;
}
static int encode(v2_layout *l)
{
    byte_buffer b;
    l->output = l->output_size ? v2_malloc(l->heap, l->output_size) : NULL;
    if (l->output_size && !l->output) return layout_fail(l, V2_LAYOUT_MEMORY, v2_no_location());
    b = (byte_buffer){l->output, l->output_size};
    if (!emit_fragments(l, 0, l->count, buffer_sink, &b) || b.left)
        return layout_fail(l, V2_LAYOUT_UNSTABLE, v2_no_location());
    return 1;
}
typedef struct { FILE *stream; size_t used; unsigned char bytes[4096]; } file_buffer;
static int flush_file_buffer(file_buffer *b)
{
    if (b->used && fwrite(b->bytes, 1, b->used, b->stream) != b->used) return 0;
    b->used = 0; return 1;
}
static int file_sink(void *context, const unsigned char *bytes, size_t n)
{
    file_buffer *b = context;
    while (n) {
        size_t take;
        /* Already large runs need no additional copy. Small instruction/data
         * fragments share one stdio call instead of locking FILE per operand. */
        if (!b->used && n >= sizeof(b->bytes)) return fwrite(bytes, 1, n, b->stream) == n;
        take = sizeof(b->bytes) - b->used;
        if (take > n) take = n;
        memcpy(b->bytes + b->used, bytes, take);
        b->used += take; bytes += take; n -= take;
        if (b->used == sizeof(b->bytes) && !flush_file_buffer(b)) return 0;
    }
    return 1;
}
int v2_layout_keep_bytes(v2_layout *l, int keep)
{
    if (!building(l, v2_no_location())) return 0;
    l->keep_bytes = keep != 0; return 1;
}
int v2_layout_write_fragments(const v2_layout *l, size_t first, size_t end, FILE *stream)
{
    file_buffer b;
    b.stream = stream; b.used = 0;
    return l->ready && stream && first <= end && end <= l->count &&
        emit_fragments(l, first, end, file_sink, &b) && flush_file_buffer(&b);
}
static v2_layout_fact fact_at(const v2_layout *l, size_t i)
{
    const fragment *f = fragment_at(l, i);
    placement stored = final_placement(l, i);
    const placement *p = &stored;
    v2_layout_fact fact = {0};
    fact.kind = f->kind == INSTRUCTION ? V2_FACT_INSTRUCTION :
        f->kind == DATA ? V2_FACT_DATA :
        f->kind == RAW || f->kind == COPY ? V2_FACT_RAW :
        f->kind == STORAGE || f->kind == PAD || f->kind == BANK_END ? V2_FACT_FILL :
        f->kind == RAM || f->kind == RAM_ORIGIN ? V2_FACT_RAM :
        f->kind == MARK ? V2_FACT_MARK : V2_FACT_PLACEMENT;
    fact.location = v2_graph_location(l->unit, f->loc); fact.address = p->address; fact.output_offset = p->output_offset;
    fact.length = p->length; fact.bank = p->bank; fact.operand = p->operand;
    fact.opcode = f->opcode; fact.has_operand = f->has_operand; fact.warning = f->warning;
    return fact;
}
int v2_layout_keep_facts(v2_layout *l, int keep)
{
    if (!building(l, v2_no_location())) return 0;
    l->keep_facts = keep != 0; return 1;
}
int v2_layout_fact_at(const v2_layout *l, v2_id id, v2_layout_fact *out)
{
    if (!l->ready || id >= l->count || !out) return 0;
    *out = l->facts ? l->facts[id] : fact_at(l, id); return 1;
}
size_t v2_layout_fact_count(const v2_layout *l) { return l->ready ? l->count : 0; }
int v2_layout_run(v2_layout *l)
{
    int shortened;
    uint64_t limit;
    if (!building(l, v2_no_location())) return 0;
    if (l->bank_calls && add(l, fresh(l, BANK_END, v2_no_location())) == V2_NONE) return 0;
    l->started = 1;
    if (!prepare(l)) return 0;
    v2_graph_release_location_index(l->unit);
    v2_free(l->heap, l->positions); l->positions = NULL;
    v2_free(l->heap, l->tree); l->tree = NULL;
    limit = l->work.candidates + 2;
    /* Always walk twice: forward addresses first become available in walk 1. */
    do {
        if (!walk(l, &shortened) || !unit_ok(l)) return 0;
        if (l->work.walks > limit) return layout_fail(l, V2_LAYOUT_UNSTABLE, v2_no_location());
    } while (shortened || l->work.walks == 1);
    if (!validate(l) || !check_copies(l) || (l->keep_bytes && !encode(l))) return 0;
    if (l->keep_facts && l->count) {
        size_t i;
        if (l->count > SIZE_MAX / sizeof(*l->facts) || !(l->facts = v2_malloc(l->heap, l->count * sizeof(*l->facts))))
            return layout_fail(l, V2_LAYOUT_MEMORY, v2_no_location());
        v2_layout_fact *facts = l->facts;
        l->facts = NULL;
        for (i = 0; i < l->count; i++) facts[i] = fact_at(l, i);
        l->facts = facts;
        v2_free(l->heap, l->checkpoints); l->checkpoints = NULL;
    }
    l->ready = 1; return 1;
}
const unsigned char *v2_layout_bytes(const v2_layout *l, size_t *size)
{
    *size = l->ready && l->keep_bytes ? l->output_size : 0;
    return l->ready ? l->output : NULL;
}
const v2_layout_fact *v2_layout_facts(const v2_layout *l, size_t *count)
{
    *count = l->ready && l->keep_facts ? l->count : 0;
    return l->ready && l->keep_facts ? l->facts : NULL;
}
