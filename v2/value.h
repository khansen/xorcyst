/* Owned expression values for the isolated v2 backend.
 * Copyright (C) 2026 Kent Hansen
 * Distributed under the GNU General Public License, version 2 or later. */
#ifndef XORCYST_V2_VALUE_H
#define XORCYST_V2_VALUE_H

#include <stddef.h>
#include <stdint.h>

typedef uint32_t v2_id;
#define V2_NONE UINT32_MAX
#define V2_MAX_STRING_BYTES (16u * 1024u * 1024u)
#define V2_MAX_LIVE_STRING_BYTES (256u * 1024u * 1024u)

/* File IDs belong to the unit. Coordinates are one-based with an exclusive
 * end. V2_NONE denotes an unavailable location; it is not a wire encoding. */
typedef struct {
    v2_id file;
    uint32_t line, column, end_line, end_column;
} v2_location;

typedef enum { V2_KNOWN, V2_DEFERRED, V2_INVALID } v2_state;
typedef enum { V2_INTEGER, V2_STRING, V2_EXTENT } v2_value_kind;
typedef enum {
    V2_ERROR_NONE, V2_ERROR_MEMORY, V2_ERROR_REFERENCE, V2_ERROR_LOCATION,
    V2_ERROR_REBIND, V2_ERROR_TYPE, V2_ERROR_CYCLE, V2_ERROR_DIV_ZERO,
    V2_ERROR_MOD_ZERO, V2_ERROR_SHIFT, V2_ERROR_STRING_LIMIT,
    V2_ERROR_LAYOUT_STRING, V2_ERROR_EXTENT
} v2_error;

typedef struct {
    v2_error code;
    v2_location location;
} v2_diagnostic;

typedef enum { V2_DEP_DEFINITION, V2_DEP_ANCHOR } v2_dependency_kind;
typedef struct {
    v2_dependency_kind kind;
    v2_id id;
} v2_dependency;

typedef struct {
    v2_value_kind kind;
    union {
        int32_t integer;
        uint64_t extent;
        struct { const unsigned char *bytes; size_t length; } string;
    } as;
} v2_value;

typedef struct {
    v2_state state;
    /* Read value only for Known, dependencies only for Deferred, and diagnostic
     * only for Invalid. No numeric placeholder accompanies an unknown value. */
    v2_value value;
    const v2_dependency *dependencies;
    size_t dependency_count;
    v2_diagnostic diagnostic;
} v2_result;

typedef enum {
    V2_NEGATE, V2_COMPLEMENT, V2_NOT, V2_LOW, V2_HIGH,
    V2_ADD, V2_SUBTRACT, V2_MULTIPLY, V2_DIVIDE, V2_REMAINDER,
    V2_SHIFT_LEFT, V2_SHIFT_RIGHT, V2_BIT_AND, V2_BIT_OR, V2_BIT_XOR,
    V2_EQUAL, V2_NOT_EQUAL, V2_LESS, V2_LESS_EQUAL, V2_GREATER, V2_GREATER_EQUAL
} v2_operator;

typedef struct v2_unit v2_unit;
typedef struct { uint64_t evaluated_nodes, dependency_visits, invalidation_edges; } v2_work;
v2_work v2_work_done(const v2_unit *unit);

v2_location v2_no_location(void);
v2_unit *v2_unit_create(void);
void v2_unit_destroy(v2_unit *unit);
v2_diagnostic v2_unit_error(const v2_unit *unit);
const char *v2_error_message(v2_error error);

/* Constructors copy all caller-owned bytes. On failure they return V2_NONE
 * and poison the builder; subsequent evaluation is Invalid. Destroying any
 * partially constructed unit is safe. IDs stay stable as tables grow. */
v2_id v2_file(v2_unit *unit, const char *name);
const char *v2_file_name(const v2_unit *unit, v2_id file);
v2_id v2_definition(v2_unit *unit, const char *name, v2_location location);
const char *v2_definition_name(const v2_unit *unit, v2_id definition);

/* A definition is either unresolved or bound exactly once. Captured uses and
 * later assignments use distinct IDs, even when their names match. Forward
 * lookup, .undef, scopes and imports are decided by the frontend/link binder,
 * not by this module. Binding does not eagerly evaluate unused definitions. */
int v2_bind(v2_unit *unit, v2_id definition, v2_id expression);
v2_id v2_anchor(v2_unit *unit, v2_location location);
int v2_place(v2_unit *unit, v2_id anchor, int known, int32_t value);

v2_id v2_integer(v2_unit *unit, int32_t value, v2_location location);
v2_id v2_string(v2_unit *unit, const void *bytes, size_t length, v2_location location);
v2_id v2_reference(v2_unit *unit, v2_id definition, v2_location location);
v2_id v2_address(v2_unit *unit, v2_id anchor, v2_location location);
v2_id v2_unary(v2_unit *unit, v2_operator op, v2_id operand, v2_location location);
v2_id v2_binary(v2_unit *unit, v2_operator op, v2_id left, v2_id right,
                v2_location location);

/* Iterative, memoized graph evaluation. Placement updates invalidate dependent
 * results, but not constants. Invalid includes a located diagnostic on every
 * request, including cache hits; callers decide how to report it.
 *
 * Known string bytes and names remain valid until unit destruction. An empty
 * string may have bytes == NULL; skip byte access and memory/string calls when
 * its length is zero, including memcpy with a zero count. Deferred
 * dependency arrays remain valid until the next evaluation or destruction.
 * Results are borrowed read-only views, never ownership transfers. The unit is
 * not thread-safe. String values depending on placement are Invalid. */
v2_result v2_evaluate(v2_unit *unit, v2_id expression);

#endif
