#include "program.h"
#include "../opcode.h"
#include <stdlib.h>
#include <string.h>

typedef struct
{
    uint32_t kind, owner, position;
    ir_id alias;
    ir_location loc;
} anchor;
typedef struct
{
    ir_id expression, target;
} bank;
typedef struct
{
    ir_location location;
    ir_id stored;
} location_entry;
typedef struct
{
    ir_object_table table;
    ir_id native, wire;
} task;
struct ir_program
{
    ir_unit *u;
    ir_object *o;
    anchor *anchors;
    size_t anchor_count, anchor_capacity;
    bank *banks;
    size_t bank_count, bank_capacity;
    task *tasks;
    size_t task_count, task_capacity;
    ir_id *nodes, *definitions, *anchor_ids, *file_ids;
    ir_id *reference_nodes, *address_nodes;
    size_t node_count, definition_count, file_count;
    ir_id ram, ram_anchor, ram_extent;
    location_entry *locations;
    size_t location_count, location_capacity;
    const char *error;
    int finished;
};
static int bad(ir_program *p, const char *message)
{
    if (!p->error)
        p->error = message;
    return 0;
}
static int reserve(ir_program *p, void **a, size_t *capacity, size_t n, size_t width)
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
ir_program *ir_program_create(ir_unit *u)
{
    ir_program *p = calloc(1, sizeof(*p));
    if (!p)
        return NULL;
    p->u = u;
    p->o = ir_object_create();
    p->ram = p->ram_anchor = p->ram_extent = IR_NONE;
    if (!p->o)
    {
        free(p);
        return NULL;
    }
    return p;
}
void ir_program_destroy(ir_program *p)
{
    if (!p)
        return;
    ir_object_destroy(p->o);
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
const char *ir_program_error(const ir_program *p)
{
    return p->error ? p->error
           : p->o   ? ir_object_error_message(ir_object_error_at(p->o))
                    : "no error";
}
static uint32_t location_hash(ir_location l)
{
    uint32_t h = 2166136261U;
    h = (h ^ l.file) * 16777619U;
    h = (h ^ l.line) * 16777619U;
    h = (h ^ l.column) * 16777619U;
    h = (h ^ l.end_line) * 16777619U;
    return (h ^ l.end_column) * 16777619U;
}
static int equal_location(ir_location a, ir_location b)
{
    return a.file == b.file && a.line == b.line && a.column == b.column &&
           a.end_line == b.end_line && a.end_column == b.end_column;
}
static size_t location_slot(const ir_program *p, ir_location loc)
{
    size_t i = location_hash(loc) & (p->location_capacity - 1);
    while (p->locations[i].stored && !equal_location(p->locations[i].location, loc))
        i = (i + 1) & (p->location_capacity - 1);
    return i;
}
static int grow_locations(ir_program *p)
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
static ir_id location_id(ir_program *p, ir_location loc)
{
    uint32_t r[5];
    size_t i, slot;
    ir_id id;
    if (loc.file == IR_NONE)
        return IR_NONE;
    if (!p->location_capacity || p->location_count >= p->location_capacity / 2)
        if (!grow_locations(p))
            return IR_NONE;
    slot = location_slot(p, loc);
    if (p->locations[slot].stored)
        return p->locations[slot].stored - 1;
    if (loc.file >= p->file_count)
    {
        size_t n = (size_t)loc.file + 1;
        ir_id *next;
        if (n > SIZE_MAX / sizeof(*next))
        {
            bad(p, "invalid object location");
            return IR_NONE;
        }
        next = realloc(p->file_ids, n * sizeof(*next));
        if (!next)
        {
            bad(p, "out of memory building object");
            return IR_NONE;
        }
        p->file_ids = next;
        for (i = p->file_count; i < n; i++)
            next[i] = IR_NONE;
        p->file_count = n;
    }
    if (p->file_ids[loc.file] == IR_NONE)
    {
        const char *name = ir_file_name(p->u, loc.file);
        uint32_t f[3] = {0};
        if (!name)
        {
            bad(p, "invalid object location");
            return IR_NONE;
        }
        f[0] = ir_object_blob(p->o, name, strlen(name));
        p->file_ids[loc.file] = ir_object_add(p->o, IR_OBJ_FILES, f);
    }
    r[0] = p->file_ids[loc.file];
    r[1] = loc.line;
    r[2] = loc.column;
    r[3] = loc.end_line;
    r[4] = loc.end_column;
    id = ir_object_add(p->o, IR_OBJ_LOCATIONS, r);
    if (id != IR_NONE)
    {
        p->locations[slot] = (location_entry){loc, id + 1};
        ++p->location_count;
    }
    return id;
}
static void locate(ir_program *p, uint32_t *fields, ir_location loc)
{
    ir_id id = location_id(p, loc);
    fields[0] = id != IR_NONE;
    fields[1] = id == IR_NONE ? 0 : id;
}
static int set_anchor(ir_program *p, ir_id id, uint32_t kind, uint32_t owner, uint32_t position,
                      ir_id alias, ir_location loc)
{
    size_t i;
    if (id == IR_NONE)
        return 1;
    if (id >= ir_graph_anchors(p->u))
        return bad(p, "invalid object anchor");
    if (!reserve(p, (void **)&p->anchors, &p->anchor_capacity, (size_t)id + 1, sizeof(anchor)))
        return 0;
    for (i = p->anchor_count; i <= (size_t)id; i++)
        p->anchors[i] = (anchor){0, 0, 0, IR_NONE, ir_no_location()};
    if (p->anchor_count <= (size_t)id)
        p->anchor_count = (size_t)id + 1;
    if (p->anchors[id].kind || p->anchors[id].alias != IR_NONE)
        return bad(p, "object anchor placed twice");
    p->anchors[id] = (anchor){kind, owner, position, alias, loc};
    return 1;
}
int ir_program_mark(ir_program *p, ir_id a, ir_location loc)
{
    uint32_t count = ir_object_count(p->o, IR_OBJ_FRAGMENTS);
    const uint32_t *last = count ? ir_object_record(p->o, IR_OBJ_FRAGMENTS, count - 1) : NULL;
    if (last && last[0] == 1)
    {
        size_t n;
        ir_object_bytes(p->o, last[1], &n);
        return set_anchor(p, a, 5, count - 1, (uint32_t)n, IR_NONE, loc);
    }
    return set_anchor(p, a, 1, count, 0, IR_NONE, loc);
}
int ir_program_raw(ir_program *p, const void *bytes, size_t n, ir_location loc)
{
    uint32_t r[7] = {1, 0, 0, 0, 0, 0, 0};
    uint32_t count = ir_object_count(p->o, IR_OBJ_FRAGMENTS);
    const uint32_t *last = count ? ir_object_record(p->o, IR_OBJ_FRAGMENTS, count - 1) : NULL;
    if (last && last[0] == 1)
        return ir_object_append_bytes(p->o, last[1], bytes, n);
    r[1] = ir_object_blob(p->o, bytes, n);
    locate(p, r + 5, loc);
    return ir_object_add(p->o, IR_OBJ_FRAGMENTS, r) != IR_NONE;
}
int ir_program_data(ir_program *p, ir_id pc, ir_id expr, unsigned width, const unsigned char *map,
                    ir_location loc)
{
    uint32_t f[6] = {expr, width, 0, 0, 0, 0}, r[7] = {2, 0, 1, 0, 0, 0, 0};
    if (map)
    {
        f[2] = 1;
        f[3] = ir_object_blob(p->o, map, 256);
    }
    locate(p, f + 4, loc);
    r[1] = ir_object_add(p->o, IR_OBJ_FIXUPS, f);
    locate(p, r + 5, loc);
    set_anchor(p, pc, 4, r[1], 0, IR_NONE, loc);
    return ir_object_add(p->o, IR_OBJ_FRAGMENTS, r) != IR_NONE;
}
int ir_program_instruction(ir_program *p, ir_id pc, unsigned char opcode, ir_id expr, int wide,
                           ir_location loc)
{
    unsigned char narrow = wide ? 0xff : opcode_zp_equiv(opcode, opcode_addressing_mode(opcode));
    uint32_t r[7] = {3, opcode, narrow != 0xff, narrow == 0xff ? 0 : narrow, 0, 0, 0};
    if (opcode_length(opcode) > 1)
    {
        uint32_t f[6] = {expr, 0, 0, 0, 0, 0};
        locate(p, f + 4, loc);
        r[4] = ir_object_add(p->o, IR_OBJ_FIXUPS, f);
    }
    ir_program_mark(p, pc, loc);
    locate(p, r + 5, loc);
    return ir_object_add(p->o, IR_OBJ_FRAGMENTS, r) != IR_NONE;
}
int ir_program_storage(ir_program *p, ir_id pc, ir_id extent, ir_location loc)
{
    uint32_t r[7] = {4, extent, 0, 0, 0, 0, 0};
    ir_program_mark(p, pc, loc);
    locate(p, r + 5, loc);
    return ir_object_add(p->o, IR_OBJ_FRAGMENTS, r) != IR_NONE;
}
static void finish_ram(ir_program *p)
{
    uint32_t r[5];
    const uint32_t *old;
    if (p->ram == IR_NONE)
        return;
    old = ir_object_record(p->o, IR_OBJ_RAM, p->ram);
    if (!old)
        return;
    memcpy(r, old, sizeof(r));
    r[0] = p->ram_extent;
    ir_object_set(p->o, IR_OBJ_RAM, p->ram, r);
}
int ir_program_ram_label(ir_program *p, ir_id a, unsigned align, int zp, ir_location loc)
{
    uint32_t r[5] = {0, align, zp != 0, 0, 0};
    finish_ram(p);
    p->ram_extent = ir_extent_literal(p->u, 0, loc);
    r[0] = p->ram_extent;
    locate(p, r + 3, loc);
    p->ram = ir_object_add(p->o, IR_OBJ_RAM, r);
    p->ram_anchor = a;
    return set_anchor(p, a, 3, p->ram, 0, IR_NONE, loc);
}
int ir_program_ram_storage(ir_program *p, ir_id pc, ir_id extent, ir_location loc)
{
    if (p->ram == IR_NONE)
    {
        /* Unnamed leading RAM bytes have no allocation identity. Their
         * count (including a use of $) does not participate in placement. */
        return 1;
    }
    if (pc != IR_NONE)
    {
        ir_id base = ir_address(p->u, p->ram_anchor, loc);
        ir_id offset = ir_extent_integer(p->u, p->ram_extent, loc);
        ir_id expr = ir_binary(p->u, IR_ADD, base, offset, loc);
        if (!set_anchor(p, pc, 0, 0, 0, expr, loc))
            return 0;
    }
    p->ram_extent = ir_extent_sum(p->u, p->ram_extent, extent, loc);
    return 1;
}
int ir_program_export(ir_program *p, const char *name, ir_id expression, ir_location loc)
{
    uint32_t r[6] = {0, 1, 1, expression, 0, 0};
    r[0] = ir_object_blob(p->o, name, strlen(name));
    locate(p, r + 4, loc);
    return ir_object_add(p->o, IR_OBJ_DEFINITIONS, r) != IR_NONE;
}
/* RAM initializers do not allocate or emit bytes in object mode, but
 * their referenced imports must still bind. Keep a non-exported root. */
int ir_program_require(ir_program *p, ir_id expression, ir_location loc)
{
    uint32_t r[6] = {0, 1, 0, expression, 0, 0};
    r[0] = ir_object_blob(p->o, "RAM operand", 11);
    locate(p, r + 4, loc);
    return ir_object_add(p->o, IR_OBJ_DEFINITIONS, r) != IR_NONE;
}
ir_id ir_program_bank(ir_program *p, ir_id operand, ir_location loc)
{
    ir_graph_node n;
    ir_id target, id, result;
    if (!ir_graph_read(p->u, operand, &n))
        return IR_NONE;
    if (n.kind == IR_GRAPH_REFERENCE)
        target = n.left;
    else
    {
        target = ir_definition(p->u, "bank owner", loc);
        ir_bind(p->u, target, operand);
    }
    id = ir_definition(p->u, "bank result", loc);
    result = ir_reference(p->u, id, loc);
    if (!reserve(p, (void **)&p->banks, &p->bank_capacity, p->bank_count + 1, sizeof(bank)))
        return IR_NONE;
    p->banks[p->bank_count++] = (bank){result, target};
    return result;
}
static ir_id request(ir_program *p, ir_object_table t, ir_id native)
{
    ir_id *map;
    size_t count;
    uint32_t zero[9] = {0};
    ir_id id;
    if (t == IR_OBJ_DEFINITIONS)
    {
        map = p->definitions;
        count = p->definition_count;
    }
    else if (t == IR_OBJ_ANCHORS)
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
        return IR_NONE;
    }
    if (map[native] != IR_NONE)
        return map[native];
    id = ir_object_add(p->o, t, zero);
    map[native] = id;
    if (!reserve(p, (void **)&p->tasks, &p->task_capacity, p->task_count + 1, sizeof(task)))
        return IR_NONE;
    p->tasks[p->task_count++] = (task){t, native, id};
    return id;
}
static ir_id extent_id(ir_program *p, ir_id id)
{
    size_t limit = p->node_count;
    ir_graph_node n;
    while (limit-- && ir_graph_read(p->u, id, &n))
    {
        if (n.kind != IR_GRAPH_REFERENCE)
        {
            if (n.kind < IR_GRAPH_EXTENT_LITERAL || n.kind > IR_GRAPH_STRING_LENGTH)
                break;
            return request(p, IR_OBJ_EXTENTS, id);
        }
        if (!ir_graph_definition(p->u, n.left, &id, NULL))
            break;
    }
    bad(p, "unresolved object extent");
    return IR_NONE;
}
static ir_id bank_target(const ir_program *, ir_id);
static ir_id node_id(ir_program *p, ir_id id)
{
    ir_graph_node n;
    ir_id *shared = NULL, wire;
    if (!ir_graph_read(p->u, id, &n))
    {
        bad(p, "invalid object expression");
        return IR_NONE;
    }
    /* These leaves are identified entirely by their definition/anchor. The
     * linker takes their location from that owner; fixups retain use spans.
     * Bank expressions and RAM-position wrappers keep their own locations. */
    if (n.kind == IR_GRAPH_REFERENCE && n.left < p->definition_count &&
        bank_target(p, id) == IR_NONE)
        shared = p->reference_nodes + n.left;
    else if (n.kind == IR_GRAPH_ADDRESS && n.left < p->anchor_count &&
             p->anchors[n.left].alias == IR_NONE)
        shared = p->address_nodes + n.left;
    if (shared && *shared != IR_NONE)
        return *shared;
    wire = request(p,
                   n.kind >= IR_GRAPH_EXTENT_LITERAL && n.kind <= IR_GRAPH_STRING_LENGTH
                       ? IR_OBJ_EXTENTS
                       : IR_OBJ_EXPRESSIONS,
                   id);
    if (shared)
        *shared = wire;
    return wire;
}
static ir_id *ids(size_t n)
{
    ir_id *p;
    size_t i;
    if (!n)
        return NULL;
    if (n > SIZE_MAX / sizeof(*p))
        return NULL;
    p = malloc(n * sizeof(*p));
    if (p)
        for (i = 0; i < n; i++)
            p[i] = IR_NONE;
    return p;
}
/* References are created in increasing node-ID order. Search the existing
 * sorted bank list without adding a second index or scanning it per operand. */
static ir_id bank_target(const ir_program *p, ir_id expression)
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
               ? p->banks[lo].target : IR_NONE;
}
ir_object *ir_program_object(ir_program *p)
{
    static const uint32_t ops[] = {16, 17, 18, 19, 20, 32, 33, 34, 35, 36, 37,
                                   38, 39, 40, 41, 44, 45, 46, 47, 48, 49};
    size_t i;
    ir_object *out;
    if (p->finished || p->error)
        return NULL;
    p->finished = 1;
    finish_ram(p);
    p->node_count = ir_graph_nodes(p->u);
    p->definition_count = ir_graph_definitions(p->u);
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
    for (i = 0; i < ir_object_count(p->o, IR_OBJ_DEFINITIONS); i++)
    {
        uint32_t r[6];
        memcpy(r, ir_object_record(p->o, IR_OBJ_DEFINITIONS, (ir_id)i), sizeof(r));
        r[3] = node_id(p, r[3]);
        ir_object_set(p->o, IR_OBJ_DEFINITIONS, (ir_id)i, r);
    }
    for (i = 0; i < ir_object_count(p->o, IR_OBJ_FIXUPS); i++)
    {
        uint32_t r[6];
        memcpy(r, ir_object_record(p->o, IR_OBJ_FIXUPS, (ir_id)i), sizeof(r));
        r[0] = node_id(p, r[0]);
        ir_object_set(p->o, IR_OBJ_FIXUPS, (ir_id)i, r);
    }
    for (i = 0; i < ir_object_count(p->o, IR_OBJ_FRAGMENTS); i++)
    {
        uint32_t r[7];
        memcpy(r, ir_object_record(p->o, IR_OBJ_FRAGMENTS, (ir_id)i), sizeof(r));
        if (r[0] == 4)
        {
            r[1] = node_id(p, r[1]);
            ir_object_set(p->o, IR_OBJ_FRAGMENTS, (ir_id)i, r);
        }
    }
    for (i = 0; i < ir_object_count(p->o, IR_OBJ_RAM); i++)
    {
        uint32_t r[5];
        memcpy(r, ir_object_record(p->o, IR_OBJ_RAM, (ir_id)i), sizeof(r));
        r[0] = node_id(p, r[0]);
        ir_object_set(p->o, IR_OBJ_RAM, (ir_id)i, r);
    }
    for (i = 0; i < p->task_count && !p->error; i++)
    {
        task task = p->tasks[i];
        uint32_t r[9] = {0};
        ir_location loc = ir_no_location();
        if (task.table == IR_OBJ_ANCHORS)
        {
            anchor a = p->anchors[task.native];
            if (!a.kind)
            {
                bad(p, "unplaced object anchor");
                break;
            }
            if (a.kind == 1 && a.owner == ir_object_count(p->o, IR_OBJ_FRAGMENTS))
            {
                a.kind = 2;
                a.owner = 0;
            }
            r[0] = a.kind;
            r[1] = a.owner;
            r[2] = a.position;
            locate(p, r + 3, a.loc);
        }
        else if (task.table == IR_OBJ_DEFINITIONS)
        {
            ir_id expr;
            const char *name = ir_definition_name(p->u, task.native);
            if (!name || !ir_graph_definition(p->u, task.native, &expr, &loc))
            {
                bad(p, "invalid object definition");
                break;
            }
            r[0] = ir_object_blob(p->o, name, strlen(name));
            r[1] = expr == IR_NONE ? 3 : 1;
            if (expr != IR_NONE)
                r[3] = node_id(p, expr);
            locate(p, r + 4, loc);
        }
        else
        {
            ir_graph_node n;
            ir_graph_read(p->u, task.native, &n);
            loc = n.location;
            if (task.table == IR_OBJ_EXPRESSIONS)
            {
                switch (n.kind)
                {
                case IR_GRAPH_INTEGER:
                    r[0] = 1;
                    r[1] = (uint32_t)n.integer;
                    break;
                case IR_GRAPH_STRING:
                    r[0] = 2;
                    r[1] = ir_object_blob(p->o, n.bytes, n.length);
                    break;
                case IR_GRAPH_REFERENCE:
                {
                    ir_id target = bank_target(p, task.native);
                    r[0] = 3;
                    if (target != IR_NONE)
                    {
                        r[0] = 5;
                        n.left = target;
                    }
                    r[1] = request(p, IR_OBJ_DEFINITIONS, n.left);
                    break;
                }
                case IR_GRAPH_ADDRESS:
                    if (n.left < p->anchor_count && p->anchors[n.left].alias != IR_NONE)
                    {
                        uint32_t d[6] = {0, 1, 0, 0, 0, 0};
                        d[0] = ir_object_blob(p->o, "RAM position", 12);
                        d[3] = node_id(p, p->anchors[n.left].alias);
                        locate(p, d + 4, loc);
                        r[0] = 3;
                        r[1] = ir_object_add(p->o, IR_OBJ_DEFINITIONS, d);
                    }
                    else
                    {
                        r[0] = 4;
                        r[1] = request(p, IR_OBJ_ANCHORS, n.left);
                    }
                    break;
                case IR_GRAPH_UNARY:
                case IR_GRAPH_BINARY:
                    r[0] = ops[n.op];
                    r[1] = node_id(p, n.left);
                    if (n.kind == IR_GRAPH_BINARY)
                        r[2] = node_id(p, n.right);
                    break;
                case IR_GRAPH_EXTENT_INTEGER:
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
                ir_result fixed = ir_evaluate_extent(p->u, task.native);
                if (ir_unit_error(p->u).code)
                {
                    bad(p, ir_error_message(ir_unit_error(p->u).code));
                    break;
                }
                if (fixed.state == IR_KNOWN && fixed.value.kind == IR_EXTENT &&
                    fixed.value.as.extent <= UINT32_MAX)
                {
                    r[0] = 1;
                    r[1] = (uint32_t)fixed.value.as.extent;
                    locate(p, r + 7, loc);
                    ir_object_set(p->o, task.table, task.wire, r);
                    continue;
                }
                switch (n.kind)
                {
                case IR_GRAPH_EXTENT_LITERAL:
                    r[0] = 1;
                    if (n.extent > UINT32_MAX)
                        bad(p, "object extent exceeds wire range");
                    r[1] = (uint32_t)n.extent;
                    break;
                case IR_GRAPH_COUNT:
                    r[0] = 2;
                    r[1] = node_id(p, n.left);
                    r[2] = n.minimum;
                    r[3] = n.maximum;
                    break;
                case IR_GRAPH_SUM:
                case IR_GRAPH_REPEAT:
                case IR_GRAPH_REMAINDER:
                    r[0] = n.kind == IR_GRAPH_SUM ? 3 : n.kind == IR_GRAPH_REPEAT ? 4 : 5;
                    r[1] = extent_id(p, n.left);
                    r[2] = extent_id(p, n.right);
                    break;
                case IR_GRAPH_BOUND:
                    r[0] = 6;
                    r[1] = extent_id(p, n.left);
                    r[2] = n.minimum;
                    r[3] = n.maximum;
                    break;
                case IR_GRAPH_STRING_LENGTH:
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
        ir_object_set(p->o, task.table, task.wire, r);
    }
    if (p->error || !ir_object_validate(p->o))
        return NULL;
    out = p->o;
    p->o = NULL;
    return out;
}
