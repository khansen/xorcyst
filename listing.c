#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "listing.h"
#include "astproc.h"
#include "symtab.h"
#include "opcode.h"

#define SOURCE_LINE_BUFFER_SIZE 1024
#define HEX_BYTES_PER_ROW 4
#define HEX_COLUMN_WIDTH 12
#define EVAL_RECURSION_LIMIT 64
#define DIVIDER "-------------------------------------------------------------------------------"

typedef struct tag_listed_label {
    char *raw;
    char *display;
    int address;
} listed_label;

static FILE *listing_fp = NULL;
static FILE *source_fp = NULL;
static char *current_source_file = NULL;
static int current_source_line_cached = 0;
static char source_line_buffer[SOURCE_LINE_BUFFER_SIZE];

static int in_dataseg = 0;
static int dataseg_pc = 0;
static int codeseg_pc = 0;

static int last_printed_line = -1;
static char *last_printed_file = NULL;

static listed_label *listed_labels = NULL;
static int listed_labels_size = 0;
static int listed_labels_capacity = 0;

static char **scope_by_namespace = NULL;
static int scope_by_namespace_size = 0;
static int scope_counter = 0;
static char *current_scope_name = NULL;

static int get_current_pc(void)
{
    return in_dataseg ? dataseg_pc : codeseg_pc;
}

static void set_current_pc(int value)
{
    if (in_dataseg) {
        dataseg_pc = value;
    } else {
        codeseg_pc = value;
    }
}

static void add_current_pc(int value)
{
    if (in_dataseg) {
        dataseg_pc += value;
    } else {
        codeseg_pc += value;
    }
}

static char *xstrdup(const char *s)
{
    char *copy;
    if (s == NULL) {
        return NULL;
    }
    copy = (char *)malloc(strlen(s) + 1);
    if (copy != NULL) {
        strcpy(copy, s);
    }
    return copy;
}

static char *strdup_prefix(const char *s, int n)
{
    char *copy;
    if (s == NULL || n < 0) {
        return NULL;
    }
    copy = (char *)malloc((size_t)n + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, s, (size_t)n);
    copy[n] = '\0';
    return copy;
}

static char *str_concat(const char *a, const char *b)
{
    size_t len_a;
    size_t len_b;
    char *s;
    if (a == NULL && b == NULL) {
        return xstrdup("");
    }
    if (a == NULL) {
        return xstrdup(b);
    }
    if (b == NULL) {
        return xstrdup(a);
    }
    len_a = strlen(a);
    len_b = strlen(b);
    s = (char *)malloc(len_a + len_b + 1);
    if (s == NULL) {
        return NULL;
    }
    memcpy(s, a, len_a);
    memcpy(s + len_a, b, len_b);
    s[len_a + len_b] = '\0';
    return s;
}

static void close_source_cache(void)
{
    if (source_fp != NULL) {
        fclose(source_fp);
        source_fp = NULL;
    }
    if (current_source_file != NULL) {
        free(current_source_file);
        current_source_file = NULL;
    }
    current_source_line_cached = 0;
}

static void reset_last_printed_source(void)
{
    if (last_printed_file != NULL) {
        free(last_printed_file);
        last_printed_file = NULL;
    }
    last_printed_line = -1;
}

static void free_listed_labels(void)
{
    int i;
    for (i = 0; i < listed_labels_size; ++i) {
        if (listed_labels[i].raw != NULL) {
            free(listed_labels[i].raw);
        }
        if (listed_labels[i].display != NULL) {
            free(listed_labels[i].display);
        }
    }
    free(listed_labels);
    listed_labels = NULL;
    listed_labels_size = 0;
    listed_labels_capacity = 0;
}

static void clear_scope_map(void)
{
    int i;
    for (i = 0; i < scope_by_namespace_size; ++i) {
        free(scope_by_namespace[i]);
    }
    free(scope_by_namespace);
    scope_by_namespace = NULL;
    scope_by_namespace_size = 0;
    scope_counter = 0;

    free(current_scope_name);
    current_scope_name = NULL;
}

static int ensure_scope_capacity(int index)
{
    int new_size;
    char **tmp;
    int i;
    if (index < scope_by_namespace_size) {
        return 1;
    }

    new_size = (scope_by_namespace_size == 0) ? 8 : scope_by_namespace_size;
    while (new_size <= index) {
        new_size *= 2;
    }

    tmp = (char **)realloc(scope_by_namespace, (size_t)new_size * sizeof(char *));
    if (tmp == NULL) {
        return 0;
    }

    for (i = scope_by_namespace_size; i < new_size; ++i) {
        tmp[i] = NULL;
    }

    scope_by_namespace = tmp;
    scope_by_namespace_size = new_size;
    return 1;
}

static void set_scope_name(int index, const char *name)
{
    if (index < 0 || !ensure_scope_capacity(index)) {
        return;
    }

    free(scope_by_namespace[index]);
    scope_by_namespace[index] = xstrdup(name);
}

static const char *get_scope_name(int index)
{
    if (index < 0 || index >= scope_by_namespace_size) {
        return NULL;
    }
    return scope_by_namespace[index];
}

static void set_current_scope(const char *scope_name)
{
    free(current_scope_name);
    current_scope_name = xstrdup(scope_name);
}

static int append_listed_label(const char *raw, const char *display, int address)
{
    listed_label *tmp;

    if (listed_labels_size >= listed_labels_capacity) {
        int new_capacity = (listed_labels_capacity == 0) ? 16 : (listed_labels_capacity * 2);
        tmp = (listed_label *)realloc(listed_labels, (size_t)new_capacity * sizeof(listed_label));
        if (tmp == NULL) {
            return 0;
        }
        listed_labels = tmp;
        listed_labels_capacity = new_capacity;
    }

    listed_labels[listed_labels_size].raw = xstrdup(raw);
    listed_labels[listed_labels_size].display = xstrdup(display);
    listed_labels[listed_labels_size].address = address;
    if (listed_labels[listed_labels_size].raw == NULL || listed_labels[listed_labels_size].display == NULL) {
        free(listed_labels[listed_labels_size].raw);
        free(listed_labels[listed_labels_size].display);
        listed_labels[listed_labels_size].raw = NULL;
        listed_labels[listed_labels_size].display = NULL;
        return 0;
    }

    listed_labels_size++;
    return 1;
}

static int is_internal_label_name(const char *raw)
{
    if (raw == NULL || raw[0] == '\0') {
        return 0;
    }
    if ((raw[0] == '@') && (raw[1] == '@')) {
        return 1;
    }
    return (raw[0] == '+') || (raw[0] == '-');
}

static int parse_namespace_suffix(const char *raw, int *ns_index)
{
    const char *p;
    int value = 0;
    if (raw == NULL || ns_index == NULL) {
        return 0;
    }
    p = strrchr(raw, '#');
    if (p == NULL || *(p + 1) == '\0') {
        return 0;
    }
    p++;
    while (*p != '\0') {
        if (!isdigit((unsigned char)*p)) {
            return 0;
        }
        value = value * 10 + (*p - '0');
        p++;
    }
    *ns_index = value;
    return 1;
}

static char *base_without_hash_suffix(const char *raw)
{
    const char *hash = strchr(raw, '#');
    if (hash == NULL) {
        return xstrdup(raw);
    }
    return strdup_prefix(raw, (int)(hash - raw));
}

static int is_scope_anchor(const char *raw, symtab_entry *e)
{
    if (raw == NULL || e == NULL) {
        return 0;
    }
    if (is_internal_label_name(raw)) {
        return 0;
    }
    return (e->type == LABEL_SYMBOL) || (e->type == PROC_SYMBOL);
}

static char *display_label_name(const char *raw)
{
    char *display = NULL;
    char *base = NULL;
    const char *owner = NULL;
    int ns_index = -1;

    if (raw == NULL) {
        return xstrdup("");
    }
    if (!is_internal_label_name(raw)) {
        return xstrdup(raw);
    }

    if ((raw[0] == '@') && (raw[1] == '@')) {
        base = base_without_hash_suffix(raw);
        if (parse_namespace_suffix(raw, &ns_index)) {
            owner = get_scope_name(ns_index);
        }
        if (owner == NULL) {
            owner = current_scope_name;
        }
        if (owner != NULL) {
            display = str_concat(owner, base);
        } else {
            display = xstrdup(base);
        }
        free(base);
        return display;
    }

    if ((raw[0] == '+') || (raw[0] == '-')) {
        if (current_scope_name != NULL) {
            return str_concat(current_scope_name, raw);
        }
        return xstrdup(raw);
    }

    return xstrdup(raw);
}

static const char *get_source_line(const char *filename, int line)
{
    char *p;
    if (filename == NULL || line <= 0) {
        return "";
    }

    if (source_fp == NULL || current_source_file == NULL || strcmp(current_source_file, filename) != 0) {
        close_source_cache();

        source_fp = fopen(filename, "r");
        if (source_fp == NULL) {
            return "";
        }

        current_source_file = xstrdup(filename);
        if (current_source_file == NULL) {
            fclose(source_fp);
            source_fp = NULL;
            return "";
        }
    }

    if (line <= current_source_line_cached) {
        rewind(source_fp);
        current_source_line_cached = 0;
    }

    while (current_source_line_cached < line) {
        if (fgets(source_line_buffer, sizeof(source_line_buffer), source_fp) == NULL) {
            return "";
        }
        current_source_line_cached++;
    }

    p = strrchr(source_line_buffer, '\n');
    if (p != NULL) {
        *p = '\0';
    }

    p = strrchr(source_line_buffer, '\r');
    if (p != NULL) {
        *p = '\0';
    }

    return source_line_buffer;
}

static int eval_expression_int(astnode *expr, int *value, int depth)
{
    int lhs = 0;
    int rhs = 0;
    symtab_entry *e;

    if (expr == NULL || value == NULL || depth > EVAL_RECURSION_LIMIT) {
        return 0;
    }

    switch (astnode_get_type(expr)) {
        case INTEGER_NODE:
            *value = expr->integer;
            return 1;

        case CURRENT_PC_NODE:
            *value = get_current_pc();
            return 1;

        case IDENTIFIER_NODE:
            e = symtab_lookup(expr->ident);
            if (e == NULL) {
                return 0;
            }
            if (e->flags & ADDR_FLAG) {
                *value = e->address;
                return 1;
            }
            if (e->type == CONSTANT_SYMBOL && e->def != NULL) {
                return eval_expression_int(e->def, value, depth + 1);
            }
            return 0;

        case ARITHMETIC_NODE:
            switch (expr->oper) {
                case NEG_OPERATOR:
                case NOT_OPERATOR:
                case LO_OPERATOR:
                case HI_OPERATOR:
                case UMINUS_OPERATOR:
                case BANK_OPERATOR:
                    if (!eval_expression_int(LHS(expr), &lhs, depth + 1)) {
                        return 0;
                    }
                    switch (expr->oper) {
                        case NEG_OPERATOR: *value = ~lhs; return 1;
                        case NOT_OPERATOR: *value = !lhs; return 1;
                        case LO_OPERATOR: *value = lhs & 0xFF; return 1;
                        case HI_OPERATOR: *value = (lhs >> 8) & 0xFF; return 1;
                        case UMINUS_OPERATOR: *value = -lhs; return 1;
                        case BANK_OPERATOR: *value = (lhs >> 16) & 0xFF; return 1;
                        default: return 0;
                    }

                default:
                    if (!eval_expression_int(LHS(expr), &lhs, depth + 1)) {
                        return 0;
                    }
                    if (!eval_expression_int(RHS(expr), &rhs, depth + 1)) {
                        return 0;
                    }
                    switch (expr->oper) {
                        case PLUS_OPERATOR: *value = lhs + rhs; return 1;
                        case MINUS_OPERATOR: *value = lhs - rhs; return 1;
                        case MUL_OPERATOR: *value = lhs * rhs; return 1;
                        case DIV_OPERATOR: *value = (rhs != 0) ? (lhs / rhs) : 0; return 1;
                        case MOD_OPERATOR: *value = (rhs != 0) ? (lhs % rhs) : 0; return 1;
                        case AND_OPERATOR: *value = lhs & rhs; return 1;
                        case OR_OPERATOR: *value = lhs | rhs; return 1;
                        case XOR_OPERATOR: *value = lhs ^ rhs; return 1;
                        case SHL_OPERATOR: *value = lhs << rhs; return 1;
                        case SHR_OPERATOR: *value = lhs >> rhs; return 1;
                        case LT_OPERATOR: *value = lhs < rhs; return 1;
                        case GT_OPERATOR: *value = lhs > rhs; return 1;
                        case EQ_OPERATOR: *value = lhs == rhs; return 1;
                        case NE_OPERATOR: *value = lhs != rhs; return 1;
                        case LE_OPERATOR: *value = lhs <= rhs; return 1;
                        case GE_OPERATOR: *value = lhs >= rhs; return 1;
                        default: return 0;
                    }
            }

        default:
            return 0;
    }
}

static void format_hex_bytes(const unsigned char *bytes, int count, char *out, int out_size)
{
    int i;
    int n;
    int pos = 0;

    if (out_size <= 0) {
        return;
    }

    out[0] = '\0';
    if (bytes == NULL || count <= 0) {
        return;
    }

    for (i = 0; i < count; ++i) {
        if (pos >= out_size - 1) {
            break;
        }
        n = snprintf(out + pos, out_size - pos, "%02X ", bytes[i]);
        if (n <= 0 || n >= out_size - pos) {
            break;
        }
        pos += n;
    }
}

static void print_listing_line(location loc, int addr, const unsigned char *bytes, int count)
{
    char hex_buf[HEX_COLUMN_WIDTH + 1];
    const char *source_line = "";
    int print_source = 0;
    int row_count;
    int offset;

    if (loc.file != NULL &&
        (last_printed_file == NULL ||
         strcmp(last_printed_file, loc.file) != 0 ||
         last_printed_line != loc.first_line)) {
        source_line = get_source_line(loc.file, loc.first_line);
        print_source = 1;

        free(last_printed_file);
        last_printed_file = xstrdup(loc.file);
        last_printed_line = loc.first_line;
    }

    row_count = (count > HEX_BYTES_PER_ROW) ? HEX_BYTES_PER_ROW : count;
    format_hex_bytes(bytes, row_count, hex_buf, sizeof(hex_buf));
    fprintf(listing_fp, "%04d  %04X  %-12s  %s\n",
            loc.first_line, addr & 0xFFFF, hex_buf, print_source ? source_line : "");

    offset = HEX_BYTES_PER_ROW;
    while (offset < count) {
        row_count = ((count - offset) > HEX_BYTES_PER_ROW) ? HEX_BYTES_PER_ROW : (count - offset);
        format_hex_bytes(bytes + offset, row_count, hex_buf, sizeof(hex_buf));
        fprintf(listing_fp, "      %04X  %-12s\n", (addr + offset) & 0xFFFF, hex_buf);
        offset += HEX_BYTES_PER_ROW;
    }
}

static int append_byte(unsigned char **buffer, int *size, int *capacity, unsigned char value)
{
    unsigned char *tmp;
    int new_capacity;

    if (*size >= *capacity) {
        new_capacity = (*capacity == 0) ? 32 : (*capacity * 2);
        tmp = (unsigned char *)realloc(*buffer, (size_t)new_capacity);
        if (tmp == NULL) {
            return 0;
        }
        *buffer = tmp;
        *capacity = new_capacity;
    }
    (*buffer)[*size] = value;
    (*size)++;
    return 1;
}

static int is_relative_branch_opcode(unsigned char op)
{
    switch (op) {
        case 0x10:
        case 0x30:
        case 0x50:
        case 0x70:
        case 0x90:
        case 0xB0:
        case 0xD0:
        case 0xF0:
            return 1;
        default:
            return 0;
    }
}

static int list_instruction(astnode *instr, void *arg, astnode **next)
{
    unsigned char op = instr->instr.opcode;
    int len = opcode_length(op);
    unsigned char bytes[3];
    int value = 0;
    int addr = get_current_pc();
    int count = 0;

    (void)arg;
    (void)next;

    if (len < 1) {
        len = 1;
    }

    bytes[count++] = op;
    if (len > 1 && LHS(instr) != NULL) {
        if (!eval_expression_int(LHS(instr), &value, 0)) {
            value = 0;
        }

        if (len == 2) {
            if (is_relative_branch_opcode(op)) {
                value = value - (addr + 2);
            }
            bytes[count++] = (unsigned char)(value & 0xFF);
        } else {
            bytes[count++] = (unsigned char)(value & 0xFF);
            bytes[count++] = (unsigned char)((value >> 8) & 0xFF);
        }
    }

    print_listing_line(instr->loc, addr, bytes, count);
    add_current_pc(len);
    return 0;
}

static int list_data(astnode *data, void *arg, astnode **next)
{
    astnode *type = LHS(data);
    astnode *expr;
    unsigned char *line_bytes = NULL;
    int line_size = 0;
    int line_capacity = 0;
    int value = 0;
    int addr_start = get_current_pc();

    (void)arg;
    (void)next;

    for (expr = RHS(data); expr != NULL; expr = astnode_get_next_sibling(expr)) {
        if (!eval_expression_int(expr, &value, 0)) {
            value = 0;
        }

        if (type->datatype == BYTE_DATATYPE) {
            append_byte(&line_bytes, &line_size, &line_capacity, (unsigned char)(value & 0xFF));
            add_current_pc(1);
        } else if (type->datatype == WORD_DATATYPE) {
            append_byte(&line_bytes, &line_size, &line_capacity, (unsigned char)(value & 0xFF));
            append_byte(&line_bytes, &line_size, &line_capacity, (unsigned char)((value >> 8) & 0xFF));
            add_current_pc(2);
        } else if (type->datatype == DWORD_DATATYPE) {
            append_byte(&line_bytes, &line_size, &line_capacity, (unsigned char)(value & 0xFF));
            append_byte(&line_bytes, &line_size, &line_capacity, (unsigned char)((value >> 8) & 0xFF));
            append_byte(&line_bytes, &line_size, &line_capacity, (unsigned char)((value >> 16) & 0xFF));
            append_byte(&line_bytes, &line_size, &line_capacity, (unsigned char)((value >> 24) & 0xFF));
            add_current_pc(4);
        }
    }

    print_listing_line(data->loc, addr_start, line_bytes, line_size);
    free(line_bytes);
    return 0;
}

static int list_storage(astnode *storage, void *arg, astnode **next)
{
    int count = 0;

    (void)arg;
    (void)next;

    print_listing_line(storage->loc, get_current_pc(), NULL, 0);
    if (eval_expression_int(RHS(storage), &count, 0) && count > 0) {
        add_current_pc(count);
    }
    return 0;
}

static int list_binary(astnode *node, void *arg, astnode **next)
{
    int addr = get_current_pc();
    (void)arg;
    (void)next;
    print_listing_line(node->loc, addr, node->binary.data, node->binary.size);
    add_current_pc(node->binary.size);
    return 0;
}

static int list_org(astnode *org, void *arg, astnode **next)
{
    int addr = get_current_pc();
    (void)arg;
    (void)next;

    if (eval_expression_int(LHS(org), &addr, 0)) {
        set_current_pc(addr);
    } else {
        addr = get_current_pc();
    }
    print_listing_line(org->loc, addr, NULL, 0);
    return 0;
}

static int list_dataseg(astnode *n, void *arg, astnode **next)
{
    (void)arg;
    (void)next;
    in_dataseg = 1;
    print_listing_line(n->loc, get_current_pc(), NULL, 0);
    return 0;
}

static int list_codeseg(astnode *n, void *arg, astnode **next)
{
    (void)arg;
    (void)next;
    in_dataseg = 0;
    print_listing_line(n->loc, get_current_pc(), NULL, 0);
    return 0;
}

static int list_label(astnode *label, void *arg, astnode **next)
{
    symtab_entry *e;
    const char *raw;
    char *display;
    int addr = get_current_pc();

    (void)arg;
    (void)next;

    raw = label->label;
    e = symtab_lookup(raw);
    if (e != NULL && (e->flags & ADDR_FLAG)) {
        addr = e->address;
    }

    display = display_label_name(raw);
    if (display == NULL) {
        display = xstrdup(raw);
    }

    print_listing_line(label->loc, addr, NULL, 0);
    append_listed_label(raw, display, addr);

    if (is_scope_anchor(raw, e)) {
        scope_counter++;
        set_scope_name(scope_counter, display);
        set_current_scope(display);
    }

    free(display);
    return 0;
}

static int list_noop(astnode *n, void *arg, astnode **next)
{
    (void)n;
    (void)arg;
    (void)next;
    return 0;
}

static void list_symbols(void)
{
    symbol_ident_list constants;
    int i;

    fprintf(listing_fp, "\n%s\n", DIVIDER);
    fprintf(listing_fp, "SYMBOL TABLE:\n");
    fprintf(listing_fp, "%s\n", DIVIDER);

    for (i = 0; i < listed_labels_size; ++i) {
        fprintf(listing_fp, "%-18s = $%04X\n",
                listed_labels[i].display, listed_labels[i].address & 0xFFFF);
    }

    symtab_list_type(CONSTANT_SYMBOL, &constants);
    for (i = 0; i < constants.size; ++i) {
        symtab_entry *e = symtab_lookup(constants.idents[i]);
        int value = 0;
        if (e == NULL) {
            continue;
        }
        if (e->flags & ADDR_FLAG) {
            value = e->address;
            fprintf(listing_fp, "%-18s = $%04X\n", e->id, value & 0xFFFF);
        } else if (e->def != NULL && eval_expression_int(e->def, &value, 0)) {
            fprintf(listing_fp, "%-18s = $%04X\n", e->id, value & 0xFFFF);
        }
    }
    symtab_list_finalize(&constants);

    fprintf(listing_fp, "%s\n", DIVIDER);
}

void generate_listing(astnode *root, const char *filename)
{
    static astnodeprocmap map[] = {
        { DATASEG_NODE, list_dataseg },
        { CODESEG_NODE, list_codeseg },
        { ORG_NODE, list_org },
        { LABEL_NODE, list_label },
        { INSTRUCTION_NODE, list_instruction },
        { DATA_NODE, list_data },
        { STORAGE_NODE, list_storage },
        { BINARY_NODE, list_binary },
        { STRUC_DECL_NODE, list_noop },
        { UNION_DECL_NODE, list_noop },
        { ENUM_DECL_NODE, list_noop },
        { RECORD_DECL_NODE, list_noop },
        { 0, NULL }
    };

    listing_fp = fopen(filename, "w");
    if (listing_fp == NULL) {
        fprintf(stderr, "error: could not open `%s' for writing\n", filename);
        return;
    }

    in_dataseg = 0;
    dataseg_pc = 0;
    codeseg_pc = 0;
    close_source_cache();
    reset_last_printed_source();
    free_listed_labels();
    clear_scope_map();

    fprintf(listing_fp, "%s\n", DIVIDER);
    fprintf(listing_fp, "LINE  ADDR  HEX BYTES     SOURCE CODE\n");
    fprintf(listing_fp, "%s\n", DIVIDER);

    astproc_walk(root, NULL, map);
    list_symbols();

    fclose(listing_fp);
    listing_fp = NULL;
    close_source_cache();
    reset_last_printed_source();
    free_listed_labels();
    clear_scope_map();
}
