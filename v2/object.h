/* XOROBJ2 owned tables and checked wire codec. GPL-2.0-or-later. */
#ifndef XORCYST_V2_OBJECT_H
#define XORCYST_V2_OBJECT_H
#include "value.h"
#include "heap.h"

typedef enum
{
    V2_OBJ_BLOBS,
    V2_OBJ_FILES,
    V2_OBJ_LOCATIONS,
    V2_OBJ_DEFINITIONS,
    V2_OBJ_EXPRESSIONS,
    V2_OBJ_EXTENTS,
    V2_OBJ_ANCHORS,
    V2_OBJ_FIXUPS,
    V2_OBJ_FRAGMENTS,
    V2_OBJ_RAM,
    V2_OBJ_TABLES
} v2_object_table;
typedef enum
{
    V2_OBJECT_OK,
    V2_OBJECT_MEMORY,
    V2_OBJECT_FORMAT
} v2_object_error;
typedef struct v2_object v2_object;

/* Tables contain u32 fields, not native wire structs. All added bytes are
 * copied. Forward references are permitted during construction; validate or
 * encode checks the entire object, without evaluating its expressions. */
v2_object *v2_object_create(void);
/* Same ownership as decode_owned, but input must be allocated from heap.
 * The heap outlives the decoded object. Encoded output stays free-compatible. */
v2_object *v2_object_decode_heap_owned(v2_heap *, void *, size_t, v2_object_error *);
void v2_object_destroy(v2_object *);
v2_object_error v2_object_error_at(const v2_object *);
const char *v2_object_error_message(v2_object_error);
unsigned v2_object_width(v2_object_table);
uint32_t v2_object_count(const v2_object *, v2_object_table);
const uint32_t *v2_object_record(const v2_object *, v2_object_table, v2_id);
v2_id v2_object_add(v2_object *, v2_object_table, const uint32_t *fields);
int v2_object_set(v2_object *, v2_object_table, v2_id, const uint32_t *fields);
v2_id v2_object_blob(v2_object *, const void *, size_t);
int v2_object_append_bytes(v2_object *, v2_id, const void *, size_t);
const unsigned char *v2_object_bytes(const v2_object *, v2_id, size_t *);
int v2_object_validate(v2_object *);
/* Decode owns a copy, and never retains the input. Failure returns NULL with
 * an error. Encode allocates *bytes only on success; the caller frees it. */
v2_object *v2_object_decode(const void *, size_t, v2_object_error *);
/* Takes a malloc-aligned, free-compatible buffer on both success and failure.
 * The decoder reuses its storage, converting table words in place. */
v2_object *v2_object_decode_owned(void *, size_t, v2_object_error *);
int v2_object_encode(v2_object *, unsigned char **bytes, size_t *length);
#endif
