/* See value.h for ownership and evaluation contracts. */
#include "value.h"
#include "../expr_int.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { INTEGER, STRING, REFERENCE, ADDRESS, UNARY, BINARY } node_kind;

typedef struct {
    v2_state state;
    v2_value value;
    v2_diagnostic diagnostic;
    unsigned binding_dependent, layout_dependent;
} evaluated;

typedef struct {
    node_kind kind;
    v2_operator op;
    v2_id left, right;
    v2_location location;
    int32_t integer;
    unsigned char *literal;
    size_t length;
    /* Only concatenations own cache bytes. Aliases share their source value. */
    unsigned char *concatenation;
    evaluated cache;
    unsigned cached;
    uint64_t bindings, layout, active, visited;
} node;

typedef struct {
    char *name;
    v2_location location;
    v2_id expression;
    uint64_t visited;
} definition;

typedef struct {
    v2_location location;
    int known;
    int32_t value;
    uint64_t visited;
} anchor;

typedef struct { v2_id id; unsigned phase; } frame;

struct v2_unit {
    char **files;
    definition *definitions;
    anchor *anchors;
    node *nodes;
    size_t file_count, file_capacity, definition_count, definition_capacity;
    size_t anchor_count, anchor_capacity, node_count, node_capacity;
    frame *stack;
    size_t stack_count, stack_capacity;
    v2_dependency *dependencies;
    size_t dependency_count, dependency_capacity;
    size_t string_bytes;
    uint64_t bindings, layout, request;
    v2_diagnostic error;
};

v2_location v2_no_location(void)
{
    v2_location loc = { V2_NONE, 0, 0, 0, 0 };
    return loc;
}

static v2_diagnostic diagnostic(v2_error code, v2_location loc)
{
    v2_diagnostic result = { code, loc };
    return result;
}

static void fail(v2_unit *u, v2_error code, v2_location loc)
{
    if (u->error.code == V2_ERROR_NONE) u->error = diagnostic(code, loc);
}

/* Tables use checked geometrical growth. An allocation failure leaves the old
 * table owned by the unit and does not publish an entry or an ID. */
static void *grow(v2_unit *u, void *table, size_t *capacity, size_t needed,
                  size_t width, v2_location loc)
{
    size_t next;
    void *p;
    if (u->error.code != V2_ERROR_NONE || needed <= *capacity) return table;
    if (needed > UINT32_MAX || needed > SIZE_MAX / width) {
        fail(u, V2_ERROR_MEMORY, loc);
        return table;
    }
    next = *capacity ? *capacity : 16;
    while (next < needed) {
        if (next > UINT32_MAX / 2 || next > SIZE_MAX / width / 2) {
            next = needed;
            break;
        }
        next *= 2;
    }
    p = realloc(table, next * width);
    if (p == NULL) { fail(u, V2_ERROR_MEMORY, loc); return table; }
    *capacity = next;
    return p;
}

static int valid_location(v2_unit *u, v2_location loc)
{
    if (u->error.code != V2_ERROR_NONE) return 0;
    if (loc.file == V2_NONE) return 1;
    if (loc.file < u->file_count && loc.line && loc.column && loc.end_column &&
        (loc.end_line > loc.line ||
         (loc.end_line == loc.line && loc.end_column >= loc.column))) return 1;
    fail(u, V2_ERROR_LOCATION, v2_no_location());
    return 0;
}

static char *copy_name(v2_unit *u, const char *name, v2_location loc)
{
    size_t length;
    char *copy;
    if (name == NULL || !*name) { fail(u, V2_ERROR_REFERENCE, loc); return NULL; }
    length = strlen(name);
    if (length == SIZE_MAX) { fail(u, V2_ERROR_MEMORY, loc); return NULL; }
    copy = malloc(length + 1);
    if (copy == NULL) { fail(u, V2_ERROR_MEMORY, loc); return NULL; }
    memcpy(copy, name, length + 1);
    return copy;
}

v2_unit *v2_unit_create(void)
{
    v2_unit *u = calloc(1, sizeof(*u));
    if (u != NULL) {
        u->bindings = u->layout = u->request = 1;
        u->error = diagnostic(V2_ERROR_NONE, v2_no_location());
    }
    return u;
}

void v2_unit_destroy(v2_unit *u)
{
    size_t i;
    if (u == NULL) return;
    for (i = 0; i < u->file_count; ++i) free(u->files[i]);
    for (i = 0; i < u->definition_count; ++i) free(u->definitions[i].name);
    for (i = 0; i < u->node_count; ++i) {
        free(u->nodes[i].literal);
        free(u->nodes[i].concatenation);
    }
    free(u->files);
    free(u->definitions);
    free(u->anchors);
    free(u->nodes);
    free(u->stack);
    free(u->dependencies);
    free(u);
}

v2_diagnostic v2_unit_error(const v2_unit *u) { return u->error; }

const char *v2_error_message(v2_error error)
{
    switch (error) {
    case V2_ERROR_NONE: return "no error";
    case V2_ERROR_MEMORY: return "out of memory evaluating values";
    case V2_ERROR_REFERENCE: return "invalid value graph reference";
    case V2_ERROR_LOCATION: return "invalid source location";
    case V2_ERROR_REBIND: return "captured definition is already bound";
    case V2_ERROR_TYPE: return "invalid expression operand type";
    case V2_ERROR_CYCLE: return "circular value definition";
    case V2_ERROR_DIV_ZERO: return "division by zero in expression";
    case V2_ERROR_MOD_ZERO: return "modulo by zero in expression";
    case V2_ERROR_SHIFT: return "invalid shift count in expression";
    case V2_ERROR_STRING_LIMIT: return "string resource limit exceeded";
    case V2_ERROR_LAYOUT_STRING: return "string value depends on placement";
    }
    return "invalid value diagnostic";
}

v2_id v2_file(v2_unit *u, const char *name)
{
    char *copy;
    u->files = grow(u, u->files, &u->file_capacity, u->file_count + 1,
                    sizeof(*u->files), v2_no_location());
    if (u->error.code != V2_ERROR_NONE) return V2_NONE;
    copy = copy_name(u, name, v2_no_location());
    if (copy == NULL) return V2_NONE;
    u->files[u->file_count] = copy;
    return (v2_id)u->file_count++;
}

const char *v2_file_name(const v2_unit *u, v2_id id)
{ return id < u->file_count ? u->files[id] : NULL; }

v2_id v2_definition(v2_unit *u, const char *name, v2_location loc)
{
    definition d;
    if (!valid_location(u, loc)) return V2_NONE;
    u->definitions = grow(u, u->definitions, &u->definition_capacity,
                          u->definition_count + 1, sizeof(d), loc);
    if (u->error.code != V2_ERROR_NONE) return V2_NONE;
    d.name = copy_name(u, name, loc);
    if (d.name == NULL) return V2_NONE;
    d.location = loc;
    d.expression = V2_NONE;
    d.visited = 0;
    u->definitions[u->definition_count] = d;
    return (v2_id)u->definition_count++;
}

const char *v2_definition_name(const v2_unit *u, v2_id id)
{ return id < u->definition_count ? u->definitions[id].name : NULL; }

/* Wraparound is rare, but must not make a very old memo entry current. */
static void advance(v2_unit *u, uint64_t *version)
{
    size_t i;
    if (++*version != 0) return;
    *version = 1;
    for (i = 0; i < u->node_count; ++i) {
        if (u->nodes[i].cache.binding_dependent || u->nodes[i].cache.layout_dependent)
            u->nodes[i].cached = 0;
        u->nodes[i].active = u->nodes[i].visited = 0;
    }
    for (i = 0; i < u->definition_count; ++i) u->definitions[i].visited = 0;
    for (i = 0; i < u->anchor_count; ++i) u->anchors[i].visited = 0;
}

int v2_bind(v2_unit *u, v2_id id, v2_id expression)
{
    if (u->error.code != V2_ERROR_NONE) return 0;
    if (id >= u->definition_count || expression >= u->node_count) {
        fail(u, V2_ERROR_REFERENCE, v2_no_location()); return 0;
    }
    if (u->definitions[id].expression != V2_NONE) {
        fail(u, V2_ERROR_REBIND, u->definitions[id].location); return 0;
    }
    u->definitions[id].expression = expression;
    advance(u, &u->bindings);
    return 1;
}

v2_id v2_anchor(v2_unit *u, v2_location loc)
{
    anchor a = {0};
    if (!valid_location(u, loc)) return V2_NONE;
    u->anchors = grow(u, u->anchors, &u->anchor_capacity, u->anchor_count + 1,
                      sizeof(a), loc);
    if (u->error.code != V2_ERROR_NONE) return V2_NONE;
    a.location = loc;
    u->anchors[u->anchor_count] = a;
    return (v2_id)u->anchor_count++;
}

int v2_place(v2_unit *u, v2_id id, int known, int32_t value)
{
    anchor *a;
    if (u->error.code != V2_ERROR_NONE) return 0;
    if (id >= u->anchor_count || (known != 0 && known != 1)) {
        fail(u, V2_ERROR_REFERENCE, v2_no_location()); return 0;
    }
    a = &u->anchors[id];
    if (a->known != known || (known && a->value != value)) {
        advance(u, &u->layout);
        a->known = known;
        a->value = value;
    }
    return 1;
}

static v2_id append(v2_unit *u, node n)
{
    if (!valid_location(u, n.location)) return V2_NONE;
    u->nodes = grow(u, u->nodes, &u->node_capacity, u->node_count + 1,
                    sizeof(n), n.location);
    if (u->error.code != V2_ERROR_NONE) return V2_NONE;
    u->nodes[u->node_count] = n;
    return (v2_id)u->node_count++;
}

v2_id v2_integer(v2_unit *u, int32_t value, v2_location loc)
{
    node n = {0};
    n.kind = INTEGER; n.location = loc; n.integer = value;
    return append(u, n);
}

static unsigned char *string_storage(size_t length)
{
    /* Empty strings have no payload allocation. */
    return length ? malloc(length) : NULL;
}

static int fits_string(const v2_unit *u, size_t length)
{
    return length <= V2_MAX_STRING_BYTES &&
           length <= V2_MAX_LIVE_STRING_BYTES - u->string_bytes;
}

v2_id v2_string(v2_unit *u, const void *bytes, size_t length, v2_location loc)
{
    node n = {0};
    v2_id id;
    if (!valid_location(u, loc)) return V2_NONE;
    if (length && bytes == NULL) { fail(u, V2_ERROR_REFERENCE, loc); return V2_NONE; }
    if (!fits_string(u, length)) { fail(u, V2_ERROR_STRING_LIMIT, loc); return V2_NONE; }
    n.kind = STRING; n.location = loc; n.length = length;
    n.literal = string_storage(length);
    if (length && n.literal == NULL) { fail(u, V2_ERROR_MEMORY, loc); return V2_NONE; }
    if (length) memcpy(n.literal, bytes, length);
    id = append(u, n);
    if (id == V2_NONE) free(n.literal);
    else u->string_bytes += length;
    return id;
}

v2_id v2_reference(v2_unit *u, v2_id id, v2_location loc)
{
    node n = {0};
    if (!valid_location(u, loc)) return V2_NONE;
    if (id >= u->definition_count) { fail(u, V2_ERROR_REFERENCE, loc); return V2_NONE; }
    n.kind = REFERENCE; n.location = loc; n.left = id;
    return append(u, n);
}

v2_id v2_address(v2_unit *u, v2_id id, v2_location loc)
{
    node n = {0};
    if (!valid_location(u, loc)) return V2_NONE;
    if (id >= u->anchor_count) { fail(u, V2_ERROR_REFERENCE, loc); return V2_NONE; }
    n.kind = ADDRESS; n.location = loc; n.left = id;
    return append(u, n);
}

v2_id v2_unary(v2_unit *u, v2_operator op, v2_id operand, v2_location loc)
{
    node n = {0};
    if (!valid_location(u, loc)) return V2_NONE;
    if (operand >= u->node_count || op < V2_NEGATE || op > V2_HIGH) {
        fail(u, V2_ERROR_REFERENCE, loc); return V2_NONE;
    }
    n.kind = UNARY; n.location = loc; n.op = op; n.left = operand;
    return append(u, n);
}

v2_id v2_binary(v2_unit *u, v2_operator op, v2_id left, v2_id right, v2_location loc)
{
    node n = {0};
    if (!valid_location(u, loc)) return V2_NONE;
    if (left >= u->node_count || right >= u->node_count ||
        op < V2_ADD || op > V2_GREATER_EQUAL) {
        fail(u, V2_ERROR_REFERENCE, loc); return V2_NONE;
    }
    n.kind = BINARY; n.location = loc; n.op = op; n.left = left; n.right = right;
    return append(u, n);
}

static evaluated known(int32_t value)
{
    evaluated r = {0};
    r.state = V2_KNOWN;
    r.value.kind = V2_INTEGER;
    r.value.as.integer = value;
    return r;
}

static evaluated invalid(v2_error code, v2_location loc)
{
    evaluated r = {0};
    r.state = V2_INVALID;
    r.diagnostic = diagnostic(code, loc);
    return r;
}

static int current(const v2_unit *u, const node *n)
{
    return n->cached &&
        (!n->cache.binding_dependent || n->bindings == u->bindings) &&
        (!n->cache.layout_dependent || n->layout == u->layout);
}

static void finish(v2_unit *u, node *n, evaluated value)
{
    n->cache = value;
    n->cached = 1;
    n->bindings = u->bindings;
    n->layout = u->layout;
    n->active = 0;
    --u->stack_count;
}

static int push(v2_unit *u, v2_id id)
{
    frame f = { id, 0 };
    u->stack = grow(u, u->stack, &u->stack_capacity, u->stack_count + 1,
                    sizeof(f), u->nodes[id].location);
    if (u->error.code != V2_ERROR_NONE) return 0;
    u->stack[u->stack_count++] = f;
    return 1;
}

static evaluated integer_operation(const node *n, int32_t lhs, int32_t rhs)
{
    int result = 0;
    switch (n->op) {
    case V2_NEGATE: result = expr_neg(lhs); break;
    case V2_COMPLEMENT: result = expr_i32(~(uint32_t)lhs); break;
    case V2_NOT: result = !lhs; break;
    case V2_LOW: result = (uint32_t)lhs & 255u; break;
    case V2_HIGH: result = ((uint32_t)lhs >> 8) & 255u; break;
    case V2_ADD: result = expr_add(lhs, rhs); break;
    case V2_SUBTRACT: result = expr_sub(lhs, rhs); break;
    case V2_MULTIPLY: result = expr_mul(lhs, rhs); break;
    case V2_DIVIDE:
        if (!rhs) return invalid(V2_ERROR_DIV_ZERO, n->location);
        result = expr_div(lhs, rhs); break;
    case V2_REMAINDER:
        if (!rhs) return invalid(V2_ERROR_MOD_ZERO, n->location);
        result = expr_mod(lhs, rhs); break;
    case V2_SHIFT_LEFT:
        if (!expr_shl(lhs, rhs, &result)) return invalid(V2_ERROR_SHIFT, n->location);
        break;
    case V2_SHIFT_RIGHT:
        if (!expr_shr(lhs, rhs, &result)) return invalid(V2_ERROR_SHIFT, n->location);
        break;
    case V2_BIT_AND: result = expr_i32((uint32_t)lhs & (uint32_t)rhs); break;
    case V2_BIT_OR: result = expr_i32((uint32_t)lhs | (uint32_t)rhs); break;
    case V2_BIT_XOR: result = expr_i32((uint32_t)lhs ^ (uint32_t)rhs); break;
    case V2_EQUAL: result = lhs == rhs; break;
    case V2_NOT_EQUAL: result = lhs != rhs; break;
    case V2_LESS: result = lhs < rhs; break;
    case V2_LESS_EQUAL: result = lhs <= rhs; break;
    case V2_GREATER: result = lhs > rhs; break;
    case V2_GREATER_EQUAL: result = lhs >= rhs; break;
    }
    return known(result);
}

static evaluated string_operation(v2_unit *u, node *n, v2_value lhs, v2_value rhs)
{
    evaluated r = {0};
    char left_number[12], right_number[12];
    const unsigned char *left, *right;
    size_t left_length, right_length, length;
    int cmp;
    if (n->op != V2_ADD &&
        (lhs.kind != V2_STRING || rhs.kind != V2_STRING))
        return invalid(V2_ERROR_TYPE, n->location);
    if (lhs.kind == V2_STRING) {
        left = lhs.as.string.bytes; left_length = lhs.as.string.length;
    } else {
        left_length = (size_t)snprintf(left_number, sizeof(left_number), "%" PRId32,
                                       lhs.as.integer);
        left = (const unsigned char *)left_number;
    }
    if (rhs.kind == V2_STRING) {
        right = rhs.as.string.bytes; right_length = rhs.as.string.length;
    } else {
        right_length = (size_t)snprintf(right_number, sizeof(right_number), "%" PRId32,
                                        rhs.as.integer);
        right = (const unsigned char *)right_number;
    }
    if (n->op == V2_ADD) {
        if (left_length > V2_MAX_STRING_BYTES - right_length)
            return invalid(V2_ERROR_STRING_LIMIT, n->location);
        length = left_length + right_length;
        if (!fits_string(u, length)) return invalid(V2_ERROR_STRING_LIMIT, n->location);
        n->concatenation = string_storage(length);
        if (length && n->concatenation == NULL) {
            fail(u, V2_ERROR_MEMORY, n->location);
            return invalid(V2_ERROR_MEMORY, n->location);
        }
        if (left_length) memcpy(n->concatenation, left, left_length);
        if (right_length) memcpy(n->concatenation + left_length, right, right_length);
        u->string_bytes += length;
        r.state = V2_KNOWN; r.value.kind = V2_STRING;
        r.value.as.string.bytes = n->concatenation;
        r.value.as.string.length = length;
        return r;
    }
    length = left_length < right_length ? left_length : right_length;
    cmp = length ? memcmp(left, right, length) : 0;
    if (!cmp) cmp = (left_length > right_length) - (left_length < right_length);
    switch (n->op) {
    case V2_EQUAL: return known(cmp == 0);
    case V2_NOT_EQUAL: return known(cmp != 0);
    case V2_LESS: return known(cmp < 0);
    case V2_LESS_EQUAL: return known(cmp <= 0);
    case V2_GREATER: return known(cmp > 0);
    case V2_GREATER_EQUAL: return known(cmp >= 0);
    default: return invalid(V2_ERROR_TYPE, n->location);
    }
}

static evaluated operation(v2_unit *u, node *n, evaluated left, evaluated right)
{
    evaluated r = {0};
    if (left.state == V2_INVALID) return left;
    if (right.state == V2_INVALID) {
        right.binding_dependent |= left.binding_dependent;
        right.layout_dependent |= left.layout_dependent;
        return right;
    }
    if (left.state == V2_DEFERRED || right.state == V2_DEFERRED) r.state = V2_DEFERRED;
    else if (left.value.kind == V2_INTEGER && right.value.kind == V2_INTEGER)
        r = integer_operation(n, left.value.as.integer, right.value.as.integer);
    else if (n->kind == UNARY) r = invalid(V2_ERROR_TYPE, n->location);
    else if (left.layout_dependent || right.layout_dependent)
        r = invalid(V2_ERROR_LAYOUT_STRING, n->location);
    else r = string_operation(u, n, left.value, right.value);
    r.binding_dependent = left.binding_dependent || right.binding_dependent;
    r.layout_dependent = left.layout_dependent || right.layout_dependent;
    return r;
}

/* Cache entries record only Deferred, not a copied dependency set at every DAG
 * node. Collect the unique unresolved leaves once for the requested root. */
static int add_dependency(v2_unit *u, v2_dependency_kind kind, v2_id id,
                          v2_location loc)
{
    v2_dependency d = { kind, id };
    u->dependencies = grow(u, u->dependencies, &u->dependency_capacity,
                           u->dependency_count + 1, sizeof(d), loc);
    if (u->error.code != V2_ERROR_NONE) return 0;
    u->dependencies[u->dependency_count++] = d;
    return 1;
}

static int collect_dependencies(v2_unit *u, v2_id root)
{
    if (!push(u, root)) return 0;
    while (u->stack_count) {
        v2_id id = u->stack[--u->stack_count].id;
        node *n = &u->nodes[id];
        if (n->visited == u->request || n->cache.state != V2_DEFERRED) continue;
        n->visited = u->request;
        switch (n->kind) {
        case REFERENCE: {
            definition *d = &u->definitions[n->left];
            if (d->expression != V2_NONE) {
                if (!push(u, d->expression)) return 0;
            } else if (d->visited != u->request) {
                d->visited = u->request;
                if (!add_dependency(u, V2_DEP_DEFINITION, n->left, n->location)) return 0;
            }
            break;
        }
        case ADDRESS:
            if (u->anchors[n->left].visited != u->request) {
                u->anchors[n->left].visited = u->request;
                if (!add_dependency(u, V2_DEP_ANCHOR, n->left, n->location)) return 0;
            }
            break;
        case BINARY:
            if (!push(u, n->right)) return 0;
            /* fall through */
        case UNARY:
            if (!push(u, n->left)) return 0;
            break;
        default: break;
        }
    }
    return 1;
}

v2_result v2_evaluate(v2_unit *u, v2_id root)
{
    v2_result result = {0};
    u->dependency_count = u->stack_count = 0;
    if (root >= u->node_count && u->error.code == V2_ERROR_NONE)
        fail(u, V2_ERROR_REFERENCE, v2_no_location());
    if (u->error.code != V2_ERROR_NONE) goto failed;
    advance(u, &u->request);
    if (!push(u, root)) goto failed;
    while (u->stack_count) {
        frame *f = &u->stack[u->stack_count - 1];
        node *n = &u->nodes[f->id];
        evaluated value = {0};
        v2_id child;
        if (current(u, n)) { --u->stack_count; continue; }
        if (f->phase == 0) {
            n->active = u->request;
            if (n->kind == INTEGER) { finish(u, n, known(n->integer)); continue; }
            if (n->kind == STRING) {
                value.state = V2_KNOWN; value.value.kind = V2_STRING;
                value.value.as.string.bytes = n->literal;
                value.value.as.string.length = n->length;
                finish(u, n, value); continue;
            }
            if (n->kind == ADDRESS) {
                anchor *a = &u->anchors[n->left];
                if (a->known) value = known(a->value);
                else value.state = V2_DEFERRED;
                value.layout_dependent = 1;
                finish(u, n, value); continue;
            }
            if (n->kind == REFERENCE && u->definitions[n->left].expression == V2_NONE) {
                value.state = V2_DEFERRED; value.binding_dependent = 1;
                finish(u, n, value); continue;
            }
            child = n->kind == REFERENCE ? u->definitions[n->left].expression : n->left;
            f->phase = 1;
        } else {
            evaluated left = u->nodes[n->kind == REFERENCE ?
                u->definitions[n->left].expression : n->left].cache;
            if (n->kind == REFERENCE || left.state == V2_INVALID) {
                finish(u, n, left); continue;
            }
            if (n->kind == UNARY) {
                finish(u, n, operation(u, n, left, known(0))); continue;
            }
            if (f->phase == 2) {
                finish(u, n, operation(u, n, left, u->nodes[n->right].cache));
                if (u->error.code != V2_ERROR_NONE) goto failed;
                continue;
            }
            child = n->right;
            f->phase = 2;
        }
        if (current(u, &u->nodes[child])) continue;
        if (u->nodes[child].active == u->request) {
            value = invalid(V2_ERROR_CYCLE, u->nodes[child].location);
            if (n->kind == BINARY && f->phase == 2) {
                value.binding_dependent = u->nodes[n->left].cache.binding_dependent;
                value.layout_dependent = u->nodes[n->left].cache.layout_dependent;
            }
            finish(u, n, value);
            continue;
        }
        /* Growing the stack may invalidate f; do not retain it across push. */
        if (!push(u, child)) goto failed;
    }
    result.state = u->nodes[root].cache.state;
    if (result.state == V2_KNOWN) result.value = u->nodes[root].cache.value;
    else if (result.state == V2_INVALID) result.diagnostic = u->nodes[root].cache.diagnostic;
    else {
        if (!collect_dependencies(u, root)) goto failed;
        result.dependencies = u->dependencies;
        result.dependency_count = u->dependency_count;
    }
    return result;

failed:
    result.state = V2_INVALID;
    result.diagnostic = u->error;
    return result;
}
