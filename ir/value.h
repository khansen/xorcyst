/* Owned expression values for the shared backend.
 * Copyright (C) 2026 Kent Hansen
 * Distributed under the GNU General Public License, version 2 or later. */
#ifndef XORCYST_IR_VALUE_H
#define XORCYST_IR_VALUE_H

#include <stddef.h>
#include <stdint.h>

typedef uint32_t ir_id;
#define IR_NONE UINT32_MAX
#define IR_MAX_STRING_BYTES (16u * 1024u * 1024u)
#define IR_MAX_LIVE_STRING_BYTES (256u * 1024u * 1024u)

/* File IDs belong to the unit. Coordinates are one-based with an exclusive
 * end. IR_NONE denotes an unavailable location; it is not a wire encoding. */
typedef struct {
    ir_id file;
    uint32_t line, column, end_line, end_column;
} ir_location;

typedef enum { IR_KNOWN, IR_DEFERRED, IR_INVALID } ir_state;
typedef enum { IR_INTEGER, IR_STRING, IR_EXTENT } ir_value_kind;
typedef enum {
    IR_ERROR_NONE, IR_ERROR_MEMORY, IR_ERROR_REFERENCE, IR_ERROR_LOCATION,
    IR_ERROR_REBIND, IR_ERROR_TYPE, IR_ERROR_CYCLE, IR_ERROR_DIV_ZERO,
    IR_ERROR_MOD_ZERO, IR_ERROR_SHIFT, IR_ERROR_STRING_LIMIT,
    IR_ERROR_LAYOUT_STRING, IR_ERROR_EXTENT
} ir_error;

typedef struct {
    ir_error code;
    ir_location location;
} ir_diagnostic;

typedef enum { IR_DEP_DEFINITION, IR_DEP_ANCHOR } ir_dependency_kind;
typedef struct {
    ir_dependency_kind kind;
    ir_id id;
} ir_dependency;

typedef struct {
    ir_value_kind kind;
    union {
        int32_t integer;
        uint64_t extent;
        struct { const unsigned char *bytes; size_t length; } string;
    } as;
} ir_value;

typedef struct {
    ir_state state;
    /* Read value only for Known, dependencies only for Deferred, and diagnostic
     * only for Invalid. No numeric placeholder accompanies an unknown value. */
    ir_value value;
    const ir_dependency *dependencies;
    size_t dependency_count;
    ir_diagnostic diagnostic;
} ir_result;

typedef enum {
    IR_NEGATE, IR_COMPLEMENT, IR_NOT, IR_LOW, IR_HIGH,
    IR_ADD, IR_SUBTRACT, IR_MULTIPLY, IR_DIVIDE, IR_REMAINDER,
    IR_SHIFT_LEFT, IR_SHIFT_RIGHT, IR_BIT_AND, IR_BIT_OR, IR_BIT_XOR,
    IR_EQUAL, IR_NOT_EQUAL, IR_LESS, IR_LESS_EQUAL, IR_GREATER, IR_GREATER_EQUAL
} ir_operator;

typedef struct ir_unit ir_unit;
typedef struct { uint64_t evaluated_nodes, dependency_visits, invalidation_edges; } ir_work;
ir_work ir_work_done(const ir_unit *unit);

ir_location ir_no_location(void);
ir_unit *ir_unit_create(void);
void ir_unit_destroy(ir_unit *unit);
/* Optional capacity hint for a known batch of incoming expressions. Creates
 * no IDs and changes no values; allocation failure poisons the builder. */
int ir_reserve_expressions(ir_unit *unit, size_t additional);
ir_diagnostic ir_unit_error(const ir_unit *unit);
const char *ir_error_message(ir_error error);

/* Constructors copy all caller-owned bytes. On failure they return IR_NONE
 * and poison the builder; subsequent evaluation is Invalid. Destroying any
 * partially constructed unit is safe. IDs stay stable as tables grow. */
ir_id ir_file(ir_unit *unit, const char *name);
const char *ir_file_name(const ir_unit *unit, ir_id file);
ir_id ir_definition(ir_unit *unit, const char *name, ir_location location);
const char *ir_definition_name(const ir_unit *unit, ir_id definition);

/* A definition is either unresolved or bound exactly once. Captured uses and
 * later assignments use distinct IDs, even when their names match. Forward
 * lookup, .undef, scopes and imports are decided by the frontend/link binder,
 * not by this module. Binding does not eagerly evaluate unused definitions. */
int ir_bind(ir_unit *unit, ir_id definition, ir_id expression);
ir_id ir_anchor(ir_unit *unit, ir_location location);
int ir_place(ir_unit *unit, ir_id anchor, int known, int32_t value);
/* Inspect/validate a placement without adding a throwaway expression node.
 * Output pointers are optional; this does not evaluate an extent prefix. */
int ir_anchor_value(ir_unit *unit, ir_id anchor, int *known, int32_t *value,
                    ir_location location);

ir_id ir_integer(ir_unit *unit, int32_t value, ir_location location);
ir_id ir_string(ir_unit *unit, const void *bytes, size_t length, ir_location location);
ir_id ir_reference(ir_unit *unit, ir_id definition, ir_location location);
ir_id ir_address(ir_unit *unit, ir_id anchor, ir_location location);
ir_id ir_unary(ir_unit *unit, ir_operator op, ir_id operand, ir_location location);
ir_id ir_binary(ir_unit *unit, ir_operator op, ir_id left, ir_id right,
                ir_location location);

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
ir_result ir_evaluate(ir_unit *unit, ir_id expression);

#endif
