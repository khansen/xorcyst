#include "heap.h"
/* See value.h for ownership and evaluation contracts. */
#include "value.h"
#include "extent.h"
#include "graph.h"
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
    union { v2_value value; v2_diagnostic diagnostic; } result;
    unsigned binding_dependent, layout_dependent;
    v2_id diagnostic_location;
} evaluated;

typedef struct { v2_id parent, next; } reverse_edge;
/* Cached values store only their active payload. A diagnostic names its
 * immutable source node, and strings borrow stable unit-owned bytes. */
#define MEMO_CACHED 1u
#define MEMO_BINDING 2u
#define MEMO_LAYOUT 4u
typedef struct {
    /* Known values cannot depend on an unbound definition: definitions bind
     * once, and every operation propagates Deferred until its inputs resolve.
     * Deferred/Invalid have no value payload, so their binding/request stamp
     * shares these eight bytes with Known's value. */
    union { int32_t integer; uint64_t extent, epoch; const unsigned char *bytes; } payload;
    /* Cycles are usable only in this request; other binding-dependent
     * results use the binding epoch. No builder runs inside a query, so a
     * request-local result does not need a second binding-epoch stamp. */
    uint32_t detail; /* string length or diagnostic node */
    unsigned char state, kind, error, flags;
} memo;

/* Operand slots are kind-specific: literals use left/right as bits, count
 * bounds use right/extra, and spans use extra for their lowered root. String
 * buffers live in a separate owned table so numeric nodes carry no pointer. */
typedef struct {
    unsigned char kind, op, has_concatenation, visit;
    v2_id parents, left, right, extra, location;
    v2_id memos[2];
} node;
typedef struct { unsigned char *bytes; uint32_t length; } owned_string;


typedef struct {
    char *name;
    v2_id references;
    v2_id location;
    v2_id expression;
    unsigned char visited;
} definition;

typedef struct {
    v2_id location;
    v2_id parents, extent_expression;
    int32_t value;
    unsigned char known, visited;
} anchor;

typedef struct { v2_id id; unsigned phase; } frame;

struct v2_unit {
    v2_heap *heap;
    char **files;
    definition *definitions;
    anchor *anchors;
    node **nodes;
    memo **memos;
    size_t memo_count, memo_capacity;
    uint64_t **locations;
    v2_location *wide_locations;
    size_t wide_location_count, wide_location_capacity;
    v2_id *location_hash;
    size_t location_count, location_capacity, location_hash_capacity;
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
    owned_string *strings;
    size_t string_count, string_capacity, string_bytes;
    uint64_t bindings, layout, request;
    v2_diagnostic error;
};

/* Fixed-size slabs avoid copying the complete graph at each growth step.
 * IDs and node addresses remain stable; the directory is the only relocatable
 * array. A capacity hint reserves directory entries, not untouched nodes. */
#define NODE_SHIFT 10u
#define NODE_BLOCK (1u << NODE_SHIFT)
static node *node_at(const v2_unit *u, size_t id)
{ return &u->nodes[id >> NODE_SHIFT][id & (NODE_BLOCK - 1)]; }

static memo *node_memo(const v2_unit *u, const node *n, unsigned mode)
{
    static memo empty;
    v2_id id = n->memos[mode];
    return id == V2_NONE ? &empty : &u->memos[id >> NODE_SHIFT][id & (NODE_BLOCK - 1)];
}

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
    p = v2_realloc(u->heap, table, next * width);
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

/* Common spans fit in eight bytes: file:16, line:24, start column:8,
 * end column:8, line delta:8. The reserved file field selects an owned full
 * span for larger coordinates; no source location is truncated. */
static uint64_t *location_slot(const v2_unit *u, size_t id)
{ return &u->locations[id >> NODE_SHIFT][id & (NODE_BLOCK - 1)]; }
static v2_location location_at(const v2_unit *u, size_t id)
{
    uint64_t bits = *location_slot(u, id);
    v2_location loc;
    if ((bits & 65535) == 65535) return u->wide_locations[bits >> 16];
    loc.file = (uint32_t)(bits & 65535);
    loc.line = (uint32_t)((bits >> 16) & 0xffffff);
    loc.column = (uint32_t)((bits >> 40) & 255);
    loc.end_column = (uint32_t)((bits >> 48) & 255);
    loc.end_line = loc.line + (uint32_t)(bits >> 56);
    return loc;
}
v2_location v2_graph_location(const v2_unit *u, v2_id id)
{ return id < u->location_count ? location_at(u, id) : v2_no_location(); }
void v2_graph_release_location_index(v2_unit *u)
{ v2_free(u->heap, u->location_hash); u->location_hash = NULL; u->location_hash_capacity = 0; }
static uint32_t location_key(v2_location l)
{
    uint32_t h = 2166136261U;
    h = (h ^ l.file) * 16777619U; h = (h ^ l.line) * 16777619U;
    h = (h ^ l.column) * 16777619U; h = (h ^ l.end_line) * 16777619U;
    return (h ^ l.end_column) * 16777619U;
}
static int same_location(v2_location a, v2_location b)
{ return a.file == b.file && a.line == b.line && a.column == b.column && a.end_line == b.end_line && a.end_column == b.end_column; }
v2_id v2_graph_location_id(v2_unit *u, v2_location loc)
{
    size_t at, i;
    if (!valid_location(u, loc) || loc.file == V2_NONE) return V2_NONE;
    if (!u->location_hash_capacity || u->location_count >= u->location_hash_capacity - u->location_hash_capacity / 4) {
        size_t cap = u->location_hash_capacity ? u->location_hash_capacity * 2 : 32;
        v2_id *table;
        while (cap - cap / 4 <= u->location_count) {
            if (cap > SIZE_MAX / 2) { fail(u, V2_ERROR_MEMORY, loc); return V2_NONE; }
            cap *= 2;
        }
        if (cap < u->location_hash_capacity || cap > SIZE_MAX / sizeof(*table) || !(table = v2_calloc(u->heap, cap, sizeof(*table)))) {
            fail(u, V2_ERROR_MEMORY, loc); return V2_NONE;
        }
        for (i = 0; i < u->location_count; i++) {
            at = location_key(location_at(u, i)) & (cap - 1);
            while (table[at]) at = (at + 1) & (cap - 1);
            table[at] = (v2_id)i + 1;
        }
        v2_free(u->heap, u->location_hash); u->location_hash = table; u->location_hash_capacity = cap;
    }
    at = location_key(loc) & (u->location_hash_capacity - 1);
    while (u->location_hash[at]) {
        v2_id id = u->location_hash[at] - 1;
        if (same_location(location_at(u, id), loc)) return id;
        at = (at + 1) & (u->location_hash_capacity - 1);
    }
    {
        size_t block = u->location_count >> NODE_SHIFT, old = u->location_capacity;
        u->locations = grow(u, u->locations, &u->location_capacity, block + 1, sizeof(*u->locations), loc);
        if (u->error.code) return V2_NONE;
        if (old < u->location_capacity) memset(u->locations + old, 0, (u->location_capacity - old) * sizeof(*u->locations));
        if (!u->locations[block] && !(u->locations[block] = v2_records(u->heap, NODE_BLOCK * sizeof(uint64_t)))) {
            fail(u, V2_ERROR_MEMORY, loc); return V2_NONE;
        }
    }
    if (loc.file < 65535 && loc.line <= 0xffffff && loc.column <= 255 &&
        loc.end_column <= 255 && loc.end_line - loc.line <= 255) {
        *location_slot(u, u->location_count) = (uint64_t)loc.file | ((uint64_t)loc.line << 16) |
            ((uint64_t)loc.column << 40) | ((uint64_t)loc.end_column << 48) |
            ((uint64_t)(loc.end_line - loc.line) << 56);
    } else {
        u->wide_locations = grow(u, u->wide_locations, &u->wide_location_capacity,
                                 u->wide_location_count + 1, sizeof(loc), loc);
        if (u->error.code) return V2_NONE;
        u->wide_locations[u->wide_location_count] = loc;
        *location_slot(u, u->location_count) = ((uint64_t)u->wide_location_count++ << 16) | 65535;
    }
    u->location_hash[at] = (v2_id)u->location_count + 1;
    return (v2_id)u->location_count++;
}

int v2_reserve_expressions(v2_unit *u, size_t additional)
{
    size_t old = u->node_capacity, blocks;
    if (additional > UINT32_MAX - u->node_count) {
        fail(u, V2_ERROR_MEMORY, v2_no_location()); return 0;
    }
    blocks = (u->node_count + additional + NODE_BLOCK - 1) / NODE_BLOCK;
    u->nodes = grow(u, u->nodes, &u->node_capacity, blocks, sizeof(*u->nodes), v2_no_location());
    if (!u->error.code && old < u->node_capacity)
        memset(u->nodes + old, 0, (u->node_capacity - old) * sizeof(*u->nodes));
    return !u->error.code;
}

static memo *new_memo(v2_unit *u, node *n, unsigned mode)
{
    size_t block = u->memo_count >> NODE_SHIFT, old = u->memo_capacity;
    memo *m;
    if (n->memos[mode] != V2_NONE) return node_memo(u, n, mode);
    if (u->memo_count == UINT32_MAX) { fail(u, V2_ERROR_MEMORY, v2_graph_location(u, n->location)); return NULL; }
    u->memos = grow(u, u->memos, &u->memo_capacity, block + 1, sizeof(*u->memos), v2_graph_location(u, n->location));
    if (u->error.code) return NULL;
    if (old < u->memo_capacity) memset(u->memos + old, 0, (u->memo_capacity - old) * sizeof(*u->memos));
    if (!u->memos[block] && !(u->memos[block] = v2_records(u->heap, NODE_BLOCK * sizeof(memo)))) {
        fail(u, V2_ERROR_MEMORY, v2_graph_location(u, n->location)); return NULL;
    }
    n->memos[mode] = (v2_id)u->memo_count++;
    m = node_memo(u, n, mode); memset(m, 0, sizeof(*m)); return m;
}

static char *copy_name(v2_unit *u, const char *name, v2_location loc)
{
    size_t length;
    char *copy;
    if (name == NULL || !*name) { fail(u, V2_ERROR_REFERENCE, loc); return NULL; }
    length = strlen(name);
    if (length == SIZE_MAX) { fail(u, V2_ERROR_MEMORY, loc); return NULL; }
    copy = v2_malloc(u->heap, length + 1);
    if (copy == NULL) { fail(u, V2_ERROR_MEMORY, loc); return NULL; }
    memcpy(copy, name, length + 1);
    return copy;
}

v2_unit *v2_unit_create_heap(v2_heap *heap)
{
    v2_unit *u = v2_calloc(heap, 1, sizeof(*u));
    if (u) u->heap = heap;
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
    for (i = 0; i < u->file_count; ++i) v2_free(u->heap, u->files[i]);
    for (i = 0; i < u->definition_count; ++i) v2_free(u->heap, u->definitions[i].name);
    for (i = 0; i < u->string_count; ++i) v2_free(u->heap, u->strings[i].bytes);
    v2_free(u->heap, u->strings);
    v2_free(u->heap, u->files);
    v2_free(u->heap, u->definitions);
    v2_free(u->heap, u->anchors);
    for (i = 0; i < u->memo_capacity; i++) v2_free(u->heap, u->memos[i]);
    v2_free(u->heap, u->memos);
    for (i = 0; i < u->node_capacity; i++) v2_free(u->heap, u->nodes[i]);
    v2_free(u->heap, u->nodes);
    for (i = 0; i < u->location_capacity; i++) v2_free(u->heap, u->locations[i]);
    v2_free(u->heap, u->locations); v2_free(u->heap, u->wide_locations); v2_free(u->heap, u->location_hash);
    v2_free(u->heap, u->edges);
    v2_free(u->heap, u->stack);
    v2_free(u->heap, u->dirty);
    v2_free(u->heap, u->dependencies);

    v2_free(u->heap, u);
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
    d.location = v2_graph_location_id(u, loc);
    if (u->error.code) { v2_free(u->heap, d.name); return V2_NONE; }
    d.expression = V2_NONE;
    d.visited = 0;
    d.references = V2_NONE;
    u->definitions[u->definition_count] = d;
    return (v2_id)u->definition_count++;
}

const char *v2_definition_name(const v2_unit *u, v2_id id)
{ return id < u->definition_count ? u->definitions[id].name : NULL; }

size_t v2_graph_nodes(const v2_unit *u) { return u->node_count; }
size_t v2_graph_definitions(const v2_unit *u) { return u->definition_count; }
size_t v2_graph_anchors(const v2_unit *u) { return u->anchor_count; }
int v2_graph_read(const v2_unit *u,v2_id id,v2_graph_node *out)
{
    static const v2_graph_kind kinds[] = {V2_GRAPH_INTEGER,V2_GRAPH_STRING,
        V2_GRAPH_REFERENCE,V2_GRAPH_ADDRESS,V2_GRAPH_UNARY,V2_GRAPH_BINARY,
        V2_GRAPH_EXTENT_LITERAL,V2_GRAPH_COUNT,V2_GRAPH_SUM,V2_GRAPH_REPEAT,
        V2_GRAPH_REMAINDER,V2_GRAPH_BOUND,V2_GRAPH_STRING_LENGTH,V2_GRAPH_EXTENT_INTEGER};
    const node *n;
    if(id>=u->node_count || !out) return 0;
    n=node_at(u, id); memset(out,0,sizeof(*out));
    /* Lowered spans retain their original ordinary subtraction on the wire. */
    out->kind=n->kind==SPAN?V2_GRAPH_BINARY:kinds[n->kind];
    out->op=(v2_operator)n->op;out->left=n->left;out->right=n->right;out->location=v2_graph_location(u, n->location);
    if (n->kind == INTEGER) out->integer=expr_i32(n->left);
    if (n->kind == EXTENT_LITERAL) out->extent=((uint64_t)n->left | ((uint64_t)n->right << 32));
    if (n->kind == EXTENT_COUNT || n->kind == EXTENT_BOUND) {
        out->minimum=n->right; out->maximum=n->extra;
    }
    if (n->kind == STRING) { out->bytes=u->strings[n->extra].bytes; out->length=u->strings[n->extra].length; }
    return 1;
}
int v2_graph_definition(const v2_unit *u,v2_id id,v2_id *expression,v2_location *loc)
{
    if(id>=u->definition_count) return 0;
    if(expression) *expression=u->definitions[id].expression;
    if(loc) *loc=v2_graph_location(u, u->definitions[id].location);
    return 1;
}
int v2_graph_anchor(const v2_unit *u,v2_id id,v2_location *loc)
{ if(id>=u->anchor_count) return 0; if(loc) *loc=v2_graph_location(u, u->anchors[id].location); return 1; }

/* Wraparound is rare, but must not make a very old memo entry current. */
static void advance(v2_unit *u, uint64_t *version)
{
    size_t i;
    if (++*version != 0) return;
    *version = 1;
    for (i = 0; i < u->node_count; ++i) {
        unsigned mode;
        for (mode = 0; mode < 2; ++mode)
            if ((node_memo(u, node_at(u, i), mode)->flags & (MEMO_BINDING | MEMO_LAYOUT)) ||
                (node_memo(u, node_at(u, i), mode)->state == V2_INVALID &&
                 node_memo(u, node_at(u, i), mode)->error == V2_ERROR_CYCLE))
                node_memo(u, node_at(u, i), mode)->flags &= (unsigned char)~MEMO_CACHED;
    }
}

static int link_parent(v2_unit *, v2_id *, v2_id);

int v2_bind(v2_unit *u, v2_id id, v2_id expression)
{
    if (u->error.code != V2_ERROR_NONE) return 0;
    if (id >= u->definition_count || expression >= u->node_count) {
        fail(u, V2_ERROR_REFERENCE, v2_no_location()); return 0;
    }
    if (u->definitions[id].expression != V2_NONE) {
        fail(u, V2_ERROR_REBIND, v2_graph_location(u, u->definitions[id].location)); return 0;
    }
    u->definitions[id].expression = expression;
    { v2_id edge = u->definitions[id].references;
      while (edge != V2_NONE) {
          reverse_edge e = u->edges[edge];
          if (!link_parent(u, &node_at(u, expression)->parents, e.parent)) return 0;
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
    a.location = v2_graph_location_id(u, loc);
    if (u->error.code) return V2_NONE;
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
    node *n = node_at(u, id);
    v2_id child;
    if (n->kind == REFERENCE && !link_parent(u, &u->definitions[n->left].references, id)) return 0;
    if (n->kind == ADDRESS) {
        anchor *a = &u->anchors[n->left];
        if (!link_parent(u, &a->parents, id)) return 0;
        return a->extent_expression == V2_NONE || link_parent(u, &node_at(u, a->extent_expression)->parents, id);
    }
    if (n->kind == INTEGER || n->kind == STRING || n->kind == EXTENT_LITERAL) return 1;
    child = n->kind == REFERENCE ? u->definitions[n->left].expression :
                    n->kind == ADDRESS ? u->anchors[n->left].extent_expression : n->left;
    if (child != V2_NONE && !link_parent(u, &node_at(u, child)->parents, id)) return 0;
    if (n->kind == SPAN && !link_parent(u, &node_at(u, n->extra)->parents, id)) return 0;
    if (n->kind == BINARY || n->kind == SPAN || n->kind == EXTENT_SUM || n->kind == EXTENT_REPEAT || n->kind == EXTENT_REMAINDER)
        return link_parent(u, &node_at(u, n->right)->parents, id);
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
        if (head == V2_NONE) { head = node_at(u, u->dirty[at++])->parents; continue; }
        reverse_edge e = u->edges[head];
        node *n = node_at(u, e.parent);
        head = e.next;
        ++u->work.invalidation_edges;
        unsigned mode, dirty = 0;
        for (mode = 0; mode < 2; ++mode) {
            memo *m = node_memo(u, n, mode);
            if ((m->flags & (MEMO_CACHED | MEMO_LAYOUT)) == (MEMO_CACHED | MEMO_LAYOUT)) {
                m->flags &= (unsigned char)~MEMO_CACHED; dirty = 1;
            }
        }
        if (!dirty) continue;
        u->dirty = grow(u, u->dirty, &u->dirty_capacity, u->dirty_count + 1,
                        sizeof(*u->dirty), v2_graph_location(u, n->location));
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
        a->known = (unsigned char)known;
        a->value = value;
    }
    return 1;
}

static v2_id append(v2_unit *u, node n)
{
    if (!valid_location(u, v2_graph_location(u, n.location))) return V2_NONE;
    if (!v2_reserve_expressions(u, 1)) return V2_NONE;
    if (!u->nodes[u->node_count >> NODE_SHIFT]) {
        u->nodes[u->node_count >> NODE_SHIFT] = v2_records(u->heap, NODE_BLOCK * sizeof(node));
        if (!u->nodes[u->node_count >> NODE_SHIFT]) { fail(u, V2_ERROR_MEMORY, v2_graph_location(u, n.location)); return V2_NONE; }
    }
    n.parents = V2_NONE; n.memos[0] = n.memos[1] = V2_NONE;
    (*node_at(u, u->node_count)) = n;
    { v2_id id = (v2_id)u->node_count++;
      if (!node_parents(u, id)) { --u->node_count; return V2_NONE; }
      return id; }
}

v2_id v2_integer(v2_unit *u, int32_t value, v2_location loc)
{
    node n = {0};
    n.kind = INTEGER; n.location = v2_graph_location_id(u, loc); n.left = (uint32_t)value;
    return append(u, n);
}

static unsigned char *string_storage(v2_unit *u, size_t length)
{
    /* Empty strings have no payload allocation. */
    return length ? v2_malloc(u->heap, length) : NULL;
}

static int fits_string(const v2_unit *u, size_t length)
{
    return length <= V2_MAX_STRING_BYTES &&
           length <= V2_MAX_LIVE_STRING_BYTES - u->string_bytes;
}

/* Commit a string slot only after its allocation succeeds. The unit owns
 * every committed slot even if appending its expression subsequently fails. */
static v2_id new_string(v2_unit *u, size_t length, v2_location loc)
{
    owned_string value;
    u->strings = grow(u, u->strings, &u->string_capacity, u->string_count + 1,
                      sizeof(*u->strings), loc);
    if (u->error.code) return V2_NONE;
    value.bytes = string_storage(u, length); value.length = (uint32_t)length;
    if (length && !value.bytes) { fail(u, V2_ERROR_MEMORY, loc); return V2_NONE; }
    u->strings[u->string_count] = value; u->string_bytes += length;
    return (v2_id)u->string_count++;
}
v2_id v2_string(v2_unit *u, const void *bytes, size_t length, v2_location loc)
{
    node n = {0};
    if (!valid_location(u, loc)) return V2_NONE;
    if (length && bytes == NULL) { fail(u, V2_ERROR_REFERENCE, loc); return V2_NONE; }
    if (!fits_string(u, length)) { fail(u, V2_ERROR_STRING_LIMIT, loc); return V2_NONE; }
    n.kind = STRING; n.location = v2_graph_location_id(u, loc);
    n.extra = new_string(u, length, loc);
    if (n.extra == V2_NONE) return V2_NONE;
    if (length) memcpy(u->strings[n.extra].bytes, bytes, length);
    return append(u, n);
}

v2_id v2_reference(v2_unit *u, v2_id id, v2_location loc)
{
    node n = {0};
    if (!valid_location(u, loc)) return V2_NONE;
    if (id >= u->definition_count) { fail(u, V2_ERROR_REFERENCE, loc); return V2_NONE; }
    n.kind = REFERENCE; n.location = v2_graph_location_id(u, loc); n.left = id;
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
    n.kind = ADDRESS; n.location = v2_graph_location_id(u, loc); n.left = id;
    return append(u, n);
}

v2_id v2_unary(v2_unit *u, v2_operator op, v2_id operand, v2_location loc)
{
    node n = {0};
    if (!valid_location(u, loc)) return V2_NONE;
    if (operand >= u->node_count || op < V2_NEGATE || op > V2_HIGH) {
        fail(u, V2_ERROR_REFERENCE, loc); return V2_NONE;
    }
    n.kind = UNARY; n.location = v2_graph_location_id(u, loc); n.op = (unsigned char)op; n.left = operand;
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
    n.kind = BINARY; n.location = v2_graph_location_id(u, loc); n.op = (unsigned char)op; n.left = left; n.right = right;
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
        fail(u, V2_ERROR_REBIND, v2_graph_location(u, u->anchors[id].location)); return 0;
    }
    u->anchors[id].extent_expression = expression;
    if (!invalidate(u, u->anchors[id].parents)) return 0;
    edge = u->anchors[id].parents;
    while (edge != V2_NONE) {
        reverse_edge e = u->edges[edge];
        if (!link_parent(u, &node_at(u, expression)->parents, e.parent)) return 0;
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
    n.kind = (unsigned char)kind; n.left = left; n.right = right; n.location = v2_graph_location_id(u, loc);
    if (kind == EXTENT_LITERAL) { n.left = (uint32_t)value; n.right = (uint32_t)(value >> 32); }
    else if (kind == EXTENT_COUNT || kind == EXTENT_BOUND) { n.right = min; n.extra = max; }
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
        const node *n = node_at(u, id);
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
        node n = (*node_at(u, i));
        v2_id left, right, replacement;
        if ((reachable && !reachable[i]) || n.kind != BINARY || n.op != V2_SUBTRACT) continue;
        left = address_leaf(u, n.left); right = address_leaf(u, n.right);
        if (left == V2_NONE || right == V2_NONE) continue;
        replacement = lower(context, left, right, v2_graph_location(u, n.location));
        if (replacement == V2_NONE) continue;
        if (replacement >= u->node_count) {
            fail(u, V2_ERROR_REFERENCE, v2_graph_location(u, n.location)); break;
        }
        changed = 1;
        node_at(u, i)->kind = SPAN;
        node_at(u, i)->extra = replacement;
        if (!link_parent(u, &node_at(u, replacement)->parents, (v2_id)i)) break;
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
                if (node_memo(u, node_at(u, i), mode)->flags & MEMO_LAYOUT)
                    node_memo(u, node_at(u, i), mode)->flags &= (unsigned char)~MEMO_CACHED;
            node_at(u, i)->parents = V2_NONE;
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
    reachable = v2_calloc(u->heap, u->node_count, 1);
    todo = v2_malloc(u->heap, u->node_count * sizeof(*todo));
    if (!reachable || !todo) {
        v2_free(u->heap, reachable); v2_free(u->heap, todo);
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
        const node *n = node_at(u, todo[--queued]);
        v2_id children[3];
        size_t child_count = 0, c;
        switch (n->kind) {
        case REFERENCE: children[child_count++] = u->definitions[n->left].expression; break;
        case ADDRESS: children[child_count++] = u->anchors[n->left].extent_expression; break;
        case SPAN: children[child_count++] = n->extra;
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
    v2_free(u->heap, todo); v2_free(u->heap, reachable);
    return result;
}

static evaluated known(int32_t value)
{
    evaluated r = {0};
    r.state = V2_KNOWN;
    r.result.value.kind = V2_INTEGER;
    r.result.value.as.integer = value;
    return r;
}

static evaluated invalid(const v2_unit *u, v2_error code, const node *n)
{
    evaluated r = {0};
    r.state = V2_INVALID;
    r.result.diagnostic = diagnostic(code, v2_graph_location(u, n->location));
    r.diagnostic_location = n->location;
    return r;
}

static evaluated known_extent(uint64_t value)
{
    evaluated r = {0};
    r.state = V2_KNOWN; r.result.value.kind = V2_EXTENT; r.result.value.as.extent = value;
    return r;
}

static evaluated extent_operation(const v2_unit *u, const node *n, v2_value left, v2_value right)
{
    uint64_t a, b;
    if (n->kind == EXTENT_COUNT) {
        if (left.kind != V2_INTEGER) return invalid(u, V2_ERROR_TYPE, n);
        if (left.as.integer < 0 || (uint32_t)left.as.integer < n->right ||
            (uint32_t)left.as.integer > n->extra)
            return invalid(u, V2_ERROR_EXTENT, n);
        return known_extent((uint32_t)left.as.integer);
    }
    if (n->kind == EXTENT_STRING) {
        if (left.kind == V2_INTEGER) return known_extent(1);
        if (left.kind != V2_STRING) return invalid(u, V2_ERROR_TYPE, n);
        return known_extent(left.as.string.length);
    }
    if (left.kind != V2_EXTENT || (binary_kind(n->kind) && right.kind != V2_EXTENT))
        return invalid(u, V2_ERROR_TYPE, n);
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
        if (a < n->right || a > n->extra) break;
        return known_extent(a);
    case EXTENT_INTEGER: return known(expr_i32((uint32_t)a));
    default: return invalid(u, V2_ERROR_TYPE, n);
    }
    return invalid(u, V2_ERROR_EXTENT, n);
}

static int current(const v2_unit *u, const node *n)
{
    const memo *m = node_memo(u, n, u->extent_mode);
    return (m->flags & MEMO_CACHED) &&
        /* A cycle's first repeated edge depends on the requested root. */
        (m->state == V2_INVALID && m->error == V2_ERROR_CYCLE
            ? m->payload.epoch == u->request
            : (!(m->flags & MEMO_BINDING) || m->payload.epoch == u->bindings));
}

static evaluated memo_value(const v2_unit *u, const memo *m)
{
    evaluated v = {0};
    v.state = (v2_state)m->state;
    v.binding_dependent = (m->flags & MEMO_BINDING) != 0;
    v.layout_dependent = (m->flags & MEMO_LAYOUT) != 0;
    if (v.state == V2_INVALID) {
        v.diagnostic_location = m->detail;
        v.result.diagnostic = diagnostic((v2_error)m->error, v2_graph_location(u, m->detail));
    } else if (v.state == V2_KNOWN) {
        v.result.value.kind = (v2_value_kind)m->kind;
        if (m->kind == V2_INTEGER) v.result.value.as.integer = m->payload.integer;
        else if (m->kind == V2_EXTENT) v.result.value.as.extent = m->payload.extent;
        else { v.result.value.as.string.bytes = m->payload.bytes; v.result.value.as.string.length = m->detail; }
    }
    return v;
}

static void finish(v2_unit *u, node *n, evaluated value)
{
    memo *m = new_memo(u, n, u->extent_mode);
    if (!m) return;
    ++u->work.evaluated_nodes;
    m->state = (unsigned char)value.state;
    m->flags = (unsigned char)(MEMO_CACHED | (value.binding_dependent ? MEMO_BINDING : 0) |
                              (value.layout_dependent ? MEMO_LAYOUT : 0));
    m->payload.epoch = value.state == V2_INVALID && value.result.diagnostic.code == V2_ERROR_CYCLE
        ? u->request : u->bindings;
    if (value.state == V2_INVALID) {
        m->error = (unsigned char)value.result.diagnostic.code; m->detail = value.diagnostic_location;
    } else if (value.state == V2_KNOWN) {
        const v2_value *v = &value.result.value;
        m->kind = (unsigned char)v->kind;
        if (v->kind == V2_INTEGER) m->payload.integer = v->as.integer;
        else if (v->kind == V2_EXTENT) m->payload.extent = v->as.extent;
        else { m->payload.bytes = v->as.string.bytes; m->detail = (uint32_t)v->as.string.length; }
    }
    /* Constants can share a result across modes, but never a layout-dependent
     * closure, an unresolved binding, or a root-dependent cycle diagnostic. */
    if (!value.layout_dependent && !value.binding_dependent &&
        !(value.state == V2_INVALID && value.result.diagnostic.code == V2_ERROR_CYCLE))
        if (n->memos[!u->extent_mode] != V2_NONE) *node_memo(u, n, !u->extent_mode) = *m;
    n->visit = 0;
    --u->stack_count;
}

static int span_redirect(const v2_unit *u, const node *n)
{ return n->kind == SPAN && u->extent_mode; }
static v2_id first_child(const v2_unit *u, const node *n)
{
    if (n->kind == REFERENCE) return u->definitions[n->left].expression;
    if (n->kind == ADDRESS) return u->anchors[n->left].extent_expression;
    return span_redirect(u, n) ? n->extra : n->left;
}

static int push(v2_unit *u, v2_id id)
{
    frame f = { id, 0 };
    u->stack = grow(u, u->stack, &u->stack_capacity, u->stack_count + 1,
                    sizeof(f), v2_graph_location(u, node_at(u, id)->location));
    if (u->error.code != V2_ERROR_NONE) return 0;
    u->stack[u->stack_count++] = f;
    return 1;
}

static evaluated integer_operation(const v2_unit *u, const node *n, int32_t lhs, int32_t rhs)
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
        if (!rhs) return invalid(u, V2_ERROR_DIV_ZERO, n);
        result = expr_div(lhs, rhs); break;
    case V2_REMAINDER:
        if (!rhs) return invalid(u, V2_ERROR_MOD_ZERO, n);
        result = expr_mod(lhs, rhs); break;
    case V2_SHIFT_LEFT:
        if (!expr_shl(lhs, rhs, &result)) return invalid(u, V2_ERROR_SHIFT, n);
        break;
    case V2_SHIFT_RIGHT:
        if (!expr_shr(lhs, rhs, &result)) return invalid(u, V2_ERROR_SHIFT, n);
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
        return invalid(u, V2_ERROR_TYPE, n);
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
            r.state = V2_KNOWN; r.result.value.kind = V2_STRING;
            r.result.value.as.string.bytes = u->strings[n->extra].bytes;
            r.result.value.as.string.length = u->strings[n->extra].length;
            return r;
        }
        if (left_length > V2_MAX_STRING_BYTES - right_length)
            return invalid(u, V2_ERROR_STRING_LIMIT, n);
        length = left_length + right_length;
        if (!fits_string(u, length)) return invalid(u, V2_ERROR_STRING_LIMIT, n);
        n->extra = new_string(u, length, v2_graph_location(u, n->location));
        if (n->extra == V2_NONE) return invalid(u, V2_ERROR_MEMORY, n);
        if (left_length) memcpy(u->strings[n->extra].bytes, left, left_length);
        if (right_length) memcpy(u->strings[n->extra].bytes + left_length, right, right_length);
        n->has_concatenation = 1;
        r.state = V2_KNOWN; r.result.value.kind = V2_STRING;
        r.result.value.as.string.bytes = u->strings[n->extra].bytes;
        r.result.value.as.string.length = length;
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
    default: return invalid(u, V2_ERROR_TYPE, n);
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
        r = extent_operation(u, n, left.result.value, right.result.value);
    else if (left.result.value.kind == V2_EXTENT || right.result.value.kind == V2_EXTENT)
        r = invalid(u, V2_ERROR_TYPE, n);
    else if (left.result.value.kind == V2_INTEGER && right.result.value.kind == V2_INTEGER)
        r = integer_operation(u, n, left.result.value.as.integer, right.result.value.as.integer);
    else if (n->kind == UNARY) r = invalid(u, V2_ERROR_TYPE, n);
    else if (left.layout_dependent || right.layout_dependent)
        r = invalid(u, V2_ERROR_LAYOUT_STRING, n);
    else r = string_operation(u, n, left.result.value, right.result.value);
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
    int ok = 0;
    size_t i;
    u->dirty_count = 0;
    if (!push(u, root)) goto done;
    while (u->stack_count) {
        ++u->work.dependency_visits;
        v2_id id = u->stack[--u->stack_count].id;
        node *n = node_at(u, id);
        if (n->visit || node_memo(u, n, u->extent_mode)->state != V2_DEFERRED) continue;
        /* Evaluation has finished, so no active DFS mark remains. Remember
         * each node we mark here in the existing invalidation workspace and
         * clear it before returning, including allocation-error paths. */
        u->dirty = grow(u, u->dirty, &u->dirty_capacity, u->dirty_count + 1,
                        sizeof(*u->dirty), v2_graph_location(u, n->location));
        if (u->error.code) goto done;
        u->dirty[u->dirty_count++] = id;
        n->visit = 1;
        switch (n->kind) {
        case REFERENCE: {
            definition *d = &u->definitions[n->left];
            if (d->expression != V2_NONE) {
                if (!push(u, d->expression)) goto done;
            } else if (!d->visited) {
                if (!add_dependency(u, V2_DEP_DEFINITION, n->left, v2_graph_location(u, n->location))) goto done;
                d->visited = 1;
            }
            break;
        }
        case ADDRESS:
            if (u->extent_mode && u->anchors[n->left].extent_expression != V2_NONE) {
                if (!push(u, u->anchors[n->left].extent_expression)) goto done;
                break;
            }
            if (!u->anchors[n->left].visited) {
                if (!add_dependency(u, V2_DEP_ANCHOR, n->left, v2_graph_location(u, n->location))) goto done;
                u->anchors[n->left].visited = 1;
            }
            break;
        case SPAN:
            if (span_redirect(u, n)) {
                if (!push(u, n->extra)) goto done;
                break;
            }
            /* fall through */
        case BINARY: case EXTENT_SUM: case EXTENT_REPEAT: case EXTENT_REMAINDER:
            if (!push(u, n->right)) goto done;
            /* fall through */
        case UNARY: case EXTENT_COUNT: case EXTENT_BOUND: case EXTENT_STRING:
        case EXTENT_INTEGER:
            if (!push(u, n->left)) goto done;
            break;
        default: break;
        }
    }
    ok = 1;
done:
    for (i = 0; i < u->dirty_count; ++i) node_at(u, u->dirty[i])->visit = 0;
    u->dirty_count = 0;
    for (i = 0; i < u->dependency_count; ++i) {
        v2_dependency d = u->dependencies[i];
        if (d.kind == V2_DEP_DEFINITION) u->definitions[d.id].visited = 0;
        else u->anchors[d.id].visited = 0;
    }
    return ok;
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
        node *n = node_at(u, f->id);
        evaluated value = {0};
        v2_id child;
        if (current(u, n)) { --u->stack_count; continue; }
        if (f->phase == 0) {
            n->visit = 1;
            if (n->kind == EXTENT_LITERAL) { finish(u, n, known_extent(((uint64_t)n->left | ((uint64_t)n->right << 32)))); continue; }
            if (n->kind == INTEGER) { finish(u, n, known(expr_i32(n->left))); continue; }
            if (n->kind == STRING) {
                value.state = V2_KNOWN; value.result.value.kind = V2_STRING;
                value.result.value.as.string.bytes = u->strings[n->extra].bytes;
                value.result.value.as.string.length = u->strings[n->extra].length;
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
            evaluated left = memo_value(u, node_memo(u, node_at(u, first_child(u, n)), u->extent_mode));
            if (n->kind == ADDRESS || span_redirect(u, n)) {
                if (left.state == V2_KNOWN && left.result.value.kind != V2_INTEGER)
                    left = invalid(u, V2_ERROR_TYPE, n);
                left.layout_dependent = 1;
                finish(u, n, left); continue;
            }
            if (n->kind == REFERENCE || left.state == V2_INVALID) {
                finish(u, n, left); continue;
            }
            if (n->kind == EXTENT_REPEAT && left.state == V2_KNOWN &&
                left.result.value.kind == V2_EXTENT && left.result.value.as.extent == 0) {
                finish(u, n, left); continue;
            }
            if (!binary_kind(n->kind)) {
                finish(u, n, operation(u, n, left, known(0))); continue;
            }
            if (f->phase == 2) {
                finish(u, n, operation(u, n, left, memo_value(u, node_memo(u, node_at(u, n->right), u->extent_mode))));
                if (u->error.code != V2_ERROR_NONE) goto failed;
                continue;
            }
            child = n->right;
            f->phase = 2;
        }
        if (current(u, node_at(u, child))) continue;
        if (node_at(u, child)->visit) {
            value = invalid(u, V2_ERROR_CYCLE, node_at(u, child));
            value.layout_dependent = u->extent_mode;
            if (binary_kind(n->kind) && f->phase == 2) {
                value.binding_dependent = (node_memo(u, node_at(u, n->left), u->extent_mode)->flags & MEMO_BINDING) != 0;
                value.layout_dependent |= (node_memo(u, node_at(u, n->left), u->extent_mode)->flags & MEMO_LAYOUT) != 0;
            }
            finish(u, n, value);
            continue;
        }
        /* Growing the stack may invalidate f; do not retain it across push. */
        if (!push(u, child)) goto failed;
    }
    if (u->error.code) goto failed;
    result.state = (v2_state)node_memo(u, node_at(u, root), u->extent_mode)->state;
    if (result.state == V2_KNOWN) result.value = memo_value(u, node_memo(u, node_at(u, root), u->extent_mode)).result.value;
    else if (result.state == V2_INVALID) result.diagnostic = memo_value(u, node_memo(u, node_at(u, root), u->extent_mode)).result.diagnostic;
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

v2_unit *v2_unit_create(void) { return v2_unit_create_heap(NULL); }
v2_heap *v2_graph_heap(v2_unit *u) { return u->heap; }
