/* Defined 32-bit integer semantics shared by expression evaluators.
 * Copyright (C) 2026 Kent Hansen
 * Distributed under the GNU General Public License, version 2 or later. */
#ifndef XORCYST_EXPR_INT_H
#define XORCYST_EXPR_INT_H

#include <stdint.h>

/* Avoid implementation-defined conversion of an out-of-range unsigned value. */
static inline int expr_i32(uint32_t bits)
{
    return bits <= INT32_MAX ? (int)bits : -1 - (int)(UINT32_MAX - bits);
}

static inline int expr_add(int lhs, int rhs)
{
    return expr_i32((uint32_t)lhs + (uint32_t)rhs);
}

static inline int expr_sub(int lhs, int rhs)
{
    return expr_i32((uint32_t)lhs - (uint32_t)rhs);
}

static inline int expr_mul(int lhs, int rhs)
{
    return expr_i32((uint32_t)lhs * (uint32_t)rhs);
}

static inline int expr_neg(int value)
{
    return expr_i32(0u - (uint32_t)value);
}

/* Callers diagnose a zero divisor. INT_MIN / -1 wraps without invoking UB. */
static inline int expr_div(int lhs, int rhs)
{
    return lhs == INT32_MIN && rhs == -1 ? INT32_MIN : lhs / rhs;
}

static inline int expr_mod(int lhs, int rhs)
{
    return lhs == INT32_MIN && rhs == -1 ? 0 : lhs % rhs;
}

static inline int expr_shl(int lhs, int rhs, int *value)
{
    if (rhs < 0 || rhs >= 32) return 0;
    *value = expr_i32((uint32_t)lhs << rhs);
    return 1;
}

static inline int expr_shr(int lhs, int rhs, int *value)
{
    if (rhs < 0 || rhs >= 32) return 0;
    *value = expr_i32((uint32_t)lhs >> rhs);
    return 1;
}

#endif
