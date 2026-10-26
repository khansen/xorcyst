/* XOROBJ2 owned tables and checked wire codec. GPL-2.0-or-later. */
#ifndef XORCYST_IR_OBJECT_H
#define XORCYST_IR_OBJECT_H
#include "value.h"
#include "heap.h"

typedef enum
{
    IR_OBJ_BLOBS,
    IR_OBJ_FILES,
    IR_OBJ_LOCATIONS,
    IR_OBJ_DEFINITIONS,
    IR_OBJ_EXPRESSIONS,
    IR_OBJ_EXTENTS,
    IR_OBJ_ANCHORS,
    IR_OBJ_FIXUPS,
    IR_OBJ_FRAGMENTS,
    IR_OBJ_RAM,
    IR_OBJ_TABLES
} ir_object_table;
typedef enum
{
    IR_OBJECT_OK,
    IR_OBJECT_MEMORY,
    IR_OBJECT_FORMAT
} ir_object_error;
typedef struct ir_object ir_object;

/* Tables contain u32 fields, not native wire structs. All added bytes are
 * copied. Forward references are permitted during construction; validate or
 * encode checks the entire object, without evaluating its expressions. */
ir_object *ir_object_create(void);
/* Same ownership as decode_owned, but input must be allocated from heap.
 * The heap outlives the decoded object. Encoded output stays free-compatible. */
ir_object *ir_object_decode_heap_owned(ir_heap *, void *, size_t, ir_object_error *);
void ir_object_destroy(ir_object *);
ir_object_error ir_object_error_at(const ir_object *);
const char *ir_object_error_message(ir_object_error);
unsigned ir_object_width(ir_object_table);
uint32_t ir_object_count(const ir_object *, ir_object_table);
const uint32_t *ir_object_record(const ir_object *, ir_object_table, ir_id);
ir_id ir_object_add(ir_object *, ir_object_table, const uint32_t *fields);
int ir_object_set(ir_object *, ir_object_table, ir_id, const uint32_t *fields);
ir_id ir_object_blob(ir_object *, const void *, size_t);
int ir_object_append_bytes(ir_object *, ir_id, const void *, size_t);
const unsigned char *ir_object_bytes(const ir_object *, ir_id, size_t *);
int ir_object_validate(ir_object *);
/* Decode owns a copy, and never retains the input. Failure returns NULL with
 * an error. Encode allocates *bytes only on success; the caller frees it. */
ir_object *ir_object_decode(const void *, size_t, ir_object_error *);
/* Takes a malloc-aligned, free-compatible buffer on both success and failure.
 * The decoder reuses its storage, converting table words in place. */
ir_object *ir_object_decode_owned(void *, size_t, ir_object_error *);
int ir_object_encode(ir_object *, unsigned char **bytes, size_t *length);
#endif
