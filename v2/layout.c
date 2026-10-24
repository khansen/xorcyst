#include "layout.h"
#include "../opcode.h"
#include "../expr_int.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { ORIGIN, BANK, MARK, RAW, COPY, DATA, INSTRUCTION, STORAGE, PAD, RAM, RAM_ORIGIN, BANK_END } kind;
typedef struct {
    kind kind;
    v2_location loc;
    v2_id anchor, expression, extent, extent_definition, length_anchor;
    unsigned char opcode, narrow;
    unsigned char *map;
    unsigned width, strings, mapped, early;
    uint32_t argument, origin, run;
    v2_pad_kind pad;
    unsigned char *bytes;
    size_t length;
    char *path;
    v2_value value;
} fragment;
typedef struct { v2_id anchor, extent; unsigned align, zp; uint32_t size, key; v2_location loc; } ram;
typedef struct { uint32_t start, end; } block;
typedef struct { v2_id anchor, definition; v2_location loc; } bank_request;
typedef struct { v2_id anchor, fragment; } anchor_position;
struct v2_layout {
    v2_unit *unit;
    v2_layout_mode mode;
    fragment *fragments;
    ram *ram;
    block *blocks;
    bank_request *banks;
    anchor_position *positions;
    v2_id *tree;
    size_t tree_base, position_count;
    size_t count, capacity, ram_count, ram_capacity, block_count, block_capacity;
    size_t bank_count, bank_capacity;
    v2_layout_fact *facts;
    unsigned char *output;
    size_t output_size;
    unsigned started, ready, bank_calls;
    v2_layout_diagnostic error;
    v2_layout_work work;
};

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
    next = realloc(p, n * width);
    if (!next) goto failed;
    *capacity = n; return next;
failed:
    layout_fail(l, V2_LAYOUT_MEMORY, v2_no_location()); return p;
}
static int building(v2_layout *l, v2_location loc)
{
    /* A layout is a single construction/run, including failed runs. */
    if (l->started) return layout_fail(l, V2_LAYOUT_INPUT, loc);
    return !l->error.code && unit_ok(l);
}
static fragment fresh(kind k, v2_location loc)
{
    fragment f;
    memset(&f, 0, sizeof(f));
    f.kind = k; f.loc = loc; f.anchor = f.expression = f.extent = V2_NONE;
    f.extent_definition = f.length_anchor = V2_NONE; f.narrow = 0xff;
    return f;
}
static v2_id add(v2_layout *l, fragment f)
{
    if (!building(l, f.loc)) goto failed;
    if (f.anchor != V2_NONE) v2_address(l->unit, f.anchor, f.loc);
    if (f.extent == V2_NONE) f.extent = v2_extent_literal(l->unit, 0, f.loc);
    l->fragments = reserve(l, l->fragments, &l->capacity, l->count + 1, sizeof(f));
    if (l->error.code || !unit_ok(l)) goto failed;
    l->fragments[l->count] = f;
    return (v2_id)l->count++;
failed:
    free(f.bytes); free(f.path); free(f.map); return V2_NONE;
}
v2_layout *v2_layout_create(v2_unit *u, v2_layout_mode mode)
{
    v2_layout *l;
    if (!u || (mode != V2_BINARY_LAYOUT && mode != V2_LINK_LAYOUT)) return NULL;
    l = calloc(1, sizeof(*l));
    if (l) { l->unit = u; l->mode = mode; l->error.location = v2_no_location(); }
    return l;
}
void v2_layout_destroy(v2_layout *l)
{
    size_t i;
    if (!l) return;
    for (i = 0; i < l->count; ++i) { free(l->fragments[i].bytes); free(l->fragments[i].path); free(l->fragments[i].map); }
    free(l->fragments); free(l->ram); free(l->blocks); free(l->banks);
    free(l->positions); free(l->tree); free(l->facts); free(l->output); free(l);
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
    fragment f = fresh(ORIGIN, loc); f.origin = origin;
    if (origin > 65535) layout_fail(l, V2_LAYOUT_INPUT, loc);
    return add(l, f);
}
v2_id v2_layout_bank(v2_layout *l, uint32_t origin, uint32_t size, v2_location loc)
{
    fragment f = fresh(BANK, loc); f.origin = origin; f.argument = size;
    v2_id id;
    if (origin > 65535 || !size) layout_fail(l, V2_LAYOUT_INPUT, loc);
    if (l->bank_calls && add(l, fresh(BANK_END, loc)) == V2_NONE) return V2_NONE;
    id = add(l, f);
    if (id != V2_NONE) ++l->bank_calls;
    return id;
}
v2_id v2_layout_mark(v2_layout *l, v2_id anchor, v2_location loc)
{
    fragment f = fresh(MARK, loc); f.anchor = anchor;
    if (anchor == V2_NONE) layout_fail(l, V2_LAYOUT_INPUT, loc);
    return add(l, f);
}
v2_id v2_layout_raw(v2_layout *l, const void *bytes, size_t size, v2_location loc)
{
    fragment f = fresh(RAW, loc);
    if ((size && !bytes) || size > UINT32_MAX) {
        layout_fail(l, V2_LAYOUT_INPUT, loc);
        return V2_NONE;
    }
    if (!building(l, loc)) return V2_NONE;
    if (size) {
        f.bytes = malloc(size);
        if (!f.bytes) { layout_fail(l, V2_LAYOUT_MEMORY, loc); return V2_NONE; }
        memcpy(f.bytes, bytes, size);
    }
    f.length = size; f.extent = v2_extent_literal(l->unit, size, loc);
    return add(l, f);
}
v2_id v2_layout_copy(v2_layout *l, const char *path, v2_location loc)
{
    FILE *file;
    long length;
    fragment f = fresh(COPY, loc);
    if (!building(l, loc)) return V2_NONE;
    if (!path || !(file = fopen(path, "rb"))) { layout_fail(l, V2_LAYOUT_INPUT, loc); return V2_NONE; }
    if (fseek(file, 0, SEEK_END) || (length = ftell(file)) < 0 ||
        (uint64_t)length > UINT32_MAX || fseek(file, 0, SEEK_SET)) {
        fclose(file); layout_fail(l, V2_LAYOUT_INPUT, loc); return V2_NONE;
    }
    f.length = (size_t)length;
    f.path = malloc(strlen(path) + 1);
    f.bytes = f.length ? malloc(f.length) : NULL;
    if (!f.path || (f.length && !f.bytes)) layout_fail(l, V2_LAYOUT_MEMORY, loc);
    else {
        strcpy(f.path, path);
        if (f.length && fread(f.bytes, 1, f.length, file) != f.length) layout_fail(l, V2_LAYOUT_INPUT, loc);
    }
    if (fclose(file)) layout_fail(l, V2_LAYOUT_INPUT, loc);
    f.extent = v2_extent_literal(l->unit, f.length, loc);
    return add(l, f);
}
v2_id v2_layout_data(v2_layout *l, v2_id pc, v2_id expr, unsigned width,
                      int strings, const unsigned char *map, v2_location loc)
{
    fragment f = fresh(DATA, loc);
    if ((width != 1 && width != 2 && width != 4) || (map && width != 1))
        layout_fail(l, V2_LAYOUT_INPUT, loc);
    if (!building(l, loc)) return V2_NONE;
    f.anchor = pc; f.expression = expr; f.width = width; f.strings = strings != 0;
    if (map) {
        f.map = malloc(256);
        if (!f.map) { layout_fail(l, V2_LAYOUT_MEMORY, loc); return V2_NONE; }
        memcpy(f.map, map, 256); f.mapped = 1;
    }
    f.extent_definition = v2_definition(l->unit, "data extent", loc);
    f.extent = v2_reference(l->unit, f.extent_definition, loc);
    return add(l, f);
}
v2_id v2_layout_instruction(v2_layout *l, v2_id pc, unsigned char opcode,
                             v2_id operand, int wide, int early, v2_location loc)
{
    fragment f = fresh(INSTRUCTION, loc);
    instr_mnemonic mnemonic;
    unsigned width = (unsigned)opcode_length(opcode);
    if (!opcode_mnemonic(opcode, &mnemonic) || (width > 1) != (operand != V2_NONE))
        layout_fail(l, V2_LAYOUT_INPUT, loc);
    if (!building(l, loc)) return V2_NONE;
    f.anchor = pc; f.opcode = opcode; f.expression = operand; f.early = early != 0;
    if (!wide) f.narrow = opcode_zp_equiv(opcode, opcode_addressing_mode(opcode));
    f.length_anchor = v2_anchor(l->unit, loc);
    v2_place(l->unit, f.length_anchor, 1, (int32_t)width);
    f.extent = v2_extent_count(l->unit, v2_address(l->unit, f.length_anchor, loc), 1, 3, loc);
    return add(l, f);
}
v2_id v2_layout_storage(v2_layout *l, v2_id pc, v2_id extent, v2_location loc)
{
    fragment f = fresh(STORAGE, loc); f.anchor = pc; f.extent = extent;
    if (extent == V2_NONE) layout_fail(l, V2_LAYOUT_INPUT, loc);
    return add(l, f);
}
v2_id v2_layout_pad(v2_layout *l, v2_pad_kind kind, uint32_t value, v2_location loc)
{
    fragment f = fresh(PAD, loc); f.pad = kind; f.argument = value;
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
    v2_address(l->unit, anchor, loc);
    r.extent = v2_extent_bound(l->unit, extent, 0, UINT32_MAX, loc);
    if (!unit_ok(l)) return 0;
    l->ram = reserve(l, l->ram, &l->ram_capacity, l->ram_count + 1, sizeof(r));
    if (l->error.code) return 0;
    { fragment f = fresh(RAM, loc); f.argument = (uint32_t)l->ram_count;
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
    fragment f = fresh(RAM_ORIGIN, loc); f.origin = origin;
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
    l->tree = malloc(l->tree_base * 2 * sizeof(*l->tree));
    if (!l->tree) return layout_fail(l, V2_LAYOUT_MEMORY, v2_no_location());
    for (i = 0; i < l->tree_base; ++i)
        l->tree[l->tree_base + i] = i < l->count ? l->fragments[i].extent : zero;
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
    if (a == V2_NONE || b == V2_NONE || l->fragments[a].run != l->fragments[b].run) return V2_NONE;
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
    if (f->narrow == 0xff) return 0;
    r = v2_evaluate(l->unit, f->expression);
    if (r.state != V2_KNOWN || r.value.kind != V2_INTEGER ||
        r.value.as.integer < 0 || r.value.as.integer > 255) return 0;
    f->opcode = f->narrow; f->narrow = 0xff;
    v2_place(l->unit, f->length_anchor, 1, 2);
    ++l->work.narrowings; return 1;
}
/* Prefixes are checked extent DAGs, not provisional integer PCs. Only extent
 * evaluation follows them; instruction decisions still see placed addresses.
 * This resolves acyclic forward counts without a second retry loop. */
static int prefixes(v2_layout *l)
{
    size_t i;
    v2_id zero = v2_extent_literal(l->unit, 0, v2_no_location());
    v2_id pc = zero, used = zero;
    v2_id ram_pc = v2_extent_literal(l->unit, l->block_count ? l->blocks[0].start : 0, v2_no_location());
    for (i = 0; i < l->count; ++i) {
        fragment *f = &l->fragments[i];
        if (f->kind == ORIGIN || f->kind == BANK) pc = v2_extent_literal(l->unit, f->origin, f->loc);
        if (f->kind == BANK) used = zero;
        if (f->anchor != V2_NONE)
            v2_extent_anchor(l->unit, f->anchor, v2_extent_integer(l->unit, pc, f->loc));
        if (f->kind == RAM_ORIGIN) ram_pc = v2_extent_literal(l->unit, f->origin, f->loc);
        if (f->kind == RAM && l->mode == V2_BINARY_LAYOUT) {
            ram *r = &l->ram[f->argument];
            v2_extent_anchor(l->unit, r->anchor, v2_extent_integer(l->unit, ram_pc, r->loc));
            ram_pc = v2_extent_sum(l->unit, ram_pc, r->extent, r->loc);
        }
        if (f->kind == PAD && f->pad != V2_PAD_SIZE)
            f->extent = v2_extent_remainder(l->unit, v2_extent_literal(l->unit, f->argument, f->loc),
                                           f->pad == V2_PAD_ORIGIN ? pc : used, f->loc);
        if (f->kind == BANK_END || f->kind == BANK || f->kind == ORIGIN ||
            f->kind == MARK || f->kind == RAM || f->kind == RAM_ORIGIN) continue;
        pc = v2_extent_sum(l->unit, pc, f->extent, f->loc);
        used = v2_extent_sum(l->unit, used, f->extent, f->loc);
    }
    return unit_ok(l);
}

static int prepare(v2_layout *l)
{
    size_t i, positions;
    uint32_t run = 0;
    int32_t bank = -1;
    if (l->count > UINT32_MAX - l->ram_count ||
        l->count + l->ram_count > SIZE_MAX / sizeof(*l->positions))
        return layout_fail(l, V2_LAYOUT_MEMORY, v2_no_location());
    positions = l->count + l->ram_count;
    l->position_count = 0;
    l->facts = l->count ? calloc(l->count, sizeof(*l->facts)) : NULL;
    l->positions = positions ? malloc(positions * sizeof(*l->positions)) : NULL;
    if ((l->count && !l->facts) || (positions && !l->positions)) return layout_fail(l, V2_LAYOUT_MEMORY, v2_no_location());
    for (i = 0; i < l->count; ++i) {
        fragment *f = &l->fragments[i];
        if (f->kind == ORIGIN || f->kind == BANK || f->kind == BANK_END || (f->kind == PAD && f->pad != V2_PAD_SIZE)) ++run;
        f->run = run;
        if (f->kind == BANK) ++bank;
        l->facts[i].bank = bank;
        l->facts[i].location = f->loc;
        l->facts[i].kind = f->kind == INSTRUCTION ? V2_FACT_INSTRUCTION :
            f->kind == DATA ? V2_FACT_DATA :
            f->kind == RAW || f->kind == COPY ? V2_FACT_RAW :
            f->kind == STORAGE || f->kind == PAD || f->kind == BANK_END ? V2_FACT_FILL :
            f->kind == RAM || f->kind == RAM_ORIGIN ? V2_FACT_RAM :
            f->kind == MARK ? V2_FACT_MARK : V2_FACT_PLACEMENT;
        if (f->anchor != V2_NONE) {
            anchor_position p; p.anchor = f->anchor; p.fragment = (v2_id)i;
            if (!l->positions || l->position_count >= positions) return layout_fail(l, V2_LAYOUT_INPUT, f->loc);
            l->positions[l->position_count++] = p;
            v2_result initial = v2_evaluate(l->unit, v2_address(l->unit, f->anchor, f->loc));
            if (initial.state != V2_DEFERRED) return layout_fail(l, V2_LAYOUT_INPUT, f->loc);
        }
        /* Dynamic pads end a common-prefix run on both sides. */
        if (f->kind == PAD && f->pad != V2_PAD_SIZE) ++run;
    }
    for (i = 0; i < l->ram_count; ++i) {
        v2_result initial = v2_evaluate(l->unit, v2_address(l->unit, l->ram[i].anchor, l->ram[i].loc));
        anchor_position p;
        if (initial.state != V2_DEFERRED) return layout_fail(l, V2_LAYOUT_INPUT, l->ram[i].loc);
        p.anchor = l->ram[i].anchor; p.fragment = (v2_id)(l->count + i);
        if (!l->positions || l->position_count >= positions) return layout_fail(l, V2_LAYOUT_INPUT, l->ram[i].loc);
        l->positions[l->position_count++] = p;
    }
    if (l->position_count) qsort(l->positions, l->position_count, sizeof(*l->positions), position_compare);
    for (i = 1; i < l->position_count; ++i) if (l->positions[i - 1].anchor == l->positions[i].anchor) {
        v2_id f = l->positions[i].fragment;
        return layout_fail(l, V2_LAYOUT_INPUT, f < l->count ? l->fragments[f].loc : l->ram[f - l->count].loc);
    }
    for (i = 0; i < l->bank_count; ++i) {
        bank_request *r = &l->banks[i];
        v2_id f = find_position(l, r->anchor);
        if (f == V2_NONE) return layout_fail(l, V2_LAYOUT_INPUT, r->loc);
        v2_bind(l->unit, r->definition, v2_integer(l->unit, l->facts[f].bank, r->loc));
    }
    for (i = 0; i < l->count; ++i) {
        fragment *f = &l->fragments[i];
        if (f->kind == DATA) {
            v2_result r = v2_evaluate(l->unit, f->expression);
            uint64_t size = f->width;
            if (r.state == V2_INVALID) {
                l->error.value_error = r.diagnostic;
                layout_fail(l, V2_LAYOUT_VALUE, r.diagnostic.location); return 0;
            }
            if (r.state == V2_KNOWN && r.value.kind == V2_STRING) {
                if (!f->strings) return layout_fail(l, V2_LAYOUT_INPUT, f->loc);
                size *= r.value.as.string.length;
            } else if (r.state == V2_DEFERRED) {
                size_t d;
                for (d = 0; d < r.dependency_count; ++d)
                    if (r.dependencies[d].kind != V2_DEP_ANCHOR)
                        return layout_fail(l, V2_LAYOUT_UNRESOLVED, f->loc);
            }
            v2_bind(l->unit, f->extent_definition, v2_extent_literal(l->unit, size, f->loc));
        }
    }
    /* Literal/constant widths also feed RAM size spans. */
    for (i = 0; i < l->count; ++i) if (l->fragments[i].kind == INSTRUCTION)
        narrow(l, &l->fragments[i]);
    if (!prefixes(l)) return 0;
    if (!v2_lower_spans(l->unit, lower_span, l) || !unit_ok(l) || l->error.code) return 0;
    if (!allocate_ram(l)) return 0;
    if (l->mode == V2_BINARY_LAYOUT) {
        uint32_t pc = l->block_count ? l->blocks[0].start : 0;
        int partial = 1;
        for (i = 0; i < l->count; ++i) {
            fragment *f = &l->fragments[i];
            if (f->kind == RAM_ORIGIN) { pc = f->origin; partial = 1; }
            else if (f->kind == RAM && partial) partial = binary_ram(l, &l->ram[f->argument], &pc, 1);
            if (l->error.code) return 0;
        }
    }
    for (i = 0; i < l->count; ++i) if (l->fragments[i].kind == INSTRUCTION) {
        if (l->ram_count || l->tree) narrow(l, &l->fragments[i]);
        if (l->fragments[i].narrow != 0xff) ++l->work.candidates;
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
    ++l->work.walks; *shortened = 0;
    for (i = 0; i < l->count; ++i) {
        fragment *f = &l->fragments[i];
        v2_layout_fact fact = l->facts[i];
        uint64_t size = 0;
        ++l->work.fragment_visits;
        if (l->mode == V2_BINARY_LAYOUT) {
            if (f->kind == RAM_ORIGIN) ram_pc = f->origin;
            else if (f->kind == RAM && !binary_ram(l, &l->ram[f->argument], &ram_pc, 0)) return 0;
        }
        if (f->kind == BANK_END && bank_size) {
            if (output - bank_start > bank_size) return layout_fail(l, V2_LAYOUT_OVERFLOW, f->loc);
            size = bank_size - (output - bank_start);
        }
        if (f->kind == ORIGIN || f->kind == BANK) pc = f->origin;
        if (f->kind == BANK) { bank_start = output; bank_size = f->argument; }
        if (f->anchor != V2_NONE && !v2_place(l->unit, f->anchor, 1, expr_i32((uint32_t)pc))) return unit_ok(l);
        fact.address = (uint32_t)pc; fact.output_offset = (uint32_t)output;
        if (f->kind == INSTRUCTION) {
            *shortened |= narrow(l, f);
            size = (unsigned)opcode_length(f->opcode);
        } else if (f->kind == PAD && f->pad != V2_PAD_SIZE) {
            uint64_t at = f->pad == V2_PAD_ORIGIN ? pc : output - bank_start;
            if (f->argument < at) return layout_fail(l, V2_LAYOUT_OVERFLOW, f->loc);
            size = f->argument - at;
        } else if (f->kind != BANK && f->kind != BANK_END && !extent(l, f->extent, f->loc, &size)) return 0;
        if (size > UINT32_MAX - output) return layout_fail(l, V2_LAYOUT_OVERFLOW, f->loc);
        fact.length = (uint32_t)size; fact.opcode = f->opcode;
        l->facts[i] = fact;
        output += size;
        if (f->kind != BANK && f->kind != BANK_END) pc += size;
        if (pc > UINT32_MAX || (l->mode == V2_LINK_LAYOUT && pc > 65536)) return layout_fail(l, V2_LAYOUT_OVERFLOW, f->loc);
        if (bank_size && output - bank_start > bank_size) return layout_fail(l, V2_LAYOUT_OVERFLOW, f->loc);
    }
    if (output > UINT32_MAX || output > SIZE_MAX) return layout_fail(l, V2_LAYOUT_OVERFLOW, v2_no_location());
    l->output_size = (size_t)output;
    return 1;
}
static int validate(v2_layout *l)
{
    size_t i;
    for (i = 0; i < l->ram_count; ++i) {
        uint64_t size;
        if (!extent(l, l->ram[i].extent, l->ram[i].loc, &size)) return 0;
        if (size != l->ram[i].size) return layout_fail(l, V2_LAYOUT_UNSTABLE, l->ram[i].loc);
    }
    for (i = 0; i < l->count; ++i) {
        fragment *f = &l->fragments[i];
        v2_layout_fact *fact = &l->facts[i];
        int32_t value;
        int warn = 0;
        if (f->kind == STORAGE || f->kind == DATA) {
            uint64_t size;
            if (!extent(l, f->extent, f->loc, &size)) return 0;
            if (size != fact->length) return layout_fail(l, V2_LAYOUT_UNSTABLE, f->loc);
        }
        if (f->kind != DATA && f->kind != INSTRUCTION) continue;
        if (f->expression == V2_NONE) continue;
        if (!evaluated(l, f->expression, f->loc, &f->value)) return 0;
        if (f->value.kind == V2_STRING && f->kind == DATA && f->strings) {
            if ((uint64_t)f->width * f->value.as.string.length != fact->length)
                return layout_fail(l, V2_LAYOUT_UNSTABLE, f->loc);
            continue;
        }
        if (f->value.kind != V2_INTEGER) return layout_fail(l, V2_LAYOUT_RANGE, f->loc);
        value = f->value.as.integer;
        fact->has_operand = 1; fact->operand = value;
        if (f->kind == DATA) {
            warn = (f->width == 1 && (value < -128 || value > 255)) ||
                   (f->width == 2 && (value < -32768 || value > 65535));
        } else {
            addressing_mode mode = opcode_addressing_mode(f->opcode);
            if (mode == RELATIVE_MODE) {
                int64_t displacement = (int64_t)value - ((int64_t)fact->address + 2);
                if (displacement < -128 || displacement > 127) return layout_fail(l, V2_LAYOUT_RANGE, f->loc);
            } else if (!f->early) {
                if (mode == ZEROPAGE_MODE || mode == ZEROPAGE_X_MODE || mode == ZEROPAGE_Y_MODE) {
                    if (value < 0 || value > 255) return layout_fail(l, V2_LAYOUT_RANGE, f->loc);
                } else if (fact->length == 2) {
                    if (value < -128 || value > 255) return layout_fail(l, V2_LAYOUT_RANGE, f->loc);
                } else if (fact->length == 3 && (value < -32768 || value > 65535)) {
                    if (l->mode == V2_LINK_LAYOUT) return layout_fail(l, V2_LAYOUT_RANGE, f->loc);
                    warn = 1;
                }
            }
        }
        fact->warning = warn != 0;
    }
    return 1;
}
static int check_copies(v2_layout *l)
{
    size_t i;
    for (i = 0; i < l->count; ++i) if (l->fragments[i].kind == COPY) {
        fragment *f = &l->fragments[i];
        unsigned char buf[4096];
        size_t at = 0, n;
        int ok = 1;
        FILE *input = fopen(f->path, "rb");
        if (!input) return layout_fail(l, V2_LAYOUT_COPY_CHANGED, f->loc);
        for (;;) {
            n = fread(buf, 1, sizeof(buf), input);
            if (!n) break;
            if (n > f->length - at || memcmp(buf, f->bytes + at, n)) { ok = 0; break; }
            at += n;
            if (n < sizeof(buf)) break;
        }
        if (ferror(input) || at != f->length) ok = 0;
        if (fclose(input)) ok = 0;
        if (!ok) return layout_fail(l, V2_LAYOUT_COPY_CHANGED, f->loc);
    }
    return 1;
}
static void put(unsigned char *to, uint32_t value, unsigned width)
{
    unsigned i;
    for (i = 0; i < width; ++i) { to[i] = (unsigned char)value; value >>= 8; }
}
static int encode(v2_layout *l)
{
    size_t i, j;
    l->output = l->output_size ? calloc(l->output_size, 1) : NULL;
    if (l->output_size && !l->output) return layout_fail(l, V2_LAYOUT_MEMORY, v2_no_location());
    for (i = 0; i < l->count; ++i) {
        fragment *f = &l->fragments[i];
        v2_layout_fact *fact = &l->facts[i];
        unsigned char *p;
        if (!fact->length) continue;
        if (!l->output || fact->output_offset > l->output_size ||
            fact->length > l->output_size - fact->output_offset)
            return layout_fail(l, V2_LAYOUT_UNSTABLE, f->loc);
        p = l->output + fact->output_offset;
        if (f->kind == RAW || f->kind == COPY) memcpy(p, f->bytes, f->length);
        else if (f->kind == INSTRUCTION) {
            uint32_t operand = (uint32_t)fact->operand;
            p[0] = f->opcode;
            if (opcode_addressing_mode(f->opcode) == RELATIVE_MODE) operand -= fact->address + 2;
            if (fact->length > 1) put(p + 1, operand, fact->length - 1);
        } else if (f->kind == DATA) {
            if (f->value.kind == V2_INTEGER) put(p, (uint32_t)f->value.as.integer, f->width);
            else for (j = 0; j < f->value.as.string.length; ++j) {
                unsigned char c = f->value.as.string.bytes[j];
                put(p + j * f->width, f->mapped ? f->map[c] : c, f->width);
            }
        }
    }
    return 1;
}
int v2_layout_run(v2_layout *l)
{
    int shortened;
    uint64_t limit;
    if (!building(l, v2_no_location())) return 0;
    if (l->bank_calls && add(l, fresh(BANK_END, v2_no_location())) == V2_NONE) return 0;
    l->started = 1;
    if (!prepare(l)) return 0;
    limit = l->work.candidates + 2;
    /* Always walk twice: forward addresses first become available in walk 1. */
    do {
        if (!walk(l, &shortened) || !unit_ok(l)) return 0;
        if (l->work.walks > limit) return layout_fail(l, V2_LAYOUT_UNSTABLE, v2_no_location());
    } while (shortened || l->work.walks == 1);
    if (!validate(l) || !check_copies(l) || !encode(l)) return 0;
    l->ready = 1; return 1;
}
const unsigned char *v2_layout_bytes(const v2_layout *l, size_t *size)
{
    *size = l->ready ? l->output_size : 0;
    return l->ready ? l->output : NULL;
}
const v2_layout_fact *v2_layout_facts(const v2_layout *l, size_t *count)
{
    *count = l->ready ? l->count : 0;
    return l->ready ? l->facts : NULL;
}
