/* See value.h for ownership and evaluation contracts. */
#include "value.h"
#include "extent.h"
#include "../expr_int.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { INTEGER, STRING, REFERENCE, ADDRESS, UNARY, BINARY, EXTENT_LITERAL, EXTENT_COUNT, EXTENT_SUM,
    EXTENT_REPEAT, EXTENT_REMAINDER, EXTENT_BOUND, EXTENT_STRING, EXTENT_INTEGER,
    SPAN } node_kind;

typedef struct {
    v2_state state;
    v2_value value;
    v2_diagnostic diagnostic;
    unsigned binding_dependent, layout_dependent;
} evaluated;

typedef struct { v2_id parent, next; } reverse_edge;
typedef struct {
    evaluated value;
    uint64_t bindings, request;
    unsigned cached;
} memo;

typedef struct {
    node_kind kind;
    v2_id parents;
    v2_operator op;
    v2_id left, right, span;
    v2_location location;
    int32_t integer;
    uint64_t extent;
    uint32_t minimum, maximum;
    unsigned char *literal;
    size_t length;
    /* Only concatenations own cache bytes. Aliases share their source value. */
    unsigned char *concatenation;
    unsigned has_concatenation;
    /* Modes have separate closures: a prefix query cannot overwrite the
     * descendant states used by an ordinary query's dependency collection. */
    memo memo[2];
    uint64_t active, visited;
} node;

typedef struct {
    char *name;
    v2_id references;
    v2_location location;
    v2_id expression;
    uint64_t visited;
} definition;

typedef struct {
    v2_location location;
    int known;
    v2_id parents, extent_expression;
    int32_t value;
    uint64_t visited;
} anchor;

typedef struct { v2_id id; unsigned phase; } frame;

struct v2_unit {
    char **files;
    definition *definitions;
    anchor *anchors;
    node *nodes;
    reverse_edge *edges;
    size_t edge_count, edge_capacity;
    v2_work work;
    unsigned extent_mode;
    size_t file_count, file_capacity, definition_count, definition_capacity;
    size_t anchor_count, anchor_capacity, node_count, node_capacity;
    frame *stack;
    size_t stack_count, stack_capacity;
    v2_id *dirty;
    size_t dirty_count, dirty_capacity;
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

int v2_reserve_expressions(v2_unit *u, size_t additional)
{
    if (additional > SIZE_MAX - u->node_count) {
        fail(u, V2_ERROR_MEMORY, v2_no_location()); return 0;
    }
    u->nodes = grow(u, u->nodes, &u->node_capacity, u->node_count + additional,
                    sizeof(*u->nodes), v2_no_location());
    return u->error.code == V2_ERROR_NONE;
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
    free(u->edges);
    free(u->stack);
    free(u->dirty);
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
    case V2_ERROR_EXTENT: return "storage extent out of range";
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
    d.references = V2_NONE;
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
        unsigned mode;
        for (mode = 0; mode < 2; ++mode)
            if (u->nodes[i].memo[mode].value.binding_dependent ||
                u->nodes[i].memo[mode].value.layout_dependent ||
                (u->nodes[i].memo[mode].value.state == V2_INVALID &&
                 u->nodes[i].memo[mode].value.diagnostic.code == V2_ERROR_CYCLE))
                u->nodes[i].memo[mode].cached = 0;
        u->nodes[i].active = u->nodes[i].visited = 0;
    }
    for (i = 0; i < u->definition_count; ++i) u->definitions[i].visited = 0;
    for (i = 0; i < u->anchor_count; ++i) u->anchors[i].visited = 0;
}

static int link_parent(v2_unit *, v2_id *, v2_id);

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
    { v2_id edge = u->definitions[id].references;
      while (edge != V2_NONE) {
          reverse_edge e = u->edges[edge];
          if (!link_parent(u, &u->nodes[expression].parents, e.parent)) return 0;
          edge = e.next;
      }
    }
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
    a.parents = a.extent_expression = V2_NONE;
    a.location = loc;
    u->anchors[u->anchor_count] = a;
    return (v2_id)u->anchor_count++;
}

static int link_parent(v2_unit *u, v2_id *head, v2_id parent)
{
    reverse_edge e;
    e.parent = parent; e.next = *head;
    u->edges = grow(u, u->edges, &u->edge_capacity, u->edge_count + 1,
                    sizeof(e), v2_no_location());
    if (u->error.code) return 0;
    u->edges[u->edge_count] = e; *head = (v2_id)u->edge_count++;
    return 1;
}
static int node_parents(v2_unit *u, v2_id id)
{
    node *n = &u->nodes[id];
    v2_id child;
    if (n->kind == REFERENCE && !link_parent(u, &u->definitions[n->left].references, id)) return 0;
    if (n->kind == ADDRESS) {
        anchor *a = &u->anchors[n->left];
        if (!link_parent(u, &a->parents, id)) return 0;
        return a->extent_expression == V2_NONE || link_parent(u, &u->nodes[a->extent_expression].parents, id);
    }
    if (n->kind == INTEGER || n->kind == STRING || n->kind == EXTENT_LITERAL) return 1;
    child = n->kind == REFERENCE ? u->definitions[n->left].expression :
                    n->kind == ADDRESS ? u->anchors[n->left].extent_expression : n->left;
    if (child != V2_NONE && !link_parent(u, &u->nodes[child].parents, id)) return 0;
    if (n->kind == SPAN && !link_parent(u, &u->nodes[n->span].parents, id)) return 0;
    if (n->kind == BINARY || n->kind == SPAN || n->kind == EXTENT_SUM || n->kind == EXTENT_REPEAT || n->kind == EXTENT_REMAINDER)
        return link_parent(u, &u->nodes[n->right].parents, id);
    return 1;
}
/* Invalidation stops at already dirty nodes. The queue is iterative, and each
 * cached ancestor becomes dirty at most once per change. Shared unrelated
 * subgraphs survive placement of an unrelated anchor. */
static int invalidate(v2_unit *u, v2_id head)
{
    size_t at = 0;
    u->dirty_count = 0;
    while (head != V2_NONE || at < u->dirty_count) {
        if (head == V2_NONE) { head = u->nodes[u->dirty[at++]].parents; continue; }
        reverse_edge e = u->edges[head];
        node *n = &u->nodes[e.parent];
        head = e.next;
        ++u->work.invalidation_edges;
        unsigned mode, dirty = 0;
        for (mode = 0; mode < 2; ++mode) {
            memo *m = &n->memo[mode];
            if (m->cached && m->value.layout_dependent) {
                m->cached = 0; dirty = 1;
            }
        }
        if (!dirty) continue;
        u->dirty = grow(u, u->dirty, &u->dirty_capacity, u->dirty_count + 1,
                        sizeof(*u->dirty), n->location);
        if (u->error.code) { u->dirty_count = 0; return 0; }
        u->dirty[u->dirty_count++] = e.parent;
    }
    u->dirty_count = 0; return 1;
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
        if (!invalidate(u, a->parents)) return 0;
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
    n.parents = V2_NONE;
    u->nodes[u->node_count] = n;
    { v2_id id = (v2_id)u->node_count++;
      if (!node_parents(u, id)) { --u->node_count; return V2_NONE; }
      return id; }
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

int v2_anchor_value(v2_unit *u, v2_id id, int *known, int32_t *value, v2_location loc)
{
    if (!valid_location(u, loc)) return 0;
    if (id >= u->anchor_count) { fail(u, V2_ERROR_REFERENCE, loc); return 0; }
    if (known) *known = u->anchors[id].known;
    if (value) *value = u->anchors[id].value;
    return 1;
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

int v2_extent_anchor(v2_unit *u, v2_id id, v2_id expression)
{
    v2_id edge;
    if (u->error.code) return 0;
    if (id >= u->anchor_count || expression >= u->node_count) {
        fail(u, V2_ERROR_REFERENCE, v2_no_location()); return 0;
    }
    if (u->anchors[id].extent_expression != V2_NONE) {
        fail(u, V2_ERROR_REBIND, u->anchors[id].location); return 0;
    }
    u->anchors[id].extent_expression = expression;
    if (!invalidate(u, u->anchors[id].parents)) return 0;
    edge = u->anchors[id].parents;
    while (edge != V2_NONE) {
        reverse_edge e = u->edges[edge];
        if (!link_parent(u, &u->nodes[expression].parents, e.parent)) return 0;
        edge = e.next;
    }
    return 1;
}

static int binary_kind(node_kind kind)
{
    return kind == BINARY || kind == SPAN || kind == EXTENT_SUM || kind == EXTENT_REPEAT ||
           kind == EXTENT_REMAINDER;
}

static v2_id extent_node(v2_unit *u, node_kind kind, v2_id left, v2_id right,
                         uint64_t value, uint32_t min, uint32_t max, v2_location loc)
{
    node n = {0};
    if (!valid_location(u, loc)) return V2_NONE;
    if ((kind != EXTENT_LITERAL && left >= u->node_count) ||
        (binary_kind(kind) && right >= u->node_count) || min > max ||
        (kind == EXTENT_COUNT && max > INT32_MAX)) {
        fail(u, V2_ERROR_REFERENCE, loc); return V2_NONE;
    }
    n.kind = kind; n.left = left; n.right = right; n.extent = value;
    n.minimum = min; n.maximum = max; n.location = loc;
    return append(u, n);
}
v2_id v2_extent_literal(v2_unit *u, uint64_t v, v2_location l)
{ return extent_node(u, EXTENT_LITERAL, 0, 0, v, 0, 0, l); }
v2_id v2_extent_count(v2_unit *u, v2_id a, uint32_t min, uint32_t max, v2_location l)
{ return extent_node(u, EXTENT_COUNT, a, 0, 0, min, max, l); }
v2_id v2_extent_bound(v2_unit *u, v2_id a, uint32_t min, uint32_t max, v2_location l)
{ return extent_node(u, EXTENT_BOUND, a, 0, 0, min, max, l); }
v2_id v2_extent_sum(v2_unit *u, v2_id a, v2_id b, v2_location l)
{ return extent_node(u, EXTENT_SUM, a, b, 0, 0, 0, l); }
v2_id v2_extent_repeat(v2_unit *u, v2_id a, v2_id b, v2_location l)
{ return extent_node(u, EXTENT_REPEAT, a, b, 0, 0, 0, l); }
v2_id v2_extent_remainder(v2_unit *u, v2_id a, v2_id b, v2_location l)
{ return extent_node(u, EXTENT_REMAINDER, a, b, 0, 0, 0, l); }
v2_id v2_extent_string(v2_unit *u, v2_id a, v2_location l)
{ return extent_node(u, EXTENT_STRING, a, 0, 0, 0, 0, l); }
v2_id v2_extent_integer(v2_unit *u, v2_id a, v2_location l)
{ return extent_node(u, EXTENT_INTEGER, a, 0, 0, 0, 0, l); }

/* A bounded alias walk also handles cycles without recursive expansion. */
static v2_id address_leaf(const v2_unit *u, v2_id id)
{
    size_t steps;
    for (steps = 0; id < u->node_count && steps <= u->definition_count; ++steps) {
        const node *n = &u->nodes[id];
        if (n->kind == ADDRESS) return n->left;
        if (n->kind != REFERENCE) break;
        id = u->definitions[n->left].expression;
    }
    return V2_NONE;
}
static int lower_spans(v2_unit *u, const unsigned char *reachable,
                       v2_span_lowerer lower, void *context)
{
    size_t i, count = u->node_count;
    int changed = 0;
    for (i = 0; i < count && u->error.code == V2_ERROR_NONE; ++i) {
        node n = u->nodes[i];
        v2_id left, right, replacement;
        if ((reachable && !reachable[i]) || n.kind != BINARY || n.op != V2_SUBTRACT) continue;
        left = address_leaf(u, n.left); right = address_leaf(u, n.right);
        if (left == V2_NONE || right == V2_NONE) continue;
        replacement = lower(context, left, right, n.location);
        if (replacement == V2_NONE) continue;
        if (replacement >= u->node_count) {
            fail(u, V2_ERROR_REFERENCE, n.location); break;
        }
        changed = 1;
        u->nodes[i].kind = SPAN;
        u->nodes[i].span = replacement;
        if (!link_parent(u, &u->nodes[replacement].parents, (v2_id)i)) break;
    }
    /* Construction may already have evaluated constants. Rewrites invalidate
     * ancestors too; this one-time operation runs before the placement loop. */
    if (changed && !u->error.code) {
        /* Both original operands and the extent-only shortcut remain edges.
         * Pure completed values (including owned strings) stay memoized. */
        u->edge_count = 0;
        for (i = 0; i < u->node_count; ++i) {
            unsigned mode;
            for (mode = 0; mode < 2; ++mode)
                if (u->nodes[i].memo[mode].value.layout_dependent)
                    u->nodes[i].memo[mode].cached = 0;
            u->nodes[i].parents = V2_NONE;
        }
        for (i = 0; i < u->anchor_count; ++i) u->anchors[i].parents = V2_NONE;
        for (i = 0; i < u->definition_count; ++i) u->definitions[i].references = V2_NONE;
        for (i = 0; i < u->node_count; ++i) if (!node_parents(u, (v2_id)i)) break;
    }
    return u->error.code == V2_ERROR_NONE;
}

int v2_lower_spans(v2_unit *u, v2_span_lowerer lower, void *context)
{
    return lower_spans(u, NULL, lower, context);
}

int v2_lower_extent_spans(v2_unit *u, const v2_id *roots, size_t count,
                          v2_span_lowerer lower, void *context)
{
    unsigned char *reachable;
    v2_id *todo;
    size_t i, queued = 0;
    int result;
    if (u->error.code) return 0;
    if (!count) return 1;
    if (!roots || u->node_count > SIZE_MAX / sizeof(*todo)) {
        fail(u, V2_ERROR_REFERENCE, v2_no_location()); return 0;
    }
    reachable = calloc(u->node_count, 1);
    todo = malloc(u->node_count * sizeof(*todo));
    if (!reachable || !todo) {
        free(reachable); free(todo);
        fail(u, V2_ERROR_MEMORY, v2_no_location()); return 0;
    }
    for (i = 0; i < count; ++i) {
        v2_id root = roots[i];
        if (root >= u->node_count) {
            fail(u, V2_ERROR_REFERENCE, v2_no_location()); break;
        }
        if (!reachable[root]) { reachable[root] = 1; todo[queued++] = root; }
    }
    while (queued && !u->error.code) {
        const node *n = &u->nodes[todo[--queued]];
        v2_id children[3];
        size_t child_count = 0, c;
        switch (n->kind) {
        case REFERENCE: children[child_count++] = u->definitions[n->left].expression; break;
        case ADDRESS: children[child_count++] = u->anchors[n->left].extent_expression; break;
        case SPAN: children[child_count++] = n->span;
            /* fall through */
        case BINARY: case EXTENT_SUM: case EXTENT_REPEAT: case EXTENT_REMAINDER:
            children[child_count++] = n->right;
            /* fall through */
        case UNARY: case EXTENT_COUNT: case EXTENT_BOUND: case EXTENT_STRING: case EXTENT_INTEGER:
            children[child_count++] = n->left; break;
        default: break;
        }
        for (c = 0; c < child_count; ++c) {
            v2_id child = children[c];
            if (child != V2_NONE && !reachable[child]) {
                reachable[child] = 1; todo[queued++] = child;
            }
        }
    }
    result = lower_spans(u, reachable, lower, context);
    free(todo); free(reachable);
    return result;
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

static evaluated known_extent(uint64_t value)
{
    evaluated r = {0};
    r.state = V2_KNOWN; r.value.kind = V2_EXTENT; r.value.as.extent = value;
    return r;
}

static evaluated extent_operation(const node *n, v2_value left, v2_value right)
{
    uint64_t a, b;
    if (n->kind == EXTENT_COUNT) {
        if (left.kind != V2_INTEGER) return invalid(V2_ERROR_TYPE, n->location);
        if (left.as.integer < 0 || (uint32_t)left.as.integer < n->minimum ||
            (uint32_t)left.as.integer > n->maximum)
            return invalid(V2_ERROR_EXTENT, n->location);
        return known_extent((uint32_t)left.as.integer);
    }
    if (n->kind == EXTENT_STRING) {
        if (left.kind != V2_STRING) return invalid(V2_ERROR_TYPE, n->location);
        return known_extent(left.as.string.length);
    }
    if (left.kind != V2_EXTENT || (binary_kind(n->kind) && right.kind != V2_EXTENT))
        return invalid(V2_ERROR_TYPE, n->location);
    a = left.as.extent; b = binary_kind(n->kind) ? right.as.extent : 0;
    switch (n->kind) {
    case EXTENT_SUM:
        if (a > UINT64_MAX - b) break;
        return known_extent(a + b);
    case EXTENT_REPEAT:
        if (b && a > UINT64_MAX / b) break;
        return known_extent(a * b);
    case EXTENT_REMAINDER:
        if (a < b) break;
        return known_extent(a - b);
    case EXTENT_BOUND:
        if (a < n->minimum || a > n->maximum) break;
        return known_extent(a);
    case EXTENT_INTEGER: return known(expr_i32((uint32_t)a));
    default: return invalid(V2_ERROR_TYPE, n->location);
    }
    return invalid(V2_ERROR_EXTENT, n->location);
}

static int current(const v2_unit *u, const node *n)
{
    const memo *m = &n->memo[u->extent_mode];
    return m->cached &&
        (!m->value.binding_dependent || m->bindings == u->bindings) &&
        /* A cycle's first repeated edge depends on the requested root. */
        (m->value.state != V2_INVALID || m->value.diagnostic.code != V2_ERROR_CYCLE ||
         m->request == u->request);
}

static void finish(v2_unit *u, node *n, evaluated value)
{
    memo *m = &n->memo[u->extent_mode];
    ++u->work.evaluated_nodes;
    m->value = value;
    m->cached = 1; m->bindings = u->bindings; m->request = u->request;
    /* Constants can share a result across modes, but never a layout-dependent
     * closure, an unresolved binding, or a root-dependent cycle diagnostic. */
    if (!value.layout_dependent && !value.binding_dependent &&
        !(value.state == V2_INVALID && value.diagnostic.code == V2_ERROR_CYCLE))
        n->memo[!u->extent_mode] = *m;
    n->active = 0;
    --u->stack_count;
}

static int span_redirect(const v2_unit *u, const node *n)
{ return n->kind == SPAN && u->extent_mode; }
static v2_id first_child(const v2_unit *u, const node *n)
{
    if (n->kind == REFERENCE) return u->definitions[n->left].expression;
    if (n->kind == ADDRESS) return u->anchors[n->left].extent_expression;
    return span_redirect(u, n) ? n->span : n->left;
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
        /* A Known non-layout string is immutable. Re-querying it after a
         * cache reset must preserve its buffer, all outstanding views and the
         * unit's live-byte budget (including the empty-string case). */
        if (n->has_concatenation) {
            r.state = V2_KNOWN; r.value.kind = V2_STRING;
            r.value.as.string.bytes = n->concatenation;
            r.value.as.string.length = n->length;
            return r;
        }
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
        n->has_concatenation = 1; n->length = length;
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
    else if (n->kind >= EXTENT_COUNT && n->kind <= EXTENT_INTEGER)
        r = extent_operation(n, left.value, right.value);
    else if (left.value.kind == V2_EXTENT || right.value.kind == V2_EXTENT)
        r = invalid(V2_ERROR_TYPE, n->location);
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
        ++u->work.dependency_visits;
        v2_id id = u->stack[--u->stack_count].id;
        node *n = &u->nodes[id];
        if (n->visited == u->request || n->memo[u->extent_mode].value.state != V2_DEFERRED) continue;
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
            if (u->extent_mode && u->anchors[n->left].extent_expression != V2_NONE) {
                if (!push(u, u->anchors[n->left].extent_expression)) return 0;
                break;
            }
            if (u->anchors[n->left].visited != u->request) {
                u->anchors[n->left].visited = u->request;
                if (!add_dependency(u, V2_DEP_ANCHOR, n->left, n->location)) return 0;
            }
            break;
        case SPAN:
            if (span_redirect(u, n)) {
                if (!push(u, n->span)) return 0;
                break;
            }
            /* fall through */
        case BINARY: case EXTENT_SUM: case EXTENT_REPEAT: case EXTENT_REMAINDER:
            if (!push(u, n->right)) return 0;
            /* fall through */
        case UNARY: case EXTENT_COUNT: case EXTENT_BOUND: case EXTENT_STRING:
        case EXTENT_INTEGER:
            if (!push(u, n->left)) return 0;
            break;
        default: break;
        }
    }
    return 1;
}

static v2_result evaluate(v2_unit *u, v2_id root, unsigned extent_mode)
{
    v2_result result = {0};
    u->extent_mode = extent_mode;
    u->dependency_count = u->stack_count = 0;
    if (root >= u->node_count && u->error.code == V2_ERROR_NONE)
        fail(u, V2_ERROR_REFERENCE, v2_no_location());
    if (u->error.code != V2_ERROR_NONE) goto failed;
    advance(u, &u->request);
    if (!push(u, root)) goto failed;
    while (u->stack_count && !u->error.code) {
        frame *f = &u->stack[u->stack_count - 1];
        node *n = &u->nodes[f->id];
        evaluated value = {0};
        v2_id child;
        if (current(u, n)) { --u->stack_count; continue; }
        if (f->phase == 0) {
            n->active = u->request;
            if (n->kind == EXTENT_LITERAL) { finish(u, n, known_extent(n->extent)); continue; }
            if (n->kind == INTEGER) { finish(u, n, known(n->integer)); continue; }
            if (n->kind == STRING) {
                value.state = V2_KNOWN; value.value.kind = V2_STRING;
                value.value.as.string.bytes = n->literal;
                value.value.as.string.length = n->length;
                finish(u, n, value); continue;
            }
            if (n->kind == ADDRESS &&
                (!u->extent_mode || u->anchors[n->left].extent_expression == V2_NONE)) {
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
            child = first_child(u, n);
            f->phase = 1;
        } else {
            evaluated left = u->nodes[first_child(u, n)].memo[u->extent_mode].value;
            if (n->kind == ADDRESS || span_redirect(u, n)) {
                if (left.state == V2_KNOWN && left.value.kind != V2_INTEGER)
                    left = invalid(V2_ERROR_TYPE, n->location);
                left.layout_dependent = 1;
                finish(u, n, left); continue;
            }
            if (n->kind == REFERENCE || left.state == V2_INVALID) {
                finish(u, n, left); continue;
            }
            if (n->kind == EXTENT_REPEAT && left.state == V2_KNOWN &&
                left.value.kind == V2_EXTENT && left.value.as.extent == 0) {
                finish(u, n, left); continue;
            }
            if (!binary_kind(n->kind)) {
                finish(u, n, operation(u, n, left, known(0))); continue;
            }
            if (f->phase == 2) {
                finish(u, n, operation(u, n, left, u->nodes[n->right].memo[u->extent_mode].value));
                if (u->error.code != V2_ERROR_NONE) goto failed;
                continue;
            }
            child = n->right;
            f->phase = 2;
        }
        if (current(u, &u->nodes[child])) continue;
        if (u->nodes[child].active == u->request) {
            value = invalid(V2_ERROR_CYCLE, u->nodes[child].location);
            value.layout_dependent = u->extent_mode;
            if (binary_kind(n->kind) && f->phase == 2) {
                value.binding_dependent = u->nodes[n->left].memo[u->extent_mode].value.binding_dependent;
                value.layout_dependent |= u->nodes[n->left].memo[u->extent_mode].value.layout_dependent;
            }
            finish(u, n, value);
            continue;
        }
        /* Growing the stack may invalidate f; do not retain it across push. */
        if (!push(u, child)) goto failed;
    }
    if (u->error.code) goto failed;
    result.state = u->nodes[root].memo[u->extent_mode].value.state;
    if (result.state == V2_KNOWN) result.value = u->nodes[root].memo[u->extent_mode].value.value;
    else if (result.state == V2_INVALID) result.diagnostic = u->nodes[root].memo[u->extent_mode].value.diagnostic;
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

v2_work v2_work_done(const v2_unit *u) { return u->work; }

v2_result v2_evaluate(v2_unit *u, v2_id root) { return evaluate(u, root, 0); }
v2_result v2_evaluate_extent(v2_unit *u, v2_id root) { return evaluate(u, root, 1); }
