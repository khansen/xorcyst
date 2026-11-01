/* Lossless source and native-fact export. Copyright (C) 2026 Kent Hansen.
 * GPL version 2 or later. This module does not parse or evaluate assembly. */
#include "source_model.h"
#include "xasm.h"
#include "astproc.h"
#include "dependencies.h"
#include "output_file.h"
#include "sha256.h"
#include "parser.h"
#include "opcode.h"
#include "listing.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef struct {
    dependency_snapshot snapshot;
    size_t *lines, line_count;
    char *logical_path;
} model_file;
typedef struct {
    size_t file, begin, end;
    const char *kind, *token;
    unsigned syntax;
} model_unit;
typedef struct {
    astnode *source, *prepared;
    unsigned parent, context, scope, declaration, observed_declaration;
    size_t file, begin, end, name_begin, name_end;
    size_t subtree_end;
    size_t operand_range_first;
    size_t statement_range;
    unsigned diagnostic;
    int is_end;
    int expression, reference, is_declaration, operand, observed, prepared_pc;
    ir_id value, pc;
    int value_set, located, reachable;
    unsigned segment, address, offset, length;
    unsigned char opcode;
    int operand_value;
} model_node;
typedef struct {
    unsigned syntax;
    int operand;
    size_t first, end;
    unsigned segment, address, offset, length;
} model_range;
typedef struct {
    const char *code, *message;
    size_t file, begin, end;
    unsigned syntax, related;
} model_diagnostic;
static model_file *files;
static model_unit *units;
static model_node *nodes;
static model_range *ranges;
static model_diagnostic *diagnostics;
static size_t file_count, unit_count, unit_capacity, node_count, node_capacity;
static size_t range_count, range_capacity, diagnostic_count, diagnostic_capacity;
static size_t last_match = SIZE_MAX;
static int failed, oom, internal_error, finished;
static ir_unit *native_values;
static const unsigned char *binary;
static size_t binary_size;
static char producer_hash[65];
static unsigned *declaration_lookup;
static size_t declaration_capacity;

int source_model_failed(void) { return failed || oom; }
int source_model_collection_failed(void) { return oom || internal_error; }
int source_model_has_source(void) { return file_count != 0; }

static int grow(void **p, size_t *capacity, size_t needed, size_t item)
{
    size_t cap = *capacity ? *capacity : 64;
    void *next;
    if (needed <= *capacity) return 1;
    while (cap < needed && cap <= SIZE_MAX / 2) cap *= 2;
    if (cap < needed || cap > SIZE_MAX / item || cap > UINT32_MAX) { oom = 1; return 0; }
    next = realloc(*p, cap * item);
    if (!next) { oom = 1; return 0; }
    *p = next; *capacity = cap;
    return 1;
}
static const char *basename_of(const char *s)
{
    const char *p = strrchr(s, '/');
    return p ? p + 1 : s;
}
static int add_file(const dependency_snapshot *s, void *unused)
{
    model_file *f, *p;
    size_t i, count = 1;
    (void)unused;
    if (s->roles & DEP_PRODUCER) memcpy(producer_hash, s->sha256, 65);
    if (!(s->roles & DEP_SOURCE) || s->missing) return 1;
    for (i = 0; i < file_count; ++i)
        if (!strcmp(files[i].snapshot.path, s->path)) return 1;
    p = realloc(files, (file_count + 1) * sizeof(*files));
    if (!p) { oom = 1; return 0; }
    files = p; f = &files[file_count]; memset(f, 0, sizeof(*f)); f->snapshot = *s;
    for (i = 0; i < s->size; ++i) if (s->bytes[i] == '\n') ++count;
    if (count > SIZE_MAX / sizeof(size_t)) { oom = 1; return 0; }
    f->lines = malloc(count * sizeof(size_t));
    if (!f->lines) { oom = 1; return 0; }
    f->lines[0] = 0; f->line_count = 1;
    for (i = 0; i < s->size; ++i)
        if (s->bytes[i] == '\n') f->lines[f->line_count++] = i + 1;
    /* Certified v1 has one source. A diagnostic can include several consumed
     * inputs, including equal basenames in distinct directories. */
    size_t path_size = strlen(basename_of(s->path));
    if (path_size > SIZE_MAX - 32 || !(f->logical_path = malloc(path_size + 32))) {
        free(f->lines); f->lines = NULL; oom = 1; return 0;
    }
    if (file_count) snprintf(f->logical_path, path_size + 32, "f%zu/%s", file_count + 1, basename_of(s->path));
    else strcpy(f->logical_path, basename_of(s->path));
    ++file_count;
    return 1;
}
static size_t file_at(location loc)
{
    size_t i;
    const char *path = loc.source_file ? loc.source_file : loc.file;
    dependencies_visit(add_file, NULL);
    if (!path) return 0;
    for (i = 0; i < file_count; ++i)
        if (!strcmp(files[i].snapshot.path, path)) return i;
    /* v1 has one source. Parser diagnostics may retain its relative name. */
    for (i = 0; i < file_count; ++i)
        if (!strcmp(basename_of(files[i].snapshot.path), basename_of(path))) return i;
    return SIZE_MAX;
}
static size_t offset_at(size_t f, int line, int column)
{
    size_t off;
    if (f >= file_count || line < 1 || column < 1 || (size_t)line > files[f].line_count)
        return SIZE_MAX;
    off = files[f].lines[line - 1] + (size_t)column - 1;
    return off <= files[f].snapshot.size ? off : SIZE_MAX;
}
static void diagnose(const char *code, const char *message, size_t f, size_t b, size_t e,
                     unsigned syntax, unsigned related)
{
    failed = 1;
    if (!strcmp(code, "SOURCE_MODEL_INTERNAL_ERROR")) internal_error = 1;
    if (!grow((void **)&diagnostics, &diagnostic_capacity, diagnostic_count + 1, sizeof(*diagnostics))) return;
    diagnostics[diagnostic_count++] = (model_diagnostic){code, message, f, b, e, syntax, related};
}
static void node_error(size_t i, const char *code, const char *message)
{
    model_node *n = &nodes[i];
    diagnose(code, message, n->file, n->begin, n->end, (unsigned)i + 1, 0);
    n->diagnostic = (unsigned)diagnostic_count;
}
void source_model_match(location loc, size_t length)
{
    size_t f, b;
    if (!source_model_enabled()) return;
    f = file_at(loc); b = offset_at(f, loc.last_line, loc.last_column);
    if (b == SIZE_MAX || f >= file_count || length > files[f].snapshot.size - b) {
        diagnose("SOURCE_MODEL_INTERNAL_ERROR", "lexer span outside snapshot", f, b, b, 0, 0); return;
    }
    if (!grow((void **)&units, &unit_capacity, unit_count + 1, sizeof(*units))) return;
    last_match = unit_count;
    units[unit_count++] = (model_unit){f, b, b + length, "token", NULL, 0};
}
void source_model_trivia(const char *kind)
{
    if (source_model_enabled() && last_match < unit_count) units[last_match].kind = kind;
}
static const char *token_name(int token)
{
    switch (token) {
#define TOKEN(t) case t: return #t
        TOKEN(MNEMONIC); TOKEN(INTEGER_LITERAL); TOKEN(STRING_LITERAL); TOKEN(IDENTIFIER);
        TOKEN(LOCAL_ID); TOKEN(LABEL); TOKEN(LOCAL_LABEL); TOKEN(FORWARD_BRANCH); TOKEN(BACKWARD_BRANCH);
        TOKEN(BYTE); TOKEN(WORD); TOKEN(EQU); TOKEN(ORG); TOKEN(END);
        TOKEN(SHL_OP); TOKEN(SHR_OP); TOKEN(LE_OP); TOKEN(GE_OP); TOKEN(EQ_OP); TOKEN(NE_OP);
        TOKEN(DEFINE); TOKEN(INCSRC); TOKEN(INCBIN); TOKEN(CHAR); TOKEN(DWORD);
#undef TOKEN
        case '\n': return "NEWLINE";
        case ',': return "COMMA"; case ':': return "COLON"; case '#': return "IMMEDIATE";
        case '(': return "LPAREN"; case ')': return "RPAREN";
        case '[': return "LBRACKET"; case ']': return "RBRACKET";
        case '+': return "PLUS"; case '-': return "MINUS"; case '*': return "STAR";
        case '/': return "SLASH"; case '%': return "PERCENT"; case '&': return "AMPERSAND";
        case '|': return "PIPE"; case '^': return "CARET"; case '<': return "LESS";
        case '>': return "GREATER"; case '~': return "TILDE"; case '!': return "BANG";
        case '$': return "CURRENT_PC";
        case 'A': return "ACCUMULATOR"; case 'X': return "REGISTER_X"; case 'Y': return "REGISTER_Y";
        default: return "UNSUPPORTED";
    }
}
void source_model_token(int token)
{
    model_unit *u;
    if (!source_model_enabled() || !token || last_match >= unit_count) return;
    u = &units[last_match]; u->token = token_name(token);
    if (token == '\n') u->kind = "newline";
    if (!strcmp(u->token, "UNSUPPORTED") || token == DEFINE || token == INCSRC || token == INCBIN ||
        token == CHAR || token == DWORD)
        diagnose("SOURCE_MODEL_UNSUPPORTED_CONSTRUCT", "token outside asm-edit-v1", u->file, u->begin, u->end, 0, 0);
}
void source_model_parse_error(location loc, const char *message)
{
    size_t f, b, e;
    (void)message; /* Native diagnostic text may have a shorter lifetime. */
    if (!source_model_enabled()) return;
    f = file_at(loc); b = offset_at(f, loc.first_line, loc.first_column);
    e = offset_at(f, loc.last_line, loc.last_column);
    diagnose("SOURCE_MODEL_INCOMPLETE", "source does not parse under the native grammar", f, b, e, 0, 0);
}
static int is_reference(astnode_type t)
{
    return t == IDENTIFIER_NODE || t == LOCAL_ID_NODE || t == FORWARD_BRANCH_NODE || t == BACKWARD_BRANCH_NODE;
}
static int is_label(astnode_type t)
{
    return t == LABEL_NODE || t == LOCAL_LABEL_NODE || t == FORWARD_BRANCH_DECL_NODE || t == BACKWARD_BRANCH_DECL_NODE;
}
static int is_expression(astnode_type t)
{
    return is_reference(t) || t == INTEGER_NODE || t == STRING_NODE || t == CURRENT_PC_NODE || t == ARITHMETIC_NODE;
}
static unsigned capture_node(astnode *a, unsigned parent, unsigned context, unsigned scope, int expr, int operand, unsigned depth)
{
    model_node *n;
    astnode *c, *copy;
    size_t i = node_count;
    if (oom || depth > 512) { oom = 1; return 0; }
    location loc = astnode_source_location(a);
    if (!grow((void **)&nodes, &node_capacity, i + 1, sizeof(*nodes))) return 0;
    ++node_count;
    astnode_set_source_model_ids(a, (unsigned)node_count, (unsigned)node_count);
    memset(&nodes[i], 0, sizeof(nodes[i]));
    copy = astnode_create(a->type, loc);
    if (!copy) { oom = 1; return 0; }
    /* Copy scalar metadata only; children are an indexed preorder table. */
    *copy = *a; copy->loc = loc;
    copy->first_child = copy->next_sibling = copy->prev_sibling = copy->parent = NULL;
    if (is_reference(a->type) || is_label(a->type) || a->type == STRING_NODE) {
        copy->string = strdup(a->string);
        if (!copy->string) oom = 1;
    }
    n = &nodes[i]; memset(n, 0, sizeof(*n)); n->source = copy;
    n->parent = parent; n->context = context ? context : (unsigned)i + 1;
    n->scope = scope; n->operand = operand; n->file = file_at(loc);
    n->begin = offset_at(n->file, loc.first_line, loc.first_column);
    n->end = offset_at(n->file, loc.last_line, loc.last_column);
    n->name_begin = n->begin; n->name_end = n->end;
    n->expression = expr && is_expression(a->type);
    n->reference = n->expression && is_reference(a->type);
    n->is_declaration = is_label(a->type) || a->type == EQU_NODE;
    n->value = n->pc = IR_NONE;
    if (is_label(a->type) && n->file < file_count && n->begin != SIZE_MAX && n->end != SIZE_MAX) {
        const unsigned char *b = files[n->file].snapshot.bytes;
        while (n->name_begin < n->end && (b[n->name_begin] == ' ' || b[n->name_begin] == '\t')) ++n->name_begin;
        n->name_end = n->name_begin + strlen(a->ident);
    }
    switch (a->type) {
        case LIST_NODE: case NULL_NODE: case LABEL_NODE: case LOCAL_LABEL_NODE:
        case FORWARD_BRANCH_DECL_NODE: case BACKWARD_BRANCH_DECL_NODE: case EQU_NODE:
        case INSTRUCTION_NODE: case DATA_NODE: case ORG_NODE: case DATATYPE_NODE:
        case INTEGER_NODE: case STRING_NODE: case IDENTIFIER_NODE: case LOCAL_ID_NODE:
        case FORWARD_BRANCH_NODE: case BACKWARD_BRANCH_NODE: case CURRENT_PC_NODE:
            break;
        case ARITHMETIC_NODE:
            if (a->oper != BANK_OPERATOR) break;
            /* fall through */
        default: node_error(i, "SOURCE_MODEL_UNSUPPORTED_CONSTRUCT", "syntax form outside asm-edit-v1");
    }
    if (a->type == DATATYPE_NODE && a->datatype != BYTE_DATATYPE && a->datatype != WORD_DATATYPE)
        node_error(i, "SOURCE_MODEL_UNSUPPORTED_CONSTRUCT", "data type outside asm-edit-v1");
    if (n->reference && nodes[n->context - 1].source->type == EQU_NODE &&
        (a->type == FORWARD_BRANCH_NODE || a->type == BACKWARD_BRANCH_NODE))
        node_error(i, "SOURCE_MODEL_UNSUPPORTED_CONSTRUCT", "anonymous reference in EQU is use-dependent");
    if (a->type == STRING_NODE && n->expression) {
        /* DATA's type and literal-only form are checked after the inventory. */
        if (nodes[n->context - 1].source->type != DATA_NODE || parent != n->context)
            node_error(i, "SOURCE_MODEL_UNSUPPORTED_CONSTRUCT", "string outside byte data operand");
    }
    int index = 0;
    unsigned child_scope = scope;
    for (c = a->first_child; c; c = c->next_sibling, ++index) {
        int child_expr = expr, child_operand = operand;
        unsigned child_context = n->context;
        if (a->type == LIST_NODE) {
            child_context = 0; child_expr = 0; child_operand = -1;
            if (c->type == LABEL_NODE) child_scope = (unsigned)node_count + 1;
        } else if (a->type == EQU_NODE) { child_expr = index == 1; child_operand = -1; }
        else if (a->type == DATA_NODE) { child_expr = index > 0; child_operand = index - 1; }
        else if (a->type == INSTRUCTION_NODE || a->type == ORG_NODE) { child_expr = 1; child_operand = 0; }
        if (!capture_node(c, (unsigned)i + 1, child_context, child_scope, child_expr, child_operand, depth + 1)) break;
        n = &nodes[i]; /* recursive appends may relocate the table */
    }
    nodes[i].subtree_end = node_count;
    return (unsigned)i + 1;
}
static const char *declaration_name(size_t i)
{
    return nodes[i].source->type == EQU_NODE ? nodes[i + 1].source->ident : nodes[i].source->ident;
}
static int declaration_order(const void *a, const void *b)
{
    size_t i = *(const size_t *)a, j = *(const size_t *)b;
    int c = strcmp(declaration_name(i), declaration_name(j));
    unsigned si = nodes[i].source->type == LOCAL_LABEL_NODE ? nodes[i].scope : 0;
    unsigned sj = nodes[j].source->type == LOCAL_LABEL_NODE ? nodes[j].scope : 0;
    return c ? c : si < sj ? -1 : si > sj;
}
int source_model_capture(astnode *root)
{
    size_t i, count = 0, *decls;
    if (!source_model_enabled()) return 1;
    dependencies_visit(add_file, NULL);
    if (root) capture_node(root, 0, 0, 0, 0, -1, 0);
    if (oom) return 0;
    if (node_count && file_count) {
        nodes[0].file = 0; nodes[0].begin = 0; nodes[0].end = files[0].snapshot.size;
        for (i = 0; i < unit_count; ++i) if (units[i].token && !strcmp(units[i].token, "END")) {
            model_node *n;
            if (!grow((void **)&nodes, &node_capacity, node_count + 1, sizeof(*nodes))) break;
            n = &nodes[node_count]; memset(n, 0, sizeof(*n));
            n->source = astnode_create(NULL_NODE, loc_preserve);
            if (!n->source) { oom = 1; break; }
            n->is_end = 1; n->parent = 1; n->context = (unsigned)node_count + 1;
            n->file = units[i].file; n->begin = units[i].begin; n->end = units[i].end;
            n->scope = node_count > 1 ? nodes[node_count - 1].scope : 0;
            n->subtree_end = ++node_count;
        }
        nodes[0].subtree_end = node_count;
    }
    decls = node_count ? malloc(node_count * sizeof(*decls)) : NULL;
    if (node_count && !decls) { oom = 1; return 0; }
    for (i = 0; i < node_count; ++i) {
        model_node *n = &nodes[i];
        astnode_type t = n->source->type;
        if (t == EQU_NODE) { n->name_begin = nodes[i + 1].begin; n->name_end = nodes[i + 1].end; }
        if (n->is_declaration && t != FORWARD_BRANCH_DECL_NODE && t != BACKWARD_BRANCH_DECL_NODE) decls[count++] = i;
        if (t == DATA_NODE && i + 1 < node_count && nodes[i + 1].source->datatype == WORD_DATATYPE) {
            size_t j;
            for (j = i + 2; j < node_count && nodes[j].context == i + 1; ++j)
                if (nodes[j].source->type == STRING_NODE)
                    node_error(j, "SOURCE_MODEL_UNSUPPORTED_CONSTRUCT", "word string outside asm-edit-v1");
        }
    }
    if (count > 1) qsort(decls, count, sizeof(*decls), declaration_order);
    for (i = 1; i < count; ++i) if (!declaration_order(&decls[i - 1], &decls[i])) {
        model_node *n = &nodes[decls[i]];
        diagnose("SOURCE_MODEL_DUPLICATE_DECLARATION", "symbol has multiple declaration sites", n->file,
                 n->name_begin, n->name_end, (unsigned)decls[i] + 1, (unsigned)decls[i - 1] + 1);
    }
    free(decls);
    unsigned lexical_segment = 0;
    for (i = 0; i < node_count; ++i) {
        nodes[i].segment = lexical_segment;
        if (nodes[i].source->type == ORG_NODE) ++lexical_segment;
    }
    declaration_capacity = 64;
    while (declaration_capacity < node_count * 2) declaration_capacity *= 2;
    declaration_lookup = calloc(declaration_capacity, sizeof(*declaration_lookup));
    if (!declaration_lookup) oom = 1;
    else for (i = 0; i < node_count; ++i) if (nodes[i].is_declaration) {
        size_t slot = (nodes[i].begin * (size_t)2654435761u + nodes[i].file) & (declaration_capacity - 1);
        while (declaration_lookup[slot]) slot = (slot + 1) & (declaration_capacity - 1);
        declaration_lookup[slot] = (unsigned)i + 1;
    }
    return !source_model_failed();
}

/* Clones are prepared at the native lexical point. Only parser provenance is
 * retained; no consumer recreates local or anonymous scope from text. */
static void prepare_tree(astnode *a, int pc)
{
    astnode *c;
    unsigned id = astnode_source_model_id(a);
    if (id && id <= node_count && nodes[id - 1].expression) {
        model_node *n = &nodes[id - 1];
        if (!n->prepared) {
            n->prepared = astnode_clone(a, loc_preserve);
            if (!n->prepared) { oom = 1; return; }
            astproc_analysis_prepare(n->prepared);
            n->prepared_pc = pc;
        }
    }
    for (c = a->first_child; c; c = c->next_sibling) prepare_tree(c, pc);
}
void source_model_prepare(astnode *statement, int pc)
{
    if (source_model_enabled() && astnode_source_model_id(statement)) prepare_tree(statement, pc);
}
size_t source_model_node_count(void) { return node_count; }
astnode *source_model_prepared(size_t i) { return i < node_count ? nodes[i].prepared : NULL; }
size_t source_model_subtree_end(size_t i) { return i < node_count ? nodes[i].subtree_end : i; }
int source_model_expression(size_t i) { return i < node_count && nodes[i].expression; }
int source_model_context_pc(size_t i) { return nodes[i].prepared_pc; }
int source_model_equ_context(size_t i) { return nodes[nodes[i].context - 1].source->type == EQU_NODE; }
static unsigned declaration_at(const location *loc)
{
    size_t slot, f, b;
    if (!loc || !declaration_lookup) return 0;
    f = file_at(*loc); b = offset_at(f, loc->first_line, loc->first_column);
    /* Native definitions preserve the original declaration span. */
    slot = (b * (size_t)2654435761u + f) & (declaration_capacity - 1);
    while (declaration_lookup[slot]) {
        unsigned id = declaration_lookup[slot];
        if (nodes[id - 1].file == f && nodes[id - 1].begin == b) return id;
        slot = (slot + 1) & (declaration_capacity - 1);
    }
    return 0;
}
void source_model_binding(size_t i, const location *loc)
{
    unsigned d;
    if (i >= node_count || !nodes[i].reference) return;
    d = declaration_at(loc);
    if (nodes[i].declaration && d != nodes[i].declaration)
        node_error(i, "SOURCE_MODEL_INTERNAL_ERROR", "native reference resolution changed");
    nodes[i].declaration = d;
}
void source_model_bound(const astnode *a, const symtab_entry *s, int analysis)
{
    size_t i;
    const location *loc;
    if (!source_model_enabled() || !a || !astnode_source_model_id(a) || !s) return;
    i = astnode_source_model_id(a) - 1;
    if (i >= node_count || !nodes[i].reference) return;
    loc = s->has_assignment_loc ? &s->assignment_loc : s->def ? &s->def->loc : NULL;
    source_model_binding(i, loc);
    if (!analysis) { nodes[i].observed = 1; nodes[i].observed_declaration = nodes[i].declaration; }
}
void source_model_value(size_t i, ir_id value, ir_id pc)
{
    if (i < node_count) { nodes[i].value = value; nodes[i].pc = pc; nodes[i].value_set = 1; }
}
void source_model_range(unsigned syntax, int operand, size_t first, size_t end)
{
    if (!source_model_enabled() || !syntax || syntax > node_count) return;
    if (!grow((void **)&ranges, &range_capacity, range_count + 1, sizeof(*ranges))) return;
    if (operand == 0) nodes[syntax - 1].operand_range_first = range_count;
    if (operand == -1) nodes[syntax - 1].statement_range = range_count;
    ranges[range_count++] = (model_range){syntax, operand, first, end, 0, 0, 0, 0};
}

/* Finish against indexed native layout facts, independent of listing groups. */
int source_model_finish(ir_unit *values, ir_layout *layout)
{
    size_t i, j, fcount;
    unsigned *segments, segment = 0;
    ir_layout_fact fact;
    if (!source_model_enabled()) return 1;
    native_values = values; binary = ir_layout_bytes(layout, &binary_size);
    fcount = ir_layout_fact_count(layout);
    segments = malloc((fcount + 1) * sizeof(*segments));
    if (!segments) { oom = 1; return 0; }
    for (i = 0; i < fcount; ++i) {
        if (!ir_layout_fact_at(layout, (ir_id)i, &fact)) { failed = 1; break; }
        if (fact.kind == IR_FACT_PLACEMENT) ++segment;
        segments[i] = segment;
    }
    segments[fcount] = segment;
    for (i = 0; i < range_count; ++i) {
        model_range *r = &ranges[i];
        unsigned address = 0, offset = 0;
        if (r->first > r->end || r->end > fcount) { failed = 1; break; }
        if (r->first < fcount && ir_layout_fact_at(layout, (ir_id)r->first, &fact)) {
            address = fact.address; offset = fact.output_offset;
        } else if (r->first && ir_layout_fact_at(layout, (ir_id)r->first - 1, &fact)) {
            address = fact.address + fact.length; offset = fact.output_offset + fact.length;
        }
        r->address = address; r->offset = offset; r->segment = segments[r->first];
        for (j = r->first; j < r->end; ++j) {
            if (!ir_layout_fact_at(layout, (ir_id)j, &fact) || fact.output_offset != offset ||
                fact.length > UINT_MAX - offset) { failed = 1; break; }
            offset += fact.length;
            if (r->operand == -1 && fact.kind == IR_FACT_INSTRUCTION) {
                nodes[r->syntax - 1].opcode = fact.opcode;
                /* Memory/control-flow facts describe the encoded operand. Native
                 * expression evaluations retain their untruncated value. */
                nodes[r->syntax - 1].operand_value = fact.length > 1 ? binary[fact.output_offset + 1] : 0;
                if (fact.length > 2) nodes[r->syntax - 1].operand_value |= binary[fact.output_offset + 2] << 8;
            }
        }
        r->length = offset - r->offset;
        if (r->operand == -1) {
            model_node *n = &nodes[r->syntax - 1];
            if (n->located) node_error(r->syntax - 1, "SOURCE_MODEL_INTERNAL_ERROR", "duplicate statement layout");
            n->located = 1; n->address = r->address; n->offset = r->offset;
            n->length = r->length; n->segment = r->segment;
        }
    }
    free(segments);
    for (i = 0; i < node_count; ++i) {
        model_node *n = &nodes[i];
        if (n->reference && !n->declaration)
            node_error(i, "SOURCE_MODEL_UNBOUND_REFERENCE", "native resolver has no declaration for reference");
        if (n->expression && (!n->value_set || ir_evaluate(values, n->value).state != IR_KNOWN))
            node_error(i, "SOURCE_MODEL_VALUE_UNAVAILABLE", "native expression value is unavailable");
        if ((is_label(n->source->type) || n->source->type == DATA_NODE || n->source->type == INSTRUCTION_NODE) && !n->located)
            node_error(i, "SOURCE_MODEL_INTERNAL_ERROR", "missing native statement location");
    }
    if (failed && !diagnostic_count)
        diagnose("SOURCE_MODEL_INTERNAL_ERROR", "native layout ranges are incomplete", 0, 0, 0, 0, 0);
    finished = !source_model_failed();
    return finished;
}

static void string(FILE *f, const char *s)
{
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    fputc('"', f);
    for (; *p; ++p) {
        if (*p == '"' || *p == '\\') { fputc('\\', f); fputc(*p, f); }
        else if (*p < 32) fprintf(f, "\\u%04x", *p);
        else fputc(*p, f);
    }
    fputc('"', f);
}
static void base64(FILE *f, const unsigned char *s, size_t size)
{
    static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i;
    fputc('"', f);
    for (i = 0; i < size; i += 3) {
        unsigned v = (unsigned)s[i] << 16;
        if (i + 1 < size) v |= (unsigned)s[i + 1] << 8;
        if (i + 2 < size) v |= s[i + 2];
        fputc(chars[v >> 18], f); fputc(chars[(v >> 12) & 63], f);
        fputc(i + 1 < size ? chars[(v >> 6) & 63] : '=', f);
        fputc(i + 2 < size ? chars[v & 63] : '=', f);
    }
    fputc('"', f);
}
static void id(FILE *f, char prefix, size_t n)
{
    if (n) fprintf(f, "\"%c%zu\"", prefix, n); else fputs("null", f);
}
static void span(FILE *f, size_t file, size_t b, size_t e)
{
    if (file >= file_count || b == SIZE_MAX || e == SIZE_MAX) { fputs("null", f); return; }
    fprintf(f, "{\"file_id\":\"f%zu\",\"byte_start\":%zu,\"byte_end_exclusive\":%zu}", file + 1, b, e);
}
static void node_span(FILE *f, const model_node *n) { span(f, n->file, n->begin, n->end); }
static void spelling(FILE *f, size_t file, size_t begin, size_t end)
{
    size_t i;
    fputc('"', f);
    if (file < file_count && begin != SIZE_MAX && end <= files[file].snapshot.size)
        for (i = begin; i < end; ++i) {
            unsigned char c = files[file].snapshot.bytes[i];
            if (c == '"' || c == '\\') { fputc('\\', f); fputc(c, f); }
            else if (c < 32 || c >= 127) fprintf(f, "\\u%04x", c);
            else fputc(c, f);
        }
    fputc('"', f);
}
static void children(FILE *f, size_t i, char prefix)
{
    size_t j; int comma = 0;
    fputc('[', f);
    for (j = i + 1; j < nodes[i].subtree_end; j = nodes[j].subtree_end)
        if (nodes[j].parent == i + 1) { if (comma++) fputc(',', f); id(f, prefix, j + 1); }
    fputc(']', f);
}
static void value(FILE *f, ir_result r)
{
    if (r.state != IR_KNOWN) fputs("null", f);
    else if (r.value.kind == IR_INTEGER) fprintf(f, "{\"kind\":\"integer\",\"value\":%d}", r.value.as.integer);
    else if (r.value.kind == IR_STRING) {
        fputs("{\"kind\":\"bytes\",\"bytes_base64\":", f);
        base64(f, r.value.as.string.bytes, r.value.as.string.length); fputc('}', f);
    } else fputs("null", f);
}
static void digest(const void *p, size_t size, char hex[65])
{
    sha256_context c; sha256_init(&c); sha256_update(&c, p, size); sha256_finish(&c, hex);
}
static void mark_reachable_equ(unsigned declaration, unsigned *queue, size_t *count)
{
    if (declaration && nodes[declaration - 1].source->type == EQU_NODE && !nodes[declaration - 1].reachable) {
        nodes[declaration - 1].reachable = 1;
        queue[(*count)++] = declaration;
    }
}
static int resolution_origins(void)
{
    size_t i, at = 0, count = 0;
    unsigned *queue = node_count ? malloc(node_count * sizeof(*queue)) : NULL;
    if (node_count && !queue) { oom = 1; return 0; }
    for (i = 0; i < node_count; ++i) nodes[i].reachable = 0;
    for (i = 0; i < node_count; ++i)
        if (nodes[i].reference && !source_model_equ_context(i)) mark_reachable_equ(nodes[i].declaration, queue, &count);
    while (at < count) {
        size_t context = queue[at++] - 1;
        for (i = context + 1; i < nodes[context].subtree_end; ++i)
            if (nodes[i].reference) mark_reachable_equ(nodes[i].declaration, queue, &count);
    }
    free(queue);
    return 1;
}
static const char *resolution_origin(size_t i)
{
    return !source_model_equ_context(i) || nodes[nodes[i].context - 1].reachable ? "assembly" : "analysis";
}
static void write_nodes(FILE *f, int certified)
{
    size_t i; int comma = 0;
    fputs(",\"syntax\":[", f);
    for (i = 0; i < node_count; ++i) {
        model_node *n = &nodes[i];
        if (i) fputc(',', f);
        fputs("{\"id\":", f); id(f, 's', i + 1);
        fputs(",\"kind\":", f); string(f, n->is_end ? "END_NODE" : astnode_type_to_string(n->source->type));
        fputs(",\"span\":", f); node_span(f, n);
        fputs(",\"parent_id\":", f); id(f, 's', n->parent);
        fputs(",\"scope_id\":", f); id(f, 'q', n->scope + 1);
        fputs(",\"children\":", f); children(f, i, 's'); fputc('}', f);
    }
    fputs("],\"scopes\":[{\"id\":\"q1\",\"kind\":\"root\",\"parent_id\":null,\"anchor_declaration_id\":null,\"extent\":", f);
    if (file_count) span(f, 0, 0, files[0].snapshot.size); else fputs("null", f);
    fputc('}', f);
    for (i = 0; i < node_count; ++i) if (nodes[i].source->type == LABEL_NODE) {
        size_t j, end = files[nodes[i].file].snapshot.size;
        for (j = i + 1; j < node_count; ++j) if (nodes[j].source->type == LABEL_NODE) { end = nodes[j].begin; break; }
        fprintf(f, ",{\"id\":\"q%zu\",\"kind\":\"global\",\"parent_id\":\"q1\",\"anchor_declaration_id\":\"d%zu\",\"extent\":", i + 2, i + 1);
        span(f, nodes[i].file, nodes[i].begin, end); fputc('}', f);
    }
    fputs("],\"declarations\":[", f);
    for (i = 0; i < node_count; ++i) if (nodes[i].is_declaration) {
        model_node *n = &nodes[i];
        if (comma++) fputc(',', f);
        fprintf(f, "{\"id\":\"d%zu\",\"syntax_id\":\"s%zu\",\"symbol_id\":\"d%zu\",\"name\":", i + 1, i + 1, i + 1);
        string(f, declaration_name(i)); fputs(",\"kind\":", f);
        string(f, n->source->type == EQU_NODE ? "equ" : n->source->type == LABEL_NODE ? "global" :
                  n->source->type == LOCAL_LABEL_NODE ? "local" : "anonymous");
        fputs(",\"name_span\":", f); span(f, n->file, n->name_begin, n->name_end);
        fputs(",\"scope_id\":", f); id(f, 'q', n->scope + 1);
        fputs(",\"lexical_owner_id\":", f); id(f, 'd', n->scope);
        fputs(",\"expression_id\":", f); id(f, 'e', n->source->type == EQU_NODE ? i + 3 : 0);
        fputs(",\"location_id\":", f); id(f, 'l', certified && n->located ? i + 1 : 0); fputc('}', f);
    }
    fputs("],\"expressions\":[", f); comma = 0;
    for (i = 0; i < node_count; ++i) if (nodes[i].expression) {
        static const char *ops[] = {"add","subtract","multiply","divide","remainder","bit_and","bit_or","bit_xor",
            "shift_left","shift_right","less","greater","equal","not_equal","less_equal","greater_equal",
            "complement","not","low","high","negate","bank"};
        model_node *n = &nodes[i]; astnode *a = n->source;
        if (comma++) fputc(',', f);
        fprintf(f, "{\"id\":\"e%zu\",\"syntax_id\":\"s%zu\",\"context_syntax_id\":\"s%u\",\"kind\":", i + 1, i + 1, n->context);
        string(f, a->type == IDENTIFIER_NODE ? "symbol" : a->type == LOCAL_ID_NODE ? "local" :
                  a->type == FORWARD_BRANCH_NODE ? "anonymous_forward" : a->type == BACKWARD_BRANCH_NODE ? "anonymous_backward" :
                  a->type == INTEGER_NODE ? "integer" : a->type == STRING_NODE ? "string" :
                  a->type == CURRENT_PC_NODE ? "pc" : nodes[i + 1].subtree_end < n->subtree_end ? "binary" : "unary");
        fputs(",\"span\":", f); node_span(f, n);
        fputs(",\"children\":", f); children(f, i, 'e');
        if (a->type == INTEGER_NODE) fprintf(f, ",\"value\":%d", a->integer);
        if (a->type == STRING_NODE) { fputs(",\"bytes_base64\":", f); base64(f, (unsigned char *)a->string, strlen(a->string)); }
        if (a->type == ARITHMETIC_NODE) { fputs(",\"operator\":", f); string(f, ops[a->oper]); }
        if (n->reference) { fputs(",\"reference_id\":", f); id(f, 'r', i + 1); }
        fputc('}', f);
    }
    fputs("],\"references\":[", f); comma = 0;
    for (i = 0; i < node_count; ++i) if (nodes[i].reference) {
        model_node *n = &nodes[i];
        if (comma++) fputc(',', f);
        fprintf(f, "{\"id\":\"r%zu\",\"expression_id\":\"e%zu\",\"context_syntax_id\":\"s%u\",\"name\":", i + 1, i + 1, n->context);
        string(f, n->source->ident);
        fputs(",\"spelling\":", f); spelling(f, n->file, n->begin, n->end);
        fputs(",\"span\":", f); node_span(f, n);
        fputs(",\"scope_id\":", f); id(f, 'q', n->scope + 1);
        fputs(",\"declaration_id\":", f); id(f, 'd', n->declaration);
        fputs(",\"resolution_origin\":", f);
        if (n->declaration) string(f, resolution_origin(i)); else fputs("null", f);
        fputs(",\"diagnostic_id\":", f); id(f, 'z', n->declaration ? 0 : n->diagnostic); fputc('}', f);
    }
    fputs("],\"evaluations\":", f);
    if (!certified) { fputs("null", f); return; }
    fputc('[', f); comma = 0;
    for (i = 0; i < node_count; ++i) if (nodes[i].expression) {
        model_node *n = &nodes[i]; int known = 0; int32_t pc = n->prepared_pc;
        if (n->pc != IR_NONE) ir_anchor_value(native_values, n->pc, &known, &pc, ir_no_location());
        if (comma++) fputc(',', f);
        fprintf(f, "{\"id\":\"v%zu\",\"expression_id\":\"e%zu\",\"context_syntax_id\":\"s%u\",\"operand_index\":", i + 1, i + 1, n->context);
        if (n->operand < 0) fputs("null", f); else fprintf(f, "%d", n->operand);
        fprintf(f, ",\"pc_context\":{\"cpu_address\":%d,\"segment_id\":", pc);
        id(f, 'g', nodes[n->context - 1].segment + 1);
        fputs("},\"value\":", f); value(f, ir_evaluate(native_values, n->value));
        fputs(",\"resolution_origin\":", f); string(f, resolution_origin(i));
        fputs(",\"reference_values\":[", f);
        size_t j; int rc = 0;
        for (j = i; j < n->subtree_end; ++j) if (nodes[j].reference) {
            if (rc++) fputc(',', f);
            fprintf(f, "{\"reference_id\":\"r%zu\",\"declaration_id\":\"d%u\",\"value\":", j + 1, nodes[j].declaration);
            value(f, ir_evaluate(native_values, nodes[j].value)); fputc('}', f);
        }
        fputs("]}", f);
    }
    fputc(']', f);
}

static void emission_ids(FILE *f, size_t index)
{
    model_node *n = &nodes[index];
    size_t j;
    int comma = 0;
    fputc('[', f);
    if (n->source->type == INSTRUCTION_NODE) {
        fprintf(f, "\"m%zu-opcode\"", n->statement_range + 1);
        if (n->length > 1) fprintf(f, ",\"m%zu-operand\"", n->statement_range + 1);
    } else {
        size_t range = n->operand_range_first;
        for (j = index + 2; j < n->subtree_end; j = nodes[j].subtree_end, ++range)
            if (ranges[range].length) {
                if (comma++) fputc(',', f);
                fprintf(f, "\"m%zu\"", range + 1);
            }
    }
    fputc(']', f);
}
static void write_layout(FILE *f, int certified)
{
    size_t i, j; int comma = 0;
    unsigned segment = 0;
    if (!certified) {
        fputs(",\"segments\":null,\"locations\":null,\"instructions\":null,\"data\":null,\"emissions\":null,\"binary\":null", f);
        return;
    }
    fputs(",\"segments\":[{\"id\":\"g1\",\"start_syntax_id\":null,\"address_space\":\"cpu\",\"origin\":0,\"output_offset\":0}", f);
    for (i = 0; i < node_count; ++i) if (nodes[i].source->type == ORG_NODE) {
        unsigned child = (unsigned)i + 2;
        ir_result v = ir_evaluate(native_values, nodes[child - 1].value);
        fprintf(f, ",{\"id\":\"g%u\",\"start_syntax_id\":\"s%zu\",\"address_space\":\"cpu\",\"origin\":%d,\"output_offset\":%u}",
                ++segment + 1, i + 1, v.value.as.integer, nodes[i].offset);
    }
    fputs("],\"locations\":[", f);
    for (i = 0; i < node_count; ++i) if (nodes[i].located) {
        model_node *n = &nodes[i];
        if (comma++) fputc(',', f);
        fprintf(f, "{\"id\":\"l%zu\",\"syntax_id\":\"s%zu\",\"segment_id\":\"g%u\",\"cpu_address\":%u,\"output_offset\":%u,\"byte_length\":%u}",
                i + 1, i + 1, n->segment + 1, n->address, n->offset, n->length);
    }
    fputs("],\"instructions\":[", f); comma = 0;
    for (i = 0; i < node_count; ++i) if (nodes[i].source->type == INSTRUCTION_NODE) {
        model_node *n = &nodes[i];
        addressing_mode mode = opcode_addressing_mode(n->opcode);
        instr_mnemonic mnemonic = n->source->instr.mnemonic.value;
        if (comma++) fputc(',', f);
        fprintf(f, "{\"id\":\"i%zu\",\"syntax_id\":\"s%zu\",\"location_id\":\"l%zu\",\"mnemonic\":", i + 1, i + 1, i + 1);
        string(f, opcode_to_string(n->opcode));
        fputs(",\"parsed_mode\":", f); string(f, addressing_mode_to_string(n->source->instr.mode));
        fputs(",\"final_mode\":", f); string(f, addressing_mode_to_string(mode));
        fprintf(f, ",\"forced_absolute\":%s,\"opcode\":%u,\"operand_ids\":[", n->source->instr.mnemonic.wide ? "true" : "false", n->opcode);
        if (i + 1 < n->subtree_end) fprintf(f, "\"o%zu-0\"", i + 1);
        fputs("],\"operands\":[", f);
        if (i + 1 < n->subtree_end) {
            fprintf(f, "{\"id\":\"o%zu-0\",\"index\":0,\"expression_id\":\"e%zu\",\"evaluation_id\":\"v%zu\",\"emission_ranges\":[{\"output_start\":%u,\"output_end_exclusive\":%u}]}",
                    i + 1, i + 2, i + 2, n->offset + 1, n->offset + n->length);
        }
        fputs("],\"emission_ids\":", f); emission_ids(f, i);
        fputs(",\"memory_access\":", f);
        analysis_write_memory_access(f, n->opcode, n->operand_value);
        fputs(",\"control_flow\":{\"kind\":", f);
        string(f, mode == RELATIVE_MODE ? "branch" : mnemonic == JMP_MNEMONIC ? "jump" :
                  mnemonic == JSR_MNEMONIC ? "call" : mnemonic == RTS_MNEMONIC || mnemonic == RTI_MNEMONIC ? "return" :
                  mnemonic == BRK_MNEMONIC ? "trap" : "none");
        fputs(",\"target\":", f);
        if (mode == RELATIVE_MODE || (mode != INDIRECT_MODE && (mnemonic == JMP_MNEMONIC || mnemonic == JSR_MNEMONIC)))
            fprintf(f, "%u", mode == RELATIVE_MODE ?
                    (n->address + 2 + (int)(int8_t)n->operand_value) & 0xffffu : (unsigned)n->operand_value);
        else fputs("null", f);
        fputs(",\"encoded_displacement\":", f);
        if (mode == RELATIVE_MODE) fprintf(f, "%d", (int)(int8_t)binary[n->offset + 1]);
        else fputs("null", f);
        fputs("}}", f);
    }
    fputs("],\"data\":[", f); comma = 0;
    for (i = 0; i < node_count; ++i) if (nodes[i].source->type == DATA_NODE) {
        int operands = 0;
        if (comma++) fputc(',', f);
        fprintf(f, "{\"id\":\"b%zu\",\"syntax_id\":\"s%zu\",\"location_id\":\"l%zu\",\"element_width\":%u,\"directive_token\":\"%s\",\"emission_ids\":", i + 1, i + 1, i + 1,
                nodes[i + 1].source->datatype == WORD_DATATYPE ? 2 : 1,
                nodes[i + 1].source->datatype == WORD_DATATYPE ? "WORD" : "BYTE");
        emission_ids(f, i);
        fputs(",\"operands\":[", f);
        for (j = i + 2; j < nodes[i].subtree_end; j = nodes[j].subtree_end) {
            model_range *range = &ranges[nodes[i].operand_range_first + (size_t)operands];
            ir_result v = ir_evaluate(native_values, nodes[j].value);
            if (range->syntax != i + 1 || range->operand != operands) {
                node_error(i, "SOURCE_MODEL_INTERNAL_ERROR", "native data operand range does not match its statement"); break;
            }
            if (operands++) fputc(',', f);
            fprintf(f, "{\"id\":\"o%zu-%d\",\"index\":%d,\"expression_id\":\"e%zu\",\"evaluation_id\":\"v%zu\",\"element_count\":%zu,\"output_offset\":%u,\"byte_length\":%u,\"emission_ranges\":[{\"output_start\":%u,\"output_end_exclusive\":%u}]}",
                    i + 1, operands - 1, operands - 1, j + 1, j + 1, v.value.kind == IR_STRING ? v.value.as.string.length : (size_t)1,
                    range->offset, range->length, range->offset, range->offset + range->length);
        }
        fputs("]}", f);
    }
    fputs("],\"emissions\":[", f); comma = 0;
    for (i = 0; i < range_count; ++i) {
        model_range *r = &ranges[i];
        astnode_type t = nodes[r->syntax - 1].source->type;
        if (!r->length || (t == DATA_NODE && r->operand < 0)) continue;
        if (t == INSTRUCTION_NODE) {
            if (comma++) fputc(',', f);
            fprintf(f, "{\"id\":\"m%zu-opcode\",\"syntax_id\":\"s%u\",\"operand_id\":null,\"segment_id\":\"g%u\",\"cpu_address\":%u,\"output_start\":%u,\"output_end_exclusive\":%u,\"kind\":\"opcode\"}",
                    i + 1, r->syntax, r->segment + 1, r->address, r->offset, r->offset + 1);
            if (r->length > 1) {
                fprintf(f, ",{\"id\":\"m%zu-operand\",\"syntax_id\":\"s%u\",\"operand_id\":\"o%u-0\",\"segment_id\":\"g%u\",\"cpu_address\":%u,\"output_start\":%u,\"output_end_exclusive\":%u,\"kind\":\"instruction_operand\"}",
                        i + 1, r->syntax, r->syntax, r->segment + 1, r->address + 1, r->offset + 1, r->offset + r->length);
            }
            continue;
        }
        if (comma++) fputc(',', f);
        fprintf(f, "{\"id\":\"m%zu\",\"syntax_id\":\"s%u\",\"operand_id\":\"o%u-%d\",\"segment_id\":\"g%u\",\"cpu_address\":%u,\"output_start\":%u,\"output_end_exclusive\":%u,\"kind\":",
                i + 1, r->syntax, r->syntax, r->operand, r->segment + 1, r->address, r->offset, r->offset + r->length);
        string(f, "data_operand");
        fputs(",\"operand_index\":", f);
        if (r->operand < 0) fputs("null", f); else fprintf(f, "%d", r->operand);
        fputc('}', f);
    }
    char hash[65]; digest(binary, binary_size, hash);
    fprintf(f, "],\"binary\":{\"byte_length\":%zu,\"sha256\":\"%s\",\"bytes_base64\":", binary_size, hash);
    base64(f, binary, binary_size); fputc('}', f);
}
static int unit_order(const void *a, const void *b)
{
    const model_unit *x = a, *y = b;
    return x->file < y->file ? -1 : x->file > y->file ? 1 : x->begin < y->begin ? -1 : x->begin > y->begin;
}
static void write_unit(FILE *f, size_t index, const model_unit *u, int comma)
{
    if (comma) fputc(',', f);
    fprintf(f, "{\"id\":\"u%zu\",\"file_id\":\"f%zu\",\"byte_start\":%zu,\"byte_end_exclusive\":%zu,\"kind\":", index, u->file + 1, u->begin, u->end);
    string(f, u->kind); fputs(",\"token_kind\":", f);
    if (u->token) string(f, u->token); else fputs("null", f);
    fputs(",\"syntax_id\":", f); id(f, 's', u->syntax); fputc('}', f);
}
static size_t write_units(FILE *f, int certified)
{
    size_t i, j, at = 0, count = 0;
    if (unit_count > 1) qsort(units, unit_count, sizeof(*units), unit_order);
    /* A preorder visit assigns the smallest containing syntax span. */
    for (i = 0; i < node_count; ++i) {
        model_node *n = &nodes[i]; size_t lo = 0, hi = unit_count;
        if (n->begin == SIZE_MAX || n->end == SIZE_MAX) continue;
        while (lo < hi) {
            size_t mid = lo + (hi - lo) / 2;
            if (units[mid].file < n->file || (units[mid].file == n->file && units[mid].begin < n->begin)) lo = mid + 1;
            else hi = mid;
        }
        for (j = lo; j < unit_count && units[j].file == n->file && units[j].end <= n->end; ++j)
            if (!strcmp(units[j].kind, "token")) units[j].syntax = (unsigned)i + 1;
    }
    fputs(",\"units\":[", f);
    for (i = 0; i < file_count; ++i) {
        size_t pos = 0;
        while (at < unit_count && units[at].file == i) {
            model_unit *u = &units[at++];
            if (u->begin < pos || u->end <= u->begin) { failed = 1; break; }
            if (pos != u->begin) {
                model_unit gap = {i, pos, u->begin, "unclassified", NULL, 0};
                if (certified) failed = 1;
                write_unit(f, count + 1, &gap, count > 0); ++count;
            }
            write_unit(f, count + 1, u, count > 0); ++count; pos = u->end;
        }
        if (pos < files[i].snapshot.size) {
            model_unit gap = {i, pos, files[i].snapshot.size, "unclassified", NULL, 0};
            if (certified) failed = 1;
            write_unit(f, count + 1, &gap, count > 0); ++count;
        }
    }
    fputc(']', f); return count;
}
/* Hash fields are length-prefixed UTF-8 or ASCII strings. This canonical byte
 * stream deliberately excludes diagnostics, working directories and outputs. */
static void hash_field(sha256_context *c, const char *s)
{
    char length[32];
    snprintf(length, sizeof(length), "%zu:", strlen(s));
    sha256_update(c, length, strlen(length)); sha256_update(c, s, strlen(s));
}
int source_model_write(const char *path, int certified, const char *comparison, const char *audit, int exit_status)
{
    output_writer out;
    FILE *f;
    size_t i, unit_total, references = 0, expressions = 0, declarations = 0;
    size_t instructions = 0, data = 0, expected_locations = 0, located = 0, evaluated = 0;
    sha256_context hash; char input_id[65], model_id[65];
    if (!source_model_enabled()) return 1;
    dependencies_visit(add_file, NULL);
    if (oom || !resolution_origins()) return 0;
    if (!file_count) { fprintf(stderr, "error: SOURCE_MODEL_IO_ERROR: root source was not captured\n"); return 0; }
    if (certified && !finished) {
        diagnose("SOURCE_MODEL_INTERNAL_ERROR", "native layout capture did not complete", 0, 0, 0, 0, 0);
        return 0;
    }
    if (!certified) {
        for (i = 0; i < node_count; ++i) if (nodes[i].reference && !nodes[i].declaration && !nodes[i].diagnostic)
            node_error(i, "SOURCE_MODEL_UNBOUND_REFERENCE", "native reference did not resolve");
        if (!diagnostic_count)
            diagnose("SOURCE_MODEL_VALUE_UNAVAILABLE", "native assembly did not complete", 0, 0, 0, 0, 0);
    }
    sha256_init(&hash); hash_field(&hash, "xasm-source-model-input-v1");
    hash_field(&hash, xasm_args.case_insensitive ? "case-insensitive" : "case-sensitive");
    hash_field(&hash, xasm_args.swap_parens ? "swapped-parens" : "native-parens");
    for (i = 0; i < file_count; ++i) {
        hash_field(&hash, files[i].logical_path); hash_field(&hash, files[i].snapshot.sha256);
    }
    sha256_finish(&hash, input_id);
    sha256_init(&hash); hash_field(&hash, "xasm-source-model:1:asm-edit-v1");
    hash_field(&hash, input_id); hash_field(&hash, producer_hash); sha256_finish(&hash, model_id);
    if (!output_file_open(&out, path)) {
        fprintf(stderr, "error: SOURCE_MODEL_IO_ERROR: could not open model output\n"); return 0;
    }
    f = out.stream;
    fprintf(f, "{\"schema\":\"xasm-source-model\",\"version\":1,\"profile\":\"asm-edit-v1\",\"target\":\"mos6502\",\"status\":\"%s\",\"analysis_outcomes\":{\"comparison\":\"%s\",\"audit\":\"%s\"},",
            certified ? "certified" : "diagnostic", comparison, audit);
    fprintf(f, "\"exit_status\":%d,", exit_status);
    fprintf(f, "\"identity\":{\"schema_id\":\"xasm-source-model:1\",\"profile_id\":\"asm-edit-v1\",\"input_id\":\"%s\",\"producer_sha256\":\"%s\",\"model_id\":\"%s\"},\"semantic_options\":{\"case_insensitive\":%s,\"swap_parens\":%s},\"files\":[",
            input_id, producer_hash, model_id, xasm_args.case_insensitive ? "true" : "false", xasm_args.swap_parens ? "true" : "false");
    for (i = 0; i < file_count; ++i) {
        dependency_snapshot *s = &files[i].snapshot;
        if (i) fputc(',', f);
        fprintf(f, "{\"id\":\"f%zu\",\"path\":", i + 1); string(f, files[i].logical_path);
        fprintf(f, ",\"byte_length\":%zu,\"sha256\":\"%s\",\"bytes_base64\":", s->size, s->sha256);
        base64(f, s->bytes, s->size); fputc('}', f);
    }
    fputs("],", f); dependencies_write_evidence(f);
    unit_total = write_units(f, certified);
    if (certified && failed && !diagnostic_count)
        diagnose("SOURCE_MODEL_INCOMPLETE", "source units are not completely covered", 0, 0, 0, 0, 0);
    write_nodes(f, certified); write_layout(f, certified);
    for (i = 0; i < node_count; ++i) {
        references += nodes[i].reference; expressions += nodes[i].expression; declarations += nodes[i].is_declaration;
        instructions += nodes[i].source->type == INSTRUCTION_NODE;
        data += nodes[i].source->type == DATA_NODE;
        expected_locations += is_label(nodes[i].source->type) || nodes[i].source->type == INSTRUCTION_NODE ||
                              nodes[i].source->type == DATA_NODE || nodes[i].source->type == ORG_NODE;
        located += nodes[i].located;
        evaluated += nodes[i].expression && nodes[i].value_set;
    }
    fprintf(f, ",\"coverage\":{\"units\":%zu,\"syntax\":%zu,\"expressions\":%zu,\"declarations\":%zu,\"expected_references\":%zu,\"bound_references\":",
            unit_total, node_count, expressions, declarations, references);
    size_t bound = 0;
    for (i = 0; i < node_count; ++i) if (nodes[i].reference && nodes[i].declaration) ++bound;
    fprintf(f, "%zu,\"instruction_statements\":%zu,\"data_statements\":%zu,\"expected_locations\":%zu,\"located_statements\":%zu,\"evaluated_expressions\":%zu},\"diagnostics\":[",
            bound, instructions, data, expected_locations, located, evaluated);
    for (i = 0; i < diagnostic_count; ++i) {
        model_diagnostic *d = &diagnostics[i];
        if (i) fputc(',', f);
        fprintf(f, "{\"id\":\"z%zu\",\"code\":", i + 1); string(f, d->code);
        fputs(",\"severity\":\"error\",\"message\":", f); string(f, d->message);
        fputs(",\"span\":", f); span(f, d->file, d->begin, d->end);
        fputs(",\"syntax_id\":", f); id(f, 's', d->syntax);
        fputs(",\"related_declaration_id\":", f); id(f, 'd', d->related); fputc('}', f);
    }
    fputs("]}\n", f);
    if (ferror(f)) {
        fprintf(stderr, "error: SOURCE_MODEL_IO_ERROR: could not write model output\n");
        output_file_discard(&out); return 0;
    }
    if ((certified && source_model_failed()) || !dependencies_validate()) {
        output_file_discard(&out); return 0;
    }
    if (!output_file_finish(&out, 1)) {
        fprintf(stderr, "error: SOURCE_MODEL_IO_ERROR: could not stage model output\n"); return 0;
    }
    return 1;
}
void source_model_report(void)
{
    size_t i;
    if (oom) fprintf(stderr, "error: SOURCE_MODEL_RESOURCE_LIMIT: source-model allocation or depth limit reached\n");
    for (i = 0; i < diagnostic_count && i < 20; ++i)
        fprintf(stderr, "error: %s: %s\n", diagnostics[i].code, diagnostics[i].message);
    if (diagnostic_count > 20) fprintf(stderr, "error: %zu additional source-model diagnostics\n", diagnostic_count - 20);
}
void source_model_clear(void)
{
    size_t i;
    for (i = 0; i < file_count; ++i) { free(files[i].lines); free(files[i].logical_path); }
    for (i = 0; i < node_count; ++i) {
        /* Unsupported nodes can contain native ownership unions. Only scalar
         * metadata cloned here belongs to the model. */
        astnode *a = nodes[i].source;
        if (a) {
            if (is_reference(a->type) || is_label(a->type) || a->type == STRING_NODE) free(a->string);
            free(a);
        }
        astnode_finalize(nodes[i].prepared);
    }
    free(files); free(nodes); free(units); free(ranges); free(diagnostics); free(declaration_lookup);
    files = NULL; nodes = NULL; units = NULL; ranges = NULL; diagnostics = NULL;
    file_count = node_count = unit_count = range_count = diagnostic_count = 0;
    node_capacity = unit_capacity = range_capacity = diagnostic_capacity = 0;
    failed = oom = internal_error = finished = 0; last_match = SIZE_MAX;
    native_values = NULL; binary = NULL; binary_size = 0;
    declaration_lookup = NULL; declaration_capacity = 0;
}
