/* Deterministic cache regression, adapted from the independent review probe.
 * Differential cache fuzzer: unit A keeps its memo/reverse-edge state, unit B
 * is identical but every cache is cleared before each query. */
#include "v2/extent.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static uint64_t rng;
static uint32_t rnd(uint32_t n) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (uint32_t)(rng % n); }
static v2_unit *A, *B;
static uint32_t na, nd, nn;
static int defbound[64], hasprefix[64];
static v2_location at(uint32_t line) { v2_location l = {0, line, 1, line, 2}; return l; }
static void same(v2_id x, v2_id y) { if (x != y) { printf("id mismatch %u %u\n", x, y); exit(2); } }
extern void test_v2_clear_memos(v2_unit *);
extern void test_v2_version_rollover(v2_unit *, int);
extern size_t test_v2_node_count(v2_unit *);
extern size_t test_v2_span_nodes(v2_unit *);
static int depcmp(const void *a, const void *b)
{ const v2_dependency *x = a, *y = b; return x->kind != y->kind ? (int)x->kind - (int)y->kind : (x->id > y->id) - (x->id < y->id); }
static long mismatches, cycle_location_only, queries, states[3];

static long kinds[3][3], known_value_mismatch;
static size_t span_nodes, rollovers[3];
static int with_lowering;
static void compare(v2_id root, int extent, long step)
{
    v2_result ra, rb;
    v2_dependency da[512], db[512];
    size_t i;
    int bad = 0, cycloc = 0;
    ra = extent ? v2_evaluate_extent(A, root) : v2_evaluate(A, root);
    if (ra.state == V2_DEFERRED) { assert(ra.dependency_count <= 512); if (ra.dependency_count) memcpy(da, ra.dependencies, ra.dependency_count * sizeof(*da)); }
    test_v2_clear_memos(B);
    rb = extent ? v2_evaluate_extent(B, root) : v2_evaluate(B, root);
    if (rb.state == V2_DEFERRED) { assert(rb.dependency_count <= 512); if (rb.dependency_count) memcpy(db, rb.dependencies, rb.dependency_count * sizeof(*db)); }
    ++queries; ++states[ra.state];
    if (ra.state != rb.state) bad = 1;
    else if (ra.state == V2_KNOWN) {
        if (ra.value.kind != rb.value.kind) bad = 1;
        else if (ra.value.kind == V2_INTEGER) bad = ra.value.as.integer != rb.value.as.integer;
        else if (ra.value.kind == V2_EXTENT) bad = ra.value.as.extent != rb.value.as.extent;
        else bad = ra.value.as.string.length != rb.value.as.string.length ||
            (ra.value.as.string.length && memcmp(ra.value.as.string.bytes, rb.value.as.string.bytes, ra.value.as.string.length));
    } else if (ra.state == V2_INVALID) {
        if (ra.diagnostic.code != rb.diagnostic.code) bad = 1;
        else if (ra.diagnostic.location.line != rb.diagnostic.location.line) {
            if (ra.diagnostic.code == V2_ERROR_CYCLE) cycloc = 1; else bad = 1;
        }
    } else {
        if (ra.dependency_count != rb.dependency_count) bad = 1;
        else {
            qsort(da, ra.dependency_count, sizeof(*da), depcmp);
            qsort(db, rb.dependency_count, sizeof(*db), depcmp);
            for (i = 0; i < ra.dependency_count; ++i) if (depcmp(&da[i], &db[i])) bad = 1;
        }
    }
    if (cycloc) ++cycle_location_only;
    if (bad) {
        if (mismatches < 8) {
            printf("MISMATCH step %ld root %u mode %d: A state %d", step, root, extent, ra.state);
            if (ra.state == V2_INVALID) printf(" code %d line %u", ra.diagnostic.code, ra.diagnostic.location.line);
            if (ra.state == V2_KNOWN) printf(" kind %d int %d ext %llu", ra.value.kind, ra.value.as.integer, (unsigned long long)ra.value.as.extent);
            if (ra.state == V2_DEFERRED) printf(" deps %zu", ra.dependency_count);
            printf(" | B state %d", rb.state);
            if (rb.state == V2_INVALID) printf(" code %d line %u", rb.diagnostic.code, rb.diagnostic.location.line);
            if (rb.state == V2_KNOWN) printf(" kind %d int %d ext %llu", rb.value.kind, rb.value.as.integer, (unsigned long long)rb.value.as.extent);
            if (rb.state == V2_DEFERRED) printf(" deps %zu", rb.dependency_count);
            printf("\n");
        }
        ++mismatches;
        ++kinds[ra.state][rb.state]; if (ra.state == V2_KNOWN && rb.state == V2_KNOWN) ++known_value_mismatch;
    }
}
/* Arbitrary extent-only replacement, deliberately using a dependency that may
 * differ from both original addresses. The cleared unit is the cache oracle;
 * this is not a layout or source-semantics oracle. */
static v2_id lower_span(void *context, v2_id left, v2_id right, v2_location loc)
{
    v2_unit *u = context;
    v2_id other = (left * 7 + right * 3) % na;
    if ((left + right) % 3 == 0) return V2_NONE;
    return v2_extent_integer(u, v2_extent_sum(u,
        v2_extent_literal(u, (left + right) % 5, loc),
        v2_extent_count(u, v2_address(u, other, loc), 0, 1000, loc), loc), loc);
}
static void lower_both(void)
{
    size_t count;
    assert(v2_lower_spans(A, lower_span, A));
    assert(v2_lower_spans(B, lower_span, B));
    count = test_v2_node_count(A);
    assert(count == test_v2_node_count(B) && count <= UINT32_MAX);
    nn = (uint32_t)count;
    span_nodes += test_v2_span_nodes(A);
}
static void lowering_controls(void)
{
    v2_id a[2], b[2], root[2];
    v2_unit *units[2];
    unsigned j, k;
    A = units[0] = v2_unit_create(); B = units[1] = v2_unit_create();
    na = 4;
    for (j = 0; j < 2; ++j) {
        v2_unit *u = units[j];
        assert(v2_file(u, "f") == 0);
        for (k = 0; k < na; ++k) assert(v2_anchor(u, at(1000 + k)) == k);
        assert(v2_place(u, 0, 1, 10) && v2_place(u, 1, 1, 2) && v2_place(u, 3, 1, 20));
        a[j] = v2_address(u, 0, at(1)); b[j] = v2_address(u, 1, at(2));
        root[j] = v2_binary(u, V2_ADD,
            v2_binary(u, V2_SUBTRACT, a[j], b[j], at(3)), v2_integer(u, 1, at(4)), at(5));
    }
    same(root[0], root[1]);
    compare(root[0], 0, -1); compare(root[0], 1, -1); /* Warm both closures. */
    lower_both();
    compare(root[0], 1, -2); /* Cached 9 must become 22. */
    assert(v2_evaluate_extent(A, root[0]).value.as.integer == 22);
    assert(v2_place(A, 3, 1, 31) && v2_place(B, 3, 1, 31));
    compare(root[0], 1, -3); /* Changing only the lowered target gives 33. */
    assert(v2_evaluate_extent(A, root[0]).value.as.integer == 33);
    compare(root[0], 0, -4);
    assert(v2_evaluate(A, root[0]).value.as.integer == 9);
    v2_unit_destroy(A); v2_unit_destroy(B);
}
static v2_id pick(void) { uint32_t r = rnd(4); return r == 0 ? rnd(nn) : nn - 1 - rnd(nn < 12 ? nn : 12); }
static void make_node(void)
{
    uint32_t k = rnd(with_lowering ? 18 : 15), line = nn + 1;
    v2_id x = V2_NONE, y = V2_NONE;
    v2_location l = at(line);
    if (nn == 0) k = 0;
    switch (k) {
    case 0: { int32_t v = (int32_t)rnd(9) - 2; x = v2_integer(A, v, l); y = v2_integer(B, v, l); break; }
    case 1: { const char *s = rnd(2) ? "ab" : ""; size_t n = strlen(s); x = v2_string(A, s, n, l); y = v2_string(B, s, n, l); break; }
    case 2: case 3: { v2_id a = rnd(na); x = v2_address(A, a, l); y = v2_address(B, a, l); break; }
    case 4: { v2_id d = rnd(nd); x = v2_reference(A, d, l); y = v2_reference(B, d, l); break; }
    case 5: { v2_operator o = (v2_operator)rnd(5); v2_id c = pick(); x = v2_unary(A, o, c, l); y = v2_unary(B, o, c, l); break; }
    case 6: case 7: { v2_operator o = (v2_operator)(5 + rnd(16)); v2_id c = pick(), e = pick();
        if (rnd(2)) o = rnd(2) ? V2_ADD : V2_SUBTRACT;
        x = v2_binary(A, o, c, e, l); y = v2_binary(B, o, c, e, l); break; }
    case 8: { uint64_t v = rnd(4); x = v2_extent_literal(A, v, l); y = v2_extent_literal(B, v, l); break; }
    case 9: { v2_id c = pick(); uint32_t mn = rnd(2), mx = 2 + rnd(300);
        x = v2_extent_count(A, c, mn, mx, l); y = v2_extent_count(B, c, mn, mx, l); break; }
    case 10: { v2_id c = pick(), e = pick(); x = v2_extent_sum(A, c, e, l); y = v2_extent_sum(B, c, e, l); break; }
    case 11: { v2_id c = pick(), e = pick(); x = v2_extent_repeat(A, c, e, l); y = v2_extent_repeat(B, c, e, l); break; }
    case 12: { v2_id c = pick(), e = pick(); x = v2_extent_remainder(A, c, e, l); y = v2_extent_remainder(B, c, e, l); break; }
    case 13: { v2_id c = pick(); uint32_t mn = rnd(2), mx = 1 + rnd(400);
        x = v2_extent_bound(A, c, mn, mx, l); y = v2_extent_bound(B, c, mn, mx, l); break; }
    case 15: case 16: case 17: {
        v2_id a = rnd(na), b = rnd(na), left, right;
        left = v2_address(A, a, l); same(left, v2_address(B, a, l)); ++nn;
        right = v2_address(A, b, at(line + 1)); same(right, v2_address(B, b, at(line + 1))); ++nn;
        x = v2_binary(A, V2_SUBTRACT, left, right, at(line + 2));
        y = v2_binary(B, V2_SUBTRACT, left, right, at(line + 2)); break;
    }
    default: { v2_id c = pick();
        if (rnd(2)) { x = v2_extent_integer(A, c, l); y = v2_extent_integer(B, c, l); }
        else { x = v2_extent_string(A, c, l); y = v2_extent_string(B, c, l); } break; }
    }
    same(x, y);
    if (x == V2_NONE) { printf("constructor failed %u\n", k); exit(3); }
    ++nn;
}
void test_layout_cache(void)
{
    long seeds = 128, steps = 600, s, i;
    lowering_controls();
    for (with_lowering = 0; with_lowering < 2; ++with_lowering) {
    for (s = 1; s <= seeds; ++s) {
        int lowered = 0;
        rng = 0x9E3779B97F4A7C15ull * (uint64_t)s + 1;
        A = v2_unit_create(); B = v2_unit_create();
        same(v2_file(A, "f"), v2_file(B, "f"));
        na = 2 + rnd(6); nd = 1 + rnd(5); nn = 0;
        memset(defbound, 0, sizeof defbound); memset(hasprefix, 0, sizeof hasprefix);
        for (i = 0; i < (long)na; ++i) same(v2_anchor(A, at(1000 + (uint32_t)i)), v2_anchor(B, at(1000 + (uint32_t)i)));
        for (i = 0; i < (long)nd; ++i) same(v2_definition(A, "d", at(2000 + (uint32_t)i)), v2_definition(B, "d", at(2000 + (uint32_t)i)));
        for (i = 0; i < 8; ++i) make_node();
        for (i = 0; i < steps; ++i) {
            uint32_t r = rnd(100);
            if (r < 20) make_node();
            else if (r < 40) { v2_id a = rnd(na); int k = rnd(4) != 0; int32_t v = (int32_t)rnd(6) * 50 - 20;
                v2_place(A, a, k, v); v2_place(B, a, k, v); }
            else if (r < 44) { v2_id d = rnd(nd); if (!defbound[d]) { v2_id e = pick(); v2_bind(A, d, e); v2_bind(B, d, e); defbound[d] = 1; } }
            else if (r < 48) { v2_id a = rnd(na); if (!hasprefix[a]) { v2_id e = pick(); v2_extent_anchor(A, a, e); v2_extent_anchor(B, a, e); hasprefix[a] = 1; } }
            else if (with_lowering && r < 50 && !lowered) { lower_both(); lowered = 1; }
            else if (with_lowering && r < 51) {
                int which = (int)rnd(3);
                test_v2_version_rollover(A, which); test_v2_version_rollover(B, which);
                ++rollovers[which];
            }
            else compare(rnd(4) ? pick() : rnd(nn), (int)rnd(2), i);
            if (v2_unit_error(A).code || v2_unit_error(B).code) { printf("unit error %d %d seed %ld\n", v2_unit_error(A).code, v2_unit_error(B).code, s); exit(4); }
        }
        v2_unit_destroy(A); v2_unit_destroy(B);
    }
    }
    printf("queries %ld (known %ld deferred %ld invalid %ld): %ld mismatches, %ld cycle-location-only differences\n",
           queries, states[0], states[1], states[2], mismatches, cycle_location_only);
    { int i2, j2; for (i2 = 0; i2 < 3; ++i2) for (j2 = 0; j2 < 3; ++j2) if (kinds[i2][j2]) printf("  A state %d vs fresh %d: %ld\n", i2, j2, kinds[i2][j2]); printf("  known-value mismatches: %ld\n", known_value_mismatch); }
    assert(mismatches == 0 && cycle_location_only == 0);
    assert(span_nodes > 1 && rollovers[0] && rollovers[1] && rollovers[2]);
    printf("span nodes %zu; forced binding/layout/request rollovers %zu/%zu/%zu\n",
           span_nodes, rollovers[0], rollovers[1], rollovers[2]);
}
