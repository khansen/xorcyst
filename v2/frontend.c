/* Source-to-owned-IR adapter. Only the non-installed I3 target includes this
 * translation unit. Reuse v1 parsing/expansion; layout and emission are v2. */
#include "frontend.h"
#include "../symtab.h"
#include "layout.h"
#include "program.h"
static uint64_t binding_epoch = 1;
static astnode *frontend_capture_constant(astnode *, symtab_entry *);
static astnode *frontend_fold_reference(astnode *);
static astnode *frontend_string_fold(astnode *);
static v2_id frontend_string_value(astnode *);
static int frontend_data(astnode *);
static int frontend_storage(astnode *);
static int frontend_type_pc(astnode *);
static astnode *frontend_sizeof_label(astnode *, astnode *);
static void frontend_capture_pc(location);
static void frontend_reset_pc(void);
static astnode *frontend_initializer_size(astnode *);
static void frontend_export_symbols(location);
#include "../astproc.c"

typedef struct
{
    unsigned char map[256];
} source_map;
static v2_unit *values;
static v2_layout *layout;
static v2_program *program;
static unsigned char *object_bytes;
static size_t object_length;
static source_map *maps;
static size_t map_count, map_capacity;
static v2_id *files;
static size_t file_count, file_capacity;
static unsigned char *literal_bytes;
static size_t literal_count, literal_capacity;
static location literal_location;
typedef struct
{
    v2_id expression;
    v2_location location;
} ram_value;
static ram_value *ram_values;
static size_t ram_value_count, ram_value_capacity;
typedef struct size_guard
{
    astnode *operand;
    int operand_index;
    int string;
    size_t length;
    location use;
    char *name;
    struct size_guard *next;
    struct size_guard *pending_next;
} size_guard;
static size_guard *size_guards;
/* Native DATA nodes carry only an index into this owned pending list table.
 * No guard retains a statement pointer. astnode_clone drops the flag, so
 * cloned statements cannot inherit pending handles. */
#define DATA_PENDING_SIZES_FLAG 0x40000
typedef struct { size_guard *sizes; v2_id extent, value; uint32_t nominal; } pending_data;
static pending_data *pending_sizes;
static size_t pending_size_count, pending_size_capacity;
/* First-pass data operands remain in the AST until second-pass reduction.
 * Borrow them only across that interval. A source-time PC use selects which
 * numeric-width assumptions must survive final-name binding. */
typedef struct
{
    astnode *operand;
    size_t previous;
    int needed, valid;
} pc_width;
typedef struct
{
    size_t width;
    location use;
} pc_use;
static pc_width *pc_widths;
static pc_use *pc_uses;
static size_t pc_width_count, pc_width_capacity, pc_width_head[2];
static size_t pc_use_count, pc_use_capacity;
typedef struct
{
    uint64_t epoch;
    v2_id reference;
} captured_constant;
static captured_constant *captures;
static size_t capture_count, capture_capacity;
typedef struct
{
    const astnode *expression;
    v2_id definition;
} capture_task;
static capture_task *tasks;
static size_t task_count, task_capacity;
static int capturing, declarations_complete;
typedef struct structural_capture
{
    astnode *expression;
    v2_id definition;
    struct structural_capture *next;
} structural_capture;
static structural_capture *structural_captures;
typedef struct pending_name
{
    char *name;
    v2_id definition;
    location loc;
    int resolved;
    struct pending_name *next;
} pending_name;
static pending_name *pending;
static v2_id expression(astnode *, v2_id);
static v2_id symbol_anchor(symtab_entry *, location);
static void finish_captures(void);

static v2_location owned_location(location loc)
{
    v2_location result = v2_no_location();
    size_t i;
    if (!loc.file)
        return result;
    for (i = 0; i < file_count; ++i)
        if (!strcmp(v2_file_name(values, files[i]), loc.file))
            break;
    if (i == file_count)
    {
        if (file_count == file_capacity)
        {
            size_t cap = file_capacity ? file_capacity * 2 : 16;
            v2_id *p = realloc(files, cap * sizeof(*p));
            if (!p)
            {
                err(loc, "out of memory recording source file");
                return result;
            }
            files = p;
            file_capacity = cap;
        }
        files[file_count] = v2_file(values, loc.file);
        if (files[file_count] == V2_NONE)
            return result;
        ++file_count;
    }
    result.file = files[i];
    result.line = loc.first_line > 0 ? (uint32_t)loc.first_line : 1;
    result.column = loc.first_column > 0 ? (uint32_t)loc.first_column : 1;
    result.end_line = loc.last_line >= (int)result.line ? (uint32_t)loc.last_line : result.line;
    result.end_column = loc.last_column > 0 ? (uint32_t)loc.last_column : result.column;
    if (result.end_line == result.line && result.end_column < result.column)
        result.end_column = result.column;
    return result;
}
static location parser_location(v2_location loc)
{
    location result = loc_preserve;
    result.file = v2_file_name(values, loc.file);
    if (!result.file)
        result.file = "location unavailable";
    result.first_line = (int)loc.line;
    result.first_column = (int)loc.column;
    result.last_line = (int)loc.end_line;
    result.last_column = (int)loc.end_column;
    return result;
}
static unsigned element_width(datatype t)
{
    return t == WORD_DATATYPE ? 2 : t == DWORD_DATATYPE ? 4 : 1;
}

static v2_id capture_symbol(symtab_entry *s, location loc)
{
    captured_constant *c;
    v2_id id, ref;
    if (!s->tag)
    {
        if (capture_count == capture_capacity)
        {
            size_t cap = capture_capacity ? capture_capacity * 2 : 16;
            captured_constant *p = realloc(captures, cap * sizeof(*p));
            if (!p)
            {
                err(loc, "out of memory capturing definition");
                return V2_NONE;
            }
            captures = p;
            capture_capacity = cap;
        }
        captures[capture_count] = (captured_constant){0, V2_NONE};
        s->tag = (int)++capture_count;
    }
    ++s->ref_count;
    c = &captures[s->tag - 1];
    if (c->epoch == binding_epoch)
        return c->reference;
    id = v2_definition(values, s->id, owned_location(loc));
    ref = v2_reference(values, id, owned_location(loc));
    c->epoch = binding_epoch;
    c->reference = ref;
    if (task_count == task_capacity)
    {
        size_t cap = task_capacity ? task_capacity * 2 : 16;
        capture_task *p = realloc(tasks, cap * sizeof(*p));
        if (!p)
        {
            err(loc, "out of memory capturing expression");
            return V2_NONE;
        }
        tasks = p;
        task_capacity = cap;
    }
    tasks[task_count++] = (capture_task){s->def, id};
    return ref;
}
static int graph_ready(const astnode *n)
{
    const astnode *c;
    if (!n)
        return 1;
    switch (n->type)
    {
    case INTEGER_NODE:
    case STRING_NODE:
    case IDENTIFIER_NODE:
    case V2_REFERENCE_NODE:
        break;
    case ARITHMETIC_NODE:
        if (n->oper == BANK_OPERATOR && xasm_args.pure_binary)
            return 0;
        break;
    default:
        return 0;
    }
    for (c = n->first_child; c; c = c->next_sibling)
        if (!graph_ready(c))
            return 0;
    return 1;
}
static void finish_captures(void)
{
    size_t i;
    if (capturing)
        return;
    capturing = 1;
    for (i = 0; i < task_count && !err_count; ++i)
    {
        capture_task t = tasks[i];
        astnode *copy = NULL, *source = (astnode *)t.expression;
        v2_id value;
        if (!graph_ready(source))
        {
            /* A scalar alias can contain another alias whose definition still
             * needs structural lowering (for example a forward T::member).
             * Capture a private AST until its namespace exists; never borrow
             * a definition across a later assignment or .undef. */
            copy = reduce_expression_complete(astnode_clone(source, source->loc), FOLD_PC_NO);
            source = copy;
            if (!graph_ready(copy) && !declarations_complete && !err_count)
            {
                structural_capture *p = malloc(sizeof(*p));
                if (!p)
                {
                    err(copy->loc, "out of memory capturing structural expression");
                    astnode_finalize(copy);
                    break;
                }
                *p = (structural_capture){copy, t.definition, structural_captures};
                structural_captures = p;
                continue;
            }
        }
        value = err_count ? V2_NONE : expression(source, V2_NONE);
        if (value != V2_NONE)
            v2_bind(values, t.definition, value);
        astnode_finalize(copy);
    }
    task_count = 0;
    capturing = 0;
}
static void finish_structural_captures(void)
{
    declarations_complete = 1;
    while (structural_captures && !err_count)
    {
        structural_capture *p = structural_captures;
        v2_id value;
        structural_captures = p->next;
        p->expression = reduce_expression_complete(p->expression, FOLD_PC_NO);
        value = err_count ? V2_NONE : expression(p->expression, V2_NONE);
        finish_captures();
        if (value != V2_NONE)
            v2_bind(values, p->definition, value);
        astnode_finalize(p->expression);
        free(p);
    }
}

static astnode *frontend_fold_reference(astnode *n)
{
    v2_result r = v2_evaluate(values, (v2_id)n->param);
    astnode *replacement = NULL;
    if (r.state == V2_KNOWN)
    {
        if (r.value.kind == V2_INTEGER)
            replacement = astnode_create_integer(r.value.as.integer, n->loc);
        else if (r.value.kind == V2_STRING)
        {
            replacement = astnode_create(STRING_NODE, n->loc);
            replacement->string = malloc(r.value.as.string.length + 1);
            if (!replacement->string)
            {
                astnode_finalize(replacement);
                err(n->loc, "out of memory capturing string");
                return n;
            }
            if (r.value.as.string.length)
                memcpy(replacement->string, r.value.as.string.bytes, r.value.as.string.length);
            replacement->string[r.value.as.string.length] = 0;
            replacement->analysis_origin_id = (unsigned long)n->param + 1;
        }
    }
    else if (r.state == V2_INVALID)
    {
        err(n->loc, "%s", v2_error_message(r.diagnostic.code));
    }
    if (replacement)
    {
        astnode_replace(n, replacement);
        astnode_finalize(n);
        return replacement;
    }
    return n;
}
static v2_id frontend_string_value(astnode *n)
{
    v2_id value;
    if (n->analysis_origin_id)
        return (v2_id)(n->analysis_origin_id - 1);
    value = v2_string(values, n->string, strlen(n->string), owned_location(n->loc));
    if (value != V2_NONE)
        n->analysis_origin_id = (unsigned long)value + 1;
    else
        err(n->loc, "%s", v2_error_message(v2_unit_error(values).code));
    return value;
}
static astnode *frontend_string_fold(astnode *n)
{
    v2_id value = expression(n, V2_NONE);
    astnode *ref = astnode_create(V2_REFERENCE_NODE, n->loc);
    ref->param = value;
    astnode_replace(n, ref);
    astnode_finalize(n);
    return frontend_fold_reference(ref);
}
static astnode *frontend_capture_constant(astnode *use, symtab_entry *s)
{
    astnode *p = use->parent;
    v2_id value;
    /* Type/member bases still need their source spelling for structural
     * lowering. Ordinary operands and index arguments capture values. */
    if (s->def->type == INTEGER_NODE)
        return NULL;
    if (p && ((p->type == DOT_NODE && LHS(p) == use) || (p->type == INDEX_NODE && LHS(p) == use)))
        return NULL;
    if (!graph_ready(s->def))
        return NULL;
    reference_constant(use, s);
    value = capture_symbol(s, use->loc);
    finish_captures();
    if (value != V2_NONE)
    {
        astnode *n = astnode_create(V2_REFERENCE_NODE, use->loc);
        n->param = value;
        astnode_replace(use, n);
        astnode_finalize(use);
        return frontend_fold_reference(n);
    }
    return use;
}
static void resolve_pending(void)
{
    pending_name *p;
    ++binding_epoch;
    for (p = pending; p; p = p->next)
    {
        symtab_entry *s;
        if (p->resolved)
            continue;
        s = symtab_lookup(p->name);
        if (s && !(s->flags & EXTRN_FLAG))
        {
            v2_id value = V2_NONE;
            if (s->type == CONSTANT_SYMBOL)
                value = capture_symbol(s, p->loc);
            else if (s->type == LABEL_SYMBOL || s->type == VAR_SYMBOL || s->type == PROC_SYMBOL)
            {
                ++s->ref_count;
                value = v2_address(values, symbol_anchor(s, p->loc), owned_location(p->loc));
            }
            if (value != V2_NONE)
            {
                v2_bind(values, p->definition, value);
                p->resolved = 1;
            }
        }
        finish_captures();
    }
}
static int frontend_type_pc(astnode *n)
{
    astnode *c;
    if (n->type == CURRENT_PC_NODE)
    {
        err(n->loc, "$ is not allowed in type field dimensions");
        return 1;
    }
    for (c = n->first_child; c; c = c->next_sibling)
        if (frontend_type_pc(c))
            return 1;
    return 0;
}
static int frontend_storage(astnode *n)
{
    astnode *e = reduce_expression(RHS(n), FOLD_PC_NO);
    unsigned width = element_width(LHS(n)->datatype);
    if (astnode_is_type(e, INTEGER_NODE))
    {
        if (e->integer <= 0 || (uint64_t)(unsigned)e->integer * width > 65535)
        {
            err(n->loc, "operand out of range");
            return 0;
        }
        if (in_dataseg)
            dataseg_pc += e->integer * (int)width;
        else
            codeseg_pc += e->integer * (int)width;
    }
    else
        deferred_pc[in_dataseg] = 1;
    return 1;
}
/* Guards own captured expressions. Bind them from the live tree after pass 1,
 * when data-line captures and anonymous-reference renaming are both complete. */
static int local_operand(astnode *n)
{
    astnode *c;
    if (n->type == V2_REFERENCE_NODE)
    {
        size_t i;
        v2_result r = v2_evaluate(values, (v2_id)n->param);
        if (r.state == V2_KNOWN)
            return r.value.kind == V2_INTEGER;
        if (r.state == V2_INVALID)
            return 0;
        for (i = 0; i < r.dependency_count; ++i)
            if (r.dependencies[i].kind != V2_DEP_ANCHOR)
                return 0;
        return 1;
    }
    if (n->type == STRING_NODE)
        return 0;
    if (n->type == IDENTIFIER_NODE)
    {
        symtab_entry *s = symtab_lookup(n->ident);
        return s && !(s->flags & EXTRN_FLAG) &&
               (s->type == LABEL_SYMBOL || s->type == VAR_SYMBOL || s->type == PROC_SYMBOL);
    }
    if (n->type != INTEGER_NODE && n->type != CURRENT_PC_NODE && n->type != ARITHMETIC_NODE)
        return 0;
    for (c = n->first_child; c; c = c->next_sibling)
        if (!local_operand(c))
            return 0;
    return 1;
}
static int pending_data_handle(astnode *data)
{
    if (!(data->flags & DATA_PENDING_SIZES_FLAG))
    {
        if (pending_size_count == pending_size_capacity)
        {
            size_t capacity = pending_size_capacity ? pending_size_capacity * 2 : 16;
            pending_data *p = realloc(pending_sizes, capacity * sizeof(*p));
            if (!p)
                return 0;
            pending_sizes = p;
            pending_size_capacity = capacity;
        }
        if (pending_size_count >= LONG_MAX)
            return 0;
        pending_sizes[pending_size_count++] = (pending_data){NULL,V2_NONE,V2_NONE,0};
        data->param = (long)pending_size_count;
        data->flags |= DATA_PENDING_SIZES_FLAG;
    }
    return 1;
}
static int defer_size_guard(astnode *data, size_guard *g)
{
    if (!pending_data_handle(data)) return 0;
    g->pending_next = pending_sizes[data->param - 1].sizes;
    pending_sizes[data->param - 1].sizes = g;
    return 1;
}
static const astnode *label_data_start(const astnode *def)
{
    const astnode *n;
    if (!astnode_is_type(def, LABEL_NODE))
        return NULL;
    /* Leading labels and internal markers share a scan. A surviving source
     * directive still ends it, just as in frontend_sizeof_label below. */
    for (n = def->next_sibling; n; n = n->next_sibling)
        if (n->type != LABEL_NODE && n->type != TOMBSTONE_NODE &&
            n->type != PUSH_BRANCH_SCOPE_NODE && n->type != POP_BRANCH_SCOPE_NODE &&
            n->type != PUSH_MACRO_BODY_NODE && n->type != POP_MACRO_BODY_NODE)
            return n;
    return NULL;
}
static int contains_label_size(astnode *n, const astnode *data_start)
{
    astnode *c;
    if (n->type == SIZEOF_NODE)
    {
        astnode *id = LHS(n);
        symtab_entry *symbol;
        if (id->type == DATATYPE_NODE)
            id = LHS(id);
        symbol = astnode_is_type(id, IDENTIFIER_NODE) ? symtab_global_lookup(id->ident) : NULL;
        if (symbol && data_start && label_data_start(symbol->def) == data_start)
            return 1;
    }
    for (c = n->first_child; c; c = c->next_sibling)
        if (contains_label_size(c, data_start))
            return 1;
    return 0;
}
/* A label-size scan may count a forward numeric initializer provisionally,
 * just as it counts its data operand. The existing operand-kind guard rejects
 * a later string; the field's declared count must still be known at this use. */
static int initializer_size_hint(astnode *n, void *arg, astnode **next)
{
    size_t i;
    (void)arg; (void)next;
    for (i = 0; i < pending_size_count; ++i)
        if (pending_sizes[i].value == (v2_id)n->param &&
            v2_evaluate(values, (v2_id)n->param).state == V2_DEFERRED)
        {
            astnode *literal = astnode_create_integer((int)pending_sizes[i].nominal, n->loc);
            astnode_replace(n, literal);
            astnode_finalize(n);
            return 0;
        }
    return 0;
}
static astnode *frontend_sizeof_label(astnode *def, astnode *use)
{
    astnode *n = def->next_sibling;
    const astnode *data_start = label_data_start(def);
    uint64_t total = 0;
    int found = 0;
    for (; n; n = n->next_sibling)
    {
        if (n->type == DATA_NODE)
        {
            astnode *e;
            int operand_index = 1;
            unsigned width = element_width(LHS(n)->datatype);
            for (e = RHS(n); e; e = e->next_sibling, ++operand_index)
            {
                /* A sizeof of this label or another leading label sharing its
                 * data contributes an integer element without re-entering the
                 * same scan. Sizes of unrelated labels are still reduced.
                 * Keep the ordinary kind guard for the entire operand: adding
                 * a string to sizeof can still change the operand's extent. */
                astnode *copy = contains_label_size(e, data_start)
                                    ? NULL
                                    : reduce_expression_complete(astnode_clone(e, e->loc), FOLD_PC_NO);
                if (copy && copy->type == STRING_NODE)
                    total += strlen(copy->string) * width;
                else
                    total += width;
                /* Even a processed native statement can still hold a forward
                 * anonymous reference. Capture surviving statements after the
                 * whole first pass. User-typed statements are flattened/freed;
                 * leave those guards unbound and report size unknown here. */
                if (e->type != INTEGER_NODE && e->type != STRING_NODE)
                {
                    size_guard *g = calloc(1, sizeof(*g));
                    if (!g)
                    {
                        err(use->loc, "out of memory checking label size");
                        astnode_finalize(copy);
                        return NULL;
                    }
                    g->operand_index = operand_index;
                    g->string = copy && copy->type == STRING_NODE;
                    g->length = g->string ? strlen(copy->string) : 0;
                    g->use = use->loc;
                    g->name = strdup(def->ident);
                    if (!g->name)
                    {
                        free(g);
                        astnode_finalize(copy);
                        err(use->loc, "out of memory checking label size");
                        return NULL;
                    }
                    if (LHS(n)->datatype != USER_DATATYPE && !defer_size_guard(n, g))
                    {
                        free(g->name);
                        free(g);
                        astnode_finalize(copy);
                        err(use->loc, "out of memory checking label size");
                        return NULL;
                    }
                    g->next = size_guards;
                    size_guards = g;
                }
                astnode_finalize(copy);
            }
            found = 1;
        }
        else if (n->type == STORAGE_NODE || n->type == FIELD_PADDING_NODE)
        {
            astnode *holder = astnode_create(LIST_NODE, n->loc);
            astnode *copy;
            static const astnodeprocmap hints[] = {{V2_REFERENCE_NODE, initializer_size_hint}, {0, NULL}};
            astnode_add_child(holder, astnode_clone(RHS(n), n->loc));
            reduce_expression_complete(holder->first_child, FOLD_PC_NO);
            if (holder->first_child->type != INTEGER_NODE)
                astproc_walk(holder, NULL, hints);
            reduce_expression_complete(holder->first_child, FOLD_PC_NO);
            copy = astnode_remove_child_at(holder, 0);
            astnode_finalize(holder);
            unsigned width = n->type == FIELD_PADDING_NODE ? 1 : element_width(LHS(n)->datatype);
            if (!copy || copy->type != INTEGER_NODE)
            {
                astnode_finalize(copy);
                return NULL;
            }
            total += (uint64_t)(unsigned)copy->integer * width;
            astnode_finalize(copy);
            found = 1;
        }
        else if (n->type == LABEL_NODE)
        {
            if (found)
                break;
        }
        else if (n->type != TOMBSTONE_NODE && n->type != PUSH_BRANCH_SCOPE_NODE &&
                 n->type != POP_BRANCH_SCOPE_NODE && n->type != PUSH_MACRO_BODY_NODE &&
                 n->type != POP_MACRO_BODY_NODE)
            break;
    }
    if (!found)
        return NULL;
    {
        astnode *c = astnode_create_integer(expr_i32((uint32_t)total), use->loc);
        astnode_replace(use, c);
        astnode_finalize(use);
        return c;
    }
}
static int fixed_data_width(astnode *);
static int bind_initializer_extent(astnode *data)
{
    if ((data->flags & DATA_PENDING_SIZES_FLAG) && pending_sizes[data->param - 1].extent != V2_NONE)
        {
            astnode *operand;
            v2_location loc = owned_location(data->loc);
            v2_id total = v2_extent_literal(values, 0, loc);
            for (operand = RHS(data); operand; operand = operand->next_sibling)
            {
                v2_id length;
                if (fixed_data_width(operand) || operand->type == FORWARD_BRANCH_NODE || operand->type == BACKWARD_BRANCH_NODE) length = v2_extent_literal(values, 1, loc);
                else if (expression_uses_pc(operand))
                {
                    err(operand->loc, "initializer string length depends on layout");
                    return 0;
                }
                else length = v2_extent_string(values, expression(operand, V2_NONE), owned_location(operand->loc));
                total = v2_extent_sum(values, total, length, loc);
            }
            total = v2_extent_repeat(values, total,
                v2_extent_literal(values, element_width(LHS(data)->datatype), loc), loc);
            v2_bind(values, pending_sizes[data->param - 1].extent, total);
            pending_sizes[data->param - 1].extent = V2_NONE;
        }
    return 1;
}
static astnode *frontend_initializer_size(astnode *data)
{
    v2_location loc = owned_location(data->loc);
    v2_id definition, value;
    astnode *ref, *operand;
    uint32_t nominal = 0;
    if (!pending_data_handle(data))
    {
        err(data->loc, "out of memory recording initializer extent");
        return astnode_create_integer(0, data->loc);
    }
    definition = v2_definition(values, "initializer extent", loc);
    pending_sizes[data->param - 1].extent = definition;
    value = v2_extent_integer(values, v2_reference(values, definition, loc), loc);
    for (operand = RHS(data); operand; operand = operand->next_sibling)
        nominal += (uint32_t)(operand->type == STRING_NODE ? strlen(operand->string) : 1) * element_width(LHS(data)->datatype);
    pending_sizes[data->param - 1].value = value;
    pending_sizes[data->param - 1].nominal = nominal;
    ref = astnode_create(V2_REFERENCE_NODE, data->loc);
    ref->param = value;
    return ref;
}
static int fixed_data_width(astnode *);
static int capture_size_guard_operands(astnode *data, void *arg, astnode **next)
{
    size_guard *g;
    (void)arg;
    (void)next;
    if (data->flags & DATA_PENDING_SIZES_FLAG)
    {
        for (g = pending_sizes[data->param - 1].sizes; g; g = g->pending_next)
        {
            astnode *e = astnode_get_child(data, g->operand_index);
            g->operand = astnode_clone(e, e->loc);
        }
        pending_sizes[data->param - 1].sizes = NULL;
        data->flags &= ~DATA_PENDING_SIZES_FLAG;
    }
    return 0;
}
static int fixed_data_width(astnode *e)
{
    if (e->type == INTEGER_NODE || e->type == CURRENT_PC_NODE)
        return 1;
    if (e->type == ARITHMETIC_NODE)
        return e->oper != PLUS_OPERATOR || (fixed_data_width(LHS(e)) && fixed_data_width(RHS(e)));
    if (e->type == IDENTIFIER_NODE)
    {
        symtab_entry *s = symtab_lookup(e->ident);
        return s && !(s->flags & EXTRN_FLAG) &&
               (s->type == LABEL_SYMBOL || s->type == PROC_SYMBOL || s->type == VAR_SYMBOL);
    }
    if (e->type == V2_REFERENCE_NODE)
    {
        v2_result r = v2_evaluate(values, (v2_id)e->param);
        size_t i;
        if (r.state == V2_KNOWN)
            return r.value.kind == V2_INTEGER;
        if (r.state == V2_INVALID)
            return 1; /* already diagnosed, never published */
        for (i = 0; i < r.dependency_count; ++i)
            if (r.dependencies[i].kind == V2_DEP_DEFINITION)
            {
                pending_name *p;
                symtab_entry *s = NULL;
                for (p = pending; p; p = p->next)
                    if (p->definition == r.dependencies[i].id)
                    {
                        s = symtab_lookup(p->name);
                        break;
                    }
                if (!s || (s->flags & EXTRN_FLAG) ||
                    (s->type != LABEL_SYMBOL && s->type != VAR_SYMBOL && s->type != PROC_SYMBOL))
                    return 0;
            }
        return 1;
    }
    return 0;
}
static void remember_pc_width(astnode *e)
{
    /* Other integer operators cannot grow into strings. Already reduced
     * literals have captured their kind and value, independent of .undef. */
    if (e->type != IDENTIFIER_NODE && e->type != V2_REFERENCE_NODE &&
        !(e->type == ARITHMETIC_NODE && e->oper == PLUS_OPERATOR))
        return;
    if (pc_width_count == pc_width_capacity)
    {
        size_t capacity = pc_width_capacity ? pc_width_capacity * 2 : 32;
        pc_width *p = realloc(pc_widths, capacity * sizeof(*p));
        if (!p)
        {
            err(e->loc, "out of memory checking data widths");
            return;
        }
        pc_widths = p;
        pc_width_capacity = capacity;
    }
    pc_widths[pc_width_count] = (pc_width){e, pc_width_head[in_dataseg], 0, 0};
    pc_width_head[in_dataseg] = ++pc_width_count;
}
static void frontend_capture_pc(location use)
{
    if (!xasm_args.pure_binary)
    {
        err(use, "cannot use $ before final layout in object output");
        return;
    }
    if (!pc_width_head[in_dataseg])
        return;
    if (pc_use_count == pc_use_capacity)
    {
        size_t capacity = pc_use_capacity ? pc_use_capacity * 2 : 16;
        pc_use *p = realloc(pc_uses, capacity * sizeof(*p));
        if (!p)
        {
            err(use, "out of memory checking source-time PC");
            return;
        }
        pc_uses = p;
        pc_use_capacity = capacity;
    }
    pc_uses[pc_use_count++] = (pc_use){pc_width_head[in_dataseg], use};
}
static void frontend_reset_pc(void)
{
    pc_width_head[in_dataseg] = 0;
}
static void clear_pc_widths(void)
{
    free(pc_widths);
    pc_widths = NULL;
    pc_width_count = pc_width_capacity = pc_width_head[0] = pc_width_head[1] = 0;
    free(pc_uses);
    pc_uses = NULL;
    pc_use_count = pc_use_capacity = 0;
}
static void check_pc_widths(void)
{
    size_t i, at;
    if (!pc_use_count)
        return;
    for (i = 0; i < pc_use_count; ++i)
        for (at = pc_uses[i].width; at && !pc_widths[at - 1].needed;
             at = pc_widths[at - 1].previous)
            pc_widths[at - 1].needed = 1;
    /* Previous entries always precede their users, even across segment changes
     * and absolute origins. Check each selected operand once, then reuse the
     * validity of its entire prefix for each PC use. */
    for (i = 0; i < pc_width_count; ++i)
        if (pc_widths[i].needed)
        {
            pc_width *p = &pc_widths[i];
            astnode *copy = reduce_expression_complete(
                astnode_clone(p->operand, p->operand->loc), FOLD_PC_NO);
            p->valid = local_operand(copy) && (!p->previous || pc_widths[p->previous - 1].valid);
            astnode_finalize(copy);
        }
    for (i = 0; i < pc_use_count; ++i)
        if (!pc_widths[pc_uses[i].width - 1].valid)
            err(pc_uses[i].use,
                "cannot use $ before final layout; preceding data no longer has a fixed numeric width");
}
static int frontend_data(astnode *data)
{
    static const astnodeprocmap local_names[] = {{LOCAL_ID_NODE, globalize_local}, {0, NULL}};
    astnode *expr, *next;
    unsigned width = element_width(LHS(data)->datatype);
    int bytes = 0;
    if (LHS(data)->datatype == CHAR_DATATYPE)
    {
        if (map_count == map_capacity)
        {
            size_t cap = map_capacity ? map_capacity * 2 : 16;
            source_map *p = realloc(maps, cap * sizeof(*p));
            if (!p)
            {
                err(data->loc, "out of memory capturing character map");
                return 0;
            }
            maps = p;
            map_capacity = cap;
        }
        memcpy(maps[map_count].map, charmap, 256);
        data->analysis_origin_id = ++map_count;
    }
    for (expr = RHS(data); expr; expr = next)
    {
        next = expr->next_sibling;
        /* The generic first-pass walk normally renames these after the data
         * handler. Resolve that same namespace before judging the width; a
         * backward local label already has a fixed integer contribution. */
        astproc_walk(expr, NULL, local_names);
        expr = reduce_expression(expr, FOLD_PC_NO);
        if (astnode_is_type(expr, NULL_NODE) || astnode_is_type(expr, STRUC_NODE))
        {
            err(expr->loc, "omitted elements are not supported in built-in array initializers");
            return 0;
        }
        if (astnode_is_type(expr, STRING_NODE))
            bytes += (int)(strlen(expr->string) * width);
        else
        {
            bytes += (int)width;
            if (!fixed_data_width(expr))
                deferred_pc[in_dataseg] = 1;
            else
                remember_pc_width(expr);
        }
    }
    if (!bind_initializer_extent(data)) return 0;
    if (in_dataseg)
        dataseg_pc += bytes;
    else
        codeseg_pc += bytes;
    return 1;
}
static int finish_data(astnode *n, void *arg, astnode **next)
{
    astnode *e, *following;
    unsigned width = element_width(LHS(n)->datatype);
    size_t size = 0, at = 0;
    unsigned char *bytes;
    const unsigned char *map = n->analysis_origin_id ? maps[n->analysis_origin_id - 1].map : NULL;
    (void)arg;
    (void)next;
    for (e = RHS(n); e; e = following)
    {
        following = e->next_sibling;
        reduce_expression_complete(e, FOLD_PC_NO);
    }
    /* All source-time sizeof scans have finished. Compact fully known data
     * before building the IR, so its per-byte AST nodes need not overlap the
     * value/layout tables. Mixed operands keep their individual fixups. */
    /* CHAR's numeric warning policy differs between code and RAM. Leave it
     * to lower_data, where the segment and the captured map are both known. */
    if (LHS(n)->datatype == CHAR_DATATYPE)
        return 0;
    for (e = RHS(n); e; e = e->next_sibling)
    {
        size_t count;
        if (e->type != INTEGER_NODE && e->type != STRING_NODE)
            return 0;
        count = e->type == STRING_NODE ? strlen(e->string) : 1;
        if (count > ((size_t)INT_MAX - size) / width)
            return 0; /* retain the general checked-extent path */
        size += count * width;
    }
    bytes = size ? malloc(size) : NULL;
    if (size && !bytes)
    {
        err(n->loc, "out of memory capturing literal data");
        return 0;
    }
    for (e = RHS(n); e; e = e->next_sibling)
    {
        size_t i, count = e->type == STRING_NODE ? strlen(e->string) : 1;
        if (e->type == INTEGER_NODE && ((width == 1 && !IS_BYTE_VALUE(e->integer)) ||
                                        (width == 2 && !IS_WORD_VALUE(e->integer))))
            warn(e->loc, "operand out of range; truncated");
        for (i = 0; i < count; ++i)
        {
            unsigned b;
            uint32_t value =
                e->type == INTEGER_NODE ? (uint32_t)e->integer : (unsigned char)e->string[i];
            if (e->type == STRING_NODE && map)
                value = map[value];
            for (b = 0; b < width; ++b)
                bytes[at++] = (unsigned char)(value >> (8 * b));
        }
    }
    while (n->first_child)
        astnode_finalize(n->first_child);
    n->type = BINARY_NODE;
    n->binary.data = bytes;
    n->binary.size = (int)size;
    return 0;
}
static int finish_storage(astnode *n, void *arg, astnode **next)
{
    (void)arg;
    (void)next;
    reduce_expression_complete(RHS(n), FOLD_PC_NO);
    return 0;
}
static v2_id symbol_anchor(symtab_entry *e, location loc)
{
    v2_id a;
    if (e->tag)
        return (v2_id)(e->tag - 1);
    a = v2_anchor(values, owned_location(loc));
    if (a != V2_NONE)
        e->tag = (int)a + 1;
    return a;
}
static v2_id expression(astnode *e, v2_id pc)
{
    static const v2_operator operators[] = {
        V2_ADD,    V2_SUBTRACT,  V2_MULTIPLY,   V2_DIVIDE,        V2_REMAINDER,  V2_BIT_AND,
        V2_BIT_OR, V2_BIT_XOR,   V2_SHIFT_LEFT, V2_SHIFT_RIGHT,   V2_LESS,       V2_GREATER,
        V2_EQUAL,  V2_NOT_EQUAL, V2_LESS_EQUAL, V2_GREATER_EQUAL, V2_COMPLEMENT, V2_NOT,
        V2_LOW,    V2_HIGH,      V2_NEGATE};
    v2_location loc;
    if (!e)
        return V2_NONE;
    loc = owned_location(e->loc);
    switch (e->type)
    {
    case V2_REFERENCE_NODE:
        return (v2_id)e->param;
    case INTEGER_NODE:
        return v2_integer(values, e->integer, loc);
    case STRING_NODE:
        return frontend_string_value(e);
    case CURRENT_PC_NODE:
        return v2_address(values, pc, loc);
    case IDENTIFIER_NODE:
    {
        symtab_entry *s = symtab_lookup(e->ident);
        if (declarations_complete && s && !(s->flags & EXTRN_FLAG) &&
            (s->type == LABEL_SYMBOL || s->type == VAR_SYMBOL || s->type == PROC_SYMBOL))
        {
            ++s->ref_count;
            return v2_address(values, symbol_anchor(s, e->loc), loc);
        }
        if (s && s->type == CONSTANT_SYMBOL && !(s->flags & EXTRN_FLAG))
            return capture_symbol(s, e->loc);
        {
            pending_name *p;
            for (p = pending; p; p = p->next)
                if (!strcmp(p->name, e->ident))
                    break;
            if (!p)
            {
                p = calloc(1, sizeof(*p));
                if (!p)
                {
                    err(e->loc, "out of memory capturing forward name");
                    return V2_NONE;
                }
                p->name = strdup(e->ident);
                if (!p->name)
                {
                    free(p);
                    err(e->loc, "out of memory capturing forward name");
                    return V2_NONE;
                }
                p->loc = e->loc;
                p->definition = v2_definition(values, p->name, loc);
                p->next = pending;
                pending = p;
            }
            return v2_reference(values, p->definition, loc);
        }
    }
    case ARITHMETIC_NODE:
    {
        v2_id lhs, rhs;
        if (e->oper == BANK_OPERATOR)
        {
            if (program)
                return v2_program_bank(program, expression(LHS(e), pc), loc);
            err(e->loc, "bank operator is not supported in pure binary output");
            return V2_NONE;
        }
        lhs = expression(LHS(e), pc);
        if (RHS(e))
        {
            rhs = expression(RHS(e), pc);
            return v2_binary(values, operators[e->oper], lhs, rhs, loc);
        }
        return v2_unary(values, operators[e->oper], lhs, loc);
    }
    default:
        err(e->loc, "expression does not evaluate to literal");
        return V2_NONE;
    }
}
static void flush_literals(void)
{
    if (literal_count)
    {
        if (program)
            v2_program_raw(program, literal_bytes, literal_count, owned_location(literal_location));
        else
            v2_layout_raw(layout, literal_bytes, literal_count, owned_location(literal_location));
        literal_count = 0;
    }
}
static unsigned char *append_literals(size_t size, location loc)
{
    unsigned char *bytes;
    if (size > UINT32_MAX - literal_count)
    {
        err(loc, "data extent out of range");
        return NULL;
    }
    if (size + literal_count > literal_capacity)
    {
        size_t capacity = literal_capacity ? literal_capacity : 256;
        while (capacity < size + literal_count)
        {
            if (capacity > SIZE_MAX / 2)
            {
                capacity = size + literal_count;
                break;
            }
            capacity *= 2;
        }
        bytes = realloc(literal_bytes, capacity);
        if (!bytes)
        {
            err(loc, "out of memory lowering literal data");
            return NULL;
        }
        literal_bytes = bytes;
        literal_capacity = capacity;
    }
    if (!literal_count)
        literal_location = loc;
    bytes = literal_bytes ? literal_bytes + literal_count : NULL;
    literal_count += size;
    return bytes;
}
/* Plain output coalesces consecutive literal data, as the v1 frontend does.
 * Labels, instructions, placement changes, and dynamic operands flush the run.
 * Dynamic operands retain individual fixups and their use anchors. */
static void lower_data(astnode *n, int ram)
{
    astnode *e = RHS(n);
    unsigned width = element_width(LHS(n)->datatype);
    const unsigned char *map = n->analysis_origin_id ? maps[n->analysis_origin_id - 1].map : NULL;
    while (e)
    {
        if (!ram && (e->type == INTEGER_NODE || e->type == STRING_NODE))
        {
            astnode *end = e;
            size_t size = 0, at = 0;
            unsigned char *bytes;
            while (end && (end->type == INTEGER_NODE || end->type == STRING_NODE))
            {
                size_t count = end->type == STRING_NODE ? strlen(end->string) : 1;
                if (count > (SIZE_MAX - size) / width)
                {
                    err(end->loc, "data extent out of range");
                    return;
                }
                size += count * width;
                end = end->next_sibling;
            }
            bytes = append_literals(size, e->loc);
            if (err_count)
                return;
            for (; e != end; e = e->next_sibling)
            {
                size_t i, count = e->type == STRING_NODE ? strlen(e->string) : 1;
                if (e->type == INTEGER_NODE && ((width == 1 && !IS_BYTE_VALUE(e->integer)) ||
                                                (width == 2 && !IS_WORD_VALUE(e->integer))))
                    warn(e->loc, "operand out of range; truncated");
                for (i = 0; i < count; ++i)
                {
                    unsigned b;
                    uint32_t value = e->type == INTEGER_NODE ? (uint32_t)e->integer
                                                             : (unsigned char)e->string[i];
                    if (e->type == STRING_NODE && map)
                        value = map[value];
                    for (b = 0; b < width; ++b)
                        bytes[at++] = (unsigned char)(value >> (8 * b));
                }
            }
        }
        else
        {
            v2_location use = owned_location(e->loc);
            flush_literals();
            v2_id pc = (ram || expression_uses_pc(e)) ? v2_anchor(values, use) : V2_NONE;
            v2_id expr = expression(e, pc);
            if (ram)
            {
                v2_result r = v2_evaluate(values, expr);
                uint64_t count =
                    r.state == V2_KNOWN && r.value.kind == V2_STRING ? r.value.as.string.length : 1;
                /* RAM consumes an extent without emitting the operand. Still
                 * validate deferred values after placement: an address-based
                 * string or invalid arithmetic must not disappear here. */
                if (r.state == V2_INVALID)
                {
                    err(e->loc, "%s", v2_error_message(r.diagnostic.code));
                    return;
                }
                if (r.state == V2_DEFERRED && !program)
                {
                    if (ram_value_count == ram_value_capacity)
                    {
                        size_t capacity = ram_value_capacity ? ram_value_capacity * 2 : 16;
                        ram_value *p = realloc(ram_values, capacity * sizeof(*p));
                        if (!p)
                        {
                            err(e->loc, "out of memory recording RAM operand");
                            return;
                        }
                        ram_values = p;
                        ram_value_capacity = capacity;
                    }
                    ram_values[ram_value_count++] = (ram_value){expr, use};
                }
                else if (r.state == V2_KNOWN && LHS(n)->datatype != CHAR_DATATYPE && r.value.kind == V2_INTEGER &&
                         ((width == 1 && !IS_BYTE_VALUE(r.value.as.integer)) ||
                          (width == 2 && !IS_WORD_VALUE(r.value.as.integer))))
                    warn(e->loc, "operand out of range; truncated");
                if (program)
                {
                    if (r.state == V2_DEFERRED)
                        v2_program_require(program, expr, use);
                    v2_program_ram_storage(program, pc, v2_extent_literal(values, 0, use), use);
                }
                else
                    v2_layout_ram(layout, pc, v2_extent_literal(values, count * width, use), 0, 0, use);
            }
            else if (program)
                v2_program_data(program, pc, expr, width, map, use);
            else
                v2_layout_data(layout, pc, expr, width, !fixed_data_width(e), map, use);
            e = e->next_sibling;
        }
    }
}
static void lower(astnode *root)
{
    astnode *n, *next;
    int ram = 0;
    size_t incoming = 0;
    /* The statement stream is already flat. Reserve a conservative common
     * case batch so large projects do not repeatedly copy the value table.
     * This is only a capacity hint: complex operands grow normally. */
    for (n = root->first_child; n; n = n->next_sibling)
    {
        size_t extra = n->type == INSTRUCTION_NODE ? 3 : n->type == LABEL_NODE ? 2 : 0;
        if (extra > SIZE_MAX - incoming)
        {
            err(n->loc, "out of memory reserving expression table");
            return;
        }
        incoming += extra;
    }
    if (!v2_reserve_expressions(values, incoming))
    {
        err(root->loc, "out of memory reserving expression table");
        return;
    }
    for (n = root->first_child; n && !err_count; n = next)
    {
        v2_location loc = owned_location(n->loc);
        v2_id pc, expr, extent;
        next = n->next_sibling;
        /* Fully captured non-branch instructions have no link-time fixup.
         * Keep them in the fixed-byte run, just as literal data. Labels and
         * instructions that depend on placement still flush the run. */
        int fixed_instruction = program && n->type == INSTRUCTION_NODE &&
            (opcode_length(n->instr.opcode) == 1 ||
             (astnode_is_type(LHS(n), INTEGER_NODE) &&
              opcode_addressing_mode(n->instr.opcode) != RELATIVE_MODE));
        if (n->type != DATA_NODE && n->type != BINARY_NODE && !fixed_instruction)
            flush_literals();
        switch (n->type)
        {
        case DATASEG_NODE:
            ram = 1;
            break;
        case CODESEG_NODE:
            ram = 0;
            break;
        case ORG_NODE:
            if (!astnode_is_type(LHS(n), INTEGER_NODE))
            {
                err(n->loc, "org address does not evaluate to literal");
                break;
            }
            if (ram)
                v2_layout_ram_origin(layout, (uint32_t)LHS(n)->integer, loc);
            else
                v2_layout_origin(layout, (uint32_t)LHS(n)->integer, loc);
            break;
        case LABEL_NODE:
        {
            symtab_entry *s = symtab_lookup(n->ident);
            /* .undef can retire a declaration while its bytes remain in the
             * stream. v1 resolves address operands against the final binding;
             * only that declaration places the live symbol's anchor. */
            int live = s && s->source_label == n;
            if (!live && !program)
                break;
            pc = live ? symbol_anchor(s, n->loc) : v2_anchor(values, loc);
            if (program && ram)
                v2_program_ram_label(program, pc, live && (s->flags & ALIGN_FLAG) ? (unsigned)s->align : 0,
                                     live && (s->flags & ZEROPAGE_FLAG), loc);
            else if (program)
                v2_program_mark(program, pc, loc);
            else if (ram)
                v2_layout_ram(layout, pc, v2_extent_literal(values, 0, loc), 0, 0, loc);
            else
                v2_layout_mark(layout, pc, loc);
            break;
        }
        case INSTRUCTION_NODE:
            if (fixed_instruction)
            {
                unsigned char opcode = n->instr.opcode;
                uint32_t operand = LHS(n) ? (uint32_t)LHS(n)->integer : 0;
                unsigned width, j;
                unsigned char *bytes;
                if (!n->instr.mnemonic.wide && operand <= 255) {
                    unsigned char narrow = opcode_zp_equiv(opcode, opcode_addressing_mode(opcode));
                    if (narrow != 0xff) opcode = narrow;
                }
                width = (unsigned)opcode_length(opcode);
                bytes = append_literals(width, n->loc);
                if (!bytes) break;
                bytes[0] = opcode;
                for (j = 1; j < width; j++) { bytes[j] = (unsigned char)operand; operand >>= 8; }
                break;
            }
            pc = LHS(n) && expression_uses_pc(LHS(n)) ? v2_anchor(values, loc) : V2_NONE;
            if (program)
                v2_program_instruction(program, pc, n->instr.opcode, expression(LHS(n), pc),
                                       n->instr.mnemonic.wide, loc);
            else if (astnode_is_type(LHS(n), INTEGER_NODE))
                v2_layout_literal_instruction(layout, pc, n->instr.opcode, LHS(n)->integer,
                                              n->instr.mnemonic.wide, 1, loc);
            else
            {
                expr = expression(LHS(n), pc);
                v2_layout_instruction(layout, pc, n->instr.opcode, expr, n->instr.mnemonic.wide, 0,
                                      loc);
            }
            break;
        case DATA_NODE:
            lower_data(n, ram);
            break;
        case STORAGE_NODE:
            /* An unused PC anchor would give otherwise unnamed leading RAM
             * storage an allocation identity that v1 does not assign it. */
            pc = program && ram && !expression_uses_pc(n) ? V2_NONE : v2_anchor(values, loc);
            expr = expression(RHS(n), pc);
            extent = v2_extent_count(values, expr, astnode_get_child(n, 2) ? 0 : 1, 65535, loc);
            extent = v2_extent_bound(
                values,
                v2_extent_repeat(values, extent,
                                 v2_extent_literal(values, element_width(LHS(n)->datatype), loc),
                                 loc),
                0, 65535, loc);
            if (astnode_get_child(n, 2))
            {
                v2_id count = expression(astnode_get_child(n, 2), pc);
                unsigned min = (unsigned)astnode_get_child(n, 4)->integer;
                v2_id check = v2_extent_count(values, count, min, 65535, loc);
                /* Validate both the count and byte extent; add a zero extent
                 * multiplied by the checked count without discarding checks. */
                v2_id zero = v2_extent_literal(values, 0, loc);
                extent =
                    v2_extent_sum(values, v2_extent_repeat(values, check, zero, loc), extent, loc);
            }
            if (program && ram)
                v2_program_ram_storage(program, pc, extent, loc);
            else if (program)
                v2_program_storage(program, pc, extent, loc);
            else if (ram)
                v2_layout_ram(layout, pc, extent, 0, 0, loc);
            else
                v2_layout_storage(layout, pc, extent, loc);
            break;
        case BINARY_NODE:
            if (program && ram)
                v2_program_ram_storage(program, V2_NONE,
                    v2_extent_literal(values, 0, loc), loc);
            else if (ram)
                v2_layout_ram(layout, v2_anchor(values, loc),
                              v2_extent_literal(values, (uint32_t)n->binary.size, loc), 0, 0, loc);
            else
            {
                unsigned char *dest = append_literals((size_t)n->binary.size, n->loc);
                if (!err_count && n->binary.size)
                    memcpy(dest, n->binary.data, (size_t)n->binary.size);
            }
            break;
        case STRUC_DECL_NODE:
        case UNION_DECL_NODE:
        case ENUM_DECL_NODE:
        case RECORD_DECL_NODE:
        case PUBLIC_NODE:
        case ALIGN_NODE:
        case CHARMAP_NODE:
        case INCSRC_NODE:
        case INCBIN_NODE:
        case TOMBSTONE_NODE:
        case PUSH_BRANCH_SCOPE_NODE:
        case POP_BRANCH_SCOPE_NODE:
        case PUSH_MACRO_BODY_NODE:
        case POP_MACRO_BODY_NODE:
        case NULL_NODE:
            break;
        default:
            err(n->loc, "unsupported frontend statement `%s'", astnode_type_to_string(n->type));
            break;
        }
        /* Structural reduction and sizeof scans are finished. These statement
         * operands have been copied into the owned IR and no symbol borrows
         * them. Release them as we lower, rather than keeping two complete
         * representations live. Declaration nodes stay until namespace teardown. */
        if (n->type == INSTRUCTION_NODE || n->type == DATA_NODE || n->type == STORAGE_NODE ||
            n->type == BINARY_NODE)
            astnode_finalize(n);
    }
    flush_literals();
}
/* The table is an AVL tree and captures do not mutate its membership. Visit
 * exports directly in the same name order as symtab_list, without allocating
 * and looking up a second copy of every (mostly private) identifier. */
static void export_symbols(symtab_entry *s, location fallback)
{
    if (!s) return;
    export_symbols(s->left, fallback);
    if ((s->flags & PUBLIC_FLAG) && !(s->flags & EXTRN_FLAG))
    {
        location loc = s->has_assignment_loc ? s->assignment_loc : s->def ? s->def->loc : fallback;
        v2_id value = s->type == CONSTANT_SYMBOL ? capture_symbol(s, loc)
            : v2_address(values, symbol_anchor(s, loc), owned_location(loc));
        v2_program_export(program, s->id, value, owned_location(loc));
    }
    export_symbols(s->right, fallback);
}
static void frontend_export_symbols(location loc)
{
    if (program)
    {
        symbol_ident_list constants = {0};
        int *refs = NULL, i;
        /* Exports retain address dependencies, but are not source uses for
         * W0201. Structural capture can also touch private constant chains. */
        if (xasm_args.warn_unused_equ)
        {
            if (symtab_list_type(CONSTANT_SYMBOL, &constants) < 0)
            {
                err(loc, "out of memory preserving equate usage");
                return;
            }
            refs = constants.size ? calloc((size_t)constants.size, sizeof(*refs)) : NULL;
            if (constants.size && !refs)
            {
                symtab_list_finalize(&constants);
                err(loc, "out of memory preserving equate usage");
                return;
            }
            for (i = 0; i < constants.size; ++i)
                refs[i] = symtab_lookup(constants.idents[i])->ref_count;
        }
        export_symbols(symtab_tos()->root, loc);
        finish_captures();
        for (i = 0; i < constants.size; ++i)
            symtab_lookup(constants.idents[i])->ref_count = refs[i];
        free(refs);
        symtab_list_finalize(&constants);
    }
}
void v2_frontend_process(astnode *root)
{
    static const astnodeprocmap size_map[] = {{DATA_NODE, capture_size_guard_operands}, {0, NULL}};
    static const astnodeprocmap map[] = {{INSTRUCTION_NODE, translate_instruction},
                                         {DATA_NODE, finish_data},
                                         {STORAGE_NODE, finish_storage},
                                         {FIELD_PADDING_NODE, finalize_field_padding},
                                         {VAR_DECL_NODE, flatten_var_decl},
                                         {PROC_NODE, flatten_proc},
                                         {STRUC_DECL_NODE, noop},
                                         {UNION_DECL_NODE, noop},
                                         {ENUM_DECL_NODE, noop},
                                         {RECORD_DECL_NODE, noop},
                                         {0, NULL}};
    values = v2_unit_create();
    if (!values)
    {
        err(root->loc, "out of memory creating v2 unit");
        return;
    }
    if (xasm_args.pure_binary)
        layout = v2_layout_create(values, V2_BINARY_LAYOUT);
    else
        program = v2_program_create(values);
    if (!layout && !program)
    {
        err(root->loc, "out of memory creating v2 layout");
        return;
    }
    astproc_first_pass(root);
    if (err_count || symtab_failed())
        return;
    if (pending_size_count)
        astproc_walk(root, NULL, size_map);
    finish_structural_captures();
    resolve_pending();
    check_pc_widths();
    clear_pc_widths();
    if (err_count)
        return;
    astproc_second_pass(root);
    if (err_count || symtab_failed())
        return;
    {
        size_guard *g;
        for (g = size_guards; g; g = g->next)
        {
            if (!g->operand)
            {
                err(g->use, "size of `%s' is unknown", g->name);
                continue;
            }
            g->operand = reduce_expression_complete(g->operand, FOLD_PC_NO);
            if (g->string ? (g->operand->type != STRING_NODE ||
                             strlen(g->operand->string) != g->length)
                          : !local_operand(g->operand))
                err(g->use, "size of `%s' is unknown", g->name);
        }
    }
    if (err_count)
        return;
    finalizing_expressions = 1;
    astproc_walk(root, NULL, map);
    warn_unused_equates();
    if (err_count || symtab_failed())
        return;
    lower(root);
    finish_captures();
    resolve_pending();
    if (err_count)
        return;
}
void v2_frontend_finish(void)
{
    const v2_layout_fact *facts;
    size_t count, i;
    if (program)
    {
        v2_object *object = v2_program_object(program);
        if (!object || !v2_object_encode(object, &object_bytes, &object_length))
            err(parser_location(v2_no_location()), "%s", object ?
                v2_object_error_message(v2_object_error_at(object)) : v2_program_error(program));
        v2_object_destroy(object);
        return;
    }
    if (!v2_layout_run(layout))
    {
        v2_layout_diagnostic d = v2_layout_error_at(layout);
        err(parser_location(d.location), "%s",
            d.value_error.code ? v2_error_message(d.value_error.code)
                               : v2_layout_error_message(d.code));
        return;
    }
    for (i = 0; i < ram_value_count; ++i)
    {
        v2_result r = v2_evaluate(values, ram_values[i].expression);
        if (r.state != V2_KNOWN || r.value.kind != V2_INTEGER)
            err(parser_location(ram_values[i].location), "%s",
                r.state == V2_INVALID ? v2_error_message(r.diagnostic.code)
                                      : "RAM data operand does not evaluate to an integer");
    }
    facts = v2_layout_facts(layout, &count);
    for (i = 0; i < count; ++i)
        if (facts[i].warning)
            warn(parser_location(facts[i].location), "operand out of range; truncated");
}
void v2_frontend_write(FILE *stream)
{
    size_t length;
    const unsigned char *bytes;
    if (program) { bytes = object_bytes; length = object_length; }
    else bytes = v2_layout_bytes(layout, &length);
    if (length && fwrite(bytes, 1, length, stream) != length)
        ++err_count;
}
void v2_frontend_clear(void)
{
    while (structural_captures)
    {
        structural_capture *p = structural_captures;
        structural_captures = p->next;
        astnode_finalize(p->expression);
        free(p);
    }
    while (pending)
    {
        pending_name *p = pending;
        pending = p->next;
        free(p->name);
        free(p);
    }
    free(captures);
    captures = NULL;
    capture_count = capture_capacity = 0;
    free(tasks);
    tasks = NULL;
    task_count = task_capacity = 0;
    capturing = declarations_complete = 0;
    binding_epoch = 1;
    while (size_guards)
    {
        size_guard *g = size_guards;
        size_guards = g->next;
        astnode_finalize(g->operand);
        free(g->name);
        free(g);
    }
    free(pending_sizes);
    pending_sizes = NULL;
    pending_size_count = pending_size_capacity = 0;
    v2_program_destroy(program);
    program = NULL;
    free(object_bytes);
    object_bytes = NULL;
    object_length = 0;
    v2_layout_destroy(layout);
    layout = NULL;
    v2_unit_destroy(values);
    values = NULL;
    free(maps);
    maps = NULL;
    map_count = map_capacity = 0;
    free(files);
    files = NULL;
    file_count = file_capacity = 0;
    free(literal_bytes);
    literal_bytes = NULL;
    literal_count = literal_capacity = 0;
    free(ram_values);
    ram_values = NULL;
    ram_value_count = ram_value_capacity = 0;
    clear_pc_widths();
}
