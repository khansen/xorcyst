#include "heap.h"
#include "link.h"
#include "graph.h"
#include "../opcode.h"
#include <stdlib.h>
#include <string.h>

typedef struct
{
    uint32_t fragment, offset;
    v2_id anchor;
    v2_id loc;
} mark;
typedef struct
{
    v2_object *object;
    char *name;
    v2_id *files, *definitions, *anchors, *ram, *pcs, *nodes, *proxies;
    v2_id *definition_values, *addresses;
    v2_id owner;
    unsigned char *state;
    mark *marks;
    size_t mark_count;
    int appended;
} input;
typedef struct
{
    char *name;
    v2_id definition, unit;
    v2_id loc;
} exported;
typedef struct
{
    v2_id owner, alias;
    char *import;
    v2_id loc;
    int present;
} binding;
typedef struct
{
    v2_id source, result;
    v2_location loc;
} bank_fixup;
struct v2_link
{
    v2_heap *heap;
    v2_unit *values;
    v2_layout *layout;
    input *inputs;
    void *workspace;
    size_t workspace_capacity;
    size_t count, capacity;
    exported *exports;
    size_t export_count, export_capacity;
    binding *bindings;
    size_t binding_count, binding_capacity;
    bank_fixup *banks;
    size_t bank_count, bank_capacity;
    const char *error, *error_symbol;
    v2_location loc;
    int prepared;
    v2_id integers[256], extents[256];
};
static int bad(v2_link *l, const char *e, v2_location loc)
{
    if (!l->error)
    {
        l->error = e;
        l->loc = loc;
    }
    return 0;
}
static void *array(v2_link *l, size_t n, size_t width)
{
    void *p;
    if (!n)
        return NULL;
    if (n > SIZE_MAX / width || !(p = v2_calloc(l->heap, n, width)))
    {
        bad(l, "out of memory linking objects", v2_no_location());
        return NULL;
    }
    return p;
}
v2_link *v2_link_create_heap(v2_heap *heap)
{
    v2_link *l = v2_calloc(heap, 1, sizeof(*l));
    if (!l)
        return NULL;
    {
        size_t i;
        for (i = 0; i < 256; i++)
            l->integers[i] = l->extents[i] = V2_NONE;
    }
    l->heap = heap;
    l->loc = v2_no_location();
    l->values = v2_unit_create_heap(heap);
    if (l->values)
        l->layout = v2_layout_create(l->values, V2_LINK_LAYOUT);
    if (!l->layout)
    {
        v2_link_destroy(l);
        return NULL;
    }
    v2_layout_keep_facts(l->layout, 0);
    return l;
}
void v2_link_destroy(v2_link *l)
{
    size_t i;
    if (!l)
        return;
    for (i = 0; i < l->count; i++)
    {
        input *u = &l->inputs[i];
        v2_object_destroy(u->object);
        v2_free(l->heap, u->name);
    }
    for (i = 0; i < l->export_count; i++)
        v2_free(l->heap, l->exports[i].name);
    for (i = 0; i < l->binding_count; i++)
        v2_free(l->heap, l->bindings[i].import);
    v2_free(l->heap, l->bindings);
    v2_free(l->heap, l->banks);
    v2_free(l->heap, l->exports);
    v2_free(l->heap, l->inputs);
    v2_free(l->heap, l->workspace);
    v2_layout_destroy(l->layout);
    v2_unit_destroy(l->values);
    v2_free(l->heap, l);
}
v2_layout *v2_link_layout(v2_link *l) { return l->layout; }
v2_unit *v2_link_values(v2_link *l) { return l->values; }
const char *v2_link_error(v2_link *l)
{
    v2_layout_diagnostic d = v2_layout_error_at(l->layout);
    v2_diagnostic value = v2_unit_error(l->values);
    if (l->error)
        return l->error;
    if (value.code)
        return v2_error_message(value.code);
    return d.value_error.code ? v2_error_message(d.value_error.code)
                             : v2_layout_error_message(d.code);
}
v2_location v2_link_error_location(v2_link *l)
{
    v2_diagnostic value = v2_unit_error(l->values);
    return l->error ? l->loc : value.code ? value.location
                                        : v2_layout_error_at(l->layout).location;
}
const char *v2_link_error_symbol(v2_link *l) { return l->error_symbol; }
static char *name_copy(v2_link *l, const void *p, size_t n)
{
    char *s = array(l, n + 1, 1);
    if (s && n)
        memcpy(s, p, n);
    return s;
}
v2_id v2_link_add(v2_link *l, v2_object *o, const char *name)
{
    input u;
    input *next;
    size_t cap;
    if (!o || l->prepared || l->error || !name)
        goto failed;
    if (!v2_object_validate(o))
    {
        bad(l, v2_object_error_message(v2_object_error_at(o)), v2_no_location());
        goto failed;
    }
    memset(&u, 0, sizeof(u));
    u.object = o;
    u.name = name_copy(l, name, strlen(name));
    if (!u.name)
        goto failed;
    if (l->count == l->capacity)
    {
        cap = l->capacity ? l->capacity * 2 : 16;
        if (cap > UINT32_MAX || cap > SIZE_MAX / sizeof(input))
        {
            v2_free(l->heap, u.name);
            goto failed;
        }
        next = v2_realloc(l->heap, l->inputs, cap * sizeof(input));
        if (!next)
        {
            v2_free(l->heap, u.name);
            bad(l, "out of memory linking objects", v2_no_location());
            goto failed;
        }
        l->inputs = next;
        l->capacity = cap;
    }
    l->inputs[l->count] = u;
    return (v2_id)l->count++;
failed:
    v2_object_destroy(o);
    bad(l, "invalid or unavailable object input", v2_no_location());
    return V2_NONE;
}
static v2_location input_location(input *u, const uint32_t *r)
{
    const uint32_t *s;
    v2_location loc = v2_no_location();
    if (!r[0])
        return loc;
    s = v2_object_record(u->object, V2_OBJ_LOCATIONS, r[1]);
    loc.file = u->files[s[0]];
    loc.line = s[1];
    loc.column = s[2];
    loc.end_line = s[3];
    loc.end_column = s[4];
    return loc;
}
static char *blob_name(v2_link *l, input *u, uint32_t id)
{
    size_t n;
    const unsigned char *b = v2_object_bytes(u->object, id, &n);
    return name_copy(l, b, n);
}
static int compare_exports(const void *a, const void *b)
{
    return strcmp(((const exported *)a)->name, ((const exported *)b)->name);
}
static exported *find_export(v2_link *l, const char *name)
{
    exported key;
    key.name = (char *)name;
    return l->export_count
               ? bsearch(&key, l->exports, l->export_count, sizeof(key), compare_exports)
               : NULL;
}
static int compare_marks(const void *a, const void *b)
{
    const mark *x = a, *y = b;
    return x->fragment != y->fragment ? (x->fragment > y->fragment ? 1 : -1)
           : x->offset != y->offset   ? (x->offset > y->offset ? 1 : -1)
           : x->anchor > y->anchor    ? 1
           : x->anchor < y->anchor    ? -1
                                      : 0;
}
static int prepare_ids(v2_link *l, input *u)
{
    v2_object *o = u->object;
    uint32_t i;
    size_t ne = v2_object_count(o, V2_OBJ_EXPRESSIONS), nx = v2_object_count(o, V2_OBJ_EXTENTS);
    if (ne + nx > UINT32_MAX)
        return bad(l, "object graph too large", v2_no_location());
    {
        size_t nf = v2_object_count(o, V2_OBJ_FILES), nd = v2_object_count(o, V2_OBJ_DEFINITIONS);
        size_t na = v2_object_count(o, V2_OBJ_ANCHORS), nr = v2_object_count(o, V2_OBJ_RAM);
        size_t np = v2_object_count(o, V2_OBJ_FIXUPS), ng = ne + nx;
        uint64_t words =
            (uint64_t)nf + 2 * (uint64_t)nd + 2 * (uint64_t)na + nr + np + 2 * (uint64_t)ng;
        uint64_t bytes = words * sizeof(v2_id) + (uint64_t)na * sizeof(mark) + ng;
        v2_id *at;
        if (bytes > SIZE_MAX)
            return bad(l, "object translation too large", v2_no_location());
        size_t needed = bytes ? (size_t)bytes : 1;
        if (needed > l->workspace_capacity)
        {
            void *next = v2_realloc(l->heap, l->workspace, needed);
            if (!next)
                return bad(l, "out of memory linking objects", v2_no_location());
            l->workspace = next;
            l->workspace_capacity = needed;
        }
        memset(l->workspace, 0, needed);
        at = l->workspace;
        memset(at, 0xff, (size_t)words * sizeof(v2_id));
        u->files = at;
        at += nf;
        u->definitions = at;
        at += nd;
        u->anchors = at;
        at += na;
        u->addresses = at;
        at += na;
        u->definition_values = at;
        at += nd;
        u->ram = at;
        at += nr;
        u->pcs = at;
        at += np;
        u->nodes = at;
        at += ng;
        u->proxies = at;
        at += ng;
        u->marks = (mark *)(void *)at;
        u->state = (unsigned char *)(u->marks + na);
    }
    for (i = 0; i < v2_object_count(o, V2_OBJ_FILES); i++)
    {
        char *name = blob_name(l, u, v2_object_record(o, V2_OBJ_FILES, i)[0]);
        if (!name)
            return 0;
        u->files[i] = v2_file(l->values, name);
        v2_free(l->heap, name);
    }
    u->owner = v2_anchor(l->values, v2_no_location());
    for (i = 0; i < v2_object_count(o, V2_OBJ_DEFINITIONS); i++)
    {
        const uint32_t *r = v2_object_record(o, V2_OBJ_DEFINITIONS, i);
        char *name = blob_name(l, u, r[0]);
        if (!name)
            return 0;
        u->definitions[i] = v2_definition(l->values, name, input_location(u, r + 4));
        v2_free(l->heap, name);
    }
    for (i = 0; i < v2_object_count(o, V2_OBJ_RAM); i++)
        u->ram[i] = v2_anchor(l->values, input_location(u, v2_object_record(o, V2_OBJ_RAM, i) + 3));
    for (i = 0; i < v2_object_count(o, V2_OBJ_ANCHORS); i++)
    {
        const uint32_t *r = v2_object_record(o, V2_OBJ_ANCHORS, i);
        v2_location loc = input_location(u, r + 3);
        if (r[0] == 3)
            u->anchors[i] = u->ram[r[1]];
        else if (r[0] == 4)
        {
            if (u->pcs[r[1]] == V2_NONE)
                u->pcs[r[1]] = v2_anchor(l->values, loc);
            u->anchors[i] = u->pcs[r[1]];
        }
        else
        {
            mark m;
            u->anchors[i] = v2_anchor(l->values, loc);
            m.anchor = u->anchors[i];
            m.loc = v2_graph_location_id(l->values, loc);
            m.fragment = r[0] == 2 ? v2_object_count(o, V2_OBJ_FRAGMENTS) : r[1];
            m.offset = r[0] == 5 ? r[2] : 0;
            u->marks[u->mark_count++] = m;
        }
    }
    if (u->mark_count > 1)
        qsort(u->marks, u->mark_count, sizeof(mark), compare_marks);
    if (v2_unit_error(l->values).code)
        return bad(l, v2_error_message(v2_unit_error(l->values).code),
                   v2_unit_error(l->values).location);
    return 1;
}
static int reserve(v2_link *l, void **data, size_t *capacity, size_t needed, size_t width)
{
    size_t n = *capacity ? *capacity : 16;
    void *next;
    if (l->error)
        return 0;
    if (needed <= *capacity)
        return 1;
    if (needed > UINT32_MAX || needed > SIZE_MAX / width)
        return bad(l, "link table too large", v2_no_location());
    while (n < needed)
    {
        if (n > UINT32_MAX / 2 || n > SIZE_MAX / width / 2)
        {
            n = needed;
            break;
        }
        n *= 2;
    }
    next = v2_realloc(l->heap, *data, n * width);
    if (!next)
        return bad(l, "out of memory linking objects", v2_no_location());
    memset((unsigned char *)next + *capacity * width, 0, (n - *capacity) * width);
    *data = next;
    *capacity = n;
    return 1;
}
static int remember_definitions(v2_link *l, input *u, v2_id unit)
{
    uint32_t i;
    for (i = 0; i < v2_object_count(u->object, V2_OBJ_DEFINITIONS); i++)
    {
        const uint32_t *r = v2_object_record(u->object, V2_OBJ_DEFINITIONS, i);
        v2_id id = u->definitions[i];
        binding *b;
        if (id == V2_NONE || !reserve(l, (void **)&l->bindings, &l->binding_capacity,
                                      (size_t)id + 1, sizeof(*l->bindings)))
            return 0;
        b = &l->bindings[id];
        b->present = 1;
        b->owner = u->owner;
        b->alias = V2_NONE;
        b->loc = v2_graph_location_id(l->values, input_location(u, r + 4));
        if (l->binding_count <= id)
            l->binding_count = (size_t)id + 1;
        if (r[1] == 3)
            b->import = blob_name(l, u, r[0]);
        else if (r[1] == 1)
        {
            const uint32_t *e = v2_object_record(u->object, V2_OBJ_EXPRESSIONS, r[3]);
            if (e[0] == 3)
                b->alias = u->definitions[e[1]];
        }
        if (r[2])
        {
            exported *e;
            if (!reserve(l, (void **)&l->exports, &l->export_capacity, l->export_count + 1,
                         sizeof(*l->exports)))
                return 0;
            e = &l->exports[l->export_count++];
            e->name = blob_name(l, u, r[0]);
            e->definition = id;
            e->unit = unit;
            e->loc = b->loc;
        }
    }
    return !l->error;
}
static v2_id bank_expression(v2_link *l, input *u, uint32_t definition, v2_location loc)
{
    v2_id result, expression;
    if (!reserve(l, (void **)&l->banks, &l->bank_capacity, l->bank_count + 1, sizeof(*l->banks)))
        return V2_NONE;
    result = v2_definition(l->values, "bank result", loc);
    expression = v2_reference(l->values, result, loc);
    l->banks[l->bank_count++] = (bank_fixup){u->definitions[definition], result, loc};
    return expression;
}
static v2_id bank_owner(v2_link *l, v2_id id)
{
    size_t limit = l->binding_count;
    while (limit-- && id < l->binding_count && l->bindings[id].present)
    {
        const binding *b = &l->bindings[id];
        if (b->alias == V2_NONE)
            return b->owner;
        id = b->alias;
    }
    return V2_NONE;
}
static v2_id address(v2_link *l, input *u, uint32_t id)
{
    if (u->addresses[id] == V2_NONE)
    {
        const uint32_t *a = v2_object_record(u->object, V2_OBJ_ANCHORS, id);
        u->addresses[id] = v2_address(l->values, u->anchors[id], input_location(u, a + 3));
    }
    return u->addresses[id];
}
/* Binding resolves aliases to the provider's graph node, without copying a
 * new identity/value cache at each use. Back-edges still get explicit proxies;
 * original definition IDs, source spans and operand fixups are retained. */
static v2_id definition_value(v2_link *l, input *u, uint32_t id)
{
    const uint32_t *d = v2_object_record(u->object, V2_OBJ_DEFINITIONS, id);
    if (u->definition_values[id] == V2_NONE)
    {
        if (d[1] == 1)
            u->definition_values[id] = u->nodes[d[3]];
        else if (d[1] == 2)
            u->definition_values[id] = address(l, u, d[3]);
        else
            u->definition_values[id] =
                v2_reference(l->values, u->definitions[id], input_location(u, d + 4));
    }
    return u->definition_values[id];
}
/* Literal leaves cannot fail during evaluation; their source spans still
 * belong to the wire records, enclosing operations, definitions and fixups.
 * Share common values across inputs without merging source identities. */
static v2_id literal(v2_link *l, uint32_t bits, int extent, v2_location loc)
{
    v2_id *cache = extent ? l->extents : l->integers;
    v2_id value;
    if (bits < 256 && cache[bits] != V2_NONE)
        return cache[bits];
    value = extent
                ? v2_extent_literal(l->values, bits, loc)
                : v2_integer(l->values,
                             bits <= INT32_MAX ? (int32_t)bits : -1 - (int32_t)(UINT32_MAX - bits),
                             loc);
    if (bits < 256)
        cache[bits] = value;
    return value;
}
/* Graph construction is iterative. A back-edge gets one definition proxy;
 * acyclic/shared nodes get no proxy and are constructed exactly once. Cycles
 * remain evaluable errors rather than decoder errors or recursive expansion. */
static int prepare_nodes(v2_link *l, input *u)
{
    typedef struct
    {
        uint32_t id, child;
    } frame;
    size_t ne = v2_object_count(u->object, V2_OBJ_EXPRESSIONS),
           total = ne + v2_object_count(u->object, V2_OBJ_EXTENTS), root;
    frame *stack = NULL;
    size_t count = 0, capacity = 0;
    for (root = 0; root < total && !l->error; root++)
    {
        if (u->state[root] == 2)
            continue;
        if (!reserve(l, (void **)&stack, &capacity, count + 1, sizeof(*stack)))
            break;
        stack[count++] = (frame){(uint32_t)root, 0};
        u->state[root] = 1;
        while (count && !l->error)
        {
            frame *f = &stack[count - 1];
            uint32_t id = f->id, deps[2], nd = 0, k;
            int ext = id >= ne;
            const uint32_t *r = v2_object_record(
                u->object, ext ? V2_OBJ_EXTENTS : V2_OBJ_EXPRESSIONS, ext ? id - (uint32_t)ne : id);
            v2_location loc = input_location(u, r + (ext ? 7 : 4));
            /* Reference/address records may omit their redundant leaf span.
             * Keep owner locations available even for cycle-proxy creation. */
            if (!ext && !r[4] && (r[0] == 3 || r[0] == 4))
            {
                const uint32_t *owner = v2_object_record(u->object,
                    r[0] == 3 ? V2_OBJ_DEFINITIONS : V2_OBJ_ANCHORS, r[1]);
                loc = input_location(u, owner + (r[0] == 3 ? 4 : 3));
            }
            v2_id value = V2_NONE;
            if (ext)
            {
                if (r[0] == 2 || r[0] == 8)
                    deps[nd++] = r[1];
                if (r[0] >= 3 && r[0] <= 6)
                    deps[nd++] = (uint32_t)ne + r[1];
                if (r[0] >= 3 && r[0] <= 5)
                    deps[nd++] = (uint32_t)ne + r[2];
            }
            else
            {
                if (r[0] == 3)
                {
                    const uint32_t *d = v2_object_record(u->object, V2_OBJ_DEFINITIONS, r[1]);
                    if (d[1] == 1)
                        deps[nd++] = d[3];
                }
                else if (r[0] == 6)
                    deps[nd++] = (uint32_t)ne + r[1];
                else if (r[0] >= 16)
                {
                    deps[nd++] = r[1];
                    if (r[0] >= 32)
                        deps[nd++] = r[2];
                }
            }
            if (f->child < nd)
            {
                k = deps[f->child++];
                if (!u->state[k])
                {
                    /* The temporary walk stack scales with expression depth,
                     * not the number of independent expressions in the unit. */
                    if (!reserve(l, (void **)&stack, &capacity, count + 1, sizeof(*stack)))
                        break;
                    u->state[k] = 1;
                    stack[count++] = (frame){k, 0};
                    continue;
                }
                if (u->state[k] == 1 && u->proxies[k] == V2_NONE)
                {
                    u->proxies[k] = v2_definition(l->values, "cyclic object expression", loc);
                    u->nodes[k] = v2_reference(l->values, u->proxies[k], loc);
                }
                continue;
            }
            if (ext)
                switch (r[0])
                {
                case 1:
                    value = literal(l, r[1], 1, loc);
                    break;
                case 2:
                    value = v2_extent_count(l->values, u->nodes[deps[0]], r[2], r[3], loc);
                    break;
                case 3:
                    value = v2_extent_sum(l->values, u->nodes[deps[0]], u->nodes[deps[1]], loc);
                    break;
                case 4:
                    value = v2_extent_repeat(l->values, u->nodes[deps[0]], u->nodes[deps[1]], loc);
                    break;
                case 5:
                    value =
                        v2_extent_remainder(l->values, u->nodes[deps[0]], u->nodes[deps[1]], loc);
                    break;
                case 6:
                    value = v2_extent_bound(l->values, u->nodes[deps[0]], r[2], r[3], loc);
                    break;
                case 8:
                    value = v2_extent_string(l->values, u->nodes[deps[0]], loc);
                    break;
                }
            else
                switch (r[0])
                {
                case 1:
                    value = literal(l, r[1], 0, loc);
                    break;
                case 2:
                {
                    size_t n;
                    const unsigned char *b = v2_object_bytes(u->object, r[1], &n);
                    value = v2_string(l->values, b, n, loc);
                    break;
                }
                case 3:
                    value = definition_value(l, u, r[1]);
                    break;
                case 4:
                    value = address(l, u, r[1]);
                    break;
                case 5:
                    value = bank_expression(l, u, r[1], loc);
                    break;
                case 6:
                    value = v2_extent_integer(l->values, u->nodes[deps[0]], loc);
                    break;
                default:
                {
                    v2_operator op = r[0] < 32   ? (v2_operator)(r[0] - 16)
                                     : r[0] < 42 ? (v2_operator)(r[0] - 32 + V2_ADD)
                                                 : (v2_operator)(r[0] - 44 + V2_EQUAL);
                    value = r[0] < 32 ? v2_unary(l->values, op, u->nodes[deps[0]], loc)
                                      : v2_binary(l->values, op, u->nodes[deps[0]],
                                                  u->nodes[deps[1]], loc);
                    break;
                }
                }
            if (value == V2_NONE)
            {
                bad(l, v2_error_message(v2_unit_error(l->values).code), loc);
                break;
            }
            if (u->proxies[id] != V2_NONE)
                v2_bind(l->values, u->proxies[id], value);
            else
                u->nodes[id] = value;
            u->state[id] = 2;
            --count;
        }
    }
    v2_free(l->heap, stack);
    return !l->error;
}
int v2_link_prepare(v2_link *l)
{
    size_t i;
    if (l->prepared || l->error)
        return bad(l, "link already prepared", v2_no_location());
    for (i = 0; i < l->count; i++)
        if (!l->inputs[i].appended)
            return bad(l, "object missing from link sequence", v2_no_location());
    l->prepared = 1;
    v2_free(l->heap, l->workspace);
    l->workspace = NULL;
    l->workspace_capacity = 0;
    if (l->export_count > 1)
        qsort(l->exports, l->export_count, sizeof(exported), compare_exports);
    for (i = 1; i < l->export_count; i++)
        if (!strcmp(l->exports[i - 1].name, l->exports[i].name))
        {
            l->error_symbol = l->exports[i].name;
            return bad(l, "duplicate exported symbol", v2_graph_location(l->values, l->exports[i].loc));
        }
    for (i = 0; i < l->binding_count; i++)
    {
        binding *b = &l->bindings[i];
        exported *provider;
        if (!b->import)
            continue;
        provider = find_export(l, b->import);
        if (!provider)
        {
            l->error_symbol = b->import;
            return bad(l, "undefined imported symbol", v2_graph_location(l->values, b->loc));
        }
        b->alias = provider->definition;
        {
            v2_id expression = V2_NONE;
            v2_graph_node node;
            /* A provider leaf has no definition edge to diagnose or cycle
             * through. Share it directly, retaining both definition IDs.
             * Composite and unbound providers keep the located reference. */
            if (!v2_graph_definition(l->values, provider->definition, &expression, NULL) ||
                !v2_graph_read(l->values, expression, &node) ||
                (node.kind != V2_GRAPH_INTEGER && node.kind != V2_GRAPH_STRING &&
                 node.kind != V2_GRAPH_ADDRESS))
                expression = v2_reference(l->values, provider->definition, v2_graph_location(l->values, b->loc));
            if (!v2_bind(l->values, (v2_id)i, expression))
                goto value_error;
        }
    }
    for (i = 0; i < l->bank_count; i++)
    {
        const bank_fixup *r = &l->banks[i];
        v2_id owner = bank_owner(l, r->source);
        /* No provider for a cyclic alias: keep it request-scoped and invalid
         * only when used, just like an ordinary cyclic expression. */
        v2_id expression = owner == V2_NONE ? v2_reference(l->values, r->result, r->loc)
                                            : v2_layout_bank_expression(l->layout, owner, r->loc);
        if (!v2_bind(l->values, r->result, expression))
            goto value_error;
    }
    if (!v2_unit_error(l->values).code)
    {
        /* The native graph owns bindings and source spans now. Export lookup
         * and bank-owner chains are construction-only metadata. */
        for (i = 0; i < l->binding_count; i++)
            v2_free(l->heap, l->bindings[i].import);
        for (i = 0; i < l->export_count; i++)
            v2_free(l->heap, l->exports[i].name);
        v2_free(l->heap, l->bindings);
        l->bindings = NULL;
        l->binding_count = l->binding_capacity = 0;
        v2_free(l->heap, l->exports);
        l->exports = NULL;
        l->export_count = l->export_capacity = 0;
        v2_free(l->heap, l->banks);
        l->banks = NULL;
        l->bank_count = l->bank_capacity = 0;
        return 1;
    }
value_error:
    return bad(l, v2_error_message(v2_unit_error(l->values).code),
               v2_unit_error(l->values).location);
}
int v2_link_append(v2_link *l, v2_id unit)
{
    input *u;
    v2_object *o;
    size_t i, j, m = 0;
    uint32_t ne;
    if (l->prepared || l->error || unit >= l->count)
        return bad(l, "invalid link sequence", v2_no_location());
    u = &l->inputs[unit];
    o = u->object;
    if (u->appended)
        return bad(l, "object linked more than once", v2_no_location());
    ne = v2_object_count(o, V2_OBJ_EXPRESSIONS);
    u->appended = 1;
    if (!prepare_ids(l, u) || !remember_definitions(l, u, unit) || !prepare_nodes(l, u))
        return 0;
    for (j = 0; j < v2_object_count(o, V2_OBJ_DEFINITIONS); j++)
    {
        const uint32_t *r = v2_object_record(o, V2_OBJ_DEFINITIONS, (v2_id)j);
        if (r[1] != 3)
            v2_bind(l->values, u->definitions[j], r[1] == 1 ? u->nodes[r[3]] : address(l, u, r[3]));
    }

    v2_layout_mark(l->layout, u->owner, v2_no_location());
    for (i = 0; i < v2_object_count(o, V2_OBJ_RAM); i++)
    {
        const uint32_t *r = v2_object_record(o, V2_OBJ_RAM, (v2_id)i);
        v2_layout_ram(l->layout, u->ram[i], u->nodes[ne + r[0]], r[1], r[2] != 0,
                      input_location(u, r + 3));
    }
    for (i = 0; i < v2_object_count(o, V2_OBJ_FRAGMENTS); i++)
    {
        const uint32_t *r = v2_object_record(o, V2_OBJ_FRAGMENTS, (v2_id)i);
        v2_location loc = input_location(u, r + 5);
        v2_id boundary = V2_NONE;
        if (r[0] == 1)
        {
            size_t n, at = 0;
            const unsigned char *b = v2_object_bytes(o, r[1], &n);
            mark pending = {0, 0, V2_NONE, V2_NONE};
            while (m < u->mark_count && u->marks[m].fragment == i)
            {
                mark a = u->marks[m++];
                if (a.offset > at)
                {
                    v2_layout_raw_anchored(l->layout, pending.anchor, b + at, a.offset - at, loc);
                    pending.anchor = V2_NONE;
                }
                if (pending.anchor != V2_NONE)
                    v2_layout_mark(l->layout, pending.anchor, v2_graph_location(l->values, pending.loc));
                pending = a;
                at = a.offset;
            }
            if (n > at)
                v2_layout_raw_anchored(l->layout, pending.anchor, b + at, n - at, loc);
            else if (pending.anchor != V2_NONE)
                v2_layout_mark(l->layout, pending.anchor, v2_graph_location(l->values, pending.loc));
            continue;
        }
        while (m < u->mark_count && u->marks[m].fragment == i)
        {
            mark a = u->marks[m++];
            if (r[0] == 2 || (m < u->mark_count && u->marks[m].fragment == i))
                v2_layout_mark(l->layout, a.anchor, v2_graph_location(l->values, a.loc));
            else
                boundary = a.anchor;
        }
        if (r[0] == 2)
            for (j = 0; j < r[2]; j++)
            {
                uint32_t fix = r[1] + (uint32_t)j;
                const uint32_t *f = v2_object_record(o, V2_OBJ_FIXUPS, fix);
                const unsigned char *map = f[2] ? v2_object_bytes(o, f[3], NULL) : NULL;
                v2_layout_data(l->layout, u->pcs[fix], u->nodes[f[0]], f[1], 1, map,
                               input_location(u, f + 4));
            }
        else if (r[0] == 3)
        {
            v2_id operand = V2_NONE;
            if (opcode_length((unsigned char)r[1]) > 1)
                operand = u->nodes[v2_object_record(o, V2_OBJ_FIXUPS, r[4])[0]];
            v2_layout_instruction(l->layout, boundary, (unsigned char)r[1], operand, !r[2], 0, loc);
        }
        else
            v2_layout_storage(l->layout, boundary, u->nodes[ne + r[1]], loc);
    }
    while (m < u->mark_count)
    {
        mark a = u->marks[m++];
        v2_layout_mark(l->layout, a.anchor, v2_graph_location(l->values, a.loc));
    }
    /* Values and fragments own all retained bytes and identities now. No wire
     * table or temporary translation map is needed during layout. */
    v2_object_destroy(u->object);
    u->object = NULL;
    u->files = u->definitions = u->definition_values = u->addresses = NULL;
    u->anchors = u->ram = u->pcs = u->nodes = u->proxies = NULL;
    u->state = NULL;
    u->marks = NULL;
    return 1;
}
int v2_link_finish(v2_link *l)
{
    size_t i;
    if (l->error)
        return 0;
    for (i = 0; i < l->count; i++)
        if (!l->inputs[i].appended)
            return bad(l, "object missing from link sequence", v2_no_location());
    if (!l->prepared && !v2_link_prepare(l))
        return 0;
    if (!v2_layout_run(l->layout))
    {
        v2_layout_diagnostic d = v2_layout_error_at(l->layout);
        return bad(l,
                   d.value_error.code ? v2_error_message(d.value_error.code)
                                      : v2_layout_error_message(d.code),
                   d.location);
    }
    return 1;
}

v2_link *v2_link_create(void) { return v2_link_create_heap(NULL); }
