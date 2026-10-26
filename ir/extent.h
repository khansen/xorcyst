/* Checked layout quantities. Internal IR operations, never source operators. */
#ifndef XORCYST_IR_EXTENT_H
#define XORCYST_IR_EXTENT_H
#include "value.h"

/* A code/source-RAM anchor can also have a checked prefix expression. Extent
 * evaluation follows that graph; ordinary instruction evaluation uses only
 * currently placed anchors, preserving in-place forward-address walk order.
 * Bind each prefix once, before layout. The result must be an integer. */
int ir_extent_anchor(ir_unit *, ir_id anchor, ir_id expression);
ir_result ir_evaluate_extent(ir_unit *, ir_id expression);

ir_id ir_extent_literal(ir_unit *, uint64_t bytes, ir_location);
ir_id ir_extent_count(ir_unit *, ir_id integer, uint32_t min, uint32_t max, ir_location);
ir_id ir_extent_sum(ir_unit *, ir_id left, ir_id right, ir_location);
ir_id ir_extent_repeat(ir_unit *, ir_id count, ir_id element, ir_location);
ir_id ir_extent_remainder(ir_unit *, ir_id declared, ir_id initialized, ir_location);
ir_id ir_extent_bound(ir_unit *, ir_id extent, uint32_t min, uint32_t max, ir_location);
/* Data/initializer operand length: one for an integer, byte length for a
 * string. Owner-approved tag-8 clarification; not source sizeof(label). */
ir_id ir_extent_string(ir_unit *, ir_id operand, ir_location);
/* Explicit low-32-bit projection, after checked extent evaluation. */
ir_id ir_extent_integer(ir_unit *, ir_id extent, ir_location);

/* Extent queries lower subtraction of two anchors in one contiguous run to
 * checked intervening extents. Ordinary queries always retain the original
 * subtraction of placed addresses. The callback returns an integer expression,
 * or IR_NONE to keep subtraction in both modes. Bound aliases are followed.
 * Call once after construction and before layout. It visits only existing
 * expression nodes; callbacks may append nodes. No caller-owned state retained. */
typedef ir_id (*ir_span_lowerer)(void *, ir_id left, ir_id right, ir_location);
int ir_lower_spans(ir_unit *, ir_span_lowerer, void *);
/* Layout only needs lowering reachable from checked extents. Ordinary data and
 * instruction differences do not require a span tree. Roots are borrowed for
 * this call; the walk follows bound definitions and extent anchor prefixes. */
int ir_lower_extent_spans(ir_unit *, const ir_id *roots, size_t count,
                          ir_span_lowerer, void *);
#endif
