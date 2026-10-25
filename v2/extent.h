/* Checked layout quantities. Internal IR operations, never source operators. */
#ifndef XORCYST_V2_EXTENT_H
#define XORCYST_V2_EXTENT_H
#include "value.h"

/* A code/source-RAM anchor can also have a checked prefix expression. Extent
 * evaluation follows that graph; ordinary instruction evaluation uses only
 * currently placed anchors, preserving in-place forward-address walk order.
 * Bind each prefix once, before layout. The result must be an integer. */
int v2_extent_anchor(v2_unit *, v2_id anchor, v2_id expression);
v2_result v2_evaluate_extent(v2_unit *, v2_id expression);

v2_id v2_extent_literal(v2_unit *, uint64_t bytes, v2_location);
v2_id v2_extent_count(v2_unit *, v2_id integer, uint32_t min, uint32_t max, v2_location);
v2_id v2_extent_sum(v2_unit *, v2_id left, v2_id right, v2_location);
v2_id v2_extent_repeat(v2_unit *, v2_id count, v2_id element, v2_location);
v2_id v2_extent_remainder(v2_unit *, v2_id declared, v2_id initialized, v2_location);
v2_id v2_extent_bound(v2_unit *, v2_id extent, uint32_t min, uint32_t max, v2_location);
/* Data/initializer operand length: one for an integer, byte length for a
 * string. Owner-approved tag-8 clarification; not source sizeof(label). */
v2_id v2_extent_string(v2_unit *, v2_id operand, v2_location);
/* Explicit low-32-bit projection, after checked extent evaluation. */
v2_id v2_extent_integer(v2_unit *, v2_id extent, v2_location);

/* Extent queries lower subtraction of two anchors in one contiguous run to
 * checked intervening extents. Ordinary queries always retain the original
 * subtraction of placed addresses. The callback returns an integer expression,
 * or V2_NONE to keep subtraction in both modes. Bound aliases are followed.
 * Call once after construction and before layout. It visits only existing
 * expression nodes; callbacks may append nodes. No caller-owned state retained. */
typedef v2_id (*v2_span_lowerer)(void *, v2_id left, v2_id right, v2_location);
int v2_lower_spans(v2_unit *, v2_span_lowerer, void *);
/* Layout only needs lowering reachable from checked extents. Ordinary data and
 * instruction differences do not require a span tree. Roots are borrowed for
 * this call; the walk follows bound definitions and extent anchor prefixes. */
int v2_lower_extent_spans(v2_unit *, const v2_id *roots, size_t count,
                          v2_span_lowerer, void *);
#endif
