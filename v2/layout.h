/* Isolated v2 layout/encoding. Copyright (C) 2026 Kent Hansen.
 * GPL version 2 or later. No parser, object I/O or publication side effects. */
#ifndef XORCYST_V2_LAYOUT_H
#define XORCYST_V2_LAYOUT_H
#include "extent.h"
#include <stdio.h>

typedef struct v2_layout v2_layout;
typedef enum { V2_BINARY_LAYOUT, V2_LINK_LAYOUT } v2_layout_mode;
typedef enum {
    V2_LAYOUT_OK, V2_LAYOUT_MEMORY, V2_LAYOUT_INPUT, V2_LAYOUT_VALUE,
    V2_LAYOUT_UNRESOLVED, V2_LAYOUT_RANGE, V2_LAYOUT_RAM,
    V2_LAYOUT_OVERFLOW, V2_LAYOUT_COPY_CHANGED, V2_LAYOUT_UNSTABLE
} v2_layout_error;
typedef struct {
    v2_layout_error code;
    v2_location location;
    v2_diagnostic value_error;
} v2_layout_diagnostic;
typedef enum {
    V2_FACT_MARK, V2_FACT_RAW, V2_FACT_DATA, V2_FACT_INSTRUCTION,
    V2_FACT_FILL, V2_FACT_PLACEMENT, V2_FACT_RAM
} v2_layout_fact_kind;
typedef struct {
    v2_layout_fact_kind kind;
    v2_location location;
    uint32_t address, output_offset, length;
    int32_t bank, operand;
    unsigned char opcode;
    unsigned has_operand, warning;
} v2_layout_fact;
typedef struct {
    uint64_t walks, fragment_visits, candidates, narrowings;
} v2_layout_work;

/* The caller owns values and must keep it alive until layout is destroyed.
 * Build once, bind definitions, then run once. The layout copies raw bytes,
 * maps and paths. Anchors attached to fragments/RAM must be distinct and
 * initially unplaced. A mark attaches any number of labels at a boundary.
 * Fragment/extent/anchor IDs occupy separate domains. Constructors fail closed.
 * Once run begins no mutation or second run is permitted. Only a successful
 * run exposes bytes/facts. Later edits to values cannot change that snapshot. */
v2_layout *v2_layout_create(v2_unit *values, v2_layout_mode mode);
void v2_layout_destroy(v2_layout *);
v2_layout_diagnostic v2_layout_error_at(const v2_layout *);
const char *v2_layout_error_message(v2_layout_error);
v2_layout_work v2_layout_work_done(const v2_layout *);

/* Origin sets the PC without emitting padding. A link origin is supplied by
 * the script, never by a serialized .org. A bank pads the previous bank and
 * starts the next bank at origin; run completion pads the last bank. */
v2_id v2_layout_origin(v2_layout *, uint32_t origin, v2_location);
v2_id v2_layout_bank(v2_layout *, uint32_t origin, uint32_t size, v2_location);
v2_id v2_layout_mark(v2_layout *, v2_id anchor, v2_location);
v2_id v2_layout_raw(v2_layout *, const void *, size_t, v2_location);
/* Attach a boundary anchor to the raw run itself, avoiding an empty mark
 * fragment when constructing object input. Bytes and placement are unchanged. */
v2_id v2_layout_raw_anchored(v2_layout *, v2_id anchor, const void *, size_t, v2_location);
v2_id v2_layout_copy(v2_layout *, const char *path, v2_location);
v2_id v2_layout_data(v2_layout *, v2_id pc, v2_id expression, unsigned width,
                      int strings, const unsigned char *map256, v2_location);
/* early_truncated records the frontend's early literal-truncation policy: a
 * fixed-width literal may retain its original value here and emit its low bits.
 * Late values get the final opcode's range policy. Force-wide disables
 * optional narrowing only; supplied opcodes are validated against opcode.c. */
v2_id v2_layout_instruction(v2_layout *, v2_id pc, unsigned char opcode,
                             v2_id operand, int force_wide, int early_truncated,
                             v2_location);
/* Equivalent to a literal integer expression, stored directly in the operand
 * fixup to avoid a full dependency/cache node for a value that cannot change. */
v2_id v2_layout_literal_instruction(v2_layout *, v2_id pc, unsigned char opcode,
                                     int32_t operand, int force_wide, int early_truncated,
                                     v2_location);
v2_id v2_layout_storage(v2_layout *, v2_id pc, v2_id checked_extent, v2_location);
typedef enum { V2_PAD_SIZE, V2_PAD_ORIGIN, V2_PAD_BANK_OFFSET } v2_pad_kind;
v2_id v2_layout_pad(v2_layout *, v2_pad_kind, uint32_t value, v2_location);

/* Half-open RAM blocks in script priority order. Append objects in unit link
 * order, and v1 emitted-label order within each unit. The checked source-span
 * extent is supplied by lowering; physical address subtraction is not a size.
 * Binary mode walks RAM markers in source order; annotations do not align or
 * constrain its cursor (v1 behavior). Earlier code/RAM dependencies are allowed.
 * Link mode resolves all extents first, applies v1's exact quicksort, then
 * first-fit allocation (alignment holes remain available). */
int v2_layout_ram_origin(v2_layout *, uint32_t origin, v2_location);
int v2_layout_ram_block(v2_layout *, uint32_t start, uint32_t end);
int v2_layout_ram(v2_layout *, v2_id anchor, v2_id extent, unsigned alignment,
                   int zeropage, v2_location);
/* ^ owner: the linker supplies an anchor in the provider unit's code run,
 * including for a RAM symbol or constant. -1 without a bank, bank index
 * otherwise; invalid in binary mode. I4 binds symbols to this owner fact. */
v2_id v2_layout_bank_expression(v2_layout *, v2_id anchor, v2_location);

/* Full analysis snapshots are enabled by default. A byte-only consumer may
 * disable the array before run; indexed facts remain available without an
 * additional allocation. v2_layout_facts then returns NULL and count zero. */
int v2_layout_keep_facts(v2_layout *, int keep);
/* Buffered output is the default. A staging-file consumer may disable it
 * before run, then write a half-open range of fragment IDs after success.
 * The same encoder serves both paths. No evaluation or input reads occur
 * during writing, and the caller owns publication and handles I/O failure.
 * With buffering disabled, bytes() returns NULL and length zero. */
int v2_layout_keep_bytes(v2_layout *, int keep);
int v2_layout_write_fragments(const v2_layout *, size_t first, size_t end, FILE *);
int v2_layout_fact_at(const v2_layout *, v2_id, v2_layout_fact *);
size_t v2_layout_fact_count(const v2_layout *);
int v2_layout_run(v2_layout *);
const unsigned char *v2_layout_bytes(const v2_layout *, size_t *length);
const v2_layout_fact *v2_layout_facts(const v2_layout *, size_t *count);
#endif
