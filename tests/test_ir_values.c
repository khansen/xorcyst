#include "ir/value.h"
#include "ir/extent.h"
#include "ir/graph.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)
extern void test_ir_allocation_failure(size_t);
extern size_t test_ir_allocation_calls(void);
extern size_t test_ir_live_allocations(void);
extern void test_ir_version_rollover(ir_unit *, int);

static ir_location loc(ir_id file, uint32_t line)
{
    ir_location l = { file, line, 1, line, 9 };
    return l;
}
static ir_id number(ir_unit *u, int32_t x) { return ir_integer(u, x, ir_no_location()); }
static ir_id binary(ir_unit *u, ir_operator op, ir_id a, ir_id b)
{ return ir_binary(u, op, a, b, ir_no_location()); }
static void integer_is(ir_unit *u, ir_id id, int32_t expected)
{
    ir_result r = ir_evaluate(u, id);
    CHECK(r.state == IR_KNOWN && r.value.kind == IR_INTEGER);
    CHECK(r.value.as.integer == expected && r.dependency_count == 0);
}
static void string_is(ir_unit *u, ir_id id, const void *bytes, size_t length)
{
    ir_result r = ir_evaluate(u, id);
    CHECK(r.state == IR_KNOWN && r.value.kind == IR_STRING);
    CHECK(r.value.as.string.length == length);
    CHECK(!length || !memcmp(r.value.as.string.bytes, bytes, length));
}
static void error_is(ir_unit *u, ir_id id, ir_error error, ir_id file, uint32_t line)
{
    ir_result r = ir_evaluate(u, id);
    CHECK(r.state == IR_INVALID && r.diagnostic.code == error);
    CHECK(r.diagnostic.location.file == file && r.diagnostic.location.line == line);
    CHECK(r.dependency_count == 0);
}
static int has_dependency(ir_result r, ir_dependency_kind kind, ir_id id)
{
    size_t i;
    for (i = 0; i < r.dependency_count; ++i)
        if (r.dependencies[i].kind == kind && r.dependencies[i].id == id) return 1;
    return 0;
}

static void location_storage(void)
{
    ir_unit *u = ir_unit_create();
    ir_id file = ir_file(u, "source.asm"), last = file, saved[7];
    ir_location cases[] = {
        {0, 1, 1, 1, 9}, {0, 0xffffff, 255, 0xffffff + 255, 255},
        {0, 0x1000000, 1, 0x1000000, 1}, {0, 2, 256, 2, 300},
        {0, 1, 1, 257, 1}, {0, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX},
        {65535, 1, 1, 1, 9}
    };
    size_t i;
    CHECK(file == 0);
    for (i = 1; i <= 65535; i++) last = ir_file(u, "unit-local.asm");
    CHECK(last == 65535);
    for (i = 0; i < 7; i++) {
        ir_graph_node n;
        ir_result result;
        ir_id value = ir_binary(u, IR_DIVIDE, number(u, 1), number(u, 0), cases[i]);
        saved[i] = ir_graph_location_id(u, cases[i]);
        CHECK(ir_graph_read(u, value, &n));
        CHECK(n.location.file == cases[i].file && n.location.line == cases[i].line &&
              n.location.column == cases[i].column && n.location.end_line == cases[i].end_line &&
              n.location.end_column == cases[i].end_column);
        result = ir_evaluate(u, value);
        CHECK(result.state == IR_INVALID && result.diagnostic.code == IR_ERROR_DIV_ZERO);
        CHECK(result.diagnostic.location.file == cases[i].file &&
              result.diagnostic.location.end_column == cases[i].end_column);
    }
    ir_graph_release_location_index(u);
    for (i = 0; i < 10000; i++) {
        ir_location l = loc(file, (uint32_t)i + 300);
        CHECK(ir_graph_location_id(u, l) != IR_NONE);
    }
    ir_graph_release_location_index(u);
    for (i = 0; i < 7; i++) CHECK(saved[i] == ir_graph_location_id(u, cases[i]));
    ir_unit_destroy(u);
    CHECK(test_ir_live_allocations() == 0);
}

static void arithmetic_stream(void)
{
    int mode, op;
    int32_t a, b;
    while (scanf("%d %d %" SCNd32 " %" SCNd32, &mode, &op, &a, &b) == 4) {
        ir_unit *u = ir_unit_create();
        ir_id x, y, root, dx = IR_NONE, dy = IR_NONE;
        ir_result r;
        CHECK(u);
        if (mode == 0) { x = number(u, a); y = number(u, b); }
        else if (mode == 1) {
            dx = ir_definition(u, "captured", ir_no_location());
            dy = ir_definition(u, "captured", ir_no_location());
            x = ir_reference(u, dx, ir_no_location());
            y = ir_reference(u, dy, ir_no_location());
        } else {
            dx = ir_anchor(u, ir_no_location()); dy = ir_anchor(u, ir_no_location());
            x = ir_address(u, dx, ir_no_location()); y = ir_address(u, dy, ir_no_location());
        }
        root = op <= IR_HIGH ? ir_unary(u, (ir_operator)op, x, ir_no_location())
                            : binary(u, (ir_operator)op, x, y);
        if (mode) {
            r = ir_evaluate(u, root); CHECK(r.state == IR_DEFERRED);
            if (mode == 1) { CHECK(ir_bind(u, dx, number(u, a))); CHECK(ir_bind(u, dy, number(u, b))); }
            else { CHECK(ir_place(u, dx, 1, a)); CHECK(ir_place(u, dy, 1, b)); }
        }
        r = ir_evaluate(u, root);
        if (r.state == IR_KNOWN) { CHECK(r.value.kind == IR_INTEGER); printf("K %" PRId32 "\n", r.value.as.integer); }
        else { CHECK(r.state == IR_INVALID); printf("E %s\n", ir_error_message(r.diagnostic.code)); }
        ir_unit_destroy(u);
    }
    CHECK(feof(stdin));
}

/* A shared graph is evaluated repeatedly while placement facts change. The
 * Python caller keeps an independent graph and predicts each requested value. */
static void graph_stream(void)
{
    ir_unit *u = ir_unit_create();
    char command;
    ir_id anchors[8], next = 0;
    int32_t value;
    unsigned x, y, op, count = 0;
    CHECK(u);
    while (scanf(" %c", &command) == 1) {
        ir_id id;
        if (command == 'i') {
            CHECK(scanf("%" SCNd32, &value) == 1); id = number(u, value);
        } else if (command == 'a') {
            CHECK(count < 8); anchors[count] = ir_anchor(u, ir_no_location());
            id = ir_address(u, anchors[count++], ir_no_location());
        } else if (command == 'b') {
            CHECK(scanf("%u %u %u", &op, &x, &y) == 3);
            id = binary(u, (ir_operator)op, x, y);
        } else if (command == 'p') {
            CHECK(scanf("%u %u %" SCNd32, &x, &y, &value) == 3 && x < count);
            CHECK(ir_place(u, anchors[x], (int)y, value)); continue;
        } else {
            ir_result r;
            CHECK(command == 'e' && scanf("%u", &x) == 1);
            r = ir_evaluate(u, x);
            if (r.state == IR_KNOWN) printf("K %" PRId32 "\n", r.value.as.integer);
            else if (r.state == IR_INVALID) printf("E %s\n", ir_error_message(r.diagnostic.code));
            else printf("D\n");
            continue;
        }
        CHECK(id == next++);
    }
    CHECK(feof(stdin)); ir_unit_destroy(u);
}

static void ownership(void)
{
    ir_unit *u = ir_unit_create(), *other = ir_unit_create();
    char file[] = "caller.asm", name[] = "Same";
    unsigned char bytes[] = { 'A', 0, 'B', 255 };
    ir_id f = ir_file(u, file), d = ir_definition(u, name, loc(f, 12));
    ir_id str = ir_string(u, bytes, sizeof(bytes), loc(f, 13));
    ir_id alias = ir_reference(u, d, loc(f, 14));
    ir_id other_d = ir_definition(other, name, ir_no_location());
    ir_id other_r = ir_reference(other, other_d, ir_no_location());
    size_t i;
    CHECK(f == 0 && d == 0 && other_d == 0);
    CHECK(ir_bind(u, d, str)); CHECK(ir_bind(other, other_d, number(other, 77)));
    memset(file, 'x', sizeof(file)); memset(name, 'x', sizeof(name)); memset(bytes, 0, sizeof(bytes));
    for (i = 0; i < 1000; ++i) {
        CHECK(ir_file(u, "caller.asm") != IR_NONE);
        CHECK(ir_definition(u, "Same", ir_no_location()) != IR_NONE);
        CHECK(number(u, (int32_t)i) != IR_NONE);
    }
    CHECK(!strcmp(ir_file_name(u, f), "caller.asm"));
    CHECK(!strcmp(ir_definition_name(u, d), "Same"));
    { const unsigned char expected[] = { 'A', 0, 'B', 255 }; string_is(u, alias, expected, sizeof(expected)); }
    ir_unit_destroy(u);
    integer_is(other, other_r, 77);
    ir_unit_destroy(other);
}

static void anchor_inspection(void)
{
    ir_unit *u = ir_unit_create();
    ir_location at = ir_no_location();
    ir_id a = ir_anchor(u, at), expr = ir_address(u, a, at);
    int known = -1;
    int32_t value = -1;
    CHECK(ir_anchor_value(u, a, &known, &value, at) && !known);
    CHECK(ir_extent_anchor(u, a, number(u, 99)));
    CHECK(ir_evaluate_extent(u, expr).value.as.integer == 99);
    CHECK(ir_anchor_value(u, a, &known, NULL, at) && !known);
    CHECK(ir_place(u, a, 1, 42));
    CHECK(ir_anchor_value(u, a, &known, &value, at) && known && value == 42);
    CHECK(ir_anchor_value(u, a, NULL, NULL, at));
    CHECK(ir_place(u, a, 0, 0));
    CHECK(ir_anchor_value(u, a, &known, NULL, at) && !known);
    CHECK(!ir_anchor_value(u, IR_NONE, NULL, NULL, at));
    CHECK(ir_unit_error(u).code == IR_ERROR_REFERENCE);
    ir_unit_destroy(u);
    u = ir_unit_create();
    a = ir_anchor(u, at);
    at.file = 77;
    CHECK(!ir_anchor_value(u, a, NULL, NULL, at));
    CHECK(ir_unit_error(u).code == IR_ERROR_LOCATION);
    CHECK(ir_unit_error(u).location.file == IR_NONE);
    ir_unit_destroy(u);
}

static void bindings(void)
{
    ir_unit *u = ir_unit_create();
    ir_id pending = ir_definition(u, "I", ir_no_location());
    ir_id forward = ir_reference(u, pending, ir_no_location());
    ir_id first = ir_definition(u, "I", ir_no_location());
    ir_id captured = ir_reference(u, first, ir_no_location());
    ir_id later = ir_definition(u, "I", ir_no_location());
    ir_id current = ir_reference(u, later, ir_no_location());
    CHECK(ir_bind(u, first, number(u, 1))); integer_is(u, captured, 1);
    CHECK(ir_evaluate(u, forward).state == IR_DEFERRED);
    CHECK(ir_bind(u, later, number(u, 2))); CHECK(ir_bind(u, pending, current));
    integer_is(u, forward, 2); integer_is(u, captured, 1); integer_is(u, current, 2);
    /* The frontend supplies the selected replacement after .undef; no lookup by
     * spelling inside this graph is allowed to retarget the earlier reference. */
    { ir_id replacement = ir_definition(u, "I", ir_no_location());
      CHECK(ir_bind(u, replacement, number(u, 3))); integer_is(u, captured, 1); }
    CHECK(!ir_bind(u, first, number(u, 4)));
    CHECK(ir_unit_error(u).code == IR_ERROR_REBIND);
    error_is(u, captured, IR_ERROR_REBIND, IR_NONE, 0);
    ir_unit_destroy(u);
}

static void strings(void)
{
    ir_unit *u = ir_unit_create();
    ir_id f = ir_file(u, "strings.asm");
    ir_id s = ir_string(u, "A", 1, loc(f, 1));
    ir_id d = ir_definition(u, "S", loc(f, 2)), ref = ir_reference(u, d, loc(f, 3));
    ir_id zero = number(u, 0), sum = binary(u, IR_ADD, ref, zero);
    ir_id empty = ir_string(u, NULL, 0, loc(f, 4));
    ir_id nul = ir_string(u, "A\0B", 3, loc(f, 5));
    ir_id prefix = ir_string(u, "A\0", 2, loc(f, 6));
    ir_id high = ir_string(u, "\377", 1, loc(f, 7));
    ir_id low = ir_string(u, "\177", 1, loc(f, 8));
    ir_id bad = ir_binary(u, IR_SUBTRACT, s, zero, loc(f, 9));
    ir_id a = ir_anchor(u, loc(f, 10)), address = ir_address(u, a, loc(f, 10));
    ir_id late_string = ir_binary(u, IR_ADD, s, address, loc(f, 11));
    ir_result r = ir_evaluate(u, sum);
    CHECK(r.state == IR_DEFERRED && r.dependency_count == 1);
    CHECK(ir_bind(u, d, s)); string_is(u, sum, "A0", 2);
    string_is(u, binary(u, IR_ADD, s, number(u, INT32_MIN)), "A-2147483648", 12);
    string_is(u, binary(u, IR_ADD, number(u, 12), s), "12A", 3);
    string_is(u, binary(u, IR_ADD, empty, nul), "A\0B", 3);
    string_is(u, binary(u, IR_ADD, empty, empty), "", 0);
    integer_is(u, binary(u, IR_GREATER, nul, prefix), 1);
    integer_is(u, binary(u, IR_GREATER, high, low), 1);
    integer_is(u, binary(u, IR_EQUAL, nul, nul), 1);
    integer_is(u, binary(u, IR_NOT_EQUAL, nul, s), 1);
    integer_is(u, binary(u, IR_LESS_EQUAL, prefix, nul), 1);
    integer_is(u, binary(u, IR_GREATER_EQUAL, empty, empty), 1);
    integer_is(u, binary(u, IR_LESS, ir_string(u, "B", 1, loc(f, 12)),
                                   ir_string(u, "a", 1, loc(f, 12))), 1);
    error_is(u, bad, IR_ERROR_TYPE, f, 9);
    error_is(u, ir_unary(u, IR_LOW, s, loc(f, 13)), IR_ERROR_TYPE, f, 13);
    CHECK(ir_evaluate(u, late_string).state == IR_DEFERRED);
    CHECK(ir_place(u, a, 1, 123)); error_is(u, late_string, IR_ERROR_LAYOUT_STRING, f, 11);
    { const unsigned char *stable = ir_evaluate(u, sum).value.as.string.bytes;
      size_t allocations = test_ir_allocation_calls();
      int i;
      for (i = 0; i < 100; ++i) {
          CHECK(ir_place(u, a, 1, i)); string_is(u, sum, "A0", 2);
          CHECK(ir_evaluate(u, sum).value.as.string.bytes == stable);
      }
      CHECK(test_ir_allocation_calls() == allocations);
      test_ir_version_rollover(u, 1);
      CHECK(ir_place(u, a, 1, 200)); string_is(u, sum, "A0", 2);
      CHECK(ir_evaluate(u, sum).value.as.string.bytes == stable); }
    ir_unit_destroy(u);
}

static void dependencies(void)
{
    ir_unit *u = ir_unit_create();
    ir_id d = ir_definition(u, "Import", ir_no_location());
    ir_id a = ir_anchor(u, ir_no_location());
    ir_id r1 = ir_reference(u, d, ir_no_location()), r2 = ir_reference(u, d, ir_no_location());
    ir_id a1 = ir_address(u, a, ir_no_location()), a2 = ir_address(u, a, ir_no_location());
    ir_id root = binary(u, IR_ADD, binary(u, IR_ADD, r1, r2), binary(u, IR_ADD, a1, a2));
    ir_result r = ir_evaluate(u, root);
    CHECK(r.state == IR_DEFERRED && r.dependency_count == 2);
    CHECK(has_dependency(r, IR_DEP_DEFINITION, d) && has_dependency(r, IR_DEP_ANCHOR, a));
    CHECK(ir_bind(u, d, number(u, 7)));
    r = ir_evaluate(u, root); CHECK(r.state == IR_DEFERRED && r.dependency_count == 1);
    CHECK(has_dependency(r, IR_DEP_ANCHOR, a));
    CHECK(ir_place(u, a, 1, 0)); integer_is(u, root, 14);
    CHECK(ir_place(u, a, 1, 0x8000)); integer_is(u, root, 65550);
    CHECK(ir_place(u, a, 0, 0)); CHECK(ir_evaluate(u, root).state == IR_DEFERRED);
    CHECK(ir_place(u, a, 1, -8)); integer_is(u, root, -2);
    ir_unit_destroy(u);
}

static void diagnostics(void)
{
    ir_unit *u = ir_unit_create();
    ir_id f = ir_file(u, "cycle.asm"), a = ir_definition(u, "A", loc(f, 1));
    ir_id b = ir_definition(u, "B", loc(f, 2));
    ir_id ra = ir_reference(u, a, loc(f, 3)), rb = ir_reference(u, b, loc(f, 4));
    ir_id harmless = number(u, 7), bad = ir_binary(u, IR_DIVIDE, harmless, number(u, 0), loc(f, 8));
    ir_result r;
    integer_is(u, harmless, 7); /* Unused invalid expressions are not evaluated. */
    CHECK(ir_bind(u, a, rb)); CHECK(ir_bind(u, b, ra));
    r = ir_evaluate(u, ra); CHECK(r.state == IR_INVALID && r.diagnostic.code == IR_ERROR_CYCLE);
    CHECK(r.diagnostic.location.file == f);
    CHECK(ir_evaluate(u, ra).diagnostic.location.line == r.diagnostic.location.line);
    CHECK(ir_evaluate(u, binary(u, IR_MULTIPLY, number(u, 0), ra)).state == IR_INVALID);
    integer_is(u, harmless, 7); error_is(u, bad, IR_ERROR_DIV_ZERO, f, 8);
    error_is(u, bad, IR_ERROR_DIV_ZERO, f, 8); /* Cached diagnostics are still returned. */
    ir_unit_destroy(u);

    u = ir_unit_create(); f = ir_file(u, "errors.asm");
    a = ir_definition(u, "Later", loc(f, 1)); ra = ir_reference(u, a, loc(f, 2));
    bad = ir_binary(u, IR_DIVIDE, number(u, 1), number(u, 0), loc(f, 3));
    b = binary(u, IR_ADD, ra, bad);
    error_is(u, b, IR_ERROR_DIV_ZERO, f, 3);
    CHECK(ir_bind(u, a, ir_binary(u, IR_SHIFT_LEFT, number(u, 1), number(u, 32), loc(f, 4))));
    error_is(u, b, IR_ERROR_SHIFT, f, 4); /* Binding invalidates the earlier error choice. */
    a = ir_anchor(u, loc(f, 5)); ra = ir_address(u, a, loc(f, 5));
    bad = ir_binary(u, IR_DIVIDE, number(u, 10), ra, loc(f, 6));
    CHECK(ir_place(u, a, 1, 0)); error_is(u, bad, IR_ERROR_DIV_ZERO, f, 6);
    CHECK(ir_place(u, a, 1, 2)); integer_is(u, bad, 5);
    CHECK(ir_place(u, a, 1, 0)); error_is(u, bad, IR_ERROR_DIV_ZERO, f, 6);
    ir_unit_destroy(u);
}

static void rollover(void)
{
    ir_unit *u = ir_unit_create();
    ir_id a = ir_anchor(u, ir_no_location());
    ir_id ref = ir_address(u, a, ir_no_location()), d, root;
    ir_result r = ir_evaluate(u, ref); /* Cached at layout epoch 1. */
    CHECK(r.state == IR_DEFERRED && r.dependency_count == 1);
    test_ir_version_rollover(u, 1); /* Only layout wraps; request must not. */
    CHECK(ir_place(u, a, 1, 5)); integer_is(u, ref, 5);
    ir_unit_destroy(u);

    u = ir_unit_create(); d = ir_definition(u, "D", ir_no_location());
    ref = ir_reference(u, d, ir_no_location());
    r = ir_evaluate(u, ref); /* Cached at binding epoch 1. */
    CHECK(r.state == IR_DEFERRED && r.dependency_count == 1);
    test_ir_version_rollover(u, 0);
    CHECK(ir_bind(u, d, number(u, 42))); integer_is(u, ref, 42);
    ir_unit_destroy(u);

    u = ir_unit_create(); d = ir_definition(u, "D", ir_no_location());
    a = ir_anchor(u, ir_no_location());
    root = binary(u, IR_ADD, ir_reference(u, d, ir_no_location()),
                  ir_address(u, a, ir_no_location()));
    /* The first request sets all node/definition/anchor visit marks to 1.
     * The second reuses 1 and must clear each kind of old visit mark. */
    { int i;
      for (i = 0; i < 2; ++i) {
          test_ir_version_rollover(u, 2);
          r = ir_evaluate(u, root);
          CHECK(r.state == IR_DEFERRED && r.dependency_count == 2);
          CHECK(has_dependency(r, IR_DEP_DEFINITION, d));
          CHECK(has_dependency(r, IR_DEP_ANCHOR, a));
      } }
    ir_unit_destroy(u);
}

static void placement_error_order(void)
{
    ir_unit *u = ir_unit_create();
    ir_id f = ir_file(u, "placement-errors.asm"), a = ir_anchor(u, loc(f, 1));
    ir_id left = ir_binary(u, IR_DIVIDE, number(u, 10),
                           ir_address(u, a, loc(f, 2)), loc(f, 2));
    ir_id right = ir_binary(u, IR_SHIFT_LEFT, number(u, 1), number(u, 40), loc(f, 3));
    ir_id root = binary(u, IR_ADD, left, right);
    error_is(u, root, IR_ERROR_SHIFT, f, 3);
    CHECK(ir_place(u, a, 1, 0)); error_is(u, root, IR_ERROR_DIV_ZERO, f, 2);
    CHECK(ir_place(u, a, 1, 2)); error_is(u, root, IR_ERROR_SHIFT, f, 3);
    CHECK(ir_place(u, a, 0, 0)); error_is(u, root, IR_ERROR_SHIFT, f, 3);
    ir_unit_destroy(u);
}

static void cycle_error_order(void)
{
    int placement, resolve_first;
    for (placement = 0; placement < 2; ++placement) {
        for (resolve_first = 0; resolve_first < 2; ++resolve_first) {
            ir_unit *u = ir_unit_create();
            ir_id f = ir_file(u, "cycle-errors.asm");
            ir_id d = ir_definition(u, "D", loc(f, 1));
            ir_id pending = placement ? ir_anchor(u, loc(f, 2)) :
                                        ir_definition(u, "U", loc(f, 2));
            ir_id r = ir_reference(u, d, loc(f, 3));
            ir_id invalid = ir_binary(u, IR_DIVIDE, number(u, 1), number(u, 0), loc(f, 8));
            ir_id left = placement ? ir_binary(u, IR_DIVIDE, number(u, 1),
                ir_address(u, pending, loc(f, 8)), loc(f, 8)) :
                ir_reference(u, pending, loc(f, 4));
            CHECK(ir_bind(u, d, binary(u, IR_ADD, left, r)));
            if (!resolve_first) error_is(u, r, IR_ERROR_CYCLE, f, 3);
            if (placement) CHECK(ir_place(u, pending, 1, 0));
            else CHECK(ir_bind(u, pending, invalid));
            /* Binding/placement before the first request and after caching the
             * cycle must select the same located left-hand error. */
            error_is(u, r, IR_ERROR_DIV_ZERO, f, 8);
            error_is(u, r, IR_ERROR_DIV_ZERO, f, 8);
            ir_unit_destroy(u);
        }
    }
}

static void string_identities(void)
{
    int forward, form;
    for (forward = 0; forward < 2; ++forward) {
        for (form = 0; form < 3; ++form) {
            ir_unit *u = ir_unit_create();
            ir_id f = ir_file(u, "identities.asm");
            ir_id d = ir_definition(u, "S", loc(f, 1));
            ir_id str = ir_string(u, "abc", 3, loc(f, 2));
            ir_id value = forward ? ir_reference(u, d, loc(f, 3)) : str;
            ir_id one = number(u, 1);
            ir_operator op = form == 1 ? IR_DIVIDE : IR_MULTIPLY;
            ir_id root = ir_binary(u, op, form == 2 ? one : value,
                                  form == 2 ? value : one, loc(f, 4));
            if (forward) {
                CHECK(ir_evaluate(u, root).state == IR_DEFERRED);
                CHECK(ir_bind(u, d, str));
            }
            error_is(u, root, IR_ERROR_TYPE, f, 4);
            integer_is(u, binary(u, op, form == 2 ? one : number(u, 42),
                                form == 2 ? number(u, 42) : one), 42);
            ir_unit_destroy(u);
        }
    }
}

static void deep_graphs(void)
{
    ir_unit *u = ir_unit_create();
    ir_id root = number(u, 1), i, d, leaf;
    for (i = 0; i < 100000; ++i) root = ir_unary(u, IR_NEGATE, root, ir_no_location());
    integer_is(u, root, 1);
    d = ir_definition(u, "Forward", ir_no_location());
    leaf = ir_reference(u, d, ir_no_location()); root = leaf;
    for (i = 0; i < 200; ++i) root = binary(u, IR_ADD, root, root);
    { ir_result r = ir_evaluate(u, root);
      CHECK(r.state == IR_DEFERRED && r.dependency_count == 1);
      CHECK(has_dependency(r, IR_DEP_DEFINITION, d)); }
    CHECK(ir_bind(u, d, number(u, 1))); integer_is(u, root, 0);
    ir_unit_destroy(u);
}

static void limits(void)
{
    ir_unit *u = ir_unit_create();
    ir_id root = ir_string(u, "x", 1, ir_no_location()), next;
    unsigned i;
    unsigned char *payload;
    for (i = 0; i < 24; ++i) root = binary(u, IR_ADD, root, root);
    { ir_result r = ir_evaluate(u, root);
      CHECK(r.state == IR_KNOWN && r.value.as.string.length == IR_MAX_STRING_BYTES);
      CHECK(r.value.as.string.bytes[0] == 'x' && r.value.as.string.bytes[IR_MAX_STRING_BYTES-1] == 'x'); }
    next = binary(u, IR_ADD, root, number(u, 0));
    error_is(u, next, IR_ERROR_STRING_LIMIT, IR_NONE, 0);
    for (i = 0; i < 100; ++i) {
        ir_id d = ir_definition(u, "Shared", ir_no_location());
        CHECK(ir_bind(u, d, root));
        CHECK(ir_evaluate(u, ir_reference(u, d, ir_no_location())).value.as.string.bytes ==
              ir_evaluate(u, root).value.as.string.bytes);
    }
    CHECK(ir_unit_error(u).code == IR_ERROR_NONE); /* Unused invalid value does not poison a unit. */
    ir_unit_destroy(u);

    payload = malloc(IR_MAX_STRING_BYTES); CHECK(payload); memset(payload, 'x', IR_MAX_STRING_BYTES);
    u = ir_unit_create();
    for (i = 0; i < IR_MAX_LIVE_STRING_BYTES / IR_MAX_STRING_BYTES; ++i)
        CHECK(ir_string(u, payload, IR_MAX_STRING_BYTES, ir_no_location()) != IR_NONE);
    CHECK(ir_string(u, "z", 1, ir_no_location()) == IR_NONE);
    CHECK(ir_unit_error(u).code == IR_ERROR_STRING_LIMIT);
    ir_unit_destroy(u); free(payload);
}

static void concat_budget(void)
{
    ir_unit *u = ir_unit_create();
    ir_id f = ir_file(u, "budget.asm"), empty = ir_string(u, NULL, 0, loc(f, 1));
    ir_id five = number(u, 5), root = ir_binary(u, IR_ADD, empty, five, loc(f, 7));
    unsigned char *payload = malloc(IR_MAX_STRING_BYTES);
    unsigned i;
    size_t calls;
    CHECK(payload); memset(payload, 'x', IR_MAX_STRING_BYTES);
    for (i = 0; i < IR_MAX_LIVE_STRING_BYTES / IR_MAX_STRING_BYTES; ++i)
        CHECK(ir_string(u, payload, IR_MAX_STRING_BYTES, loc(f, 2)) != IR_NONE);
    free(payload);
    integer_is(u, five, 5); /* Reserve the evaluator's stack before counting. */
    calls = test_ir_allocation_calls();
    error_is(u, root, IR_ERROR_STRING_LIMIT, f, 7);
    CHECK(test_ir_allocation_calls() == calls); /* No excess payload allocation. */
    CHECK(ir_unit_error(u).code == IR_ERROR_NONE);
    integer_is(u, five, 5);
    string_is(u, binary(u, IR_ADD, empty, empty), "", 0);
    error_is(u, root, IR_ERROR_STRING_LIMIT, f, 7);
    ir_unit_destroy(u);
}

static void builder_locations(void)
{
    int form, bad_location;
    for (form = 0; form < 4; ++form) {
        for (bad_location = 0; bad_location < 2; ++bad_location) {
            ir_unit *u = ir_unit_create();
            ir_id f = ir_file(u, "builder.asm"), result;
            ir_location l = loc(bad_location ? 77 : f, 2);
            switch (form) {
            case 0: result = ir_reference(u, IR_NONE, l); break;
            case 1: result = ir_address(u, IR_NONE, l); break;
            case 2: result = ir_unary(u, IR_NEGATE, IR_NONE, l); break;
            default: result = ir_binary(u, IR_ADD, IR_NONE, IR_NONE, l); break;
            }
            CHECK(result == IR_NONE);
            if (bad_location) error_is(u, result, IR_ERROR_LOCATION, IR_NONE, 0);
            else error_is(u, result, IR_ERROR_REFERENCE, f, 2);
            ir_unit_destroy(u);
        }
    }
}

static void invalid_builders(void)
{
    ir_unit *u = ir_unit_create(); ir_id f = ir_file(u, "input.asm");
    ir_location l = loc(f, 1); l.end_column = 0;
    CHECK(ir_integer(u, 1, l) == IR_NONE);
    error_is(u, 0, IR_ERROR_LOCATION, IR_NONE, 0); ir_unit_destroy(u);
    u = ir_unit_create(); f = ir_file(u, "input.asm");
    l = loc(f, 1); l.end_line = 2; l.end_column = 0;
    CHECK(ir_integer(u, 1, l) == IR_NONE);
    CHECK(ir_unit_error(u).code == IR_ERROR_LOCATION); ir_unit_destroy(u);
    u = ir_unit_create(); CHECK(ir_reference(u, 0, ir_no_location()) == IR_NONE);
    error_is(u, 0, IR_ERROR_REFERENCE, IR_NONE, 0); ir_unit_destroy(u);
    u = ir_unit_create(); CHECK(ir_string(u, NULL, 1, ir_no_location()) == IR_NONE);
    CHECK(ir_unit_error(u).code == IR_ERROR_REFERENCE); ir_unit_destroy(u);
    u = ir_unit_create(); CHECK(ir_unary(u, IR_ADD, number(u, 1), ir_no_location()) == IR_NONE);
    CHECK(ir_unit_error(u).code == IR_ERROR_REFERENCE); ir_unit_destroy(u);
    ir_unit_destroy(NULL);
}

static int allocation_scenario(void)
{
    ir_unit *u = ir_unit_create();
    ir_id definitions[40], anchors[40], root = IR_NONE, str;
    int i, okay = 0;
    ir_result result;
    if (u == NULL) return 0;
    /* Fail node-table growth after acquiring a literal's owned payload. */
    for (i = 0; i < 16; ++i) {
        if (number(u, i) == IR_NONE) goto done;
    }
    if (ir_string(u, "owned", 5, ir_no_location()) == IR_NONE) goto done;
    root = number(u, 0);
    for (i = 0; i < 40; ++i) {
        ir_id f = ir_file(u, "allocation.asm");
        definitions[i] = ir_definition(u, "same name", loc(f, (uint32_t)i+1));
        anchors[i] = ir_anchor(u, loc(f, (uint32_t)i+1));
        root = binary(u, IR_ADD, root, binary(u, IR_ADD,
                      ir_reference(u, definitions[i], ir_no_location()),
                      ir_address(u, anchors[i], ir_no_location())));
        if (ir_unit_error(u).code) goto done;
    }
    result = ir_evaluate(u, root);
    if (ir_unit_error(u).code) goto done;
    CHECK(result.state == IR_DEFERRED && result.dependency_count == 80);
    for (i = 0; i < 40; ++i) {
        if (!ir_place(u, anchors[i], 1, i)) goto done;
        if (!ir_bind(u, definitions[i], number(u, i))) goto done;
    }
    integer_is(u, root, 1560);
    str = ir_string(u, "A\0B", 3, ir_no_location());
    root = binary(u, IR_ADD, str, number(u, -1));
    result = ir_evaluate(u, root);
    if (ir_unit_error(u).code) goto done;
    CHECK(result.state == IR_KNOWN && result.value.as.string.length == 5);
    CHECK(!memcmp(result.value.as.string.bytes, "A\0B-1", 5));
    okay = 1;
done:
    if (!okay) {
        CHECK(ir_unit_error(u).code == IR_ERROR_MEMORY);
        error_is(u, root, IR_ERROR_MEMORY, ir_unit_error(u).location.file,
                 ir_unit_error(u).location.line);
    }
    ir_unit_destroy(u);
    return okay;
}

static void allocations(void)
{
    size_t i, count;
    test_ir_allocation_failure(0); CHECK(allocation_scenario());
    count = test_ir_allocation_calls(); CHECK(count > 100 && count < 1000);
    CHECK(test_ir_live_allocations() == 0);
    for (i = 1; i <= count; ++i) {
        test_ir_allocation_failure(i);
        CHECK(!allocation_scenario());
        CHECK(test_ir_live_allocations() == 0);
    }
    test_ir_allocation_failure(0);
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
    else if (!strcmp(argv[1], "locations")) location_storage();
    else if (!strcmp(argv[1], "limits")) limits();
    else if (!strcmp(argv[1], "concat-budget")) concat_budget();
    else if (!strcmp(argv[1], "invalid-builders")) invalid_builders();
    else if (!strcmp(argv[1], "builder-locations")) builder_locations();
    else if (!strcmp(argv[1], "allocations")) allocations();
    else CHECK(0);
    CHECK(test_ir_live_allocations() == 0);
    return 0;
}
