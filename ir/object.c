#include "heap.h"
#include "object.h"
#include "../opcode.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    uint32_t *words;
    size_t count, capacity;
    int shared;
} table;
typedef struct
{
    unsigned char *bytes;
    size_t length, capacity;
    int owned;
} blob;
struct ir_object
{
    ir_heap *heap;
    table tables[IR_OBJ_TABLES];
    blob *blobs;
    size_t blob_count, blob_capacity;
    ir_object_error error;
    unsigned char *storage;
    int validated;
};
static const unsigned widths[IR_OBJ_TABLES] = {2, 3, 5, 6, 6, 9, 5, 6, 7, 5};
static int fail(ir_object *o, ir_object_error e)
{
    if (!o->error)
        o->error = e;
    return 0;
}
static int grow(ir_object *o, void **p, size_t *cap, size_t count, size_t width)
{
    size_t n = *cap ? *cap : 16;
    void *q;
    if (o->error)
        return 0;
    if (count <= *cap)
        return 1;
    if (count > UINT32_MAX || count > SIZE_MAX / width)
        return fail(o, IR_OBJECT_MEMORY);
    while (n < count)
    {
        if (n > UINT32_MAX / 2 || n > SIZE_MAX / width / 2)
        {
            n = count;
            break;
        }
        n *= 2;
    }
    q = ir_realloc(o->heap, *p, n * width);
    if (!q)
        return fail(o, IR_OBJECT_MEMORY);
    *p = q;
    *cap = n;
    return 1;
}
static ir_object *create_heap(ir_heap *heap) {
    ir_object *o = ir_calloc(heap, 1, sizeof(*o));
    if (o) o->heap = heap;
    return o;
}
ir_object *ir_object_create(void) { return create_heap(NULL); }
void ir_object_destroy(ir_object *o)
{
    size_t i;
    if (!o)
        return;
    for (i = 0; i < IR_OBJ_TABLES; i++)
        if (!o->tables[i].shared)
            ir_free(o->heap, o->tables[i].words);
    for (i = 0; o->blobs && i < o->blob_count; i++)
        if (o->blobs[i].owned)
            ir_free(o->heap, o->blobs[i].bytes);
    ir_free(o->heap, o->blobs);
    ir_free(o->heap, o->storage);
    ir_free(o->heap, o);
}
ir_object_error ir_object_error_at(const ir_object *o) { return o->error; }
const char *ir_object_error_message(ir_object_error e)
{
    return e == IR_OBJECT_OK       ? "no error"
           : e == IR_OBJECT_MEMORY ? "out of memory reading object"
                                   : "invalid XOROBJ2 object";
}
unsigned ir_object_width(ir_object_table t) { return (unsigned)t < IR_OBJ_TABLES ? widths[t] : 0; }
uint32_t ir_object_count(const ir_object *o, ir_object_table t)
{
    return t == IR_OBJ_BLOBS             ? (uint32_t)o->blob_count
           : (unsigned)t < IR_OBJ_TABLES ? (uint32_t)o->tables[t].count
                                         : 0;
}
const uint32_t *ir_object_record(const ir_object *o, ir_object_table t, ir_id i)
{
    return t > IR_OBJ_BLOBS && t < IR_OBJ_TABLES && i < o->tables[t].count
               ? o->tables[t].words + (size_t)i * widths[t]
               : NULL;
}
ir_id ir_object_add(ir_object *o, ir_object_table t, const uint32_t *f)
{
    table *a;
    uint32_t fields[9];
    if (t <= IR_OBJ_BLOBS || t >= IR_OBJ_TABLES || !f)
    {
        fail(o, IR_OBJECT_FORMAT);
        return IR_NONE;
    }
    memcpy(fields, f, widths[t] * sizeof(*fields));
    o->validated = 0;
    a = &o->tables[t];
    if (a->shared)
    {
        uint32_t *copy = a->count ? ir_malloc(o->heap, a->count * widths[t] * sizeof(*copy)) : NULL;
        if (a->count && !copy)
        {
            fail(o, IR_OBJECT_MEMORY);
            return IR_NONE;
        }
        if (a->count)
            memcpy(copy, a->words, a->count * widths[t] * sizeof(*copy));
        a->words = copy;
        a->capacity = a->count;
        a->shared = 0;
    }
    if (!grow(o, (void **)&a->words, &a->capacity, a->count + 1, widths[t] * sizeof(uint32_t)))
        return IR_NONE;
    memcpy(a->words + a->count * widths[t], fields, widths[t] * sizeof(uint32_t));
    return (ir_id)a->count++;
}
int ir_object_set(ir_object *o, ir_object_table t, ir_id id, const uint32_t *f)
{
    if (o->error)
        return 0;
    if (!f || !ir_object_record(o, t, id))
        return fail(o, IR_OBJECT_FORMAT);
    o->validated = 0;
    memmove(o->tables[t].words + (size_t)id * widths[t], f, widths[t] * sizeof(uint32_t));
    return 1;
}
static uint32_t read32(const unsigned char *p);
/* A decoded object already owns checked offset/length pairs in its wire
 * storage. Read those directly until a builder actually edits a blob. */
static blob blob_at(const ir_object *o, size_t i)
{
    if (o->blobs)
        return o->blobs[i];
    const unsigned char *record = o->storage + 92 + i * 8;
    size_t n = read32(record + 4);
    return (blob){o->storage + read32(record), n, n, 0};
}
static int materialize_blobs(ir_object *o)
{
    size_t i;
    blob *next;
    if (o->error)
        return 0;
    if (o->blobs || !o->blob_count)
        return 1;
    if (o->blob_count > SIZE_MAX / sizeof(*next) || !(next = ir_malloc(o->heap, o->blob_count * sizeof(*next))))
        return fail(o, IR_OBJECT_MEMORY);
    for (i = 0; i < o->blob_count; ++i)
        next[i] = blob_at(o, i);
    o->blobs = next;
    o->blob_capacity = o->blob_count;
    return 1;
}
ir_id ir_object_blob(ir_object *o, const void *bytes, size_t n)
{
    unsigned char *copy = NULL;
    if ((n && !bytes) || n > UINT32_MAX)
    {
        fail(o, IR_OBJECT_FORMAT);
        return IR_NONE;
    }
    if (!materialize_blobs(o))
        return IR_NONE;
    if (!grow(o, (void **)&o->blobs, &o->blob_capacity, o->blob_count + 1, sizeof(blob)))
        return IR_NONE;
    if (n)
    {
        copy = ir_malloc(o->heap, n);
        if (!copy)
        {
            fail(o, IR_OBJECT_MEMORY);
            return IR_NONE;
        }
        memcpy(copy, bytes, n);
    }
    o->validated = 0;
    o->blobs[o->blob_count] = (blob){copy, n, n, 1};
    return (ir_id)o->blob_count++;
}
/* Appending also accepts bytes borrowed from this object. Materialize a
 * shared/expanded buffer before releasing any old storage. */
int ir_object_append_bytes(ir_object *o, ir_id id, const void *bytes, size_t n)
{
    blob *b;
    size_t cap;
    unsigned char *p;
    if (o->error)
        return 0;
    if (id >= o->blob_count || (n && !bytes))
        return fail(o, IR_OBJECT_FORMAT);
    if (!materialize_blobs(o))
        return 0;
    b = &o->blobs[id];
    if (n > UINT32_MAX - b->length)
        return fail(o, IR_OBJECT_MEMORY);
    if (!n)
        return 1;
    if (!b->owned || n > b->capacity - b->length)
    {
        cap = b->capacity ? b->capacity : 16;
        while (cap < b->length + n)
        {
            if (cap > UINT32_MAX / 2)
            {
                cap = b->length + n;
                break;
            }
            cap *= 2;
        }
        p = ir_malloc(o->heap, cap);
        if (!p)
            return fail(o, IR_OBJECT_MEMORY);
        if (b->length)
            memcpy(p, b->bytes, b->length);
        memcpy(p + b->length, bytes, n);
        if (b->owned)
            ir_free(o->heap, b->bytes);
        b->bytes = p;
        b->owned = 1;
        b->capacity = cap;
    }
    else
        memmove(b->bytes + b->length, bytes, n);
    b->length += n;
    o->validated = 0;
    return 1;
}
const unsigned char *ir_object_bytes(const ir_object *o, ir_id i, size_t *n)
{
    blob b = {0};
    if (i < o->blob_count)
        b = blob_at(o, i);
    if (n)
        *n = b.length;
    return b.bytes;
}
static int index_ok(ir_object *o, ir_object_table t, uint32_t i)
{
    return i < ir_object_count(o, t);
}
static int optional(ir_object *o, ir_object_table t, uint32_t present, uint32_t i)
{
    return present <= 1 && (present ? index_ok(o, t, i) : i == 0);
}
static int name_ok(ir_object *o, uint32_t i)
{
    size_t n;
    const unsigned char *bytes = ir_object_bytes(o, i, &n);
    return n && !memchr(bytes, 0, n);
}
int ir_object_validate(ir_object *o)
{
    uint32_t t, i, j;
    unsigned char *owners = NULL;
    int ok = 1;
    if (o->error)
        return 0;
    if (o->validated)
        return 1;
    for (t = 1; t < IR_OBJ_TABLES && ok; t++)
        for (i = 0; i < ir_object_count(o, (ir_object_table)t) && ok; i++)
        {
            const uint32_t *r = ir_object_record(o, (ir_object_table)t, i);
            unsigned w = widths[t];
            if (t >= IR_OBJ_DEFINITIONS && !optional(o, IR_OBJ_LOCATIONS, r[w - 2], r[w - 1]))
            {
                ok = 0;
                break;
            }
            switch (t)
            {
            case IR_OBJ_FILES:
                ok = name_ok(o, r[0]) && optional(o, IR_OBJ_BLOBS, r[1], r[2]);
                break;
            case IR_OBJ_LOCATIONS:
                ok = index_ok(o, IR_OBJ_FILES, r[0]) && r[1] && r[2] && r[4] &&
                     (r[3] > r[1] || (r[3] == r[1] && r[4] >= r[2]));
                break;
            case IR_OBJ_DEFINITIONS:
                ok = name_ok(o, r[0]) && r[2] <= 1 &&
                     (r[1] == 1   ? index_ok(o, IR_OBJ_EXPRESSIONS, r[3])
                      : r[1] == 2 ? index_ok(o, IR_OBJ_ANCHORS, r[3])
                                  : r[1] == 3 && !r[2] && !r[3]);
                break;
            case IR_OBJ_EXPRESSIONS:
                ok = !r[3];
                switch (r[0])
                {
                case 1:
                    ok = ok && !r[2];
                    break;
                case 2:
                    ok = ok && !r[2] && index_ok(o, IR_OBJ_BLOBS, r[1]);
                    break;
                case 3:
                case 5:
                    ok = ok && !r[2] && index_ok(o, IR_OBJ_DEFINITIONS, r[1]);
                    break;
                case 4:
                    ok = ok && !r[2] && index_ok(o, IR_OBJ_ANCHORS, r[1]);
                    break;
                case 6:
                    ok = ok && !r[2] && index_ok(o, IR_OBJ_EXTENTS, r[1]);
                    break;
                default:
                    if (r[0] >= 16 && r[0] <= 20)
                        ok = ok && !r[2] && index_ok(o, IR_OBJ_EXPRESSIONS, r[1]);
                    else if ((r[0] >= 32 && r[0] <= 41) || (r[0] >= 44 && r[0] <= 49))
                        ok = ok && index_ok(o, IR_OBJ_EXPRESSIONS, r[1]) &&
                             index_ok(o, IR_OBJ_EXPRESSIONS, r[2]);
                    else
                        ok = 0;
                }
                break;
            case IR_OBJ_EXTENTS:
                ok = !r[4] && optional(o, IR_OBJ_BLOBS, r[5], r[6]) && (!r[5] || name_ok(o, r[6]));
                switch (r[0])
                {
                case 1:
                    ok = ok && !r[2] && !r[3];
                    break;
                case 2:
                    ok = ok && index_ok(o, IR_OBJ_EXPRESSIONS, r[1]) && r[2] <= r[3] &&
                         r[3] <= INT32_MAX;
                    break;
                case 3:
                case 4:
                case 5:
                    ok = ok && !r[3] && index_ok(o, IR_OBJ_EXTENTS, r[1]) &&
                         index_ok(o, IR_OBJ_EXTENTS, r[2]);
                    break;
                case 6:
                    ok = ok && index_ok(o, IR_OBJ_EXTENTS, r[1]) && r[2] <= r[3];
                    break;
                case 8:
                    ok = ok && !r[2] && !r[3] && index_ok(o, IR_OBJ_EXPRESSIONS, r[1]);
                    break;
                default:
                    ok = 0;
                }
                break;
            case IR_OBJ_ANCHORS:
                switch (r[0])
                {
                case 1:
                    ok = !r[2] && index_ok(o, IR_OBJ_FRAGMENTS, r[1]);
                    break;
                case 2:
                    ok = !r[1] && !r[2];
                    break;
                case 3:
                    ok = !r[2] && index_ok(o, IR_OBJ_RAM, r[1]);
                    break;
                case 4:
                    ok = !r[2] && index_ok(o, IR_OBJ_FIXUPS, r[1]) &&
                         ir_object_record(o, IR_OBJ_FIXUPS, r[1])[1] != 0;
                    break;
                case 5:
                {
                    const uint32_t *f = ir_object_record(o, IR_OBJ_FRAGMENTS, r[1]);
                    ok = f && f[0] == 1 && f[1] < o->blob_count && r[2] <= blob_at(o, f[1]).length;
                    break;
                }
                default:
                    ok = 0;
                }
                break;
            case IR_OBJ_FIXUPS:
                ok = index_ok(o, IR_OBJ_EXPRESSIONS, r[0]) &&
                     (r[1] == 0 || r[1] == 1 || r[1] == 2 || r[1] == 4) &&
                     optional(o, IR_OBJ_BLOBS, r[2], r[3]) &&
                     (!r[2] || (r[1] == 1 && blob_at(o, r[3]).length == 256));
                break;
            case IR_OBJ_FRAGMENTS:
                switch (r[0])
                {
                case 1:
                    ok = index_ok(o, IR_OBJ_BLOBS, r[1]) && !r[2] && !r[3] && !r[4];
                    break;
                case 2:
                    ok = !r[3] && !r[4] && r[1] <= ir_object_count(o, IR_OBJ_FIXUPS) &&
                         r[2] <= ir_object_count(o, IR_OBJ_FIXUPS) - r[1];
                    break;
                case 3:
                {
                    instr_mnemonic m;
                    unsigned char narrow;
                    ok = r[1] <= 255 && opcode_mnemonic((unsigned char)r[1], &m) && r[2] <= 1;
                    if (!ok)
                        break;
                    narrow = opcode_zp_equiv((unsigned char)r[1],
                                             opcode_addressing_mode((unsigned char)r[1]));
                    ok = r[2] ? (narrow != 0xff && r[3] == narrow) : r[3] == 0;
                    ok = ok && (opcode_length((unsigned char)r[1]) == 1
                                    ? !r[4]
                                    : index_ok(o, IR_OBJ_FIXUPS, r[4]));
                    break;
                }
                case 4:
                    ok = index_ok(o, IR_OBJ_EXTENTS, r[1]) && !r[2] && !r[3] && !r[4];
                    break;
                default:
                    ok = 0;
                }
                break;
            case IR_OBJ_RAM:
                ok = index_ok(o, IR_OBJ_EXTENTS, r[0]) && r[1] <= 15 && r[2] <= 1;
                break;
            }
        }
    if (!ok)
        return fail(o, IR_OBJECT_FORMAT);
    if (ir_object_count(o, IR_OBJ_FIXUPS))
    {
        owners = ir_calloc(o->heap, ir_object_count(o, IR_OBJ_FIXUPS), 1);
        if (!owners)
            return fail(o, IR_OBJECT_MEMORY);
    }
    for (i = 0; i < ir_object_count(o, IR_OBJ_FRAGMENTS) && ok; i++)
    {
        const uint32_t *r = ir_object_record(o, IR_OBJ_FRAGMENTS, i);
        uint32_t start = 0, count = 0;
        if (r[0] == 2)
        {
            start = r[1];
            count = r[2];
        }
        else if (r[0] == 3 && opcode_length((unsigned char)r[1]) > 1)
        {
            start = r[4];
            count = 1;
        }
        for (j = 0; j < count; j++)
        {
            const uint32_t *f = ir_object_record(o, IR_OBJ_FIXUPS, start + j);
            if (owners[start + j] || ((r[0] == 3) != (f[1] == 0)))
            {
                ok = 0;
                break;
            }
            owners[start + j] = 1;
        }
    }
    for (i = 0; i < ir_object_count(o, IR_OBJ_FIXUPS); i++)
        if (!owners[i])
            ok = 0;
    ir_free(o->heap, owners);
    if (ok)
    {
        o->validated = 1;
        return 1;
    }
    return fail(o, IR_OBJECT_FORMAT);
}
static uint32_t read32(const unsigned char *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void write32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}
/* Also exercised directly by the codec test on little-endian hosts. Input
 * and output may be identical; read each complete wire word before writing. */
static void decode_words(uint32_t *words, const unsigned char *bytes, size_t count)
{
    size_t i;
    for (i = 0; i < count; i++)
        words[i] = read32(bytes + i * 4);
}
static ir_object *decode(const void *input, size_t n, ir_object_error *error, int take, ir_heap *heap)
{
    void *owned_input = take ? (void *)input : NULL;
    const unsigned char *p = input;
    uint64_t at = 92;
    uint32_t offsets[10], counts[10], t, i;
    const uint32_t native_one = 1;
    ir_object *o = NULL;
    ir_object_error e = IR_OBJECT_FORMAT;
    if (!p || n < 92 || n > UINT32_MAX || memcmp(p, "XOROBJ2\n", 8) || read32(p + 8) != n)
        goto failed;
    for (t = 0; t < 10; t++)
    {
        offsets[t] = read32(p + 12 + t * 8);
        counts[t] = read32(p + 16 + t * 8);
        if (offsets[t] != at)
            goto failed;
        at += (uint64_t)counts[t] * widths[t] * 4;
        if (at > n)
            goto failed;
    }
    /* Validate packed payloads before allocating or copying any table. */
    for (i = 0; i < counts[0]; i++)
    {
        const unsigned char *b = p + offsets[0] + (size_t)i * 8;
        uint32_t size = read32(b + 4);
        if (read32(b) != at || size > n - at)
            goto failed;
        at += size;
        while (at % 4)
        {
            if (at >= n || p[at])
                goto failed;
            ++at;
        }
    }
    if (at != n)
        goto failed;
    o = create_heap(heap);
    if (!o)
    {
        e = IR_OBJECT_MEMORY;
        goto failed;
    }
    /* One owned slab holds decoded table words and blob bytes. Little-endian
     * hosts already have native words; other hosts convert each checked slot.
     * This avoids hundreds of differently sized allocator blocks per link. */
    o->storage = take ? owned_input : ir_malloc(o->heap, n);
    owned_input = NULL;
    if (!o->storage)
    {
        fail(o, IR_OBJECT_MEMORY);
        goto owned_failed;
    }
    if (!take)
        memcpy(o->storage, p, n);
    o->blob_count = counts[0];
    for (t = 1; t < 10; t++)
        if (counts[t])
        {
            table *a = &o->tables[t];
            a->words = (uint32_t *)(void *)(o->storage + offsets[t]);
            a->shared = 1;
            a->count = a->capacity = counts[t];
            if (*(const unsigned char *)&native_one != 1)
                decode_words(a->words, p + offsets[t], (size_t)counts[t] * widths[t]);
        }
    if (!ir_object_validate(o))
        goto owned_failed;
    if (error)
        *error = IR_OBJECT_OK;
    return o;
owned_failed:
    e = o->error;
failed:
    ir_free(heap, owned_input);
    ir_object_destroy(o);
    if (error)
        *error = e;
    return NULL;
}
ir_object *ir_object_decode(const void *input, size_t n, ir_object_error *error)
{
    return decode(input, n, error, 0, NULL);
}
ir_object *ir_object_decode_owned(void *input, size_t n, ir_object_error *error)
{
    return decode(input, n, error, 1, NULL);
}
int ir_object_encode(ir_object *o, unsigned char **bytes, size_t *length)
{
    uint64_t total = 92, at;
    unsigned char *p;
    uint32_t t, i, j;
    if (!bytes || !length)
        return fail(o, IR_OBJECT_FORMAT);
    *bytes = NULL;
    *length = 0;
    if (!ir_object_validate(o))
        return 0;
    for (t = 0; t < 10; t++)
        total += (uint64_t)ir_object_count(o, (ir_object_table)t) * widths[t] * 4;
    for (i = 0; i < o->blob_count; i++)
        total += ((uint64_t)blob_at(o, i).length + 3) & ~UINT64_C(3);
    if (total > UINT32_MAX || total > SIZE_MAX)
        return fail(o, IR_OBJECT_MEMORY);
    p = calloc((size_t)total, 1);
    if (!p)
        return fail(o, IR_OBJECT_MEMORY);
    memcpy(p, "XOROBJ2\n", 8);
    write32(p + 8, (uint32_t)total);
    at = 92;
    for (t = 0; t < 10; t++)
    {
        write32(p + 12 + t * 8, (uint32_t)at);
        write32(p + 16 + t * 8, ir_object_count(o, (ir_object_table)t));
        if (t)
            for (i = 0; i < ir_object_count(o, (ir_object_table)t); i++)
                for (j = 0; j < widths[t]; j++)
                    write32(p + at + ((size_t)i * widths[t] + j) * 4,
                            o->tables[t].words[(size_t)i * widths[t] + j]);
        at += (uint64_t)ir_object_count(o, (ir_object_table)t) * widths[t] * 4;
    }
    for (i = 0; i < o->blob_count; i++)
    {
        blob b = blob_at(o, i);
        write32(p + 92 + (size_t)i * 8, (uint32_t)at);
        write32(p + 96 + (size_t)i * 8, (uint32_t)b.length);
        if (b.length)
            memcpy(p + at, b.bytes, b.length);
        at += ((uint64_t)b.length + 3) & ~UINT64_C(3);
    }
    *bytes = p;
    *length = (size_t)total;
    return 1;
}

ir_object *ir_object_decode_heap_owned(ir_heap *heap, void *input, size_t n, ir_object_error *error) { return decode(input, n, error, 1, heap); }
