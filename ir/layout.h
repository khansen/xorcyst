/* Shared layout/encoding. Copyright (C) 2026 Kent Hansen.
 * GPL version 2 or later. No parser, object I/O or publication side effects. */
#ifndef XORCYST_IR_LAYOUT_H
#define XORCYST_IR_LAYOUT_H
#include "extent.h"
#include <stdio.h>

typedef struct ir_layout ir_layout;
typedef enum { IR_BINARY_LAYOUT, IR_LINK_LAYOUT } ir_layout_mode;
typedef enum {
    IR_LAYOUT_OK, IR_LAYOUT_MEMORY, IR_LAYOUT_INPUT, IR_LAYOUT_VALUE,
    IR_LAYOUT_UNRESOLVED, IR_LAYOUT_RANGE, IR_LAYOUT_RAM,
    IR_LAYOUT_OVERFLOW, IR_LAYOUT_COPY_CHANGED, IR_LAYOUT_UNSTABLE
} ir_layout_error;
typedef struct {
    ir_layout_error code;
    ir_location location;
    ir_diagnostic value_error;
} ir_layout_diagnostic;
typedef enum {
    IR_FACT_MARK, IR_FACT_RAW, IR_FACT_DATA, IR_FACT_INSTRUCTION,
    IR_FACT_FILL, IR_FACT_PLACEMENT, IR_FACT_RAM
} ir_layout_fact_kind;
typedef struct {
    ir_layout_fact_kind kind;
    ir_location location;
    /* Binary RAM facts describe the data cursor/extent, with no emitted bytes.
     * Their output_offset is the contemporaneous code-output position. */
    uint32_t address, output_offset, length;
    int32_t bank, operand;
    unsigned char opcode;
    unsigned has_operand, warning;
} ir_layout_fact;
typedef struct {
    uint64_t walks, fragment_visits, candidates, narrowings;
} ir_layout_work;

/* The caller owns values and must keep it alive until layout is destroyed.
 * Build once, bind definitions, then run once. The layout copies raw bytes,
 * maps and paths. Anchors attached to fragments/RAM must be distinct and
 * initially unplaced. A mark attaches any number of labels at a boundary.
 * Fragment/extent/anchor IDs occupy separate domains. Constructors fail closed.
 * Once run begins no mutation or second run is permitted. Only a successful
 * run exposes bytes/facts. Later edits to values cannot change that snapshot. */
ir_layout *ir_layout_create(ir_unit *values, ir_layout_mode mode);
void ir_layout_destroy(ir_layout *);
ir_layout_diagnostic ir_layout_error_at(const ir_layout *);
const char *ir_layout_error_message(ir_layout_error);
ir_layout_work ir_layout_work_done(const ir_layout *);

/* Origin sets the PC without emitting padding. A link origin is supplied by
 * the script, never by a serialized .org. A bank pads the previous bank and
 * starts the next bank at origin; run completion pads the last bank. */
ir_id ir_layout_origin(ir_layout *, uint32_t origin, ir_location);
ir_id ir_layout_bank(ir_layout *, uint32_t origin, uint32_t size, ir_location);
ir_id ir_layout_mark(ir_layout *, ir_id anchor, ir_location);
ir_id ir_layout_raw(ir_layout *, const void *, size_t, ir_location);
/* Attach a boundary anchor to the raw run itself, avoiding an empty mark
 * fragment when constructing object input. Bytes and placement are unchanged. */
ir_id ir_layout_raw_anchored(ir_layout *, ir_id anchor, const void *, size_t, ir_location);
ir_id ir_layout_copy(ir_layout *, const char *path, ir_location);
ir_id ir_layout_data(ir_layout *, ir_id pc, ir_id expression, unsigned width,
                      int strings, const unsigned char *map256, ir_location);
/* early_truncated records the frontend's early literal-truncation policy: a
 * fixed-width literal may retain its original value here and emit its low bits.
 * Late values get the final opcode's range policy. Force-wide disables
 * optional narrowing only; supplied opcodes are validated against opcode.c. */
ir_id ir_layout_instruction(ir_layout *, ir_id pc, unsigned char opcode,
                             ir_id operand, int force_wide, int early_truncated,
                             ir_location);
/* Equivalent to a literal integer expression, stored directly in the operand
 * fixup to avoid a full dependency/cache node for a value that cannot change. */
ir_id ir_layout_literal_instruction(ir_layout *, ir_id pc, unsigned char opcode,
                                     int32_t operand, int force_wide, int early_truncated,
                                     ir_location);
ir_id ir_layout_storage(ir_layout *, ir_id pc, ir_id checked_extent, ir_location);
typedef enum { IR_PAD_SIZE, IR_PAD_ORIGIN, IR_PAD_BANK_OFFSET } ir_pad_kind;
ir_id ir_layout_pad(ir_layout *, ir_pad_kind, uint32_t value, ir_location);

/* Half-open RAM blocks in script priority order. Append objects in unit link
 * order, and emitted-label order within each unit. The checked source-span
 * extent is supplied by lowering; physical address subtraction is not a size.
 * Binary mode walks RAM markers in source order; annotations do not align or
 * constrain its cursor. Earlier code/RAM dependencies are allowed.
 * Link mode resolves all extents first, applies the specified quicksort, then
 * first-fit allocation (alignment holes remain available). */
int ir_layout_ram_origin(ir_layout *, uint32_t origin, ir_location);
int ir_layout_ram_block(ir_layout *, uint32_t start, uint32_t end);
int ir_layout_ram(ir_layout *, ir_id anchor, ir_id extent, unsigned alignment,
                   int zeropage, ir_location);
/* ^ owner: the linker supplies an anchor in the provider unit's code run,
 * including for a RAM symbol or constant. -1 without a bank, bank index
 * otherwise; invalid in binary mode. The linker binds symbols to this owner fact. */
ir_id ir_layout_bank_expression(ir_layout *, ir_id anchor, ir_location);

/* Full analysis snapshots are enabled by default. A byte-only consumer may
 * disable the array before run; indexed facts remain available without an
 * additional allocation. ir_layout_facts then returns NULL and count zero. */
int ir_layout_keep_facts(ir_layout *, int keep);
/* Buffered output is the default. A staging-file consumer may disable it
 * before run, then write a half-open range of fragment IDs after success.
 * The same encoder serves both paths. No evaluation or input reads occur
 * during writing, and the caller owns publication and handles I/O failure.
 * With buffering disabled, bytes() returns NULL and length zero. */
int ir_layout_keep_bytes(ir_layout *, int keep);
int ir_layout_write_fragments(const ir_layout *, size_t first, size_t end, FILE *);
int ir_layout_fact_at(const ir_layout *, ir_id, ir_layout_fact *);
size_t ir_layout_fact_count(const ir_layout *);
/* Number of appended fragments, including while building. A source collector
 * can retain half-open fragment ranges without inspecting unresolved facts. */
size_t ir_layout_fragment_count(const ir_layout *);
int ir_layout_run(ir_layout *);
const unsigned char *ir_layout_bytes(const ir_layout *, size_t *length);
const ir_layout_fact *ir_layout_facts(const ir_layout *, size_t *count);
#endif
