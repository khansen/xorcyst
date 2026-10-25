#include "program.h"
#include "../opcode.h"
#include <stdlib.h>
#include <string.h>

typedef struct
{
    uint32_t kind, owner, position;
    v2_id alias;
    v2_location loc;
} anchor;
typedef struct
{
    v2_id expression, target;
} bank;
typedef struct
{
    v2_location location;
    v2_id stored;
} location_entry;
typedef struct
{
    v2_object_table table;
    v2_id native, wire;
} task;
struct v2_program
{
    v2_unit *u;
    v2_object *o;
    anchor *anchors;
    size_t anchor_count, anchor_capacity;
    bank *banks;
    size_t bank_count, bank_capacity;
    task *tasks;
    size_t task_count, task_capacity;
    v2_id *nodes, *definitions, *anchor_ids, *file_ids;
    v2_id *reference_nodes, *address_nodes;
    size_t node_count, definition_count, file_count;
    v2_id ram, ram_anchor, ram_extent;
    location_entry *locations;
    size_t location_count, location_capacity;
    const char *error;
    int finished;
};
static int bad(v2_program *p, const char *message)
{
    if (!p->error)
        p->error = message;
    return 0;
}
static int reserve(v2_program *p, void **a, size_t *capacity, size_t n, size_t width)
{
    size_t cap = *capacity ? *capacity : 16;
    void *q;
    if (p->error)
        return 0;
    if (n <= *capacity)
        return 1;
    if (n > UINT32_MAX || n > SIZE_MAX / width)
        return bad(p, "object table too large");
    while (cap < n)
    {
        if (cap > SIZE_MAX / width / 2)
        {
            cap = n;
            break;
        }
        cap *= 2;
    }
    q = realloc(*a, cap * width);
    if (!q)
        return bad(p, "out of memory building object");
    *a = q;
    *capacity = cap;
    return 1;
}
v2_program *v2_program_create(v2_unit *u)
{
    v2_program *p = calloc(1, sizeof(*p));
    if (!p)
        return NULL;
    p->u = u;
    p->o = v2_object_create();
    p->ram = p->ram_anchor = p->ram_extent = V2_NONE;
    if (!p->o)
    {
        free(p);
        return NULL;
    }
    return p;
}
void v2_program_destroy(v2_program *p)
{
    if (!p)
        return;
    v2_object_destroy(p->o);
    free(p->anchors);
    free(p->banks);
    free(p->tasks);
    free(p->nodes);
    free(p->definitions);
    free(p->anchor_ids);
    free(p->reference_nodes);
    free(p->address_nodes);
    free(p->file_ids);
    free(p->locations);
    free(p);
}
const char *v2_program_error(const v2_program *p)
{
    return p->error ? p->error
           : p->o   ? v2_object_error_message(v2_object_error_at(p->o))
                    : "no error";
}
static uint32_t location_hash(v2_location l)
{
    uint32_t h = 2166136261U;
    h = (h ^ l.file) * 16777619U;
    h = (h ^ l.line) * 16777619U;
    h = (h ^ l.column) * 16777619U;
    h = (h ^ l.end_line) * 16777619U;
    return (h ^ l.end_column) * 16777619U;
}
static int equal_location(v2_location a, v2_location b)
{
    return a.file == b.file && a.line == b.line && a.column == b.column &&
           a.end_line == b.end_line && a.end_column == b.end_column;
}
static size_t location_slot(const v2_program *p, v2_location loc)
{
    size_t i = location_hash(loc) & (p->location_capacity - 1);
    while (p->locations[i].stored && !equal_location(p->locations[i].location, loc))
        i = (i + 1) & (p->location_capacity - 1);
    return i;
}
static int grow_locations(v2_program *p)
{
    size_t cap = p->location_capacity ? p->location_capacity * 2 : 16, i;
    location_entry *old = p->locations, *next;
    if (cap < p->location_capacity || cap > SIZE_MAX / sizeof(*next))
        return bad(p, "too many object locations");
    next = calloc(cap, sizeof(*next));
    if (!next)
        return bad(p, "out of memory recording object locations");
    p->locations = next;
    cap = p->location_capacity;
    p->location_capacity = cap ? cap * 2 : 16;
    for (i = 0; i < cap; i++)
        if (old[i].stored)
            p->locations[location_slot(p, old[i].location)] = old[i];
    free(old);
    return 1;
}
static v2_id location_id(v2_program *p, v2_location loc)
{
    uint32_t r[5];
    size_t i, slot;
    v2_id id;
    if (loc.file == V2_NONE)
        return V2_NONE;
    if (!p->location_capacity || p->location_count >= p->location_capacity / 2)
        if (!grow_locations(p))
            return V2_NONE;
    slot = location_slot(p, loc);
    if (p->locations[slot].stored)
        return p->locations[slot].stored - 1;
    if (loc.file >= p->file_count)
    {
        size_t n = (size_t)loc.file + 1;
        v2_id *next;
        if (n > SIZE_MAX / sizeof(*next))
        {
            bad(p, "invalid object location");
            return V2_NONE;
        }
        next = realloc(p->file_ids, n * sizeof(*next));
        if (!next)
        {
            bad(p, "out of memory building object");
            return V2_NONE;
        }
        p->file_ids = next;
        for (i = p->file_count; i < n; i++)
            next[i] = V2_NONE;
        p->file_count = n;
    }
    if (p->file_ids[loc.file] == V2_NONE)
    {
        const char *name = v2_file_name(p->u, loc.file);
        uint32_t f[3] = {0};
        if (!name)
        {
            bad(p, "invalid object location");
            return V2_NONE;
        }
        f[0] = v2_object_blob(p->o, name, strlen(name));
        p->file_ids[loc.file] = v2_object_add(p->o, V2_OBJ_FILES, f);
    }
    r[0] = p->file_ids[loc.file];
    r[1] = loc.line;
    r[2] = loc.column;
    r[3] = loc.end_line;
    r[4] = loc.end_column;
    id = v2_object_add(p->o, V2_OBJ_LOCATIONS, r);
    if (id != V2_NONE)
    {
        p->locations[slot] = (location_entry){loc, id + 1};
        ++p->location_count;
    }
    return id;
}
static void locate(v2_program *p, uint32_t *fields, v2_location loc)
{
    v2_id id = location_id(p, loc);
    fields[0] = id != V2_NONE;
    fields[1] = id == V2_NONE ? 0 : id;
}
static int set_anchor(v2_program *p, v2_id id, uint32_t kind, uint32_t owner, uint32_t position,
                      v2_id alias, v2_location loc)
{
    size_t i;
    if (id == V2_NONE)
        return 1;
    if (id >= v2_graph_anchors(p->u))
        return bad(p, "invalid object anchor");
    if (!reserve(p, (void **)&p->anchors, &p->anchor_capacity, (size_t)id + 1, sizeof(anchor)))
        return 0;
    for (i = p->anchor_count; i <= (size_t)id; i++)
        p->anchors[i] = (anchor){0, 0, 0, V2_NONE, v2_no_location()};
    if (p->anchor_count <= (size_t)id)
        p->anchor_count = (size_t)id + 1;
    if (p->anchors[id].kind || p->anchors[id].alias != V2_NONE)
        return bad(p, "object anchor placed twice");
    p->anchors[id] = (anchor){kind, owner, position, alias, loc};
    return 1;
}
int v2_program_mark(v2_program *p, v2_id a, v2_location loc)
{
    uint32_t count = v2_object_count(p->o, V2_OBJ_FRAGMENTS);
    const uint32_t *last = count ? v2_object_record(p->o, V2_OBJ_FRAGMENTS, count - 1) : NULL;
    if (last && last[0] == 1)
    {
        size_t n;
        v2_object_bytes(p->o, last[1], &n);
        return set_anchor(p, a, 5, count - 1, (uint32_t)n, V2_NONE, loc);
    }
    return set_anchor(p, a, 1, count, 0, V2_NONE, loc);
}
int v2_program_raw(v2_program *p, const void *bytes, size_t n, v2_location loc)
{
    uint32_t r[7] = {1, 0, 0, 0, 0, 0, 0};
    uint32_t count = v2_object_count(p->o, V2_OBJ_FRAGMENTS);
    const uint32_t *last = count ? v2_object_record(p->o, V2_OBJ_FRAGMENTS, count - 1) : NULL;
    if (last && last[0] == 1)
        return v2_object_append_bytes(p->o, last[1], bytes, n);
    r[1] = v2_object_blob(p->o, bytes, n);
    locate(p, r + 5, loc);
    return v2_object_add(p->o, V2_OBJ_FRAGMENTS, r) != V2_NONE;
}
int v2_program_data(v2_program *p, v2_id pc, v2_id expr, unsigned width, const unsigned char *map,
                    v2_location loc)
{
    uint32_t f[6] = {expr, width, 0, 0, 0, 0}, r[7] = {2, 0, 1, 0, 0, 0, 0};
    if (map)
    {
        f[2] = 1;
        f[3] = v2_object_blob(p->o, map, 256);
    }
    locate(p, f + 4, loc);
    r[1] = v2_object_add(p->o, V2_OBJ_FIXUPS, f);
    locate(p, r + 5, loc);
    set_anchor(p, pc, 4, r[1], 0, V2_NONE, loc);
    return v2_object_add(p->o, V2_OBJ_FRAGMENTS, r) != V2_NONE;
}
int v2_program_instruction(v2_program *p, v2_id pc, unsigned char opcode, v2_id expr, int wide,
                           v2_location loc)
{
    unsigned char narrow = wide ? 0xff : opcode_zp_equiv(opcode, opcode_addressing_mode(opcode));
    uint32_t r[7] = {3, opcode, narrow != 0xff, narrow == 0xff ? 0 : narrow, 0, 0, 0};
    if (opcode_length(opcode) > 1)
    {
        uint32_t f[6] = {expr, 0, 0, 0, 0, 0};
        locate(p, f + 4, loc);
        r[4] = v2_object_add(p->o, V2_OBJ_FIXUPS, f);
    }
    v2_program_mark(p, pc, loc);
    locate(p, r + 5, loc);
    return v2_object_add(p->o, V2_OBJ_FRAGMENTS, r) != V2_NONE;
}
int v2_program_storage(v2_program *p, v2_id pc, v2_id extent, v2_location loc)
{
    uint32_t r[7] = {4, extent, 0, 0, 0, 0, 0};
    v2_program_mark(p, pc, loc);
    locate(p, r + 5, loc);
    return v2_object_add(p->o, V2_OBJ_FRAGMENTS, r) != V2_NONE;
}
static void finish_ram(v2_program *p)
{
    uint32_t r[5];
    const uint32_t *old;
    if (p->ram == V2_NONE)
        return;
    old = v2_object_record(p->o, V2_OBJ_RAM, p->ram);
    if (!old)
        return;
    memcpy(r, old, sizeof(r));
    r[0] = p->ram_extent;
    v2_object_set(p->o, V2_OBJ_RAM, p->ram, r);
}
int v2_program_ram_label(v2_program *p, v2_id a, unsigned align, int zp, v2_location loc)
{
    uint32_t r[5] = {0, align, zp != 0, 0, 0};
    finish_ram(p);
    p->ram_extent = v2_extent_literal(p->u, 0, loc);
    r[0] = p->ram_extent;
    locate(p, r + 3, loc);
    p->ram = v2_object_add(p->o, V2_OBJ_RAM, r);
    p->ram_anchor = a;
    return set_anchor(p, a, 3, p->ram, 0, V2_NONE, loc);
}
int v2_program_ram_storage(v2_program *p, v2_id pc, v2_id extent, v2_location loc)
{
    if (p->ram == V2_NONE)
    {
        /* Unnamed leading RAM bytes have no allocation identity in v1. Their
         * count (including a use of $) does not participate in placement. */
        return 1;
    }
    if (pc != V2_NONE)
    {
        v2_id base = v2_address(p->u, p->ram_anchor, loc);
        v2_id offset = v2_extent_integer(p->u, p->ram_extent, loc);
        v2_id expr = v2_binary(p->u, V2_ADD, base, offset, loc);
        if (!set_anchor(p, pc, 0, 0, 0, expr, loc))
            return 0;
    }
    p->ram_extent = v2_extent_sum(p->u, p->ram_extent, extent, loc);
    return 1;
}
int v2_program_export(v2_program *p, const char *name, v2_id expression, v2_location loc)
{
    uint32_t r[6] = {0, 1, 1, expression, 0, 0};
    r[0] = v2_object_blob(p->o, name, strlen(name));
    locate(p, r + 4, loc);
    return v2_object_add(p->o, V2_OBJ_DEFINITIONS, r) != V2_NONE;
}
/* RAM initializers do not allocate or emit bytes in v1 object mode, but
 * their referenced imports must still bind. Keep a non-exported root. */
int v2_program_require(v2_program *p, v2_id expression, v2_location loc)
{
    uint32_t r[6] = {0, 1, 0, expression, 0, 0};
    r[0] = v2_object_blob(p->o, "RAM operand", 11);
    locate(p, r + 4, loc);
    return v2_object_add(p->o, V2_OBJ_DEFINITIONS, r) != V2_NONE;
}
v2_id v2_program_bank(v2_program *p, v2_id operand, v2_location loc)
{
    v2_graph_node n;
    v2_id target, id, result;
    if (!v2_graph_read(p->u, operand, &n))
        return V2_NONE;
    if (n.kind == V2_GRAPH_REFERENCE)
        target = n.left;
    else
    {
        target = v2_definition(p->u, "bank owner", loc);
        v2_bind(p->u, target, operand);
    }
    id = v2_definition(p->u, "bank result", loc);
    result = v2_reference(p->u, id, loc);
    if (!reserve(p, (void **)&p->banks, &p->bank_capacity, p->bank_count + 1, sizeof(bank)))
        return V2_NONE;
    p->banks[p->bank_count++] = (bank){result, target};
    return result;
}
static v2_id request(v2_program *p, v2_object_table t, v2_id native)
{
    v2_id *map;
    size_t count;
    uint32_t zero[9] = {0};
    v2_id id;
    if (t == V2_OBJ_DEFINITIONS)
    {
        map = p->definitions;
        count = p->definition_count;
    }
    else if (t == V2_OBJ_ANCHORS)
    {
        map = p->anchor_ids;
        count = p->anchor_count;
    }
    else
    {
        map = p->nodes;
        count = p->node_count;
    }
    if (native >= count)
    {
        bad(p, "unplaced or invalid object graph reference");
        return V2_NONE;
    }
    if (map[native] != V2_NONE)
        return map[native];
    id = v2_object_add(p->o, t, zero);
    map[native] = id;
    if (!reserve(p, (void **)&p->tasks, &p->task_capacity, p->task_count + 1, sizeof(task)))
        return V2_NONE;
    p->tasks[p->task_count++] = (task){t, native, id};
    return id;
}
static v2_id extent_id(v2_program *p, v2_id id)
{
    size_t limit = p->node_count;
    v2_graph_node n;
    while (limit-- && v2_graph_read(p->u, id, &n))
    {
        if (n.kind != V2_GRAPH_REFERENCE)
        {
            if (n.kind < V2_GRAPH_EXTENT_LITERAL || n.kind > V2_GRAPH_STRING_LENGTH)
                break;
            return request(p, V2_OBJ_EXTENTS, id);
        }
        if (!v2_graph_definition(p->u, n.left, &id, NULL))
            break;
    }
    bad(p, "unresolved object extent");
    return V2_NONE;
}
static v2_id bank_target(const v2_program *, v2_id);
static v2_id node_id(v2_program *p, v2_id id)
{
    v2_graph_node n;
    v2_id *shared = NULL, wire;
    if (!v2_graph_read(p->u, id, &n))
    {
        bad(p, "invalid object expression");
        return V2_NONE;
    }
    /* These leaves are identified entirely by their definition/anchor. The
     * linker takes their location from that owner; fixups retain use spans.
     * Bank expressions and RAM-position wrappers keep their own locations. */
    if (n.kind == V2_GRAPH_REFERENCE && n.left < p->definition_count &&
        bank_target(p, id) == V2_NONE)
        shared = p->reference_nodes + n.left;
    else if (n.kind == V2_GRAPH_ADDRESS && n.left < p->anchor_count &&
             p->anchors[n.left].alias == V2_NONE)
        shared = p->address_nodes + n.left;
    if (shared && *shared != V2_NONE)
        return *shared;
    wire = request(p,
                   n.kind >= V2_GRAPH_EXTENT_LITERAL && n.kind <= V2_GRAPH_STRING_LENGTH
                       ? V2_OBJ_EXTENTS
                       : V2_OBJ_EXPRESSIONS,
                   id);
    if (shared)
        *shared = wire;
    return wire;
}
static v2_id *ids(size_t n)
{
    v2_id *p;
    size_t i;
    if (!n)
        return NULL;
    if (n > SIZE_MAX / sizeof(*p))
        return NULL;
    p = malloc(n * sizeof(*p));
    if (p)
        for (i = 0; i < n; i++)
            p[i] = V2_NONE;
    return p;
}
/* References are created in increasing node-ID order. Search the existing
 * sorted bank list without adding a second index or scanning it per operand. */
static v2_id bank_target(const v2_program *p, v2_id expression)
{
    size_t lo = 0, hi = p->bank_count;
    while (lo < hi)
    {
        size_t mid = lo + (hi - lo) / 2;
        if (p->banks[mid].expression < expression)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo < p->bank_count && p->banks[lo].expression == expression
               ? p->banks[lo].target : V2_NONE;
}
v2_object *v2_program_object(v2_program *p)
{
    static const uint32_t ops[] = {16, 17, 18, 19, 20, 32, 33, 34, 35, 36, 37,
                                   38, 39, 40, 41, 44, 45, 46, 47, 48, 49};
    size_t i;
    v2_object *out;
    if (p->finished || p->error)
        return NULL;
    p->finished = 1;
    finish_ram(p);
    p->node_count = v2_graph_nodes(p->u);
    p->definition_count = v2_graph_definitions(p->u);
    p->nodes = ids(p->node_count);
    p->definitions = ids(p->definition_count);
    p->anchor_ids = ids(p->anchor_count);
    p->reference_nodes = ids(p->definition_count);
    p->address_nodes = ids(p->anchor_count);
    if ((p->node_count && !p->nodes) || (p->definition_count && !p->definitions) ||
        (p->anchor_count && !p->anchor_ids) ||
        (p->definition_count && !p->reference_nodes) ||
        (p->anchor_count && !p->address_nodes))
    {
        bad(p, "out of memory building object");
        return NULL;
    }
    /* Only reachable graph nodes are serialized. Captured but unused aliases
     * need not carry retired/unplaced anchors into a new object. */
    for (i = 0; i < v2_object_count(p->o, V2_OBJ_DEFINITIONS); i++)
    {
        uint32_t r[6];
        memcpy(r, v2_object_record(p->o, V2_OBJ_DEFINITIONS, (v2_id)i), sizeof(r));
        r[3] = node_id(p, r[3]);
        v2_object_set(p->o, V2_OBJ_DEFINITIONS, (v2_id)i, r);
    }
    for (i = 0; i < v2_object_count(p->o, V2_OBJ_FIXUPS); i++)
    {
        uint32_t r[6];
        memcpy(r, v2_object_record(p->o, V2_OBJ_FIXUPS, (v2_id)i), sizeof(r));
        r[0] = node_id(p, r[0]);
        v2_object_set(p->o, V2_OBJ_FIXUPS, (v2_id)i, r);
    }
    for (i = 0; i < v2_object_count(p->o, V2_OBJ_FRAGMENTS); i++)
    {
        uint32_t r[7];
        memcpy(r, v2_object_record(p->o, V2_OBJ_FRAGMENTS, (v2_id)i), sizeof(r));
        if (r[0] == 4)
        {
            r[1] = node_id(p, r[1]);
            v2_object_set(p->o, V2_OBJ_FRAGMENTS, (v2_id)i, r);
        }
    }
    for (i = 0; i < v2_object_count(p->o, V2_OBJ_RAM); i++)
    {
        uint32_t r[5];
        memcpy(r, v2_object_record(p->o, V2_OBJ_RAM, (v2_id)i), sizeof(r));
        r[0] = node_id(p, r[0]);
        v2_object_set(p->o, V2_OBJ_RAM, (v2_id)i, r);
    }
    for (i = 0; i < p->task_count && !p->error; i++)
    {
        task task = p->tasks[i];
        uint32_t r[9] = {0};
        v2_location loc = v2_no_location();
        if (task.table == V2_OBJ_ANCHORS)
        {
            anchor a = p->anchors[task.native];
            if (!a.kind)
            {
                bad(p, "unplaced object anchor");
                break;
            }
            if (a.kind == 1 && a.owner == v2_object_count(p->o, V2_OBJ_FRAGMENTS))
            {
                a.kind = 2;
                a.owner = 0;
            }
            r[0] = a.kind;
            r[1] = a.owner;
            r[2] = a.position;
            locate(p, r + 3, a.loc);
        }
        else if (task.table == V2_OBJ_DEFINITIONS)
        {
            v2_id expr;
            const char *name = v2_definition_name(p->u, task.native);
            if (!name || !v2_graph_definition(p->u, task.native, &expr, &loc))
            {
                bad(p, "invalid object definition");
                break;
            }
            r[0] = v2_object_blob(p->o, name, strlen(name));
            r[1] = expr == V2_NONE ? 3 : 1;
            if (expr != V2_NONE)
                r[3] = node_id(p, expr);
            locate(p, r + 4, loc);
        }
        else
        {
            v2_graph_node n;
            v2_graph_read(p->u, task.native, &n);
            loc = n.location;
            if (task.table == V2_OBJ_EXPRESSIONS)
            {
                switch (n.kind)
                {
                case V2_GRAPH_INTEGER:
                    r[0] = 1;
                    r[1] = (uint32_t)n.integer;
                    break;
                case V2_GRAPH_STRING:
                    r[0] = 2;
                    r[1] = v2_object_blob(p->o, n.bytes, n.length);
                    break;
                case V2_GRAPH_REFERENCE:
                {
                    v2_id target = bank_target(p, task.native);
                    r[0] = 3;
                    if (target != V2_NONE)
                    {
                        r[0] = 5;
                        n.left = target;
                    }
                    r[1] = request(p, V2_OBJ_DEFINITIONS, n.left);
                    break;
                }
                case V2_GRAPH_ADDRESS:
                    if (n.left < p->anchor_count && p->anchors[n.left].alias != V2_NONE)
                    {
                        uint32_t d[6] = {0, 1, 0, 0, 0, 0};
                        d[0] = v2_object_blob(p->o, "RAM position", 12);
                        d[3] = node_id(p, p->anchors[n.left].alias);
                        locate(p, d + 4, loc);
                        r[0] = 3;
                        r[1] = v2_object_add(p->o, V2_OBJ_DEFINITIONS, d);
                    }
                    else
                    {
                        r[0] = 4;
                        r[1] = request(p, V2_OBJ_ANCHORS, n.left);
                    }
                    break;
                case V2_GRAPH_UNARY:
                case V2_GRAPH_BINARY:
                    r[0] = ops[n.op];
                    r[1] = node_id(p, n.left);
                    if (n.kind == V2_GRAPH_BINARY)
                        r[2] = node_id(p, n.right);
                    break;
                case V2_GRAPH_EXTENT_INTEGER:
                    r[0] = 6;
                    r[1] = extent_id(p, n.left);
                    break;
                default:
                    bad(p, "invalid expression kind in object");
                    break;
                }
                if (r[0] != 3 && r[0] != 4)
                    locate(p, r + 4, loc);
            }
            else
            {
                /* The wire retains deferred checks, but already validated
                 * extents need only their result. In particular RAM labels
                 * with known counts do not carry an arithmetic tree to link. */
                v2_result fixed = v2_evaluate_extent(p->u, task.native);
                if (v2_unit_error(p->u).code)
                {
                    bad(p, v2_error_message(v2_unit_error(p->u).code));
                    break;
                }
                if (fixed.state == V2_KNOWN && fixed.value.kind == V2_EXTENT &&
                    fixed.value.as.extent <= UINT32_MAX)
                {
                    r[0] = 1;
                    r[1] = (uint32_t)fixed.value.as.extent;
                    locate(p, r + 7, loc);
                    v2_object_set(p->o, task.table, task.wire, r);
                    continue;
                }
                switch (n.kind)
                {
                case V2_GRAPH_EXTENT_LITERAL:
                    r[0] = 1;
                    if (n.extent > UINT32_MAX)
                        bad(p, "object extent exceeds wire range");
                    r[1] = (uint32_t)n.extent;
                    break;
                case V2_GRAPH_COUNT:
                    r[0] = 2;
                    r[1] = node_id(p, n.left);
                    r[2] = n.minimum;
                    r[3] = n.maximum;
                    break;
                case V2_GRAPH_SUM:
                case V2_GRAPH_REPEAT:
                case V2_GRAPH_REMAINDER:
                    r[0] = n.kind == V2_GRAPH_SUM ? 3 : n.kind == V2_GRAPH_REPEAT ? 4 : 5;
                    r[1] = extent_id(p, n.left);
                    r[2] = extent_id(p, n.right);
                    break;
                case V2_GRAPH_BOUND:
                    r[0] = 6;
                    r[1] = extent_id(p, n.left);
                    r[2] = n.minimum;
                    r[3] = n.maximum;
                    break;
                case V2_GRAPH_STRING_LENGTH:
                    r[0] = 8;
                    r[1] = node_id(p, n.left);
                    break;
                default:
                    bad(p, "invalid extent kind in object");
                    break;
                }
                locate(p, r + 7, loc);
            }
        }
        v2_object_set(p->o, task.table, task.wire, r);
    }
    if (p->error || !v2_object_validate(p->o))
        return NULL;
    out = p->o;
    p->o = NULL;
    return out;
}
