#include "v2/value.h"
#include "v2/extent.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)
extern void test_v2_allocation_failure(size_t);
extern size_t test_v2_allocation_calls(void);
extern size_t test_v2_live_allocations(void);
extern void test_v2_version_rollover(v2_unit *, int);

static v2_location loc(v2_id file, uint32_t line)
{
    v2_location l = { file, line, 1, line, 9 };
    return l;
}
static v2_id number(v2_unit *u, int32_t x) { return v2_integer(u, x, v2_no_location()); }
static v2_id binary(v2_unit *u, v2_operator op, v2_id a, v2_id b)
{ return v2_binary(u, op, a, b, v2_no_location()); }
static void integer_is(v2_unit *u, v2_id id, int32_t expected)
{
    v2_result r = v2_evaluate(u, id);
    CHECK(r.state == V2_KNOWN && r.value.kind == V2_INTEGER);
    CHECK(r.value.as.integer == expected && r.dependency_count == 0);
}
static void string_is(v2_unit *u, v2_id id, const void *bytes, size_t length)
{
    v2_result r = v2_evaluate(u, id);
    CHECK(r.state == V2_KNOWN && r.value.kind == V2_STRING);
    CHECK(r.value.as.string.length == length);
    CHECK(!length || !memcmp(r.value.as.string.bytes, bytes, length));
}
static void error_is(v2_unit *u, v2_id id, v2_error error, v2_id file, uint32_t line)
{
    v2_result r = v2_evaluate(u, id);
    CHECK(r.state == V2_INVALID && r.diagnostic.code == error);
    CHECK(r.diagnostic.location.file == file && r.diagnostic.location.line == line);
    CHECK(r.dependency_count == 0);
}
static int has_dependency(v2_result r, v2_dependency_kind kind, v2_id id)
{
    size_t i;
    for (i = 0; i < r.dependency_count; ++i)
        if (r.dependencies[i].kind == kind && r.dependencies[i].id == id) return 1;
    return 0;
}

static void arithmetic_stream(void)
{
    int mode, op;
    int32_t a, b;
    while (scanf("%d %d %" SCNd32 " %" SCNd32, &mode, &op, &a, &b) == 4) {
        v2_unit *u = v2_unit_create();
        v2_id x, y, root, dx = V2_NONE, dy = V2_NONE;
        v2_result r;
        CHECK(u);
        if (mode == 0) { x = number(u, a); y = number(u, b); }
        else if (mode == 1) {
            dx = v2_definition(u, "captured", v2_no_location());
            dy = v2_definition(u, "captured", v2_no_location());
            x = v2_reference(u, dx, v2_no_location());
            y = v2_reference(u, dy, v2_no_location());
        } else {
            dx = v2_anchor(u, v2_no_location()); dy = v2_anchor(u, v2_no_location());
            x = v2_address(u, dx, v2_no_location()); y = v2_address(u, dy, v2_no_location());
        }
        root = op <= V2_HIGH ? v2_unary(u, (v2_operator)op, x, v2_no_location())
                            : binary(u, (v2_operator)op, x, y);
        if (mode) {
            r = v2_evaluate(u, root); CHECK(r.state == V2_DEFERRED);
            if (mode == 1) { CHECK(v2_bind(u, dx, number(u, a))); CHECK(v2_bind(u, dy, number(u, b))); }
            else { CHECK(v2_place(u, dx, 1, a)); CHECK(v2_place(u, dy, 1, b)); }
        }
        r = v2_evaluate(u, root);
        if (r.state == V2_KNOWN) { CHECK(r.value.kind == V2_INTEGER); printf("K %" PRId32 "\n", r.value.as.integer); }
        else { CHECK(r.state == V2_INVALID); printf("E %s\n", v2_error_message(r.diagnostic.code)); }
        v2_unit_destroy(u);
    }
    CHECK(feof(stdin));
}

/* A shared graph is evaluated repeatedly while placement facts change. The
 * Python caller keeps an independent graph and predicts each requested value. */
static void graph_stream(void)
{
    v2_unit *u = v2_unit_create();
    char command;
    v2_id anchors[8], next = 0;
    int32_t value;
    unsigned x, y, op, count = 0;
    CHECK(u);
    while (scanf(" %c", &command) == 1) {
        v2_id id;
        if (command == 'i') {
            CHECK(scanf("%" SCNd32, &value) == 1); id = number(u, value);
        } else if (command == 'a') {
            CHECK(count < 8); anchors[count] = v2_anchor(u, v2_no_location());
            id = v2_address(u, anchors[count++], v2_no_location());
        } else if (command == 'b') {
            CHECK(scanf("%u %u %u", &op, &x, &y) == 3);
            id = binary(u, (v2_operator)op, x, y);
        } else if (command == 'p') {
            CHECK(scanf("%u %u %" SCNd32, &x, &y, &value) == 3 && x < count);
            CHECK(v2_place(u, anchors[x], (int)y, value)); continue;
        } else {
            v2_result r;
            CHECK(command == 'e' && scanf("%u", &x) == 1);
            r = v2_evaluate(u, x);
            if (r.state == V2_KNOWN) printf("K %" PRId32 "\n", r.value.as.integer);
            else if (r.state == V2_INVALID) printf("E %s\n", v2_error_message(r.diagnostic.code));
            else printf("D\n");
            continue;
        }
        CHECK(id == next++);
    }
    CHECK(feof(stdin)); v2_unit_destroy(u);
}

static void ownership(void)
{
    v2_unit *u = v2_unit_create(), *other = v2_unit_create();
    char file[] = "caller.asm", name[] = "Same";
    unsigned char bytes[] = { 'A', 0, 'B', 255 };
    v2_id f = v2_file(u, file), d = v2_definition(u, name, loc(f, 12));
    v2_id str = v2_string(u, bytes, sizeof(bytes), loc(f, 13));
    v2_id alias = v2_reference(u, d, loc(f, 14));
    v2_id other_d = v2_definition(other, name, v2_no_location());
    v2_id other_r = v2_reference(other, other_d, v2_no_location());
    size_t i;
    CHECK(f == 0 && d == 0 && other_d == 0);
    CHECK(v2_bind(u, d, str)); CHECK(v2_bind(other, other_d, number(other, 77)));
    memset(file, 'x', sizeof(file)); memset(name, 'x', sizeof(name)); memset(bytes, 0, sizeof(bytes));
    for (i = 0; i < 1000; ++i) {
        CHECK(v2_file(u, "caller.asm") != V2_NONE);
        CHECK(v2_definition(u, "Same", v2_no_location()) != V2_NONE);
        CHECK(number(u, (int32_t)i) != V2_NONE);
    }
    CHECK(!strcmp(v2_file_name(u, f), "caller.asm"));
    CHECK(!strcmp(v2_definition_name(u, d), "Same"));
    { const unsigned char expected[] = { 'A', 0, 'B', 255 }; string_is(u, alias, expected, sizeof(expected)); }
    v2_unit_destroy(u);
    integer_is(other, other_r, 77);
    v2_unit_destroy(other);
}

static void anchor_inspection(void)
{
    v2_unit *u = v2_unit_create();
    v2_location at = v2_no_location();
    v2_id a = v2_anchor(u, at), expr = v2_address(u, a, at);
    int known = -1;
    int32_t value = -1;
    CHECK(v2_anchor_value(u, a, &known, &value, at) && !known);
    CHECK(v2_extent_anchor(u, a, number(u, 99)));
    CHECK(v2_evaluate_extent(u, expr).value.as.integer == 99);
    CHECK(v2_anchor_value(u, a, &known, NULL, at) && !known);
    CHECK(v2_place(u, a, 1, 42));
    CHECK(v2_anchor_value(u, a, &known, &value, at) && known && value == 42);
    CHECK(v2_anchor_value(u, a, NULL, NULL, at));
    CHECK(v2_place(u, a, 0, 0));
    CHECK(v2_anchor_value(u, a, &known, NULL, at) && !known);
    CHECK(!v2_anchor_value(u, V2_NONE, NULL, NULL, at));
    CHECK(v2_unit_error(u).code == V2_ERROR_REFERENCE);
    v2_unit_destroy(u);
    u = v2_unit_create();
    a = v2_anchor(u, at);
    at.file = 77;
    CHECK(!v2_anchor_value(u, a, NULL, NULL, at));
    CHECK(v2_unit_error(u).code == V2_ERROR_LOCATION);
    CHECK(v2_unit_error(u).location.file == V2_NONE);
    v2_unit_destroy(u);
}

static void bindings(void)
{
    v2_unit *u = v2_unit_create();
    v2_id pending = v2_definition(u, "I", v2_no_location());
    v2_id forward = v2_reference(u, pending, v2_no_location());
    v2_id first = v2_definition(u, "I", v2_no_location());
    v2_id captured = v2_reference(u, first, v2_no_location());
    v2_id later = v2_definition(u, "I", v2_no_location());
    v2_id current = v2_reference(u, later, v2_no_location());
    CHECK(v2_bind(u, first, number(u, 1))); integer_is(u, captured, 1);
    CHECK(v2_evaluate(u, forward).state == V2_DEFERRED);
    CHECK(v2_bind(u, later, number(u, 2))); CHECK(v2_bind(u, pending, current));
    integer_is(u, forward, 2); integer_is(u, captured, 1); integer_is(u, current, 2);
    /* The frontend supplies the selected replacement after .undef; no lookup by
     * spelling inside this graph is allowed to retarget the earlier reference. */
    { v2_id replacement = v2_definition(u, "I", v2_no_location());
      CHECK(v2_bind(u, replacement, number(u, 3))); integer_is(u, captured, 1); }
    CHECK(!v2_bind(u, first, number(u, 4)));
    CHECK(v2_unit_error(u).code == V2_ERROR_REBIND);
    error_is(u, captured, V2_ERROR_REBIND, V2_NONE, 0);
    v2_unit_destroy(u);
}

static void strings(void)
{
    v2_unit *u = v2_unit_create();
    v2_id f = v2_file(u, "strings.asm");
    v2_id s = v2_string(u, "A", 1, loc(f, 1));
    v2_id d = v2_definition(u, "S", loc(f, 2)), ref = v2_reference(u, d, loc(f, 3));
    v2_id zero = number(u, 0), sum = binary(u, V2_ADD, ref, zero);
    v2_id empty = v2_string(u, NULL, 0, loc(f, 4));
    v2_id nul = v2_string(u, "A\0B", 3, loc(f, 5));
    v2_id prefix = v2_string(u, "A\0", 2, loc(f, 6));
    v2_id high = v2_string(u, "\377", 1, loc(f, 7));
    v2_id low = v2_string(u, "\177", 1, loc(f, 8));
    v2_id bad = v2_binary(u, V2_SUBTRACT, s, zero, loc(f, 9));
    v2_id a = v2_anchor(u, loc(f, 10)), address = v2_address(u, a, loc(f, 10));
    v2_id late_string = v2_binary(u, V2_ADD, s, address, loc(f, 11));
    v2_result r = v2_evaluate(u, sum);
    CHECK(r.state == V2_DEFERRED && r.dependency_count == 1);
    CHECK(v2_bind(u, d, s)); string_is(u, sum, "A0", 2);
    string_is(u, binary(u, V2_ADD, s, number(u, INT32_MIN)), "A-2147483648", 12);
    string_is(u, binary(u, V2_ADD, number(u, 12), s), "12A", 3);
    string_is(u, binary(u, V2_ADD, empty, nul), "A\0B", 3);
    string_is(u, binary(u, V2_ADD, empty, empty), "", 0);
    integer_is(u, binary(u, V2_GREATER, nul, prefix), 1);
    integer_is(u, binary(u, V2_GREATER, high, low), 1);
    integer_is(u, binary(u, V2_EQUAL, nul, nul), 1);
    integer_is(u, binary(u, V2_NOT_EQUAL, nul, s), 1);
    integer_is(u, binary(u, V2_LESS_EQUAL, prefix, nul), 1);
    integer_is(u, binary(u, V2_GREATER_EQUAL, empty, empty), 1);
    integer_is(u, binary(u, V2_LESS, v2_string(u, "B", 1, loc(f, 12)),
                                   v2_string(u, "a", 1, loc(f, 12))), 1);
    error_is(u, bad, V2_ERROR_TYPE, f, 9);
    error_is(u, v2_unary(u, V2_LOW, s, loc(f, 13)), V2_ERROR_TYPE, f, 13);
    CHECK(v2_evaluate(u, late_string).state == V2_DEFERRED);
    CHECK(v2_place(u, a, 1, 123)); error_is(u, late_string, V2_ERROR_LAYOUT_STRING, f, 11);
    { const unsigned char *stable = v2_evaluate(u, sum).value.as.string.bytes;
      size_t allocations = test_v2_allocation_calls();
      int i;
      for (i = 0; i < 100; ++i) {
          CHECK(v2_place(u, a, 1, i)); string_is(u, sum, "A0", 2);
          CHECK(v2_evaluate(u, sum).value.as.string.bytes == stable);
      }
      CHECK(test_v2_allocation_calls() == allocations);
      test_v2_version_rollover(u, 1);
      CHECK(v2_place(u, a, 1, 200)); string_is(u, sum, "A0", 2);
      CHECK(v2_evaluate(u, sum).value.as.string.bytes == stable); }
    v2_unit_destroy(u);
}

static void dependencies(void)
{
    v2_unit *u = v2_unit_create();
    v2_id d = v2_definition(u, "Import", v2_no_location());
    v2_id a = v2_anchor(u, v2_no_location());
    v2_id r1 = v2_reference(u, d, v2_no_location()), r2 = v2_reference(u, d, v2_no_location());
    v2_id a1 = v2_address(u, a, v2_no_location()), a2 = v2_address(u, a, v2_no_location());
    v2_id root = binary(u, V2_ADD, binary(u, V2_ADD, r1, r2), binary(u, V2_ADD, a1, a2));
    v2_result r = v2_evaluate(u, root);
    CHECK(r.state == V2_DEFERRED && r.dependency_count == 2);
    CHECK(has_dependency(r, V2_DEP_DEFINITION, d) && has_dependency(r, V2_DEP_ANCHOR, a));
    CHECK(v2_bind(u, d, number(u, 7)));
    r = v2_evaluate(u, root); CHECK(r.state == V2_DEFERRED && r.dependency_count == 1);
    CHECK(has_dependency(r, V2_DEP_ANCHOR, a));
    CHECK(v2_place(u, a, 1, 0)); integer_is(u, root, 14);
    CHECK(v2_place(u, a, 1, 0x8000)); integer_is(u, root, 65550);
    CHECK(v2_place(u, a, 0, 0)); CHECK(v2_evaluate(u, root).state == V2_DEFERRED);
    CHECK(v2_place(u, a, 1, -8)); integer_is(u, root, -2);
    v2_unit_destroy(u);
}

static void diagnostics(void)
{
    v2_unit *u = v2_unit_create();
    v2_id f = v2_file(u, "cycle.asm"), a = v2_definition(u, "A", loc(f, 1));
    v2_id b = v2_definition(u, "B", loc(f, 2));
    v2_id ra = v2_reference(u, a, loc(f, 3)), rb = v2_reference(u, b, loc(f, 4));
    v2_id harmless = number(u, 7), bad = v2_binary(u, V2_DIVIDE, harmless, number(u, 0), loc(f, 8));
    v2_result r;
    integer_is(u, harmless, 7); /* Unused invalid expressions are not evaluated. */
    CHECK(v2_bind(u, a, rb)); CHECK(v2_bind(u, b, ra));
    r = v2_evaluate(u, ra); CHECK(r.state == V2_INVALID && r.diagnostic.code == V2_ERROR_CYCLE);
    CHECK(r.diagnostic.location.file == f);
    CHECK(v2_evaluate(u, ra).diagnostic.location.line == r.diagnostic.location.line);
    CHECK(v2_evaluate(u, binary(u, V2_MULTIPLY, number(u, 0), ra)).state == V2_INVALID);
    integer_is(u, harmless, 7); error_is(u, bad, V2_ERROR_DIV_ZERO, f, 8);
    error_is(u, bad, V2_ERROR_DIV_ZERO, f, 8); /* Cached diagnostics are still returned. */
    v2_unit_destroy(u);

    u = v2_unit_create(); f = v2_file(u, "errors.asm");
    a = v2_definition(u, "Later", loc(f, 1)); ra = v2_reference(u, a, loc(f, 2));
    bad = v2_binary(u, V2_DIVIDE, number(u, 1), number(u, 0), loc(f, 3));
    b = binary(u, V2_ADD, ra, bad);
    error_is(u, b, V2_ERROR_DIV_ZERO, f, 3);
    CHECK(v2_bind(u, a, v2_binary(u, V2_SHIFT_LEFT, number(u, 1), number(u, 32), loc(f, 4))));
    error_is(u, b, V2_ERROR_SHIFT, f, 4); /* Binding invalidates the earlier error choice. */
    a = v2_anchor(u, loc(f, 5)); ra = v2_address(u, a, loc(f, 5));
    bad = v2_binary(u, V2_DIVIDE, number(u, 10), ra, loc(f, 6));
    CHECK(v2_place(u, a, 1, 0)); error_is(u, bad, V2_ERROR_DIV_ZERO, f, 6);
    CHECK(v2_place(u, a, 1, 2)); integer_is(u, bad, 5);
    CHECK(v2_place(u, a, 1, 0)); error_is(u, bad, V2_ERROR_DIV_ZERO, f, 6);
    v2_unit_destroy(u);
}

static void rollover(void)
{
    v2_unit *u = v2_unit_create();
    v2_id a = v2_anchor(u, v2_no_location());
    v2_id ref = v2_address(u, a, v2_no_location()), d, root;
    v2_result r = v2_evaluate(u, ref); /* Cached at layout epoch 1. */
    CHECK(r.state == V2_DEFERRED && r.dependency_count == 1);
    test_v2_version_rollover(u, 1); /* Only layout wraps; request must not. */
    CHECK(v2_place(u, a, 1, 5)); integer_is(u, ref, 5);
    v2_unit_destroy(u);

    u = v2_unit_create(); d = v2_definition(u, "D", v2_no_location());
    ref = v2_reference(u, d, v2_no_location());
    r = v2_evaluate(u, ref); /* Cached at binding epoch 1. */
    CHECK(r.state == V2_DEFERRED && r.dependency_count == 1);
    test_v2_version_rollover(u, 0);
    CHECK(v2_bind(u, d, number(u, 42))); integer_is(u, ref, 42);
    v2_unit_destroy(u);

    u = v2_unit_create(); d = v2_definition(u, "D", v2_no_location());
    a = v2_anchor(u, v2_no_location());
    root = binary(u, V2_ADD, v2_reference(u, d, v2_no_location()),
                  v2_address(u, a, v2_no_location()));
    /* The first request sets all node/definition/anchor visit marks to 1.
     * The second reuses 1 and must clear each kind of old visit mark. */
    { int i;
      for (i = 0; i < 2; ++i) {
          test_v2_version_rollover(u, 2);
          r = v2_evaluate(u, root);
          CHECK(r.state == V2_DEFERRED && r.dependency_count == 2);
          CHECK(has_dependency(r, V2_DEP_DEFINITION, d));
          CHECK(has_dependency(r, V2_DEP_ANCHOR, a));
      } }
    v2_unit_destroy(u);
}

static void placement_error_order(void)
{
    v2_unit *u = v2_unit_create();
    v2_id f = v2_file(u, "placement-errors.asm"), a = v2_anchor(u, loc(f, 1));
    v2_id left = v2_binary(u, V2_DIVIDE, number(u, 10),
                           v2_address(u, a, loc(f, 2)), loc(f, 2));
    v2_id right = v2_binary(u, V2_SHIFT_LEFT, number(u, 1), number(u, 40), loc(f, 3));
    v2_id root = binary(u, V2_ADD, left, right);
    error_is(u, root, V2_ERROR_SHIFT, f, 3);
    CHECK(v2_place(u, a, 1, 0)); error_is(u, root, V2_ERROR_DIV_ZERO, f, 2);
    CHECK(v2_place(u, a, 1, 2)); error_is(u, root, V2_ERROR_SHIFT, f, 3);
    CHECK(v2_place(u, a, 0, 0)); error_is(u, root, V2_ERROR_SHIFT, f, 3);
    v2_unit_destroy(u);
}

static void cycle_error_order(void)
{
    int placement, resolve_first;
    for (placement = 0; placement < 2; ++placement) {
        for (resolve_first = 0; resolve_first < 2; ++resolve_first) {
            v2_unit *u = v2_unit_create();
            v2_id f = v2_file(u, "cycle-errors.asm");
            v2_id d = v2_definition(u, "D", loc(f, 1));
            v2_id pending = placement ? v2_anchor(u, loc(f, 2)) :
                                        v2_definition(u, "U", loc(f, 2));
            v2_id r = v2_reference(u, d, loc(f, 3));
            v2_id invalid = v2_binary(u, V2_DIVIDE, number(u, 1), number(u, 0), loc(f, 8));
            v2_id left = placement ? v2_binary(u, V2_DIVIDE, number(u, 1),
                v2_address(u, pending, loc(f, 8)), loc(f, 8)) :
                v2_reference(u, pending, loc(f, 4));
            CHECK(v2_bind(u, d, binary(u, V2_ADD, left, r)));
            if (!resolve_first) error_is(u, r, V2_ERROR_CYCLE, f, 3);
            if (placement) CHECK(v2_place(u, pending, 1, 0));
            else CHECK(v2_bind(u, pending, invalid));
            /* Binding/placement before the first request and after caching the
             * cycle must select the same located left-hand error. */
            error_is(u, r, V2_ERROR_DIV_ZERO, f, 8);
            error_is(u, r, V2_ERROR_DIV_ZERO, f, 8);
            v2_unit_destroy(u);
        }
    }
}

static void string_identities(void)
{
    int forward, form;
    for (forward = 0; forward < 2; ++forward) {
        for (form = 0; form < 3; ++form) {
            v2_unit *u = v2_unit_create();
            v2_id f = v2_file(u, "identities.asm");
            v2_id d = v2_definition(u, "S", loc(f, 1));
            v2_id str = v2_string(u, "abc", 3, loc(f, 2));
            v2_id value = forward ? v2_reference(u, d, loc(f, 3)) : str;
            v2_id one = number(u, 1);
            v2_operator op = form == 1 ? V2_DIVIDE : V2_MULTIPLY;
            v2_id root = v2_binary(u, op, form == 2 ? one : value,
                                  form == 2 ? value : one, loc(f, 4));
            if (forward) {
                CHECK(v2_evaluate(u, root).state == V2_DEFERRED);
                CHECK(v2_bind(u, d, str));
            }
            error_is(u, root, V2_ERROR_TYPE, f, 4);
            integer_is(u, binary(u, op, form == 2 ? one : number(u, 42),
                                form == 2 ? number(u, 42) : one), 42);
            v2_unit_destroy(u);
        }
    }
}

static void deep_graphs(void)
{
    v2_unit *u = v2_unit_create();
    v2_id root = number(u, 1), i, d, leaf;
    for (i = 0; i < 100000; ++i) root = v2_unary(u, V2_NEGATE, root, v2_no_location());
    integer_is(u, root, 1);
    d = v2_definition(u, "Forward", v2_no_location());
    leaf = v2_reference(u, d, v2_no_location()); root = leaf;
    for (i = 0; i < 200; ++i) root = binary(u, V2_ADD, root, root);
    { v2_result r = v2_evaluate(u, root);
      CHECK(r.state == V2_DEFERRED && r.dependency_count == 1);
      CHECK(has_dependency(r, V2_DEP_DEFINITION, d)); }
    CHECK(v2_bind(u, d, number(u, 1))); integer_is(u, root, 0);
    v2_unit_destroy(u);
}

static void limits(void)
{
    v2_unit *u = v2_unit_create();
    v2_id root = v2_string(u, "x", 1, v2_no_location()), next;
    unsigned i;
    unsigned char *payload;
    for (i = 0; i < 24; ++i) root = binary(u, V2_ADD, root, root);
    { v2_result r = v2_evaluate(u, root);
      CHECK(r.state == V2_KNOWN && r.value.as.string.length == V2_MAX_STRING_BYTES);
      CHECK(r.value.as.string.bytes[0] == 'x' && r.value.as.string.bytes[V2_MAX_STRING_BYTES-1] == 'x'); }
    next = binary(u, V2_ADD, root, number(u, 0));
    error_is(u, next, V2_ERROR_STRING_LIMIT, V2_NONE, 0);
    for (i = 0; i < 100; ++i) {
        v2_id d = v2_definition(u, "Shared", v2_no_location());
        CHECK(v2_bind(u, d, root));
        CHECK(v2_evaluate(u, v2_reference(u, d, v2_no_location())).value.as.string.bytes ==
              v2_evaluate(u, root).value.as.string.bytes);
    }
    CHECK(v2_unit_error(u).code == V2_ERROR_NONE); /* Unused invalid value does not poison a unit. */
    v2_unit_destroy(u);

    payload = malloc(V2_MAX_STRING_BYTES); CHECK(payload); memset(payload, 'x', V2_MAX_STRING_BYTES);
    u = v2_unit_create();
    for (i = 0; i < V2_MAX_LIVE_STRING_BYTES / V2_MAX_STRING_BYTES; ++i)
        CHECK(v2_string(u, payload, V2_MAX_STRING_BYTES, v2_no_location()) != V2_NONE);
    CHECK(v2_string(u, "z", 1, v2_no_location()) == V2_NONE);
    CHECK(v2_unit_error(u).code == V2_ERROR_STRING_LIMIT);
    v2_unit_destroy(u); free(payload);
}

static void concat_budget(void)
{
    v2_unit *u = v2_unit_create();
    v2_id f = v2_file(u, "budget.asm"), empty = v2_string(u, NULL, 0, loc(f, 1));
    v2_id five = number(u, 5), root = v2_binary(u, V2_ADD, empty, five, loc(f, 7));
    unsigned char *payload = malloc(V2_MAX_STRING_BYTES);
    unsigned i;
    size_t calls;
    CHECK(payload); memset(payload, 'x', V2_MAX_STRING_BYTES);
    for (i = 0; i < V2_MAX_LIVE_STRING_BYTES / V2_MAX_STRING_BYTES; ++i)
        CHECK(v2_string(u, payload, V2_MAX_STRING_BYTES, loc(f, 2)) != V2_NONE);
    free(payload);
    integer_is(u, five, 5); /* Reserve the evaluator's stack before counting. */
    calls = test_v2_allocation_calls();
    error_is(u, root, V2_ERROR_STRING_LIMIT, f, 7);
    CHECK(test_v2_allocation_calls() == calls); /* No excess payload allocation. */
    CHECK(v2_unit_error(u).code == V2_ERROR_NONE);
    integer_is(u, five, 5);
    string_is(u, binary(u, V2_ADD, empty, empty), "", 0);
    error_is(u, root, V2_ERROR_STRING_LIMIT, f, 7);
    v2_unit_destroy(u);
}

static void builder_locations(void)
{
    int form, bad_location;
    for (form = 0; form < 4; ++form) {
        for (bad_location = 0; bad_location < 2; ++bad_location) {
            v2_unit *u = v2_unit_create();
            v2_id f = v2_file(u, "builder.asm"), result;
            v2_location l = loc(bad_location ? 77 : f, 2);
            switch (form) {
            case 0: result = v2_reference(u, V2_NONE, l); break;
            case 1: result = v2_address(u, V2_NONE, l); break;
            case 2: result = v2_unary(u, V2_NEGATE, V2_NONE, l); break;
            default: result = v2_binary(u, V2_ADD, V2_NONE, V2_NONE, l); break;
            }
            CHECK(result == V2_NONE);
            if (bad_location) error_is(u, result, V2_ERROR_LOCATION, V2_NONE, 0);
            else error_is(u, result, V2_ERROR_REFERENCE, f, 2);
            v2_unit_destroy(u);
        }
    }
}

static void invalid_builders(void)
{
    v2_unit *u = v2_unit_create(); v2_id f = v2_file(u, "input.asm");
    v2_location l = loc(f, 1); l.end_column = 0;
    CHECK(v2_integer(u, 1, l) == V2_NONE);
    error_is(u, 0, V2_ERROR_LOCATION, V2_NONE, 0); v2_unit_destroy(u);
    u = v2_unit_create(); f = v2_file(u, "input.asm");
    l = loc(f, 1); l.end_line = 2; l.end_column = 0;
    CHECK(v2_integer(u, 1, l) == V2_NONE);
    CHECK(v2_unit_error(u).code == V2_ERROR_LOCATION); v2_unit_destroy(u);
    u = v2_unit_create(); CHECK(v2_reference(u, 0, v2_no_location()) == V2_NONE);
    error_is(u, 0, V2_ERROR_REFERENCE, V2_NONE, 0); v2_unit_destroy(u);
    u = v2_unit_create(); CHECK(v2_string(u, NULL, 1, v2_no_location()) == V2_NONE);
    CHECK(v2_unit_error(u).code == V2_ERROR_REFERENCE); v2_unit_destroy(u);
    u = v2_unit_create(); CHECK(v2_unary(u, V2_ADD, number(u, 1), v2_no_location()) == V2_NONE);
    CHECK(v2_unit_error(u).code == V2_ERROR_REFERENCE); v2_unit_destroy(u);
    v2_unit_destroy(NULL);
}

static int allocation_scenario(void)
{
    v2_unit *u = v2_unit_create();
    v2_id definitions[40], anchors[40], root = V2_NONE, str;
    int i, okay = 0;
    v2_result result;
    if (u == NULL) return 0;
    /* Fail node-table growth after acquiring a literal's owned payload. */
    for (i = 0; i < 16; ++i) {
        if (number(u, i) == V2_NONE) goto done;
    }
    if (v2_string(u, "owned", 5, v2_no_location()) == V2_NONE) goto done;
    root = number(u, 0);
    for (i = 0; i < 40; ++i) {
        v2_id f = v2_file(u, "allocation.asm");
        definitions[i] = v2_definition(u, "same name", loc(f, (uint32_t)i+1));
        anchors[i] = v2_anchor(u, loc(f, (uint32_t)i+1));
        root = binary(u, V2_ADD, root, binary(u, V2_ADD,
                      v2_reference(u, definitions[i], v2_no_location()),
                      v2_address(u, anchors[i], v2_no_location())));
        if (v2_unit_error(u).code) goto done;
    }
    result = v2_evaluate(u, root);
    if (v2_unit_error(u).code) goto done;
    CHECK(result.state == V2_DEFERRED && result.dependency_count == 80);
    for (i = 0; i < 40; ++i) {
        if (!v2_place(u, anchors[i], 1, i)) goto done;
        if (!v2_bind(u, definitions[i], number(u, i))) goto done;
    }
    integer_is(u, root, 1560);
    str = v2_string(u, "A\0B", 3, v2_no_location());
    root = binary(u, V2_ADD, str, number(u, -1));
    result = v2_evaluate(u, root);
    if (v2_unit_error(u).code) goto done;
    CHECK(result.state == V2_KNOWN && result.value.as.string.length == 5);
    CHECK(!memcmp(result.value.as.string.bytes, "A\0B-1", 5));
    okay = 1;
done:
    if (!okay) {
        CHECK(v2_unit_error(u).code == V2_ERROR_MEMORY);
        error_is(u, root, V2_ERROR_MEMORY, v2_unit_error(u).location.file,
                 v2_unit_error(u).location.line);
    }
    v2_unit_destroy(u);
    return okay;
}

static void allocations(void)
{
    size_t i, count;
    test_v2_allocation_failure(0); CHECK(allocation_scenario());
    count = test_v2_allocation_calls(); CHECK(count > 100 && count < 1000);
    CHECK(test_v2_live_allocations() == 0);
    for (i = 1; i <= count; ++i) {
        test_v2_allocation_failure(i);
        CHECK(!allocation_scenario());
        CHECK(test_v2_live_allocations() == 0);
    }
    test_v2_allocation_failure(0);
    printf("checked %zu allocation failures\n", count);
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    if (!strcmp(argv[1], "arithmetic-stream")) arithmetic_stream();
    else if (!strcmp(argv[1], "graph-stream")) graph_stream();
    else if (!strcmp(argv[1], "ownership")) ownership();
    else if (!strcmp(argv[1], "anchor-inspection")) anchor_inspection();
    else if (!strcmp(argv[1], "bindings")) bindings();
    else if (!strcmp(argv[1], "strings")) strings();
    else if (!strcmp(argv[1], "dependencies")) dependencies();
    else if (!strcmp(argv[1], "diagnostics")) diagnostics();
    else if (!strcmp(argv[1], "rollover")) rollover();
    else if (!strcmp(argv[1], "placement-error-order")) placement_error_order();
    else if (!strcmp(argv[1], "cycle-error-order")) cycle_error_order();
    else if (!strcmp(argv[1], "string-identities")) string_identities();
    else if (!strcmp(argv[1], "deep-graphs")) deep_graphs();
    else if (!strcmp(argv[1], "limits")) limits();
    else if (!strcmp(argv[1], "concat-budget")) concat_budget();
    else if (!strcmp(argv[1], "invalid-builders")) invalid_builders();
    else if (!strcmp(argv[1], "builder-locations")) builder_locations();
    else if (!strcmp(argv[1], "allocations")) allocations();
    else CHECK(0);
    CHECK(test_v2_live_allocations() == 0);
    return 0;
}
