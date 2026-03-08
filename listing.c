#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
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
static listing_format current_listing_format = LISTING_FORMAT_TEXT;
static const char *listing_source_file_arg = NULL;
static const char *listing_output_file_arg = NULL;
static long listing_output_offset = 0;
static int json_record_count = 0;

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

static const char *addressing_mode_name(addressing_mode mode)
{
    switch (mode) {
        case IMPLIED_MODE: return "implied";
        case ACCUMULATOR_MODE: return "accumulator";
        case IMMEDIATE_MODE: return "immediate";
        case ZEROPAGE_MODE: return "zeropage";
        case ZEROPAGE_X_MODE: return "zeropage_x";
        case ZEROPAGE_Y_MODE: return "zeropage_y";
        case ABSOLUTE_MODE: return "absolute";
        case ABSOLUTE_X_MODE: return "absolute_x";
        case ABSOLUTE_Y_MODE: return "absolute_y";
        case PREINDEXED_INDIRECT_MODE: return "preindexed_indirect";
        case POSTINDEXED_INDIRECT_MODE: return "postindexed_indirect";
        case INDIRECT_MODE: return "indirect";
        case RELATIVE_MODE: return "relative";
        default: return NULL;
    }
}

static const char *datatype_directive_name(datatype type)
{
    switch (type) {
        case BYTE_DATATYPE:
        case CHAR_DATATYPE:
            return ".DB";
        case WORD_DATATYPE:
            return ".DW";
        case DWORD_DATATYPE:
            return ".DD";
        default:
            return ".DB";
    }
}

static void print_json_string(FILE *fp, const char *s)
{
    const unsigned char *p = (const unsigned char *)s;
    fputc('"', fp);
    if (p != NULL) {
        while (*p != '\0') {
            unsigned char c = *p++;
            switch (c) {
                case '\"': fputs("\\\"", fp); break;
                case '\\': fputs("\\\\", fp); break;
                case '\b': fputs("\\b", fp); break;
                case '\f': fputs("\\f", fp); break;
                case '\n': fputs("\\n", fp); break;
                case '\r': fputs("\\r", fp); break;
                case '\t': fputs("\\t", fp); break;
                default:
                if (c < 0x20) {
                    fprintf(fp, "\\u%04X", c);
                } else {
                    fputc(c, fp);
                }
                break;
            }
        }
    }
    fputc('"', fp);
}

static void emit_structured_record(location loc,
                                   int addr,
                                   const unsigned char *bytes,
                                   int count,
                                   const char *source_text,
                                   const char *directive_or_opcode,
                                   const char *addressing_mode,
                                   int continuation,
                                   long row_output_offset)
{
    int i;
    int cpu_end = addr + ((count > 0) ? (count - 1) : 0);
    long output_end = row_output_offset + ((count > 0) ? (count - 1) : 0);

    if (current_listing_format == LISTING_FORMAT_JSON) {
        if (json_record_count > 0) {
            fprintf(listing_fp, ",\n");
        }
        fprintf(listing_fp, "    {");
        json_record_count++;
    } else {
        fprintf(listing_fp, "{");
    }

    fprintf(listing_fp, "\"file\":");
    print_json_string(listing_fp, loc.file != NULL ? loc.file : "");
    fprintf(listing_fp, ",\"line\":%d", loc.first_line);
    fprintf(listing_fp, ",\"column\":%d", loc.first_column);
    fprintf(listing_fp, ",\"source_text\":");
    print_json_string(listing_fp, source_text != NULL ? source_text : "");

    fprintf(listing_fp, ",\"cpu_address_start\":\"0x%04X\"", addr & 0xFFFF);
    fprintf(listing_fp, ",\"cpu_address_end\":\"0x%04X\"", cpu_end & 0xFFFF);
    fprintf(listing_fp, ",\"output_offset_start\":%ld", row_output_offset);
    fprintf(listing_fp, ",\"output_offset_end\":%ld", output_end);

    fprintf(listing_fp, ",\"bytes_hex\":[");
    for (i = 0; i < count; ++i) {
        if (i > 0) {
            fprintf(listing_fp, ",");
        }
        fprintf(listing_fp, "\"%02X\"", bytes[i]);
    }
    fprintf(listing_fp, "]");

    fprintf(listing_fp, ",\"directive_or_opcode\":");
    print_json_string(listing_fp, directive_or_opcode != NULL ? directive_or_opcode : "");
    fprintf(listing_fp, ",\"addressing_mode\":");
    if (addressing_mode != NULL) {
        print_json_string(listing_fp, addressing_mode);
    } else {
        fprintf(listing_fp, "null");
    }
    fprintf(listing_fp, ",\"continuation_of_record\":%s", continuation ? "true" : "false");
    fprintf(listing_fp, "}");
    if (current_listing_format == LISTING_FORMAT_NDJSON) {
        fprintf(listing_fp, "\n");
    }
}

static void print_listing_line(location loc,
                               int addr,
                               const unsigned char *bytes,
                               int count,
                               const char *directive_or_opcode,
                               const char *addressing_mode)
{
    char hex_buf[HEX_COLUMN_WIDTH + 1];
    const char *source_line = "";
    int print_source = 0;
    int row_count;
    int offset;

    if (current_listing_format == LISTING_FORMAT_TEXT) {
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
    } else {
        source_line = get_source_line(loc.file, loc.first_line);
        if (count <= 0) {
            emit_structured_record(loc,
                                   addr,
                                   NULL,
                                   0,
                                   source_line,
                                   directive_or_opcode,
                                   addressing_mode,
                                   0,
                                   listing_output_offset);
        } else {
            offset = 0;
            while (offset < count) {
                row_count = ((count - offset) > HEX_BYTES_PER_ROW) ? HEX_BYTES_PER_ROW : (count - offset);
                emit_structured_record(loc,
                                       addr + offset,
                                       bytes + offset,
                                       row_count,
                                       source_line,
                                       directive_or_opcode,
                                       addressing_mode,
                                       (offset > 0),
                                       listing_output_offset + offset);
                offset += row_count;
            }
        }
    }

    if (count > 0) {
        listing_output_offset += count;
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
    const char *opcode_name = opcode_to_string(op);
    const char *mode_name = addressing_mode_name(instr->instr.mode);

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

    print_listing_line(instr->loc, addr, bytes, count, opcode_name, mode_name);
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

    print_listing_line(data->loc,
                       addr_start,
                       line_bytes,
                       line_size,
                       datatype_directive_name(type->datatype),
                       NULL);
    free(line_bytes);
    return 0;
}

static int list_storage(astnode *storage, void *arg, astnode **next)
{
    int count = 0;

    (void)arg;
    (void)next;

    print_listing_line(storage->loc, get_current_pc(), NULL, 0, ".DSB", NULL);
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
    print_listing_line(node->loc, addr, node->binary.data, node->binary.size, "INCBIN", NULL);
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
    print_listing_line(org->loc, addr, NULL, 0, ".ORG", NULL);
    return 0;
}

static int list_dataseg(astnode *n, void *arg, astnode **next)
{
    (void)arg;
    (void)next;
    in_dataseg = 1;
    print_listing_line(n->loc, get_current_pc(), NULL, 0, ".DATASEG", NULL);
    return 0;
}

static int list_codeseg(astnode *n, void *arg, astnode **next)
{
    (void)arg;
    (void)next;
    in_dataseg = 0;
    print_listing_line(n->loc, get_current_pc(), NULL, 0, ".CODESEG", NULL);
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

    print_listing_line(label->loc, addr, NULL, 0, display, NULL);
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

int generate_listing(astnode *root,
                     const char *filename,
                     listing_format format,
                     const char *source_file,
                     const char *output_file)
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
        { MACRO_NODE, list_noop },
        { MACRO_DECL_NODE, list_noop },
        { PROC_NODE, list_noop },
        { REPT_NODE, list_noop },
        { WHILE_NODE, list_noop },
        { DO_NODE, list_noop },
        { EXITM_NODE, list_noop },
        { PUSH_MACRO_BODY_NODE, list_noop },
        { POP_MACRO_BODY_NODE, list_noop },
        { PUSH_BRANCH_SCOPE_NODE, list_noop },
        { POP_BRANCH_SCOPE_NODE, list_noop },
        { UNDEF_NODE, list_noop },
        { TOMBSTONE_NODE, list_noop },
        { 0, NULL }
    };

    listing_fp = fopen(filename, "w");
    if (listing_fp == NULL) {
        fprintf(stderr, "error: could not open `%s' for writing\n", filename);
        return 0;
    }

    current_listing_format = format;
    listing_source_file_arg = source_file;
    listing_output_file_arg = output_file;
    listing_output_offset = 0;
    json_record_count = 0;
    in_dataseg = 0;
    dataseg_pc = 0;
    codeseg_pc = 0;
    close_source_cache();
    reset_last_printed_source();
    free_listed_labels();
    clear_scope_map();

    if (current_listing_format == LISTING_FORMAT_TEXT) {
        fprintf(listing_fp, "%s\n", DIVIDER);
        fprintf(listing_fp, "LINE  ADDR  HEX BYTES     SOURCE CODE\n");
        fprintf(listing_fp, "%s\n", DIVIDER);
    } else if (current_listing_format == LISTING_FORMAT_JSON) {
        fprintf(listing_fp, "{\n");
        fprintf(listing_fp, "  \"version\": \"1\",\n");
        fprintf(listing_fp, "  \"source_file\": ");
        print_json_string(listing_fp, listing_source_file_arg != NULL ? listing_source_file_arg : "");
        fprintf(listing_fp, ",\n");
        fprintf(listing_fp, "  \"output_file\": ");
        print_json_string(listing_fp, listing_output_file_arg != NULL ? listing_output_file_arg : "");
        fprintf(listing_fp, ",\n");
        fprintf(listing_fp, "  \"records\": [\n");
    } else if (current_listing_format != LISTING_FORMAT_NDJSON) {
        fprintf(stderr, "error: invalid listing format\n");
        fclose(listing_fp);
        listing_fp = NULL;
        close_source_cache();
        reset_last_printed_source();
        free_listed_labels();
        clear_scope_map();
        return 0;
    }

    astproc_walk(root, NULL, map);
    if (current_listing_format == LISTING_FORMAT_TEXT) {
        list_symbols();
    } else if (current_listing_format == LISTING_FORMAT_JSON) {
        if (json_record_count > 0) {
            fprintf(listing_fp, "\n");
        }
        fprintf(listing_fp, "  ]\n");
        fprintf(listing_fp, "}\n");
    }

    fclose(listing_fp);
    listing_fp = NULL;
    close_source_cache();
    reset_last_printed_source();
    free_listed_labels();
    clear_scope_map();
    return 1;
}

static long lookup_target_offset = -1;
static long lookup_current_offset = 0;
static int lookup_found = 0;
static listing_lookup_result lookup_result;

static void lookup_consume_bytes(location loc, int addr, int count)
{
    if (count <= 0) {
        return;
    }
    if (!lookup_found &&
        lookup_target_offset >= lookup_current_offset &&
        lookup_target_offset < lookup_current_offset + count) {
        lookup_found = 1;
        lookup_result.file = loc.file;
        lookup_result.line = loc.first_line;
        lookup_result.column = loc.first_column;
        lookup_result.cpu_address = (addr + (int)(lookup_target_offset - lookup_current_offset)) & 0xFFFF;
        lookup_result.cpu_address_known = 1;
    }
    lookup_current_offset += count;
}

static int lookup_dataseg(astnode *n, void *arg, astnode **next)
{
    (void)n;
    (void)arg;
    (void)next;
    in_dataseg = 1;
    return 0;
}

static int lookup_codeseg(astnode *n, void *arg, astnode **next)
{
    (void)n;
    (void)arg;
    (void)next;
    in_dataseg = 0;
    return 0;
}

static int lookup_org(astnode *org, void *arg, astnode **next)
{
    int addr = get_current_pc();
    (void)arg;
    (void)next;
    if (eval_expression_int(LHS(org), &addr, 0)) {
        set_current_pc(addr);
    }
    return 0;
}

static int lookup_instruction(astnode *instr, void *arg, astnode **next)
{
    int len = opcode_length(instr->instr.opcode);
    int addr = get_current_pc();
    (void)arg;
    (void)next;
    if (len < 1) {
        len = 1;
    }
    if (!in_dataseg) {
        lookup_consume_bytes(instr->loc, addr, len);
    }
    add_current_pc(len);
    return 0;
}

static int lookup_data(astnode *data, void *arg, astnode **next)
{
    int bytes_per_item = 1;
    int count = 0;
    int total_bytes;
    astnode *expr;
    int addr = get_current_pc();
    (void)arg;
    (void)next;

    switch (LHS(data)->datatype) {
        case BYTE_DATATYPE:
        case CHAR_DATATYPE:
            bytes_per_item = 1;
            break;
        case WORD_DATATYPE:
            bytes_per_item = 2;
            break;
        case DWORD_DATATYPE:
            bytes_per_item = 4;
            break;
        default:
            bytes_per_item = 1;
            break;
    }

    for (expr = RHS(data); expr != NULL; expr = astnode_get_next_sibling(expr)) {
        count++;
    }
    total_bytes = count * bytes_per_item;

    if (!in_dataseg) {
        lookup_consume_bytes(data->loc, addr, total_bytes);
    }
    add_current_pc(total_bytes);
    return 0;
}

static int lookup_storage(astnode *storage, void *arg, astnode **next)
{
    int count = 0;
    int addr = get_current_pc();
    (void)arg;
    (void)next;
    if (eval_expression_int(RHS(storage), &count, 0) && count > 0) {
        if (!in_dataseg) {
            lookup_consume_bytes(storage->loc, addr, count);
        }
        add_current_pc(count);
    }
    return 0;
}

static int lookup_binary(astnode *node, void *arg, astnode **next)
{
    int addr = get_current_pc();
    (void)arg;
    (void)next;
    if (!in_dataseg) {
        lookup_consume_bytes(node->loc, addr, node->binary.size);
    }
    add_current_pc(node->binary.size);
    return 0;
}

int listing_lookup_output_offset(astnode *root, long output_offset, listing_lookup_result *out)
{
    static astnodeprocmap map[] = {
        { DATASEG_NODE, lookup_dataseg },
        { CODESEG_NODE, lookup_codeseg },
        { ORG_NODE, lookup_org },
        { INSTRUCTION_NODE, lookup_instruction },
        { DATA_NODE, lookup_data },
        { STORAGE_NODE, lookup_storage },
        { BINARY_NODE, lookup_binary },
        { STRUC_DECL_NODE, list_noop },
        { UNION_DECL_NODE, list_noop },
        { ENUM_DECL_NODE, list_noop },
        { RECORD_DECL_NODE, list_noop },
        { MACRO_NODE, list_noop },
        { MACRO_DECL_NODE, list_noop },
        { PROC_NODE, list_noop },
        { REPT_NODE, list_noop },
        { WHILE_NODE, list_noop },
        { DO_NODE, list_noop },
        { EXITM_NODE, list_noop },
        { PUSH_MACRO_BODY_NODE, list_noop },
        { POP_MACRO_BODY_NODE, list_noop },
        { PUSH_BRANCH_SCOPE_NODE, list_noop },
        { POP_BRANCH_SCOPE_NODE, list_noop },
        { UNDEF_NODE, list_noop },
        { TOMBSTONE_NODE, list_noop },
        { 0, NULL }
    };

    if (root == NULL || out == NULL || output_offset < 0) {
        return 0;
    }

    lookup_target_offset = output_offset;
    lookup_current_offset = 0;
    lookup_found = 0;
    lookup_result.file = NULL;
    lookup_result.line = 0;
    lookup_result.column = 0;
    lookup_result.cpu_address = 0;
    lookup_result.cpu_address_known = 0;

    in_dataseg = 0;
    dataseg_pc = 0;
    codeseg_pc = 0;
    astproc_walk(root, NULL, map);

    if (!lookup_found) {
        return 0;
    }
    *out = lookup_result;
    return 1;
}

typedef struct tag_xref_symbol {
    char *name;
    char *kind;
    char *scope;
    int defined;
    location definition_loc;
    int has_cpu_address;
    int cpu_address;
    int has_output_offset;
    long output_offset;
    int has_value;
    long value;
    int is_dataseg;
    int segment_id;
} xref_symbol;

typedef struct tag_xref_ref {
    char *symbol;
    location use_loc;
    int has_cpu_address;
    int cpu_address;
    int has_output_offset;
    long output_offset;
    char *opcode;
    char *addressing_mode;
    char *access;
    char *expression;
    int is_dataseg;
    int segment_id;
} xref_ref;

typedef struct tag_xref_meta {
    int has_cpu_address;
    int cpu_address;
    int has_output_offset;
    long output_offset;
    const char *opcode;
    const char *addressing_mode;
    const char *access;
    const char *expression;
    int is_dataseg;
    int segment_id;
} xref_meta;

typedef struct tag_xref_build_context {
    xref_symbol *symbols;
    int symbol_count;
    int symbol_capacity;
    xref_ref *refs;
    int ref_count;
    int ref_capacity;
    struct tag_xref_instr *instrs;
    int instr_count;
    int instr_capacity;
    int include_locals;
    int include_anon;
    int include_data;
    int include_owner;
    int pure_binary;
    long output_offset;
    int current_segment_id;
    int next_segment_id;
    int failed;
} xref_build_context;

typedef struct tag_xref_instr {
    int cpu_address;
    int is_dataseg;
    int segment_id;
    int mnemonic;
    addressing_mode mode;
    char *direct_symbol;
    int direct_displacement;
    int direct_access_kind; /* 1=read, 2=write */
    int zp_write_addr_known;
    int zp_write_addr;
    int indirect_ptr_addr_known;
    int indirect_ptr_addr;
    int indirect_access_kind; /* 1=read, 2=write */
} xref_instr;

typedef struct tag_xref_data_edge {
    char *symbol;
    int site_addr;
    int segment_id;
    char *routine;
    char *owner_routine;
    int owner_routine_addr;
    int has_owner_routine_addr;
    int displacement;
    char *addressing_mode;
} xref_data_edge;

typedef struct tag_xref_indirect_flow {
    char *ptr_symbol;
    int producer_site;
    int consumer_site;
    int segment_id;
    char *access_kind;
    char *routine;
    char *owner_routine;
    int owner_routine_addr;
    int has_owner_routine_addr;
} xref_indirect_flow;

typedef struct tag_xref_owner_info {
    const char *name;
    int has_addr;
    int addr;
} xref_owner_info;

static int extract_symbol_and_displacement(astnode *expr, const char **symbol, int *displacement);

static int starts_with(const char *s, const char *prefix)
{
    size_t n;
    if (s == NULL || prefix == NULL) {
        return 0;
    }
    n = strlen(prefix);
    return strncmp(s, prefix, n) == 0;
}

static int count_char_occurrences(const char *s, char ch)
{
    int count = 0;
    if (s == NULL) {
        return 0;
    }
    while (*s != '\0') {
        if (*s == ch) {
            count++;
        }
        s++;
    }
    return count;
}

static void classify_symbol_name(const char *name, const char **kind, const char **scope)
{
    if (name != NULL) {
        if (name[0] == '+' || name[0] == '-') {
            *kind = "anonymous_label";
            *scope = "anonymous";
            return;
        }
        if (strstr(name, "+#") != NULL || strstr(name, "-#") != NULL) {
            *kind = "anonymous_label";
            *scope = "anonymous";
            return;
        }
        if (strstr(name, "@@") != NULL) {
            if (count_char_occurrences(name, '#') >= 2) {
                *kind = "macro_label";
            } else {
                *kind = "local_label";
            }
            *scope = "local";
            return;
        }
    }
    *kind = "label";
    *scope = "global";
}

static int scope_allowed(const char *scope, int include_locals, int include_anon)
{
    if (scope == NULL) {
        return 1;
    }
    if (strcmp(scope, "local") == 0 && !include_locals) {
        return 0;
    }
    if (strcmp(scope, "anonymous") == 0 && !include_anon) {
        return 0;
    }
    return 1;
}

static int ensure_xref_symbol_capacity(xref_build_context *ctx)
{
    xref_symbol *tmp;
    int new_capacity;
    if (ctx->symbol_count < ctx->symbol_capacity) {
        return 1;
    }
    new_capacity = (ctx->symbol_capacity == 0) ? 32 : (ctx->symbol_capacity * 2);
    tmp = (xref_symbol *)realloc(ctx->symbols, (size_t)new_capacity * sizeof(xref_symbol));
    if (tmp == NULL) {
        ctx->failed = 1;
        return 0;
    }
    ctx->symbols = tmp;
    ctx->symbol_capacity = new_capacity;
    return 1;
}

static int ensure_xref_ref_capacity(xref_build_context *ctx)
{
    xref_ref *tmp;
    int new_capacity;
    if (ctx->ref_count < ctx->ref_capacity) {
        return 1;
    }
    new_capacity = (ctx->ref_capacity == 0) ? 64 : (ctx->ref_capacity * 2);
    tmp = (xref_ref *)realloc(ctx->refs, (size_t)new_capacity * sizeof(xref_ref));
    if (tmp == NULL) {
        ctx->failed = 1;
        return 0;
    }
    ctx->refs = tmp;
    ctx->ref_capacity = new_capacity;
    return 1;
}

static int ensure_xref_instr_capacity(xref_build_context *ctx)
{
    xref_instr *tmp;
    int new_capacity;
    if (ctx->instr_count < ctx->instr_capacity) {
        return 1;
    }
    new_capacity = (ctx->instr_capacity == 0) ? 64 : (ctx->instr_capacity * 2);
    tmp = (xref_instr *)realloc(ctx->instrs, (size_t)new_capacity * sizeof(xref_instr));
    if (tmp == NULL) {
        ctx->failed = 1;
        return 0;
    }
    ctx->instrs = tmp;
    ctx->instr_capacity = new_capacity;
    return 1;
}

static int find_xref_symbol_index(const xref_build_context *ctx, const char *name)
{
    int i;
    for (i = 0; i < ctx->symbol_count; ++i) {
        if (strcmp(ctx->symbols[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int add_or_update_xref_symbol(xref_build_context *ctx,
                                     const char *name,
                                     const char *kind,
                                     const char *scope,
                                     int defined,
                                     const location *loc,
                                     int has_cpu_address,
                                     int cpu_address,
                                     int segment_id,
                                     int has_output_offset,
                                     long output_offset,
                                     int has_value,
                                     long value)
{
    int index;
    xref_symbol *s;

    if (!scope_allowed(scope, ctx->include_locals, ctx->include_anon)) {
        return 1;
    }

    index = find_xref_symbol_index(ctx, name);
    if (index < 0) {
        if (!ensure_xref_symbol_capacity(ctx)) {
            return 0;
        }
        s = &ctx->symbols[ctx->symbol_count++];
        memset(s, 0, sizeof(*s));
        s->name = xstrdup(name);
        s->kind = xstrdup(kind);
        s->scope = xstrdup(scope);
        if (s->name == NULL || s->kind == NULL || s->scope == NULL) {
            ctx->failed = 1;
            return 0;
        }
    } else {
        s = &ctx->symbols[index];
        if (defined && !s->defined) {
            free(s->kind);
            free(s->scope);
            s->kind = xstrdup(kind);
            s->scope = xstrdup(scope);
            if (s->kind == NULL || s->scope == NULL) {
                ctx->failed = 1;
                return 0;
            }
        }
    }

    if (defined) {
        s->defined = 1;
        if (loc != NULL) {
            s->definition_loc = *loc;
        }
        s->has_cpu_address = has_cpu_address;
        s->cpu_address = cpu_address;
        s->segment_id = segment_id;
        s->has_output_offset = has_output_offset;
        s->output_offset = output_offset;
        s->has_value = has_value;
        s->value = value;
    }
    return 1;
}

static int add_xref_reference(xref_build_context *ctx,
                              const char *symbol,
                              const location *use_loc,
                              const xref_meta *meta)
{
    const char *kind;
    const char *scope;
    xref_ref *r;

    classify_symbol_name(symbol, &kind, &scope);
    if (!scope_allowed(scope, ctx->include_locals, ctx->include_anon)) {
        return 1;
    }

    if (!add_or_update_xref_symbol(ctx,
                                   symbol,
                                   kind,
                                   scope,
                                   0,
                                   NULL,
                                   0,
                                   0,
                                   -1,
                                   0,
                                   0,
                                   0,
                                   0)) {
        return 0;
    }

    if (!ensure_xref_ref_capacity(ctx)) {
        return 0;
    }

    r = &ctx->refs[ctx->ref_count++];
    memset(r, 0, sizeof(*r));
    r->symbol = xstrdup(symbol);
    r->use_loc = *use_loc;
    r->has_cpu_address = meta->has_cpu_address;
    r->cpu_address = meta->cpu_address;
    r->has_output_offset = meta->has_output_offset;
    r->output_offset = meta->output_offset;
    r->opcode = xstrdup(meta->opcode != NULL ? meta->opcode : "");
    r->addressing_mode = xstrdup(meta->addressing_mode != NULL ? meta->addressing_mode : "");
    r->access = xstrdup(meta->access != NULL ? meta->access : "other");
    r->expression = xstrdup(meta->expression != NULL ? meta->expression : "");
    r->is_dataseg = meta->is_dataseg;
    r->segment_id = meta->segment_id;
    if (r->symbol == NULL || r->opcode == NULL || r->addressing_mode == NULL
        || r->access == NULL || r->expression == NULL) {
        ctx->failed = 1;
        return 0;
    }
    return 1;
}

static void start_xref_segment(xref_build_context *ctx)
{
    ctx->current_segment_id = ctx->next_segment_id++;
}

static void trim_line_copy(char *dst, int dst_size, const char *src)
{
    const char *start;
    const char *end;
    int len;
    const char *comment;
    if (dst_size <= 0) {
        return;
    }
    dst[0] = '\0';
    if (src == NULL) {
        return;
    }
    comment = strchr(src, ';');
    if (comment != NULL) {
        len = (int)(comment - src);
    } else {
        len = (int)strlen(src);
    }
    start = src;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }
    end = src + len;
    while (end > start && isspace((unsigned char)*(end - 1))) {
        end--;
    }
    len = (int)(end - start);
    if (len >= dst_size) {
        len = dst_size - 1;
    }
    if (len > 0) {
        memcpy(dst, start, (size_t)len);
    }
    dst[len] = '\0';
}

static void extract_operand_from_line(location loc, char *dst, int dst_size)
{
    const char *line;
    const char *p;
    int col;
    if (dst_size <= 0) {
        return;
    }
    dst[0] = '\0';
    if (loc.file == NULL || loc.first_line <= 0) {
        return;
    }
    line = get_source_line(loc.file, loc.first_line);
    if (line == NULL) {
        return;
    }
    col = loc.first_column > 0 ? loc.first_column - 1 : 0;
    if ((size_t)col > strlen(line)) {
        col = 0;
    }
    p = line + col;
    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }
    while (*p != '\0' && !isspace((unsigned char)*p)) {
        p++;
    }
    while (*p != '\0' && isspace((unsigned char)*p)) {
        p++;
    }
    trim_line_copy(dst, dst_size, p);
}

static const char *classify_instruction_access(const char *opcode, astnode *operand, addressing_mode mode)
{
    if (opcode == NULL || *opcode == '\0') {
        return "other";
    }
    if (strcmp(opcode, "JSR") == 0) {
        return "call";
    }
    if (strcmp(opcode, "JMP") == 0) {
        return "jump";
    }
    if ((strlen(opcode) == 3) && opcode[0] == 'B') {
        return "branch";
    }
    if (mode == IMMEDIATE_MODE && operand != NULL && astnode_is_type(operand, ARITHMETIC_NODE)) {
        if (operand->oper == LO_OPERATOR) {
            return "pointer_lo";
        }
        if (operand->oper == HI_OPERATOR) {
            return "pointer_hi";
        }
    }
    if (starts_with(opcode, "ST")) {
        return "write";
    }
    if (starts_with(opcode, "LD")
        || starts_with(opcode, "CP")
        || strcmp(opcode, "BIT") == 0
        || strcmp(opcode, "ADC") == 0
        || strcmp(opcode, "SBC") == 0
        || strcmp(opcode, "AND") == 0
        || strcmp(opcode, "ORA") == 0
        || strcmp(opcode, "EOR") == 0) {
        return "read";
    }
    return "other";
}

static int xref_data_access_kind_from_string(const char *access)
{
    if (strcmp(access, "read") == 0) {
        return 1;
    }
    if (strcmp(access, "write") == 0) {
        return 2;
    }
    return 0;
}

static int is_supported_xref_data_direct_mode(addressing_mode mode)
{
    return (mode == ABSOLUTE_MODE
         || mode == ABSOLUTE_X_MODE
         || mode == ABSOLUTE_Y_MODE
         || mode == ZEROPAGE_MODE
         || mode == ZEROPAGE_X_MODE
         || mode == ZEROPAGE_Y_MODE);
}

static int collect_expr_refs_recursive(astnode *expr, xref_build_context *ctx, const xref_meta *meta)
{
    astnode *child;
    if (expr == NULL) {
        return 1;
    }
    if (astnode_is_type(expr, IDENTIFIER_NODE)) {
        if (!add_xref_reference(ctx, expr->ident, &expr->loc, meta)) {
            return 0;
        }
    }
    for (child = astnode_get_first_child(expr); child != NULL; child = astnode_get_next_sibling(child)) {
        if (!collect_expr_refs_recursive(child, ctx, meta)) {
            return 0;
        }
    }
    return 1;
}

static int xref_visit_dataseg(astnode *n, void *arg, astnode **next)
{
    xref_build_context *ctx = (xref_build_context *)arg;
    (void)n;
    (void)next;
    in_dataseg = 1;
    start_xref_segment(ctx);
    return 0;
}

static int xref_visit_codeseg(astnode *n, void *arg, astnode **next)
{
    xref_build_context *ctx = (xref_build_context *)arg;
    (void)n;
    (void)next;
    in_dataseg = 0;
    start_xref_segment(ctx);
    return 0;
}

static int xref_visit_org(astnode *org, void *arg, astnode **next)
{
    int addr = get_current_pc();
    xref_build_context *ctx = (xref_build_context *)arg;
    (void)next;
    if (eval_expression_int(LHS(org), &addr, 0)) {
        set_current_pc(addr);
    }
    start_xref_segment(ctx);
    return 0;
}

static int xref_visit_label(astnode *label, void *arg, astnode **next)
{
    xref_build_context *ctx = (xref_build_context *)arg;
    const char *kind;
    const char *scope;
    int addr;
    (void)next;
    classify_symbol_name(label->label, &kind, &scope);
    addr = get_current_pc();
    if (!add_or_update_xref_symbol(ctx,
                                   label->label,
                                   kind,
                                   scope,
                                   1,
                                   &label->loc,
                                   1,
                                   addr,
                                   ctx->current_segment_id,
                                   ctx->pure_binary && !in_dataseg,
                                   ctx->output_offset,
                                   0,
                                   0)) {
        return 0;
    }
    /* Record section for the defined symbol */
    {
        int idx = find_xref_symbol_index(ctx, label->label);
        if (idx >= 0) {
            ctx->symbols[idx].is_dataseg = in_dataseg;
        }
    }
    return 0;
}

static int xref_visit_instruction(astnode *instr, void *arg, astnode **next)
{
    xref_build_context *ctx = (xref_build_context *)arg;
    int addr = get_current_pc();
    int len = opcode_length(instr->instr.opcode);
    xref_meta meta;
    xref_instr *out = NULL;
    char expr_buf[512];
    const char *resolved_symbol = NULL;
    int resolved_displacement = 0;
    int resolved_value = 0;
    int record_instruction = 0;
    (void)next;

    if (len < 1) {
        len = 1;
    }
    if (ctx->include_data || ctx->include_owner) {
        if (!ensure_xref_instr_capacity(ctx)) {
            goto finish;
        }
        out = &ctx->instrs[ctx->instr_count];
        memset(out, 0, sizeof(*out));
        out->cpu_address = addr;
        out->is_dataseg = in_dataseg;
        out->segment_id = ctx->current_segment_id;
        out->mnemonic = instr->instr.mnemonic.value;
        out->mode = instr->instr.mode;
    }

    memset(&meta, 0, sizeof(meta));
    meta.has_cpu_address = 1;
    meta.cpu_address = addr;
    meta.has_output_offset = ctx->pure_binary && !in_dataseg;
    meta.output_offset = ctx->output_offset;
    meta.opcode = opcode_to_string(instr->instr.opcode);
    meta.addressing_mode = addressing_mode_name(instr->instr.mode);
    meta.access = classify_instruction_access(meta.opcode, LHS(instr), instr->instr.mode);
    extract_operand_from_line(instr->loc, expr_buf, sizeof(expr_buf));
    meta.expression = expr_buf;
    meta.is_dataseg = in_dataseg;
    meta.segment_id = ctx->current_segment_id;

    if (ctx->include_data) {
        if (LHS(instr) != NULL
            && is_supported_xref_data_direct_mode(instr->instr.mode)
            && extract_symbol_and_displacement(LHS(instr), &resolved_symbol, &resolved_displacement)) {
            out->direct_symbol = xstrdup(resolved_symbol);
            out->direct_displacement = resolved_displacement;
            out->direct_access_kind = xref_data_access_kind_from_string(meta.access);
            if (out->direct_symbol == NULL) {
                ctx->failed = 1;
                goto finish;
            }
        }

        if (LHS(instr) != NULL
            && xref_data_access_kind_from_string(meta.access) == 2
            && (instr->instr.mode == ABSOLUTE_MODE || instr->instr.mode == ZEROPAGE_MODE)
            && eval_expression_int(LHS(instr), &resolved_value, 0)
            && resolved_value >= 0 && resolved_value <= 0xFF) {
            out->zp_write_addr_known = 1;
            out->zp_write_addr = resolved_value;
        }

        if (LHS(instr) != NULL
            && xref_data_access_kind_from_string(meta.access) != 0
            && (instr->instr.mode == PREINDEXED_INDIRECT_MODE
                || instr->instr.mode == POSTINDEXED_INDIRECT_MODE)
            && eval_expression_int(LHS(instr), &resolved_value, 0)
            && resolved_value >= 0 && resolved_value <= 0xFF) {
            out->indirect_ptr_addr_known = 1;
            out->indirect_ptr_addr = resolved_value;
            out->indirect_access_kind = xref_data_access_kind_from_string(meta.access);
        }
    }

    if (LHS(instr) != NULL) {
        if (!collect_expr_refs_recursive(LHS(instr), ctx, &meta)) {
            ctx->failed = 1;
            goto finish;
        }
    }

    record_instruction = (ctx->include_data || ctx->include_owner);

finish:
    if (record_instruction) {
        ctx->instr_count++;
    }
    add_current_pc(len);
    if (ctx->pure_binary && !in_dataseg) {
        ctx->output_offset += len;
    }
    return 0;
}

static int xref_visit_data(astnode *data, void *arg, astnode **next)
{
    xref_build_context *ctx = (xref_build_context *)arg;
    astnode *expr;
    int bytes_per_item;
    int addr = get_current_pc();
    int item_count = 0;
    int index = 0;
    xref_meta meta;
    char expr_buf[512];
    (void)next;

    switch (LHS(data)->datatype) {
        case BYTE_DATATYPE:
        case CHAR_DATATYPE:
            bytes_per_item = 1;
            break;
        case WORD_DATATYPE:
            bytes_per_item = 2;
            break;
        case DWORD_DATATYPE:
            bytes_per_item = 4;
            break;
        default:
            bytes_per_item = 1;
            break;
    }

    extract_operand_from_line(data->loc, expr_buf, sizeof(expr_buf));

    for (expr = RHS(data); expr != NULL; expr = astnode_get_next_sibling(expr)) {
        item_count++;
    }

    for (expr = RHS(data); expr != NULL; expr = astnode_get_next_sibling(expr)) {
        memset(&meta, 0, sizeof(meta));
        meta.has_cpu_address = 1;
        meta.cpu_address = addr + (index * bytes_per_item);
        meta.has_output_offset = ctx->pure_binary && !in_dataseg;
        meta.output_offset = ctx->output_offset + (index * bytes_per_item);
        meta.opcode = NULL;
        meta.addressing_mode = NULL;
        meta.access = "address_compute";
        meta.expression = expr_buf;
        meta.is_dataseg = in_dataseg;
        meta.segment_id = ctx->current_segment_id;
        if (!collect_expr_refs_recursive(expr, ctx, &meta)) {
            ctx->failed = 1;
            break;
        }
        index++;
    }

    add_current_pc(item_count * bytes_per_item);
    if (ctx->pure_binary && !in_dataseg) {
        ctx->output_offset += item_count * bytes_per_item;
    }
    return 0;
}

static int xref_visit_storage(astnode *storage, void *arg, astnode **next)
{
    xref_build_context *ctx = (xref_build_context *)arg;
    int count = 0;
    (void)next;
    if (eval_expression_int(RHS(storage), &count, 0) && count > 0) {
        add_current_pc(count);
        if (ctx->pure_binary && !in_dataseg) {
            ctx->output_offset += count;
        }
    }
    return 0;
}

static int xref_visit_binary(astnode *node, void *arg, astnode **next)
{
    xref_build_context *ctx = (xref_build_context *)arg;
    (void)next;
    add_current_pc(node->binary.size);
    if (ctx->pure_binary && !in_dataseg) {
        ctx->output_offset += node->binary.size;
    }
    return 0;
}

static void free_xref_context(xref_build_context *ctx)
{
    int i;
    for (i = 0; i < ctx->symbol_count; ++i) {
        free(ctx->symbols[i].name);
        free(ctx->symbols[i].kind);
        free(ctx->symbols[i].scope);
    }
    free(ctx->symbols);
    ctx->symbols = NULL;
    ctx->symbol_count = 0;
    ctx->symbol_capacity = 0;

    for (i = 0; i < ctx->ref_count; ++i) {
        free(ctx->refs[i].symbol);
        free(ctx->refs[i].opcode);
        free(ctx->refs[i].addressing_mode);
        free(ctx->refs[i].access);
        free(ctx->refs[i].expression);
    }
    free(ctx->refs);
    ctx->refs = NULL;
    ctx->ref_count = 0;
    ctx->ref_capacity = 0;

    for (i = 0; i < ctx->instr_count; ++i) {
        free(ctx->instrs[i].direct_symbol);
    }
    free(ctx->instrs);
    ctx->instrs = NULL;
    ctx->instr_count = 0;
    ctx->instr_capacity = 0;
}

static int xref_symbol_compare(const void *a, const void *b)
{
    const xref_symbol *lhs = (const xref_symbol *)a;
    const xref_symbol *rhs = (const xref_symbol *)b;
    if (lhs->has_cpu_address != rhs->has_cpu_address) {
        return rhs->has_cpu_address - lhs->has_cpu_address;
    }
    if (lhs->has_cpu_address && rhs->has_cpu_address && lhs->cpu_address != rhs->cpu_address) {
        return lhs->cpu_address - rhs->cpu_address;
    }
    if (lhs->segment_id != rhs->segment_id) {
        return lhs->segment_id - rhs->segment_id;
    }
    return strcmp(lhs->name, rhs->name);
}

static int xref_ref_compare(const void *a, const void *b)
{
    const xref_ref *lhs = (const xref_ref *)a;
    const xref_ref *rhs = (const xref_ref *)b;
    if (lhs->has_cpu_address != rhs->has_cpu_address) {
        return rhs->has_cpu_address - lhs->has_cpu_address;
    }
    if (lhs->has_cpu_address && rhs->has_cpu_address && lhs->cpu_address != rhs->cpu_address) {
        return lhs->cpu_address - rhs->cpu_address;
    }
    if (lhs->segment_id != rhs->segment_id) {
        return lhs->segment_id - rhs->segment_id;
    }
    if (lhs->has_output_offset != rhs->has_output_offset) {
        return rhs->has_output_offset - lhs->has_output_offset;
    }
    if (lhs->has_output_offset && rhs->has_output_offset && lhs->output_offset != rhs->output_offset) {
        return (lhs->output_offset < rhs->output_offset) ? -1 : 1;
    }
    return strcmp(lhs->symbol, rhs->symbol);
}

static int has_xref_instruction_at_address(const xref_build_context *ctx,
                                           int addr,
                                           int is_dataseg,
                                           int segment_id)
{
    int i;
    for (i = 0; i < ctx->instr_count; i++) {
        if (ctx->instrs[i].cpu_address == addr
            && ctx->instrs[i].is_dataseg == is_dataseg
            && ctx->instrs[i].segment_id == segment_id) {
            return 1;
        }
    }
    return 0;
}

static void lookup_xref_routine_owner(const xref_build_context *ctx,
                                      int site_addr,
                                      int is_dataseg,
                                      int segment_id,
                                      xref_owner_info *out)
{
    int i;
    const xref_symbol *best = NULL;
    int best_addr = -1;
    out->name = NULL;
    out->has_addr = 0;
    out->addr = 0;
    for (i = 0; i < ctx->symbol_count; i++) {
        const xref_symbol *s = &ctx->symbols[i];
        if (!s->defined || !s->has_cpu_address) {
            continue;
        }
        if (s->is_dataseg != is_dataseg || s->segment_id != segment_id) {
            continue;
        }
        if (strcmp(s->scope, "global") != 0 || strcmp(s->kind, "label") != 0) {
            continue;
        }
        if (s->cpu_address > site_addr) {
            continue;
        }
        if (!has_xref_instruction_at_address(ctx, s->cpu_address, is_dataseg, segment_id)) {
            continue;
        }
        if (s->cpu_address > best_addr) {
            best = s;
            best_addr = s->cpu_address;
        }
    }
    if (best != NULL) {
        out->name = best->name;
        out->has_addr = 1;
        out->addr = best->cpu_address;
    }
}

static int ensure_xref_data_edge_capacity(xref_data_edge **edges, int *count, int *capacity)
{
    xref_data_edge *tmp;
    int new_capacity;
    if (*count < *capacity) {
        return 1;
    }
    new_capacity = (*capacity == 0) ? 16 : (*capacity * 2);
    tmp = (xref_data_edge *)realloc(*edges, (size_t)new_capacity * sizeof(xref_data_edge));
    if (tmp == NULL) {
        return 0;
    }
    *edges = tmp;
    *capacity = new_capacity;
    return 1;
}

static int ensure_xref_indirect_flow_capacity(xref_indirect_flow **flows, int *count, int *capacity)
{
    xref_indirect_flow *tmp;
    int new_capacity;
    if (*count < *capacity) {
        return 1;
    }
    new_capacity = (*capacity == 0) ? 8 : (*capacity * 2);
    tmp = (xref_indirect_flow *)realloc(*flows, (size_t)new_capacity * sizeof(xref_indirect_flow));
    if (tmp == NULL) {
        return 0;
    }
    *flows = tmp;
    *capacity = new_capacity;
    return 1;
}

static void free_xref_data_edge(xref_data_edge *edge)
{
    free(edge->symbol);
    free(edge->routine);
    free(edge->owner_routine);
    free(edge->addressing_mode);
}

static void free_xref_indirect_flow(xref_indirect_flow *flow)
{
    free(flow->ptr_symbol);
    free(flow->access_kind);
    free(flow->routine);
    free(flow->owner_routine);
}

static int xref_data_edge_compare(const void *a, const void *b)
{
    const xref_data_edge *lhs = (const xref_data_edge *)a;
    const xref_data_edge *rhs = (const xref_data_edge *)b;
    int cmp;
    if (lhs->site_addr != rhs->site_addr) {
        return lhs->site_addr - rhs->site_addr;
    }
    if (lhs->segment_id != rhs->segment_id) {
        return lhs->segment_id - rhs->segment_id;
    }
    cmp = strcmp(lhs->symbol, rhs->symbol);
    if (cmp != 0) {
        return cmp;
    }
    if (lhs->routine == NULL && rhs->routine != NULL) {
        return -1;
    }
    if (lhs->routine != NULL && rhs->routine == NULL) {
        return 1;
    }
    if (lhs->routine != NULL && rhs->routine != NULL) {
        cmp = strcmp(lhs->routine, rhs->routine);
        if (cmp != 0) {
            return cmp;
        }
    }
    if (lhs->owner_routine == NULL && rhs->owner_routine != NULL) {
        return -1;
    }
    if (lhs->owner_routine != NULL && rhs->owner_routine == NULL) {
        return 1;
    }
    if (lhs->owner_routine != NULL && rhs->owner_routine != NULL) {
        cmp = strcmp(lhs->owner_routine, rhs->owner_routine);
        if (cmp != 0) {
            return cmp;
        }
    }
    if (lhs->displacement != rhs->displacement) {
        return lhs->displacement - rhs->displacement;
    }
    return strcmp(lhs->addressing_mode, rhs->addressing_mode);
}

static int xref_indirect_flow_compare(const void *a, const void *b)
{
    const xref_indirect_flow *lhs = (const xref_indirect_flow *)a;
    const xref_indirect_flow *rhs = (const xref_indirect_flow *)b;
    int cmp;
    cmp = strcmp(lhs->ptr_symbol, rhs->ptr_symbol);
    if (cmp != 0) {
        return cmp;
    }
    if (lhs->producer_site != rhs->producer_site) {
        return lhs->producer_site - rhs->producer_site;
    }
    if (lhs->consumer_site != rhs->consumer_site) {
        return lhs->consumer_site - rhs->consumer_site;
    }
    if (lhs->segment_id != rhs->segment_id) {
        return lhs->segment_id - rhs->segment_id;
    }
    cmp = strcmp(lhs->access_kind, rhs->access_kind);
    if (cmp != 0) {
        return cmp;
    }
    if (lhs->routine == NULL && rhs->routine != NULL) {
        return -1;
    }
    if (lhs->routine != NULL && rhs->routine == NULL) {
        return 1;
    }
    if (lhs->routine != NULL && rhs->routine != NULL) {
        cmp = strcmp(lhs->routine, rhs->routine);
        if (cmp != 0) {
            return cmp;
        }
    }
    if (lhs->owner_routine == NULL && rhs->owner_routine != NULL) {
        return -1;
    }
    if (lhs->owner_routine != NULL && rhs->owner_routine == NULL) {
        return 1;
    }
    if (lhs->owner_routine == NULL) {
        return 0;
    }
    return strcmp(lhs->owner_routine, rhs->owner_routine);
}

static const char *find_symbol_at_address(const xref_build_context *ctx,
                                          int addr,
                                          int is_dataseg,
                                          int segment_id)
{
    int i;
    for (i = 0; i < ctx->symbol_count; i++) {
        const xref_symbol *s = &ctx->symbols[i];
        if (!s->defined || !s->has_cpu_address) {
            continue;
        }
        if (s->cpu_address == addr && s->is_dataseg == is_dataseg && s->segment_id == segment_id) {
            return s->name;
        }
    }
    return NULL;
}

static const char *find_symbol_at_any_section(const xref_build_context *ctx, int addr)
{
    int i;
    for (i = 0; i < ctx->symbol_count; i++) {
        const xref_symbol *s = &ctx->symbols[i];
        if (!s->defined || !s->has_cpu_address) {
            continue;
        }
        if (s->cpu_address == addr) {
            return s->name;
        }
    }
    return NULL;
}

static const char *resolve_pointer_pair_symbol(const xref_build_context *ctx,
                                               int low_addr,
                                               int is_dataseg,
                                               int segment_id)
{
    const char *low_name = find_symbol_at_address(ctx, low_addr, is_dataseg, segment_id);
    if (low_name == NULL) {
        low_name = find_symbol_at_any_section(ctx, low_addr);
    }
    if (low_name != NULL) {
        return low_name;
    }
    return NULL;
}

static int nullable_string_equal(const char *lhs, const char *rhs)
{
    if (lhs == NULL && rhs == NULL) {
        return 1;
    }
    if (lhs == NULL || rhs == NULL) {
        return 0;
    }
    return strcmp(lhs, rhs) == 0;
}

static void format_xref_addr(char *buf, int buf_size, int addr)
{
    if (buf_size <= 0) {
        return;
    }
    if (addr >= 0 && addr <= 0xFFFF) {
        snprintf(buf, (size_t)buf_size, "0x%04X", addr & 0xFFFF);
    } else {
        snprintf(buf, (size_t)buf_size, "0x%X", addr);
    }
}

static void format_xref_owner_addr(char *buf, int buf_size, int addr)
{
    if (buf_size <= 0) {
        return;
    }
    if (addr >= 0 && addr <= 0xFFFF) {
        snprintf(buf, (size_t)buf_size, "0x%04x", addr & 0xFFFF);
    } else {
        snprintf(buf, (size_t)buf_size, "0x%x", addr);
    }
}

static int append_xref_data_edge(xref_data_edge **edges,
                                 int *count,
                                 int *capacity,
                                 const char *symbol,
                                 int site_addr,
                                 int segment_id,
                                 const char *routine,
                                 const char *owner_routine,
                                 int owner_routine_addr,
                                 int has_owner_routine_addr,
                                 int displacement,
                                 const char *addressing_mode)
{
    xref_data_edge *edge;
    if (!ensure_xref_data_edge_capacity(edges, count, capacity)) {
        return 0;
    }
    edge = &(*edges)[*count];
    memset(edge, 0, sizeof(*edge));
    edge->symbol = xstrdup(symbol);
    edge->site_addr = site_addr;
    edge->segment_id = segment_id;
    edge->routine = routine != NULL ? xstrdup(routine) : NULL;
    edge->owner_routine = owner_routine != NULL ? xstrdup(owner_routine) : NULL;
    edge->owner_routine_addr = owner_routine_addr;
    edge->has_owner_routine_addr = has_owner_routine_addr;
    edge->displacement = displacement;
    edge->addressing_mode = xstrdup(addressing_mode);
    if (edge->symbol == NULL || edge->addressing_mode == NULL
        || (routine != NULL && edge->routine == NULL)
        || (owner_routine != NULL && edge->owner_routine == NULL)) {
        free_xref_data_edge(edge);
        memset(edge, 0, sizeof(*edge));
        return 0;
    }
    (*count)++;
    return 1;
}

static int append_xref_indirect_flow(xref_indirect_flow **flows,
                                     int *count,
                                     int *capacity,
                                     const char *ptr_symbol,
                                     int producer_site,
                                     int consumer_site,
                                     int segment_id,
                                     const char *access_kind,
                                     const char *routine,
                                     const char *owner_routine,
                                     int owner_routine_addr,
                                     int has_owner_routine_addr)
{
    xref_indirect_flow *flow;
    if (!ensure_xref_indirect_flow_capacity(flows, count, capacity)) {
        return 0;
    }
    flow = &(*flows)[*count];
    memset(flow, 0, sizeof(*flow));
    flow->ptr_symbol = xstrdup(ptr_symbol);
    flow->producer_site = producer_site;
    flow->consumer_site = consumer_site;
    flow->segment_id = segment_id;
    flow->access_kind = xstrdup(access_kind);
    flow->routine = routine != NULL ? xstrdup(routine) : NULL;
    flow->owner_routine = owner_routine != NULL ? xstrdup(owner_routine) : NULL;
    flow->owner_routine_addr = owner_routine_addr;
    flow->has_owner_routine_addr = has_owner_routine_addr;
    if (flow->ptr_symbol == NULL || flow->access_kind == NULL
        || (routine != NULL && flow->routine == NULL)
        || (owner_routine != NULL && flow->owner_routine == NULL)) {
        free_xref_indirect_flow(flow);
        memset(flow, 0, sizeof(*flow));
        return 0;
    }
    (*count)++;
    return 1;
}

static void dedupe_xref_data_edges(xref_data_edge *edges, int *count)
{
    int write_index;
    int i;
    if (*count <= 1) {
        return;
    }
    qsort(edges, (size_t)*count, sizeof(*edges), xref_data_edge_compare);
    write_index = 1;
    for (i = 1; i < *count; i++) {
        if (xref_data_edge_compare(&edges[write_index - 1], &edges[i]) == 0) {
            free_xref_data_edge(&edges[i]);
            memset(&edges[i], 0, sizeof(edges[i]));
            continue;
        }
        if (write_index != i) {
            edges[write_index] = edges[i];
            memset(&edges[i], 0, sizeof(edges[i]));
        }
        write_index++;
    }
    *count = write_index;
}

static void dedupe_xref_indirect_flows(xref_indirect_flow *flows, int *count)
{
    int write_index;
    int i;
    if (*count <= 1) {
        return;
    }
    qsort(flows, (size_t)*count, sizeof(*flows), xref_indirect_flow_compare);
    write_index = 1;
    for (i = 1; i < *count; i++) {
        if (xref_indirect_flow_compare(&flows[write_index - 1], &flows[i]) == 0) {
            free_xref_indirect_flow(&flows[i]);
            memset(&flows[i], 0, sizeof(flows[i]));
            continue;
        }
        if (write_index != i) {
            flows[write_index] = flows[i];
            memset(&flows[i], 0, sizeof(flows[i]));
        }
        write_index++;
    }
    *count = write_index;
}

static int build_xref_data_edges(const xref_build_context *ctx,
                                 xref_data_edge **reads_out,
                                 int *read_count_out,
                                 xref_data_edge **writes_out,
                                 int *write_count_out)
{
    xref_data_edge *reads = NULL;
    xref_data_edge *writes = NULL;
    int read_count = 0;
    int read_capacity = 0;
    int write_count = 0;
    int write_capacity = 0;
    int i;

    for (i = 0; i < ctx->instr_count; i++) {
        const xref_instr *instr = &ctx->instrs[i];
        const char *kind;
        const char *scope;
        xref_owner_info owner;
        if (instr->direct_symbol == NULL || instr->direct_access_kind == 0) {
            continue;
        }
        classify_symbol_name(instr->direct_symbol, &kind, &scope);
        if (!scope_allowed(scope, ctx->include_locals, ctx->include_anon)) {
            continue;
        }
        lookup_xref_routine_owner(ctx, instr->cpu_address, instr->is_dataseg, instr->segment_id, &owner);
        if (instr->direct_access_kind == 1) {
            if (!append_xref_data_edge(&reads,
                                       &read_count,
                                       &read_capacity,
                                       instr->direct_symbol,
                                       instr->cpu_address,
                                       instr->segment_id,
                                       owner.name,
                                       ctx->include_owner ? owner.name : NULL,
                                       owner.addr,
                                       ctx->include_owner && owner.has_addr,
                                       instr->direct_displacement,
                                       addressing_mode_name(instr->mode))) {
                goto fail;
            }
        } else if (instr->direct_access_kind == 2) {
            if (!append_xref_data_edge(&writes,
                                       &write_count,
                                       &write_capacity,
                                       instr->direct_symbol,
                                       instr->cpu_address,
                                       instr->segment_id,
                                       owner.name,
                                       ctx->include_owner ? owner.name : NULL,
                                       owner.addr,
                                       ctx->include_owner && owner.has_addr,
                                       instr->direct_displacement,
                                       addressing_mode_name(instr->mode))) {
                goto fail;
            }
        }
    }

    dedupe_xref_data_edges(reads, &read_count);
    dedupe_xref_data_edges(writes, &write_count);

    *reads_out = reads;
    *read_count_out = read_count;
    *writes_out = writes;
    *write_count_out = write_count;
    return 1;

fail:
    for (i = 0; i < read_count; i++) {
        free_xref_data_edge(&reads[i]);
    }
    free(reads);
    for (i = 0; i < write_count; i++) {
        free_xref_data_edge(&writes[i]);
    }
    free(writes);
    return 0;
}

static int build_xref_indirect_flows(const xref_build_context *ctx,
                                     xref_indirect_flow **flows_out,
                                     int *flow_count_out)
{
    typedef struct tag_xref_pair_state {
        int last_low_write;
        int last_high_write;
        int valid_mask;
    } xref_pair_state;

    xref_indirect_flow *flows = NULL;
    int flow_count = 0;
    int flow_capacity = 0;
    xref_pair_state pair_states[255];
    const char *current_routine = NULL;
    int current_section = -1;
    int current_segment_id = -1;
    int i;
    int j;

    for (j = 0; j < 255; j++) {
        pair_states[j].last_low_write = -1;
        pair_states[j].last_high_write = -1;
        pair_states[j].valid_mask = 0;
    }

    for (i = 0; i < ctx->instr_count; i++) {
        const xref_instr *instr = &ctx->instrs[i];
        xref_owner_info owner;
        lookup_xref_routine_owner(ctx, instr->cpu_address, instr->is_dataseg, instr->segment_id, &owner);
        if (instr->is_dataseg != current_section
            || instr->segment_id != current_segment_id
            || !nullable_string_equal(owner.name, current_routine)) {
            for (j = 0; j < 255; j++) {
                pair_states[j].last_low_write = -1;
                pair_states[j].last_high_write = -1;
                pair_states[j].valid_mask = 0;
            }
            current_section = instr->is_dataseg;
            current_segment_id = instr->segment_id;
            current_routine = owner.name;
        }

        if (instr->zp_write_addr_known) {
            if (instr->zp_write_addr >= 0 && instr->zp_write_addr <= 0xFE) {
                pair_states[instr->zp_write_addr].last_low_write = instr->cpu_address;
                if (pair_states[instr->zp_write_addr].valid_mask == 3) {
                    pair_states[instr->zp_write_addr].valid_mask = 1;
                } else {
                    pair_states[instr->zp_write_addr].valid_mask |= 1;
                }
            }
            if (instr->zp_write_addr >= 1 && instr->zp_write_addr <= 0xFF) {
                pair_states[instr->zp_write_addr - 1].last_high_write = instr->cpu_address;
                if (pair_states[instr->zp_write_addr - 1].valid_mask == 3) {
                    pair_states[instr->zp_write_addr - 1].valid_mask = 2;
                } else {
                    pair_states[instr->zp_write_addr - 1].valid_mask |= 2;
                }
            }
        }

        if (instr->indirect_ptr_addr_known && instr->indirect_ptr_addr >= 0 && instr->indirect_ptr_addr <= 0xFE) {
            const xref_pair_state *pair = &pair_states[instr->indirect_ptr_addr];
            if (pair->valid_mask == 3) {
                const char *ptr_symbol = resolve_pointer_pair_symbol(ctx,
                                                                    instr->indirect_ptr_addr,
                                                                    instr->is_dataseg,
                                                                    instr->segment_id);
                if (ptr_symbol != NULL) {
                    if (!append_xref_indirect_flow(&flows,
                                                   &flow_count,
                                                   &flow_capacity,
                                                   ptr_symbol,
                                                   pair->last_low_write > pair->last_high_write
                                                       ? pair->last_low_write
                                                       : pair->last_high_write,
                                                   instr->cpu_address,
                                                   instr->segment_id,
                                                   instr->indirect_access_kind == 1 ? "read" : "write",
                                                   owner.name,
                                                   ctx->include_owner ? owner.name : NULL,
                                                   owner.addr,
                                                   ctx->include_owner && owner.has_addr)) {
                        goto fail;
                    }
                }
            }
        }
    }

    dedupe_xref_indirect_flows(flows, &flow_count);
    *flows_out = flows;
    *flow_count_out = flow_count;
    return 1;

fail:
    for (i = 0; i < flow_count; i++) {
        free_xref_indirect_flow(&flows[i]);
    }
    free(flows);
    return 0;
}

static void format_timestamp_utc(char *buf, int buf_size)
{
    time_t now;
    struct tm *utc;
    if (buf_size <= 0) {
        return;
    }
    buf[0] = '\0';
    now = time(NULL);
    utc = gmtime(&now);
    if (utc == NULL) {
        return;
    }
    strftime(buf, (size_t)buf_size, "%Y-%m-%dT%H:%M:%SZ", utc);
}

static void csv_write_field(FILE *fp, const char *s)
{
    const char *p;
    int need_quotes = 0;
    if (s == NULL) {
        return;
    }
    for (p = s; *p != '\0'; ++p) {
        if (*p == ',' || *p == '"' || *p == '\n' || *p == '\r') {
            need_quotes = 1;
            break;
        }
    }
    if (!need_quotes) {
        fputs(s, fp);
        return;
    }
    fputc('"', fp);
    for (p = s; *p != '\0'; ++p) {
        if (*p == '"') {
            fputc('"', fp);
        }
        fputc(*p, fp);
    }
    fputc('"', fp);
}

static int emit_xref_json(const char *filename,
                          const xref_build_context *ctx,
                          int include_data,
                          const char *source_file,
                          const char *output_file,
                          int pure_binary)
{
    FILE *fp;
    int i;
    char ts[64];
    xref_data_edge *data_reads = NULL;
    xref_data_edge *data_writes = NULL;
    xref_indirect_flow *indirect_flows = NULL;
    int data_read_count = 0;
    int data_write_count = 0;
    int indirect_flow_count = 0;
    fp = fopen(filename, "w");
    if (fp == NULL) {
        fprintf(stderr, "error: could not open `%s' for writing\n", filename);
        return 0;
    }
    if (include_data) {
        if (!build_xref_data_edges(ctx, &data_reads, &data_read_count, &data_writes, &data_write_count)
            || !build_xref_indirect_flows(ctx, &indirect_flows, &indirect_flow_count)) {
            for (i = 0; i < data_read_count; i++) {
                free_xref_data_edge(&data_reads[i]);
            }
            for (i = 0; i < data_write_count; i++) {
                free_xref_data_edge(&data_writes[i]);
            }
            for (i = 0; i < indirect_flow_count; i++) {
                free_xref_indirect_flow(&indirect_flows[i]);
            }
            fclose(fp);
            free(data_reads);
            free(data_writes);
            free(indirect_flows);
            fprintf(stderr, "error: could not build xref data records\n");
            return 0;
        }
    }
    format_timestamp_utc(ts, sizeof(ts));
    fprintf(fp, "{\n");
    fprintf(fp, "  \"version\": \"1\",\n");
    fprintf(fp, "  \"build\": {\n");
    fprintf(fp, "    \"source_file\": ");
    print_json_string(fp, source_file != NULL ? source_file : "");
    fprintf(fp, ",\n");
    fprintf(fp, "    \"output_file\": ");
    print_json_string(fp, output_file != NULL ? output_file : "");
    fprintf(fp, ",\n");
    fprintf(fp, "    \"pure_binary\": %s,\n", pure_binary ? "true" : "false");
    fprintf(fp, "    \"timestamp_utc\": ");
    print_json_string(fp, ts);
    fprintf(fp, "\n  },\n");

    fprintf(fp, "  \"symbols\": [");
    for (i = 0; i < ctx->symbol_count; ++i) {
        const xref_symbol *s = &ctx->symbols[i];
        fprintf(fp, "%s\n    {", (i == 0) ? "\n" : ",\n");
        fprintf(fp, "\"name\":");
        print_json_string(fp, s->name);
        fprintf(fp, ",\"kind\":");
        print_json_string(fp, s->kind);
        fprintf(fp, ",\"scope\":");
        print_json_string(fp, s->scope);
        fprintf(fp, ",\"defined\":%s", s->defined ? "true" : "false");
        fprintf(fp, ",\"definition\":{");
        fprintf(fp, "\"file\":");
        if (s->definition_loc.file != NULL) {
            print_json_string(fp, s->definition_loc.file);
        } else {
            fprintf(fp, "null");
        }
        fprintf(fp, ",\"line\":%d", s->definition_loc.first_line);
        fprintf(fp, ",\"column\":%d", s->definition_loc.first_column);
        fprintf(fp, ",\"cpu_address\":");
        if (s->has_cpu_address) {
            char hex[16];
            snprintf(hex, sizeof(hex), "0x%04X", s->cpu_address & 0xFFFF);
            print_json_string(fp, hex);
        } else {
            fprintf(fp, "null");
        }
        fprintf(fp, ",\"output_offset\":");
        if (s->has_output_offset) {
            fprintf(fp, "%ld", s->output_offset);
        } else {
            fprintf(fp, "null");
        }
        fprintf(fp, ",\"value\":");
        if (s->has_value) {
            fprintf(fp, "%ld", s->value);
        } else {
            fprintf(fp, "null");
        }
        fprintf(fp, "}}");
    }
    if (ctx->symbol_count > 0) {
        fprintf(fp, "\n  ");
    }
    fprintf(fp, "],\n");

    fprintf(fp, "  \"references\": [");
    for (i = 0; i < ctx->ref_count; ++i) {
        const xref_ref *r = &ctx->refs[i];
        xref_owner_info owner;
        fprintf(fp, "%s\n    {", (i == 0) ? "\n" : ",\n");
        fprintf(fp, "\"symbol\":");
        print_json_string(fp, r->symbol);
        fprintf(fp, ",\"file\":");
        print_json_string(fp, r->use_loc.file != NULL ? r->use_loc.file : "");
        fprintf(fp, ",\"line\":%d,\"column\":%d", r->use_loc.first_line, r->use_loc.first_column);
        fprintf(fp, ",\"use_cpu_address\":");
        if (r->has_cpu_address) {
            char hex[16];
            snprintf(hex, sizeof(hex), "0x%04X", r->cpu_address & 0xFFFF);
            print_json_string(fp, hex);
        } else {
            fprintf(fp, "null");
        }
        fprintf(fp, ",\"use_output_offset\":");
        if (r->has_output_offset) {
            fprintf(fp, "%ld", r->output_offset);
        } else {
            fprintf(fp, "null");
        }
        fprintf(fp, ",\"opcode\":");
        if (r->opcode != NULL && r->opcode[0] != '\0') {
            print_json_string(fp, r->opcode);
        } else {
            fprintf(fp, "null");
        }
        fprintf(fp, ",\"addressing_mode\":");
        if (r->addressing_mode != NULL && r->addressing_mode[0] != '\0') {
            print_json_string(fp, r->addressing_mode);
        } else {
            fprintf(fp, "null");
        }
        fprintf(fp, ",\"access\":");
        print_json_string(fp, r->access != NULL ? r->access : "other");
        fprintf(fp, ",\"expression\":");
        print_json_string(fp, r->expression != NULL ? r->expression : "");
        if (ctx->include_owner && r->has_cpu_address) {
            lookup_xref_routine_owner(ctx, r->cpu_address, r->is_dataseg, r->segment_id, &owner);
            if (owner.name != NULL && owner.has_addr) {
                char owner_addr[16];
                format_xref_owner_addr(owner_addr, sizeof(owner_addr), owner.addr);
                fprintf(fp, ",\"owner_routine\":");
                print_json_string(fp, owner.name);
                fprintf(fp, ",\"owner_routine_addr\":");
                print_json_string(fp, owner_addr);
            }
        }
        fprintf(fp, "}");
    }
    if (ctx->ref_count > 0) {
        fprintf(fp, "\n  ");
    }
    fprintf(fp, "]");
    if (include_data) {
        fprintf(fp, ",\n");
        fprintf(fp, "  \"data_reads\": [");
        for (i = 0; i < data_read_count; ++i) {
            char site_addr[16];
            const xref_data_edge *edge = &data_reads[i];
            format_xref_addr(site_addr, sizeof(site_addr), edge->site_addr);
            fprintf(fp, "%s\n    {", (i == 0) ? "\n" : ",\n");
            fprintf(fp, "\"symbol\":");
            print_json_string(fp, edge->symbol);
            fprintf(fp, ",\"site_addr\":");
            print_json_string(fp, site_addr);
            if (edge->routine != NULL) {
                fprintf(fp, ",\"routine\":");
                print_json_string(fp, edge->routine);
            }
            if (edge->owner_routine != NULL && edge->has_owner_routine_addr) {
                char owner_addr[16];
                format_xref_owner_addr(owner_addr, sizeof(owner_addr), edge->owner_routine_addr);
                fprintf(fp, ",\"owner_routine\":");
                print_json_string(fp, edge->owner_routine);
                fprintf(fp, ",\"owner_routine_addr\":");
                print_json_string(fp, owner_addr);
            }
            fprintf(fp, ",\"displacement\":%d", edge->displacement);
            fprintf(fp, ",\"addressing_mode\":");
            print_json_string(fp, edge->addressing_mode);
            fprintf(fp, "}");
        }
        if (data_read_count > 0) {
            fprintf(fp, "\n  ");
        }
        fprintf(fp, "],\n");

        fprintf(fp, "  \"data_writes\": [");
        for (i = 0; i < data_write_count; ++i) {
            char site_addr[16];
            const xref_data_edge *edge = &data_writes[i];
            format_xref_addr(site_addr, sizeof(site_addr), edge->site_addr);
            fprintf(fp, "%s\n    {", (i == 0) ? "\n" : ",\n");
            fprintf(fp, "\"symbol\":");
            print_json_string(fp, edge->symbol);
            fprintf(fp, ",\"site_addr\":");
            print_json_string(fp, site_addr);
            if (edge->routine != NULL) {
                fprintf(fp, ",\"routine\":");
                print_json_string(fp, edge->routine);
            }
            if (edge->owner_routine != NULL && edge->has_owner_routine_addr) {
                char owner_addr[16];
                format_xref_owner_addr(owner_addr, sizeof(owner_addr), edge->owner_routine_addr);
                fprintf(fp, ",\"owner_routine\":");
                print_json_string(fp, edge->owner_routine);
                fprintf(fp, ",\"owner_routine_addr\":");
                print_json_string(fp, owner_addr);
            }
            fprintf(fp, ",\"displacement\":%d", edge->displacement);
            fprintf(fp, ",\"addressing_mode\":");
            print_json_string(fp, edge->addressing_mode);
            fprintf(fp, "}");
        }
        if (data_write_count > 0) {
            fprintf(fp, "\n  ");
        }
        fprintf(fp, "],\n");

        fprintf(fp, "  \"indirect_data_flows\": [");
        for (i = 0; i < indirect_flow_count; ++i) {
            char producer_site[16];
            char consumer_site[16];
            const xref_indirect_flow *flow = &indirect_flows[i];
            format_xref_addr(producer_site, sizeof(producer_site), flow->producer_site);
            format_xref_addr(consumer_site, sizeof(consumer_site), flow->consumer_site);
            fprintf(fp, "%s\n    {", (i == 0) ? "\n" : ",\n");
            fprintf(fp, "\"ptr_symbol\":");
            print_json_string(fp, flow->ptr_symbol);
            fprintf(fp, ",\"producer_site\":");
            print_json_string(fp, producer_site);
            fprintf(fp, ",\"consumer_site\":");
            print_json_string(fp, consumer_site);
            fprintf(fp, ",\"access_kind\":");
            print_json_string(fp, flow->access_kind);
            if (flow->routine != NULL) {
                fprintf(fp, ",\"routine\":");
                print_json_string(fp, flow->routine);
            }
            if (flow->owner_routine != NULL && flow->has_owner_routine_addr) {
                char owner_addr[16];
                format_xref_owner_addr(owner_addr, sizeof(owner_addr), flow->owner_routine_addr);
                fprintf(fp, ",\"owner_routine\":");
                print_json_string(fp, flow->owner_routine);
                fprintf(fp, ",\"owner_routine_addr\":");
                print_json_string(fp, owner_addr);
            }
            fprintf(fp, "}");
        }
        if (indirect_flow_count > 0) {
            fprintf(fp, "\n  ");
        }
        fprintf(fp, "]\n");
    } else {
        fprintf(fp, "\n");
    }
    fprintf(fp, "}\n");
    fclose(fp);
    if (include_data) {
        for (i = 0; i < data_read_count; i++) {
            free_xref_data_edge(&data_reads[i]);
        }
        free(data_reads);
        for (i = 0; i < data_write_count; i++) {
            free_xref_data_edge(&data_writes[i]);
        }
        free(data_writes);
        for (i = 0; i < indirect_flow_count; i++) {
            free_xref_indirect_flow(&indirect_flows[i]);
        }
        free(indirect_flows);
    }
    return 1;
}

static int emit_xref_text(const char *filename, const xref_build_context *ctx)
{
    FILE *fp;
    int i;
    fp = fopen(filename, "w");
    if (fp == NULL) {
        fprintf(stderr, "error: could not open `%s' for writing\n", filename);
        return 0;
    }
    fprintf(fp, "SYMBOLS\n");
    for (i = 0; i < ctx->symbol_count; ++i) {
        const xref_symbol *s = &ctx->symbols[i];
        fprintf(fp, "%s,%s,%s,defined=%d", s->name, s->kind, s->scope, s->defined);
        if (s->has_cpu_address) {
            fprintf(fp, ",cpu=$%04X", s->cpu_address & 0xFFFF);
        }
        if (s->has_output_offset) {
            fprintf(fp, ",off=%ld", s->output_offset);
        }
        if (s->has_value) {
            fprintf(fp, ",value=%ld", s->value);
        }
        fprintf(fp, "\n");
    }
    fprintf(fp, "\nREFERENCES\n");
    for (i = 0; i < ctx->ref_count; ++i) {
        const xref_ref *r = &ctx->refs[i];
        fprintf(fp, "%s,%s:%d:%d", r->symbol, r->use_loc.file, r->use_loc.first_line, r->use_loc.first_column);
        if (r->has_cpu_address) {
            fprintf(fp, ",cpu=$%04X", r->cpu_address & 0xFFFF);
        }
        if (r->has_output_offset) {
            fprintf(fp, ",off=%ld", r->output_offset);
        }
        if (r->opcode != NULL && r->opcode[0] != '\0') {
            fprintf(fp, ",opcode=%s", r->opcode);
        }
        if (r->addressing_mode != NULL && r->addressing_mode[0] != '\0') {
            fprintf(fp, ",mode=%s", r->addressing_mode);
        }
        fprintf(fp, ",access=%s,expr=%s\n", r->access, r->expression);
    }
    fclose(fp);
    return 1;
}

static int emit_xref_csv(const char *filename, const xref_build_context *ctx)
{
    FILE *sym_fp;
    FILE *ref_fp;
    char sym_name[1024];
    char ref_name[1024];
    int i;
    snprintf(sym_name, sizeof(sym_name), "%s.symbols.csv", filename);
    snprintf(ref_name, sizeof(ref_name), "%s.refs.csv", filename);
    sym_fp = fopen(sym_name, "w");
    if (sym_fp == NULL) {
        fprintf(stderr, "error: could not open `%s' for writing\n", sym_name);
        return 0;
    }
    ref_fp = fopen(ref_name, "w");
    if (ref_fp == NULL) {
        fclose(sym_fp);
        fprintf(stderr, "error: could not open `%s' for writing\n", ref_name);
        return 0;
    }

    fprintf(sym_fp, "name,kind,scope,defined,file,line,column,cpu_address,output_offset,value\n");
    for (i = 0; i < ctx->symbol_count; ++i) {
        const xref_symbol *s = &ctx->symbols[i];
        char cpu[16];
        char off[32];
        char val[32];
        cpu[0] = '\0';
        off[0] = '\0';
        val[0] = '\0';
        if (s->has_cpu_address) {
            snprintf(cpu, sizeof(cpu), "0x%04X", s->cpu_address & 0xFFFF);
        }
        if (s->has_output_offset) {
            snprintf(off, sizeof(off), "%ld", s->output_offset);
        }
        if (s->has_value) {
            snprintf(val, sizeof(val), "%ld", s->value);
        }
        csv_write_field(sym_fp, s->name); fprintf(sym_fp, ",");
        csv_write_field(sym_fp, s->kind); fprintf(sym_fp, ",");
        csv_write_field(sym_fp, s->scope); fprintf(sym_fp, ",");
        fprintf(sym_fp, "%d,", s->defined);
        csv_write_field(sym_fp, s->definition_loc.file != NULL ? s->definition_loc.file : ""); fprintf(sym_fp, ",");
        fprintf(sym_fp, "%d,%d,", s->definition_loc.first_line, s->definition_loc.first_column);
        csv_write_field(sym_fp, cpu); fprintf(sym_fp, ",");
        csv_write_field(sym_fp, off); fprintf(sym_fp, ",");
        csv_write_field(sym_fp, val); fprintf(sym_fp, "\n");
    }

    fprintf(ref_fp, "symbol,file,line,column,use_cpu_address,use_output_offset,opcode,addressing_mode,access,expression\n");
    for (i = 0; i < ctx->ref_count; ++i) {
        const xref_ref *r = &ctx->refs[i];
        char cpu[16];
        char off[32];
        cpu[0] = '\0';
        off[0] = '\0';
        if (r->has_cpu_address) {
            snprintf(cpu, sizeof(cpu), "0x%04X", r->cpu_address & 0xFFFF);
        }
        if (r->has_output_offset) {
            snprintf(off, sizeof(off), "%ld", r->output_offset);
        }
        csv_write_field(ref_fp, r->symbol); fprintf(ref_fp, ",");
        csv_write_field(ref_fp, r->use_loc.file != NULL ? r->use_loc.file : ""); fprintf(ref_fp, ",");
        fprintf(ref_fp, "%d,%d,", r->use_loc.first_line, r->use_loc.first_column);
        csv_write_field(ref_fp, cpu); fprintf(ref_fp, ",");
        csv_write_field(ref_fp, off); fprintf(ref_fp, ",");
        csv_write_field(ref_fp, r->opcode != NULL ? r->opcode : ""); fprintf(ref_fp, ",");
        csv_write_field(ref_fp, r->addressing_mode != NULL ? r->addressing_mode : ""); fprintf(ref_fp, ",");
        csv_write_field(ref_fp, r->access != NULL ? r->access : "other"); fprintf(ref_fp, ",");
        csv_write_field(ref_fp, r->expression != NULL ? r->expression : ""); fprintf(ref_fp, "\n");
    }

    fclose(sym_fp);
    fclose(ref_fp);
    return 1;
}

/* ---- xref-summary implementation ---- */

#include <regex.h>

typedef struct tag_xref_summary_referrer {
    char *routine;
    int count;
} xref_summary_referrer;

typedef struct tag_xref_summary_entry {
    char *label;
    int cpu_address;
    int jsr_count;
    int jmp_count;
    int branch_count;
    int read_count;
    int write_count;
    int total_ref_count;
    xref_summary_referrer *referrers;
    int referrer_count;
    char *first_run_terminator;
    int next_symbol_distance_bytes; /* -1 = omit */
    int has_refs_from_other_routines;
    char **nearby_symbols;
    int nearby_symbol_count;
} xref_summary_entry;

typedef struct tag_xref_summary_instr {
    int cpu_address;
    int mnemonic;
    int length;
} xref_summary_instr;

static xref_summary_instr *summary_instrs = NULL;
static int summary_instr_count = 0;
static int summary_instr_capacity = 0;

static int is_code_symbol(int addr);

static int record_summary_instr(int cpu_address, int mnemonic, int length)
{
    if (summary_instr_count >= summary_instr_capacity) {
        int new_cap = (summary_instr_capacity == 0) ? 256 : (summary_instr_capacity * 2);
        xref_summary_instr *tmp = (xref_summary_instr *)realloc(
            summary_instrs, (size_t)new_cap * sizeof(xref_summary_instr));
        if (tmp == NULL) return 0;
        summary_instrs = tmp;
        summary_instr_capacity = new_cap;
    }
    summary_instrs[summary_instr_count].cpu_address = cpu_address;
    summary_instrs[summary_instr_count].mnemonic = mnemonic;
    summary_instrs[summary_instr_count].length = length;
    summary_instr_count++;
    return 1;
}

static int summary_visit_instruction(astnode *instr, void *arg, astnode **next)
{
    xref_build_context *ctx = (xref_build_context *)arg;
    int addr = get_current_pc();
    int len = opcode_length(instr->instr.opcode);
    xref_meta meta;
    char expr_buf[512];
    (void)next;

    if (len < 1) len = 1;

    record_summary_instr(addr, instr->instr.mnemonic.value, len);

    memset(&meta, 0, sizeof(meta));
    meta.has_cpu_address = 1;
    meta.cpu_address = addr;
    meta.has_output_offset = ctx->pure_binary && !in_dataseg;
    meta.output_offset = ctx->output_offset;
    meta.opcode = opcode_to_string(instr->instr.opcode);
    meta.addressing_mode = addressing_mode_name(instr->instr.mode);
    meta.access = classify_instruction_access(meta.opcode, LHS(instr), instr->instr.mode);
    extract_operand_from_line(instr->loc, expr_buf, sizeof(expr_buf));
    meta.expression = expr_buf;

    if (LHS(instr) != NULL) {
        if (!collect_expr_refs_recursive(LHS(instr), ctx, &meta)) {
            return 0;
        }
    }

    add_current_pc(len);
    if (ctx->pure_binary && !in_dataseg) {
        ctx->output_offset += len;
    }
    return 0;
}

static void free_summary_entry(xref_summary_entry *e)
{
    int i;
    free(e->label);
    free(e->first_run_terminator);
    for (i = 0; i < e->referrer_count; i++) {
        free(e->referrers[i].routine);
    }
    free(e->referrers);
    for (i = 0; i < e->nearby_symbol_count; i++) {
        free(e->nearby_symbols[i]);
    }
    free(e->nearby_symbols);
    memset(e, 0, sizeof(*e));
}

static int ensure_summary_entry_capacity(xref_summary_entry **entries, int *capacity, int count)
{
    xref_summary_entry *tmp;
    int new_capacity;
    if (count < *capacity) {
        return 1;
    }
    new_capacity = (*capacity == 0) ? 16 : (*capacity * 2);
    tmp = (xref_summary_entry *)realloc(*entries, (size_t)new_capacity * sizeof(xref_summary_entry));
    if (tmp == NULL) {
        return 0;
    }
    *entries = tmp;
    *capacity = new_capacity;
    return 1;
}

static int init_summary_entry(xref_summary_entry *entry,
                              const char *sym_name,
                              int sym_addr,
                              int jsr_c,
                              int jmp_c,
                              int branch_c,
                              int read_c,
                              int write_c,
                              int total_c,
                              const xref_summary_referrer *tmp_refs,
                              int tmp_ref_count,
                              int top_referrers_limit,
                              const char *terminator,
                              int next_sym_dist,
                              int has_external,
                              char **nearby_symbols,
                              int nearby_symbol_count)
{
    int i;
    memset(entry, 0, sizeof(*entry));
    entry->label = xstrdup(sym_name);
    entry->cpu_address = sym_addr;
    entry->jsr_count = jsr_c;
    entry->jmp_count = jmp_c;
    entry->branch_count = branch_c;
    entry->read_count = read_c;
    entry->write_count = write_c;
    entry->total_ref_count = total_c;
    entry->referrer_count = (tmp_ref_count < top_referrers_limit) ? tmp_ref_count : top_referrers_limit;
    entry->first_run_terminator = xstrdup(terminator);
    entry->next_symbol_distance_bytes = next_sym_dist;
    entry->has_refs_from_other_routines = has_external;
    entry->nearby_symbol_count = nearby_symbol_count;

    if (entry->label == NULL || entry->first_run_terminator == NULL) {
        free_summary_entry(entry);
        return 0;
    }

    if (entry->referrer_count > 0) {
        entry->referrers = (xref_summary_referrer *)calloc((size_t)entry->referrer_count, sizeof(xref_summary_referrer));
        if (entry->referrers == NULL) {
            free_summary_entry(entry);
            return 0;
        }
        for (i = 0; i < entry->referrer_count; i++) {
            entry->referrers[i].routine = xstrdup(tmp_refs[i].routine);
            entry->referrers[i].count = tmp_refs[i].count;
            if (entry->referrers[i].routine == NULL) {
                free_summary_entry(entry);
                return 0;
            }
        }
    }

    if (nearby_symbol_count > 0) {
        entry->nearby_symbols = (char **)calloc((size_t)nearby_symbol_count, sizeof(char *));
        if (entry->nearby_symbols == NULL) {
            free_summary_entry(entry);
            return 0;
        }
        for (i = 0; i < nearby_symbol_count; i++) {
            entry->nearby_symbols[i] = xstrdup(nearby_symbols[i]);
            if (entry->nearby_symbols[i] == NULL) {
                free_summary_entry(entry);
                return 0;
            }
        }
    }

    return 1;
}

static void truncate_summary_entries(xref_summary_entry *entries, int *count, int limit)
{
    int i;
    if (*count <= limit) {
        return;
    }
    for (i = limit; i < *count; i++) {
        free_summary_entry(&entries[i]);
    }
    *count = limit;
}

static int summary_instr_compare(const void *a, const void *b)
{
    const xref_summary_instr *ia = (const xref_summary_instr *)a;
    const xref_summary_instr *ib = (const xref_summary_instr *)b;
    if (ia->cpu_address != ib->cpu_address) {
        return ia->cpu_address - ib->cpu_address;
    }
    return ia->length - ib->length;
}

static const char *determine_first_run_terminator(int sym_addr, int next_sym_addr)
{
    int i;
    /* Find the first instruction at or after sym_addr */
    for (i = 0; i < summary_instr_count; i++) {
        if (summary_instrs[i].cpu_address >= sym_addr) {
            break;
        }
    }
    /* Scan forward looking for a terminator */
    for (; i < summary_instr_count; i++) {
        int addr = summary_instrs[i].cpu_address;
        int mnem = summary_instrs[i].mnemonic;
        int len = summary_instrs[i].length;
        if (next_sym_addr >= 0 && addr >= next_sym_addr) {
            break; /* reached next symbol without terminator */
        }
        if (mnem == RTS_MNEMONIC) return "rts";
        if (mnem == RTI_MNEMONIC) return "rti";
        if (mnem == JMP_MNEMONIC) return "jmp";
        /* Check if next instruction would be at or past next symbol */
        if (next_sym_addr >= 0 && (addr + len) >= next_sym_addr) {
            return "fallthrough";
        }
    }
    if (next_sym_addr >= 0) return "fallthrough";
    return "unknown";
}

/* Find routine owner: nearest preceding non-local code label */
static const char *find_routine_owner(const xref_build_context *ctx,
                                      int site_addr)
{
    int i;
    const char *best = NULL;
    int best_addr = -1;
    for (i = 0; i < ctx->symbol_count; i++) {
        const xref_symbol *s = &ctx->symbols[i];
        if (!s->has_cpu_address) continue;
        if (s->cpu_address > site_addr) continue;
        if (strcmp(s->scope, "global") != 0) continue;
        if (strcmp(s->kind, "label") != 0) continue;
        if (!is_code_symbol(s->cpu_address)) continue;
        if (s->cpu_address > best_addr) {
            best_addr = s->cpu_address;
            best = s->name;
        }
    }
    return best;
}

/* Check if symbol address has an instruction (code label) */
static int is_code_symbol(int addr)
{
    int lo = 0, hi = summary_instr_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (summary_instrs[mid].cpu_address == addr) return 1;
        if (summary_instrs[mid].cpu_address < addr) lo = mid + 1;
        else hi = mid - 1;
    }
    return 0;
}

static int callable_compare(const void *a, const void *b)
{
    const xref_summary_entry *la = (const xref_summary_entry *)a;
    const xref_summary_entry *lb = (const xref_summary_entry *)b;
    if (lb->jsr_count != la->jsr_count) return lb->jsr_count - la->jsr_count;
    if (lb->total_ref_count != la->total_ref_count) return lb->total_ref_count - la->total_ref_count;
    return la->cpu_address - lb->cpu_address;
}

static int jump_target_compare(const void *a, const void *b)
{
    const xref_summary_entry *la = (const xref_summary_entry *)a;
    const xref_summary_entry *lb = (const xref_summary_entry *)b;
    int la_sum = la->jmp_count + la->branch_count;
    int lb_sum = lb->jmp_count + lb->branch_count;
    if (lb_sum != la_sum) return lb_sum - la_sum;
    return la->cpu_address - lb->cpu_address;
}

static int data_label_compare(const void *a, const void *b)
{
    const xref_summary_entry *la = (const xref_summary_entry *)a;
    const xref_summary_entry *lb = (const xref_summary_entry *)b;
    int la_sum = la->read_count + la->write_count;
    int lb_sum = lb->read_count + lb->write_count;
    if (lb_sum != la_sum) return lb_sum - la_sum;
    return la->cpu_address - lb->cpu_address;
}

static int referrer_sort_compare(const void *a, const void *b)
{
    const xref_summary_referrer *ra = (const xref_summary_referrer *)a;
    const xref_summary_referrer *rb = (const xref_summary_referrer *)b;
    if (rb->count != ra->count) return rb->count - ra->count;
    return strcmp(ra->routine, rb->routine);
}

/* Build sorted list of non-local symbols with CPU addresses */
typedef struct tag_sorted_sym {
    const char *name;
    int cpu_address;
    int is_code;
    int is_dataseg;
} sorted_sym;

static int sorted_sym_compare(const void *a, const void *b)
{
    const sorted_sym *sa = (const sorted_sym *)a;
    const sorted_sym *sb = (const sorted_sym *)b;
    if (sa->cpu_address != sb->cpu_address) return sa->cpu_address - sb->cpu_address;
    return strcmp(sa->name, sb->name);
}

static void emit_xref_summary_json(FILE *fp,
                                    const xref_summary_entry *callables, int callable_count,
                                    const xref_summary_entry *jump_targets, int jump_target_count,
                                    const xref_summary_entry *data_labels, int data_label_count,
                                    const char *kind_filter)
{
    int i, j;
    int first_section = 1;
    fprintf(fp, "{\n");

    if (strcmp(kind_filter, "all") == 0 || strcmp(kind_filter, "callable") == 0) {
        if (!first_section) fprintf(fp, ",\n");
        first_section = 0;
        fprintf(fp, "  \"top_callables\": [");
        for (i = 0; i < callable_count; i++) {
            const xref_summary_entry *e = &callables[i];
            fprintf(fp, "%s\n    {", i > 0 ? "," : "");
            fprintf(fp, "\"label\":");
            print_json_string(fp, e->label);
            fprintf(fp, ",\"addr\":\"0x%04X\"", e->cpu_address & 0xFFFF);
            fprintf(fp, ",\"jsr_count\":%d", e->jsr_count);
            fprintf(fp, ",\"jmp_count\":%d", e->jmp_count);
            fprintf(fp, ",\"total_ref_count\":%d", e->total_ref_count);
            fprintf(fp, ",\"top_referring_routines\":[");
            for (j = 0; j < e->referrer_count; j++) {
                fprintf(fp, "%s{\"routine\":", j > 0 ? "," : "");
                print_json_string(fp, e->referrers[j].routine);
                fprintf(fp, ",\"count\":%d}", e->referrers[j].count);
            }
            fprintf(fp, "]");
            fprintf(fp, ",\"first_run_terminator\":");
            print_json_string(fp, e->first_run_terminator);
            if (e->next_symbol_distance_bytes >= 0) {
                fprintf(fp, ",\"next_symbol_distance_bytes\":%d", e->next_symbol_distance_bytes);
            }
            fprintf(fp, ",\"has_refs_from_other_routines\":%s",
                    e->has_refs_from_other_routines ? "true" : "false");
            if (e->nearby_symbol_count > 0) {
                fprintf(fp, ",\"nearby_symbols\":[");
                for (j = 0; j < e->nearby_symbol_count; j++) {
                    fprintf(fp, "%s", j > 0 ? "," : "");
                    print_json_string(fp, e->nearby_symbols[j]);
                }
                fprintf(fp, "]");
            }
            fprintf(fp, "}");
        }
        if (callable_count > 0) fprintf(fp, "\n  ");
        fprintf(fp, "]");
    }

    if (strcmp(kind_filter, "all") == 0 || strcmp(kind_filter, "jump_target") == 0) {
        if (!first_section) fprintf(fp, ",\n");
        first_section = 0;
        fprintf(fp, "  \"top_jump_targets\": [");
        for (i = 0; i < jump_target_count; i++) {
            const xref_summary_entry *e = &jump_targets[i];
            fprintf(fp, "%s\n    {", i > 0 ? "," : "");
            fprintf(fp, "\"label\":");
            print_json_string(fp, e->label);
            fprintf(fp, ",\"addr\":\"0x%04X\"", e->cpu_address & 0xFFFF);
            fprintf(fp, ",\"jmp_count\":%d", e->jmp_count);
            fprintf(fp, ",\"branch_count\":%d", e->branch_count);
            fprintf(fp, ",\"total_ref_count\":%d", e->total_ref_count);
            fprintf(fp, ",\"top_referring_routines\":[");
            for (j = 0; j < e->referrer_count; j++) {
                fprintf(fp, "%s{\"routine\":", j > 0 ? "," : "");
                print_json_string(fp, e->referrers[j].routine);
                fprintf(fp, ",\"count\":%d}", e->referrers[j].count);
            }
            fprintf(fp, "]");
            fprintf(fp, ",\"first_run_terminator\":");
            print_json_string(fp, e->first_run_terminator);
            if (e->next_symbol_distance_bytes >= 0) {
                fprintf(fp, ",\"next_symbol_distance_bytes\":%d", e->next_symbol_distance_bytes);
            }
            fprintf(fp, ",\"has_refs_from_other_routines\":%s",
                    e->has_refs_from_other_routines ? "true" : "false");
            if (e->nearby_symbol_count > 0) {
                fprintf(fp, ",\"nearby_symbols\":[");
                for (j = 0; j < e->nearby_symbol_count; j++) {
                    fprintf(fp, "%s", j > 0 ? "," : "");
                    print_json_string(fp, e->nearby_symbols[j]);
                }
                fprintf(fp, "]");
            }
            fprintf(fp, "}");
        }
        if (jump_target_count > 0) fprintf(fp, "\n  ");
        fprintf(fp, "]");
    }

    if (strcmp(kind_filter, "all") == 0 || strcmp(kind_filter, "data") == 0) {
        if (!first_section) fprintf(fp, ",\n");
        first_section = 0;
        fprintf(fp, "  \"top_data_labels\": [");
        for (i = 0; i < data_label_count; i++) {
            const xref_summary_entry *e = &data_labels[i];
            fprintf(fp, "%s\n    {", i > 0 ? "," : "");
            fprintf(fp, "\"label\":");
            print_json_string(fp, e->label);
            fprintf(fp, ",\"addr\":\"0x%04X\"", e->cpu_address & 0xFFFF);
            fprintf(fp, ",\"read_count\":%d", e->read_count);
            fprintf(fp, ",\"write_count\":%d", e->write_count);
            fprintf(fp, ",\"total_ref_count\":%d", e->total_ref_count);
            fprintf(fp, ",\"top_referring_routines\":[");
            for (j = 0; j < e->referrer_count; j++) {
                fprintf(fp, "%s{\"routine\":", j > 0 ? "," : "");
                print_json_string(fp, e->referrers[j].routine);
                fprintf(fp, ",\"count\":%d}", e->referrers[j].count);
            }
            fprintf(fp, "]");
            if (e->next_symbol_distance_bytes >= 0) {
                fprintf(fp, ",\"next_symbol_distance_bytes\":%d", e->next_symbol_distance_bytes);
            }
            fprintf(fp, ",\"has_refs_from_other_routines\":%s",
                    e->has_refs_from_other_routines ? "true" : "false");
            if (e->nearby_symbol_count > 0) {
                fprintf(fp, ",\"nearby_symbols\":[");
                for (j = 0; j < e->nearby_symbol_count; j++) {
                    fprintf(fp, "%s", j > 0 ? "," : "");
                    print_json_string(fp, e->nearby_symbols[j]);
                }
                fprintf(fp, "]");
            }
            fprintf(fp, "}");
        }
        if (data_label_count > 0) fprintf(fp, "\n  ");
        fprintf(fp, "]");
    }

    fprintf(fp, "\n}\n");
}

static void emit_xref_summary_entry_ndjson(FILE *fp, const char *section, const xref_summary_entry *e, int is_code)
{
    int j;
    fprintf(fp, "{\"section\":");
    print_json_string(fp, section);
    fprintf(fp, ",\"label\":");
    print_json_string(fp, e->label);
    fprintf(fp, ",\"addr\":\"0x%04X\"", e->cpu_address & 0xFFFF);
    if (is_code) {
        fprintf(fp, ",\"jsr_count\":%d,\"jmp_count\":%d", e->jsr_count, e->jmp_count);
        if (strcmp(section, "top_jump_targets") == 0) {
            fprintf(fp, ",\"branch_count\":%d", e->branch_count);
        }
    } else {
        fprintf(fp, ",\"read_count\":%d,\"write_count\":%d", e->read_count, e->write_count);
    }
    fprintf(fp, ",\"total_ref_count\":%d", e->total_ref_count);
    fprintf(fp, ",\"top_referring_routines\":[");
    for (j = 0; j < e->referrer_count; j++) {
        fprintf(fp, "%s{\"routine\":", j > 0 ? "," : "");
        print_json_string(fp, e->referrers[j].routine);
        fprintf(fp, ",\"count\":%d}", e->referrers[j].count);
    }
    fprintf(fp, "]");
    if (is_code) {
        fprintf(fp, ",\"first_run_terminator\":");
        print_json_string(fp, e->first_run_terminator);
    }
    if (e->next_symbol_distance_bytes >= 0) {
        fprintf(fp, ",\"next_symbol_distance_bytes\":%d", e->next_symbol_distance_bytes);
    }
    fprintf(fp, ",\"has_refs_from_other_routines\":%s",
            e->has_refs_from_other_routines ? "true" : "false");
    if (e->nearby_symbol_count > 0) {
        fprintf(fp, ",\"nearby_symbols\":[");
        for (j = 0; j < e->nearby_symbol_count; j++) {
            fprintf(fp, "%s", j > 0 ? "," : "");
            print_json_string(fp, e->nearby_symbols[j]);
        }
        fprintf(fp, "]");
    }
    fprintf(fp, "}\n");
}

static void emit_xref_summary_ndjson(FILE *fp,
                                      const xref_summary_entry *callables, int callable_count,
                                      const xref_summary_entry *jump_targets, int jump_target_count,
                                      const xref_summary_entry *data_labels, int data_label_count,
                                      const char *kind_filter)
{
    int i;
    if (strcmp(kind_filter, "all") == 0 || strcmp(kind_filter, "callable") == 0) {
        for (i = 0; i < callable_count; i++)
            emit_xref_summary_entry_ndjson(fp, "top_callables", &callables[i], 1);
    }
    if (strcmp(kind_filter, "all") == 0 || strcmp(kind_filter, "jump_target") == 0) {
        for (i = 0; i < jump_target_count; i++)
            emit_xref_summary_entry_ndjson(fp, "top_jump_targets", &jump_targets[i], 1);
    }
    if (strcmp(kind_filter, "all") == 0 || strcmp(kind_filter, "data") == 0) {
        for (i = 0; i < data_label_count; i++)
            emit_xref_summary_entry_ndjson(fp, "top_data_labels", &data_labels[i], 0);
    }
}

static void emit_xref_summary_text(FILE *fp,
                                    const xref_summary_entry *callables, int callable_count,
                                    const xref_summary_entry *jump_targets, int jump_target_count,
                                    const xref_summary_entry *data_labels, int data_label_count,
                                    const char *kind_filter)
{
    int i, j;
    if (strcmp(kind_filter, "all") == 0 || strcmp(kind_filter, "callable") == 0) {
        fprintf(fp, "top_callables\n");
        for (i = 0; i < callable_count; i++) {
            const xref_summary_entry *e = &callables[i];
            fprintf(fp, "  %s @ 0x%04X jsr=%d jmp=%d total=%d term=%s",
                    e->label, e->cpu_address & 0xFFFF,
                    e->jsr_count, e->jmp_count, e->total_ref_count,
                    e->first_run_terminator);
            if (e->next_symbol_distance_bytes >= 0) fprintf(fp, " size=%d", e->next_symbol_distance_bytes);
            fprintf(fp, " external=%s\n", e->has_refs_from_other_routines ? "true" : "false");
            if (e->referrer_count > 0) {
                fprintf(fp, "    referrers:");
                for (j = 0; j < e->referrer_count; j++)
                    fprintf(fp, "%s %s(%d)", j > 0 ? "," : "", e->referrers[j].routine, e->referrers[j].count);
                fprintf(fp, "\n");
            }
            if (e->nearby_symbol_count > 0) {
                fprintf(fp, "    nearby:");
                for (j = 0; j < e->nearby_symbol_count; j++)
                    fprintf(fp, "%s %s", j > 0 ? "," : "", e->nearby_symbols[j]);
                fprintf(fp, "\n");
            }
        }
        fprintf(fp, "\n");
    }

    if (strcmp(kind_filter, "all") == 0 || strcmp(kind_filter, "jump_target") == 0) {
        fprintf(fp, "top_jump_targets\n");
        for (i = 0; i < jump_target_count; i++) {
            const xref_summary_entry *e = &jump_targets[i];
            fprintf(fp, "  %s @ 0x%04X jmp=%d branch=%d total=%d term=%s",
                    e->label, e->cpu_address & 0xFFFF,
                    e->jmp_count, e->branch_count, e->total_ref_count,
                    e->first_run_terminator);
            if (e->next_symbol_distance_bytes >= 0) fprintf(fp, " size=%d", e->next_symbol_distance_bytes);
            fprintf(fp, " external=%s\n", e->has_refs_from_other_routines ? "true" : "false");
            if (e->referrer_count > 0) {
                fprintf(fp, "    referrers:");
                for (j = 0; j < e->referrer_count; j++)
                    fprintf(fp, "%s %s(%d)", j > 0 ? "," : "", e->referrers[j].routine, e->referrers[j].count);
                fprintf(fp, "\n");
            }
            if (e->nearby_symbol_count > 0) {
                fprintf(fp, "    nearby:");
                for (j = 0; j < e->nearby_symbol_count; j++)
                    fprintf(fp, "%s %s", j > 0 ? "," : "", e->nearby_symbols[j]);
                fprintf(fp, "\n");
            }
        }
        fprintf(fp, "\n");
    }

    if (strcmp(kind_filter, "all") == 0 || strcmp(kind_filter, "data") == 0) {
        fprintf(fp, "top_data_labels\n");
        for (i = 0; i < data_label_count; i++) {
            const xref_summary_entry *e = &data_labels[i];
            fprintf(fp, "  %s @ 0x%04X read=%d write=%d total=%d",
                    e->label, e->cpu_address & 0xFFFF,
                    e->read_count, e->write_count, e->total_ref_count);
            if (e->next_symbol_distance_bytes >= 0) fprintf(fp, " size=%d", e->next_symbol_distance_bytes);
            fprintf(fp, " external=%s\n", e->has_refs_from_other_routines ? "true" : "false");
            if (e->referrer_count > 0) {
                fprintf(fp, "    referrers:");
                for (j = 0; j < e->referrer_count; j++)
                    fprintf(fp, "%s %s(%d)", j > 0 ? "," : "", e->referrers[j].routine, e->referrers[j].count);
                fprintf(fp, "\n");
            }
            if (e->nearby_symbol_count > 0) {
                fprintf(fp, "    nearby:");
                for (j = 0; j < e->nearby_symbol_count; j++)
                    fprintf(fp, "%s %s", j > 0 ? "," : "", e->nearby_symbols[j]);
                fprintf(fp, "\n");
            }
        }
        fprintf(fp, "\n");
    }
}

int generate_xref_summary(astnode *root,
                          const char *output_path,
                          xref_summary_format format,
                          int include_locals,
                          int include_anon,
                          const char *kind_filter,
                          int limit,
                          int top_referrers_limit,
                          int nearby_window,
                          const char *include_regex,
                          const char *exclude_regex,
                          const char *source_file,
                          const char *output_file,
                          int pure_binary)
{
    static astnodeprocmap map[] = {
        { DATASEG_NODE, xref_visit_dataseg },
        { CODESEG_NODE, xref_visit_codeseg },
        { ORG_NODE, xref_visit_org },
        { LABEL_NODE, xref_visit_label },
        { INSTRUCTION_NODE, summary_visit_instruction },
        { DATA_NODE, xref_visit_data },
        { STORAGE_NODE, xref_visit_storage },
        { BINARY_NODE, xref_visit_binary },
        { STRUC_DECL_NODE, list_noop },
        { UNION_DECL_NODE, list_noop },
        { ENUM_DECL_NODE, list_noop },
        { RECORD_DECL_NODE, list_noop },
        { MACRO_NODE, list_noop },
        { MACRO_DECL_NODE, list_noop },
        { PROC_NODE, list_noop },
        { REPT_NODE, list_noop },
        { WHILE_NODE, list_noop },
        { DO_NODE, list_noop },
        { EXITM_NODE, list_noop },
        { PUSH_MACRO_BODY_NODE, list_noop },
        { POP_MACRO_BODY_NODE, list_noop },
        { PUSH_BRANCH_SCOPE_NODE, list_noop },
        { POP_BRANCH_SCOPE_NODE, list_noop },
        { UNDEF_NODE, list_noop },
        { TOMBSTONE_NODE, list_noop },
        { 0, NULL }
    };

    xref_build_context ctx;
    sorted_sym *all_syms = NULL;
    int all_sym_count = 0;

    xref_summary_entry *callables = NULL;
    xref_summary_entry *jump_targets = NULL;
    xref_summary_entry *data_labels = NULL;
    int callable_count = 0, jump_target_count = 0, data_label_count = 0;
    int callable_cap = 0, jump_target_cap = 0, data_label_cap = 0;

    regex_t inc_re, exc_re;
    int have_inc_re = 0, have_exc_re = 0;

    FILE *fp = NULL;
    int ok = 1;
    int i, j;

    (void)source_file;
    (void)output_file;

    /* Compile regex filters if provided */
    if (include_regex != NULL && include_regex[0] != '\0') {
        if (regcomp(&inc_re, include_regex, REG_EXTENDED | REG_NOSUB) != 0) {
            fprintf(stderr, "error: invalid regex for --xref-summary-include: `%s'\n", include_regex);
            return 0;
        }
        have_inc_re = 1;
    }
    if (exclude_regex != NULL && exclude_regex[0] != '\0') {
        if (regcomp(&exc_re, exclude_regex, REG_EXTENDED | REG_NOSUB) != 0) {
            fprintf(stderr, "error: invalid regex for --xref-summary-exclude: `%s'\n", exclude_regex);
            if (have_inc_re) regfree(&inc_re);
            return 0;
        }
        have_exc_re = 1;
    }

    /* Collect xref data with instruction recording */
    memset(&ctx, 0, sizeof(ctx));
    ctx.include_locals = include_locals;
    ctx.include_anon = include_anon;
    ctx.pure_binary = pure_binary;
    ctx.output_offset = 0;

    summary_instrs = NULL;
    summary_instr_count = 0;
    summary_instr_capacity = 0;

    in_dataseg = 0;
    dataseg_pc = 0;
    codeseg_pc = 0;
    close_source_cache();
    astproc_walk(root, &ctx, map);
    if (summary_instr_count > 1) {
        qsort(summary_instrs, (size_t)summary_instr_count, sizeof(xref_summary_instr), summary_instr_compare);
    }

    /* Build sorted symbol list respecting scope filters */
    for (i = 0; i < ctx.symbol_count; i++) {
        const xref_symbol *s = &ctx.symbols[i];
        if (!s->has_cpu_address) continue;
        if (!scope_allowed(s->scope, include_locals, include_anon)) continue;
        all_sym_count++;
    }
    all_syms = (sorted_sym *)calloc((size_t)(all_sym_count > 0 ? all_sym_count : 1), sizeof(sorted_sym));
    if (all_syms == NULL) { ok = 0; goto cleanup; }
    j = 0;
    for (i = 0; i < ctx.symbol_count; i++) {
        const xref_symbol *s = &ctx.symbols[i];
        if (!s->has_cpu_address) continue;
        if (!scope_allowed(s->scope, include_locals, include_anon)) continue;
        all_syms[j].name = s->name;
        all_syms[j].cpu_address = s->cpu_address;
        all_syms[j].is_code = is_code_symbol(s->cpu_address);
        all_syms[j].is_dataseg = s->is_dataseg;
        j++;
    }
    qsort(all_syms, (size_t)all_sym_count, sizeof(sorted_sym), sorted_sym_compare);

    /* For each non-local symbol, aggregate references and classify */
    for (i = 0; i < all_sym_count; i++) {
        const char *sym_name = all_syms[i].name;
        int sym_addr = all_syms[i].cpu_address;
        int sym_is_code = all_syms[i].is_code;
        int jsr_c = 0, jmp_c = 0, branch_c = 0, read_c = 0, write_c = 0, total_c = 0;
        int is_callable = 0, is_jump_target = 0, is_data_label = 0;

        /* Temporary referrer aggregation */
        xref_summary_referrer *tmp_refs = NULL;
        int tmp_ref_count = 0, tmp_ref_cap = 0;

        int next_sym_dist = -1;
        const char *terminator = "unknown";
        int has_external = 0;

        /* Apply regex filters */
        if (have_inc_re && regexec(&inc_re, sym_name, 0, NULL, 0) != 0) continue;
        if (have_exc_re && regexec(&exc_re, sym_name, 0, NULL, 0) == 0) continue;

        /* Count references to this symbol */
        for (j = 0; j < ctx.ref_count; j++) {
            const xref_ref *r = &ctx.refs[j];
            const char *routine;
            int k, found;
            if (strcmp(r->symbol, sym_name) != 0) continue;
            total_c++;
            if (strcmp(r->access, "call") == 0) { jsr_c++; is_callable = 1; }
            else if (strcmp(r->access, "jump") == 0) { jmp_c++; is_jump_target = 1; }
            else if (strcmp(r->access, "branch") == 0) { branch_c++; is_jump_target = 1; }
            else if (strcmp(r->access, "read") == 0) { read_c++; is_data_label = 1; }
            else if (strcmp(r->access, "write") == 0) { write_c++; is_data_label = 1; }

            /* Aggregate referrer */
            routine = r->has_cpu_address ? find_routine_owner(&ctx, r->cpu_address) : NULL;
            if (routine == NULL) routine = "(unknown)";

            found = -1;
            for (k = 0; k < tmp_ref_count; k++) {
                if (strcmp(tmp_refs[k].routine, routine) == 0) { found = k; break; }
            }
            if (found >= 0) {
                tmp_refs[found].count++;
            } else {
                if (tmp_ref_count >= tmp_ref_cap) {
                    int nc = (tmp_ref_cap == 0) ? 8 : (tmp_ref_cap * 2);
                    xref_summary_referrer *nr = (xref_summary_referrer *)realloc(
                        tmp_refs, (size_t)nc * sizeof(xref_summary_referrer));
                    if (nr == NULL) { free(tmp_refs); ok = 0; goto cleanup; }
                    tmp_refs = nr;
                    tmp_ref_cap = nc;
                }
                tmp_refs[tmp_ref_count].routine = xstrdup(routine);
                tmp_refs[tmp_ref_count].count = 1;
                tmp_ref_count++;
            }
        }

        if (total_c == 0) { free(tmp_refs); continue; }

        /* Sort referrers and truncate */
        if (tmp_ref_count > 1) {
            qsort(tmp_refs, (size_t)tmp_ref_count, sizeof(xref_summary_referrer), referrer_sort_compare);
        }

        /* Determine has_refs_from_other_routines.
           Callable entries own themselves. Other symbols inherit the nearest
           preceding code label in the same section, which matches the
           lexical-routine grouping expected by the summary spec. */
        {
            const char *owner_name = NULL;
            int ki;

            if (is_callable) {
                owner_name = sym_name;
            } else {
                int oi;
                for (oi = i - 1; oi >= 0; oi--) {
                    if (all_syms[oi].is_dataseg != all_syms[i].is_dataseg) continue;
                    if (!all_syms[oi].is_code) continue;
                    owner_name = all_syms[oi].name;
                    break;
                }
                if (owner_name == NULL && sym_is_code) {
                    owner_name = sym_name;
                }
            }

            for (ki = 0; ki < tmp_ref_count; ki++) {
                if (strcmp(tmp_refs[ki].routine, "(unknown)") == 0) continue;
                if (owner_name == NULL || strcmp(tmp_refs[ki].routine, owner_name) != 0) {
                    has_external = 1;
                    break;
                }
            }
        }

        /* Compute next_symbol_distance_bytes (same section only) */
        {
            int ni2;
            for (ni2 = i + 1; ni2 < all_sym_count; ni2++) {
                if (all_syms[ni2].is_dataseg == all_syms[i].is_dataseg) {
                    next_sym_dist = all_syms[ni2].cpu_address - sym_addr;
                    break;
                }
            }
        }

        /* Compute first_run_terminator for code symbols (same section boundary) */
        if (sym_is_code) {
            int next_addr = -1;
            int ni2;
            for (ni2 = i + 1; ni2 < all_sym_count; ni2++) {
                if (all_syms[ni2].is_dataseg == all_syms[i].is_dataseg) {
                    next_addr = all_syms[ni2].cpu_address;
                    break;
                }
            }
            terminator = determine_first_run_terminator(sym_addr, next_addr);
        }

        /* Build nearby symbols list, sorted by absolute distance */
        {
            typedef struct { char *name; int abs_dist; } nearby_entry;
            int ns_count = 0, ns_cap = 0;
            char **ns_list = NULL;
            nearby_entry *ne_list = NULL;
            int ni;
            for (ni = 0; ni < all_sym_count; ni++) {
                int dist;
                if (ni == i) continue;
                dist = all_syms[ni].cpu_address - sym_addr;
                if (dist < 0) dist = -dist;
                if (dist > nearby_window) continue;
                if (ns_count >= ns_cap) {
                    int nnc = (ns_cap == 0) ? 8 : (ns_cap * 2);
                    nearby_entry *nne = (nearby_entry *)realloc(ne_list, (size_t)nnc * sizeof(nearby_entry));
                    if (nne == NULL) break;
                    ne_list = nne;
                    ns_cap = nnc;
                }
                ne_list[ns_count].name = xstrdup(all_syms[ni].name);
                ne_list[ns_count].abs_dist = dist;
                ns_count++;
            }
            /* Sort by absolute distance, then alphabetically */
            if (ns_count > 1) {
                int a_idx, b_idx;
                for (a_idx = 0; a_idx < ns_count - 1; a_idx++) {
                    for (b_idx = a_idx + 1; b_idx < ns_count; b_idx++) {
                        int swap = 0;
                        if (ne_list[b_idx].abs_dist < ne_list[a_idx].abs_dist) swap = 1;
                        else if (ne_list[b_idx].abs_dist == ne_list[a_idx].abs_dist &&
                                 strcmp(ne_list[b_idx].name, ne_list[a_idx].name) < 0) swap = 1;
                        if (swap) {
                            nearby_entry tmp_ne = ne_list[a_idx];
                            ne_list[a_idx] = ne_list[b_idx];
                            ne_list[b_idx] = tmp_ne;
                        }
                    }
                }
            }
            /* Extract sorted names */
            ns_list = (char **)calloc((size_t)(ns_count > 0 ? ns_count : 1), sizeof(char *));
            if (ns_list != NULL) {
                for (ni = 0; ni < ns_count; ni++) {
                    ns_list[ni] = ne_list[ni].name;
                }
            } else {
                for (ni = 0; ni < ns_count; ni++) free(ne_list[ni].name);
                ns_count = 0;
            }
            free(ne_list);

            if (is_callable) {
                if (!ensure_summary_entry_capacity(&callables, &callable_cap, callable_count)
                    || !init_summary_entry(&callables[callable_count],
                                           sym_name,
                                           sym_addr,
                                           jsr_c,
                                           jmp_c,
                                           branch_c,
                                           read_c,
                                           write_c,
                                           total_c,
                                           tmp_refs,
                                           tmp_ref_count,
                                           top_referrers_limit,
                                           terminator,
                                           next_sym_dist,
                                           has_external,
                                           ns_list,
                                           ns_count)) {
                    ok = 0;
                    goto entry_done;
                }
                callables[callable_count].nearby_symbol_count = ns_count;
                callable_count++;
            }
            if (is_jump_target) {
                if (!ensure_summary_entry_capacity(&jump_targets, &jump_target_cap, jump_target_count)
                    || !init_summary_entry(&jump_targets[jump_target_count],
                                           sym_name,
                                           sym_addr,
                                           jsr_c,
                                           jmp_c,
                                           branch_c,
                                           read_c,
                                           write_c,
                                           total_c,
                                           tmp_refs,
                                           tmp_ref_count,
                                           top_referrers_limit,
                                           terminator,
                                           next_sym_dist,
                                           has_external,
                                           ns_list,
                                           ns_count)) {
                    ok = 0;
                    goto entry_done;
                }
                jump_targets[jump_target_count].nearby_symbol_count = ns_count;
                jump_target_count++;
            }
            if (is_data_label) {
                if (!ensure_summary_entry_capacity(&data_labels, &data_label_cap, data_label_count)
                    || !init_summary_entry(&data_labels[data_label_count],
                                           sym_name,
                                           sym_addr,
                                           jsr_c,
                                           jmp_c,
                                           branch_c,
                                           read_c,
                                           write_c,
                                           total_c,
                                           tmp_refs,
                                           tmp_ref_count,
                                           top_referrers_limit,
                                           terminator,
                                           next_sym_dist,
                                           has_external,
                                           ns_list,
                                           ns_count)) {
                    ok = 0;
                    goto entry_done;
                }
                data_labels[data_label_count].nearby_symbol_count = ns_count;
                data_label_count++;
            }

entry_done:
            for (ni = 0; ni < ns_count; ni++) free(ns_list[ni]);
            free(ns_list);
        }

        for (j = 0; j < tmp_ref_count; j++) {
            free(tmp_refs[j].routine);
        }
        free(tmp_refs);
        if (!ok) goto cleanup;
    }

    /* Sort each category */
    if (callable_count > 1) qsort(callables, (size_t)callable_count, sizeof(xref_summary_entry), callable_compare);
    if (jump_target_count > 1) qsort(jump_targets, (size_t)jump_target_count, sizeof(xref_summary_entry), jump_target_compare);
    if (data_label_count > 1) qsort(data_labels, (size_t)data_label_count, sizeof(xref_summary_entry), data_label_compare);

    /* Truncate to limit */
    truncate_summary_entries(callables, &callable_count, limit);
    truncate_summary_entries(jump_targets, &jump_target_count, limit);
    truncate_summary_entries(data_labels, &data_label_count, limit);

    /* Open output */
    if (output_path != NULL) {
        fp = fopen(output_path, "w");
        if (fp == NULL) {
            fprintf(stderr, "error: could not open `%s' for writing\n", output_path);
            ok = 0;
            goto cleanup;
        }
    } else {
        fp = stdout;
    }

    /* Emit */
    if (format == XREF_SUMMARY_FORMAT_JSON) {
        emit_xref_summary_json(fp, callables, callable_count, jump_targets, jump_target_count, data_labels, data_label_count, kind_filter);
    } else if (format == XREF_SUMMARY_FORMAT_NDJSON) {
        emit_xref_summary_ndjson(fp, callables, callable_count, jump_targets, jump_target_count, data_labels, data_label_count, kind_filter);
    } else if (format == XREF_SUMMARY_FORMAT_TEXT) {
        emit_xref_summary_text(fp, callables, callable_count, jump_targets, jump_target_count, data_labels, data_label_count, kind_filter);
    }

    if (output_path != NULL && fp != NULL) {
        fclose(fp);
    }

cleanup:
    /* Free entries */
    for (i = 0; i < callable_count; i++) free_summary_entry(&callables[i]);
    free(callables);
    for (i = 0; i < jump_target_count; i++) free_summary_entry(&jump_targets[i]);
    free(jump_targets);
    for (i = 0; i < data_label_count; i++) free_summary_entry(&data_labels[i]);
    free(data_labels);

    free(all_syms);
    free(summary_instrs);
    summary_instrs = NULL;
    summary_instr_count = 0;
    summary_instr_capacity = 0;

    if (have_inc_re) regfree(&inc_re);
    if (have_exc_re) regfree(&exc_re);

    close_source_cache();
    free_xref_context(&ctx);
    return ok;
}

/* ---- end xref-summary ---- */

/* ---- analyze-index-patterns implementation ---- */

typedef enum tag_index_event_kind {
    INDEX_EVENT_LABEL = 0,
    INDEX_EVENT_INSTR,
    INDEX_EVENT_BARRIER
} index_event_kind;

typedef enum tag_index_access_kind {
    INDEX_ACCESS_NONE = 0,
    INDEX_ACCESS_READ,
    INDEX_ACCESS_WRITE
} index_access_kind;

typedef enum tag_index_value_source_kind {
    INDEX_VALUE_SOURCE_REGISTER = 0,
    INDEX_VALUE_SOURCE_IMMEDIATE,
    INDEX_VALUE_SOURCE_SCALED_ACCUMULATOR,
    INDEX_VALUE_SOURCE_SCALED_REGISTER,
    INDEX_VALUE_SOURCE_UNKNOWN
} index_value_source_kind;

typedef enum tag_index_access_pattern {
    INDEX_PATTERN_BASE = 0,
    INDEX_PATTERN_BASE_PLUS_CONST,
    INDEX_PATTERN_BASE_MINUS_CONST,
    INDEX_PATTERN_PAIRED_BYTE_READS,
    INDEX_PATTERN_SCALED_INDEX_STRIDE_2,
    INDEX_PATTERN_SCALED_INDEX_STRIDE_4,
    INDEX_PATTERN_SPLIT_LO_HI_TABLES
} index_access_pattern;

typedef struct tag_index_label {
    char *name;
    char *scope;
    int cpu_address;
    int is_dataseg;
    int segment_id;
    int is_branch_target;
    int is_code;
    int event_index;
} index_label;

typedef struct tag_index_instr {
    location loc;
    int cpu_address;
    int is_dataseg;
    int mnemonic;
    addressing_mode mode;
    int length;
    int event_index;
    index_access_kind access_kind;
    int supported_access;
    int has_symbol_access;
    int is_indexed_access;
    char index_register;
    char *base_symbol;
    char *base_scope;
    int displacement;
    int immediate_value_known;
    int immediate_value;
    int segment_id;
    char *branch_target_symbol;
} index_instr;

typedef struct tag_index_event {
    int kind;
    int index;
} index_event;

typedef struct tag_index_segment {
    int is_dataseg;
    int end_exclusive;
} index_segment;

typedef struct tag_index_data_range {
    int start;
    int end_exclusive;
    int segment_id;
} index_data_range;

typedef struct tag_index_analysis_context {
    index_label *labels;
    int label_count;
    int label_capacity;
    index_instr *instrs;
    int instr_count;
    int instr_capacity;
    index_event *events;
    int event_count;
    int event_capacity;
    int failed;
    index_segment *segments;
    int segment_count;
    int segment_capacity;
    int current_segment_id;
    index_data_range *data_ranges;
    int data_range_count;
    int data_range_capacity;
} index_analysis_context;

typedef struct tag_index_suffix_pair {
    char *lo;
    char *hi;
} index_suffix_pair;

typedef struct tag_index_pattern_record {
    char *table_label;
    char *routine;
    int site_addr;
    int access_kind;
    int access_pattern;
    char index_register;
    int displacement;
    int source_kind;
    int estimated_record_width;
    char *table_label_lo;
    char *table_label_hi;
    int negative_displacement;
    int adjacent_read_pair;
    int scaled_index;
    int split_named_lo_hi;
    int write_access;
} index_pattern_record;

static int ensure_index_label_capacity(index_analysis_context *ctx)
{
    index_label *tmp;
    int new_capacity;
    if (ctx->label_count < ctx->label_capacity) {
        return 1;
    }
    new_capacity = (ctx->label_capacity == 0) ? 32 : (ctx->label_capacity * 2);
    tmp = (index_label *)realloc(ctx->labels, (size_t)new_capacity * sizeof(index_label));
    if (tmp == NULL) {
        return 0;
    }
    ctx->labels = tmp;
    ctx->label_capacity = new_capacity;
    return 1;
}

static int ensure_index_instr_capacity(index_analysis_context *ctx)
{
    index_instr *tmp;
    int new_capacity;
    if (ctx->instr_count < ctx->instr_capacity) {
        return 1;
    }
    new_capacity = (ctx->instr_capacity == 0) ? 64 : (ctx->instr_capacity * 2);
    tmp = (index_instr *)realloc(ctx->instrs, (size_t)new_capacity * sizeof(index_instr));
    if (tmp == NULL) {
        return 0;
    }
    ctx->instrs = tmp;
    ctx->instr_capacity = new_capacity;
    return 1;
}

static int ensure_index_event_capacity(index_analysis_context *ctx)
{
    index_event *tmp;
    int new_capacity;
    if (ctx->event_count < ctx->event_capacity) {
        return 1;
    }
    new_capacity = (ctx->event_capacity == 0) ? 64 : (ctx->event_capacity * 2);
    tmp = (index_event *)realloc(ctx->events, (size_t)new_capacity * sizeof(index_event));
    if (tmp == NULL) {
        return 0;
    }
    ctx->events = tmp;
    ctx->event_capacity = new_capacity;
    return 1;
}

static int ensure_index_segment_capacity(index_analysis_context *ctx)
{
    index_segment *tmp;
    int new_capacity;
    if (ctx->segment_count < ctx->segment_capacity) {
        return 1;
    }
    new_capacity = (ctx->segment_capacity == 0) ? 8 : (ctx->segment_capacity * 2);
    tmp = (index_segment *)realloc(ctx->segments, (size_t)new_capacity * sizeof(index_segment));
    if (tmp == NULL) {
        return 0;
    }
    ctx->segments = tmp;
    ctx->segment_capacity = new_capacity;
    return 1;
}

static int ensure_index_data_range_capacity(index_analysis_context *ctx)
{
    index_data_range *tmp;
    int new_capacity;
    if (ctx->data_range_count < ctx->data_range_capacity) {
        return 1;
    }
    new_capacity = (ctx->data_range_capacity == 0) ? 16 : (ctx->data_range_capacity * 2);
    tmp = (index_data_range *)realloc(ctx->data_ranges, (size_t)new_capacity * sizeof(index_data_range));
    if (tmp == NULL) {
        return 0;
    }
    ctx->data_ranges = tmp;
    ctx->data_range_capacity = new_capacity;
    return 1;
}

static int start_index_segment(index_analysis_context *ctx)
{
    if (!ensure_index_segment_capacity(ctx)) {
        return 0;
    }
    ctx->current_segment_id = ctx->segment_count;
    ctx->segments[ctx->segment_count].is_dataseg = in_dataseg;
    ctx->segments[ctx->segment_count].end_exclusive = get_current_pc();
    ctx->segment_count++;
    return 1;
}

static void update_index_segment_end(index_analysis_context *ctx, int end_exclusive)
{
    if (ctx->current_segment_id < 0 || ctx->current_segment_id >= ctx->segment_count) {
        return;
    }
    if (end_exclusive > ctx->segments[ctx->current_segment_id].end_exclusive) {
        ctx->segments[ctx->current_segment_id].end_exclusive = end_exclusive;
    }
}

static int record_index_data_range(index_analysis_context *ctx, int start, int end_exclusive)
{
    if (end_exclusive <= start) {
        return 1;
    }
    if (!ensure_index_data_range_capacity(ctx)) {
        return 0;
    }
    ctx->data_ranges[ctx->data_range_count].start = start;
    ctx->data_ranges[ctx->data_range_count].end_exclusive = end_exclusive;
    ctx->data_ranges[ctx->data_range_count].segment_id = ctx->current_segment_id;
    ctx->data_range_count++;
    update_index_segment_end(ctx, end_exclusive);
    return 1;
}

static int add_index_event(index_analysis_context *ctx, int kind, int index)
{
    if (!ensure_index_event_capacity(ctx)) {
        ctx->failed = 1;
        return 0;
    }
    ctx->events[ctx->event_count].kind = kind;
    ctx->events[ctx->event_count].index = index;
    ctx->event_count++;
    return 1;
}

static int find_index_label_by_name(const index_analysis_context *ctx, const char *name)
{
    int i;
    for (i = 0; i < ctx->label_count; i++) {
        if (strcmp(ctx->labels[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int is_conditional_branch_mnemonic(int mnemonic)
{
    switch (mnemonic) {
        case BCC_MNEMONIC:
        case BCS_MNEMONIC:
        case BEQ_MNEMONIC:
        case BMI_MNEMONIC:
        case BNE_MNEMONIC:
        case BPL_MNEMONIC:
        case BVC_MNEMONIC:
        case BVS_MNEMONIC:
            return 1;
        default:
            return 0;
    }
}

static int is_unconditional_transfer_mnemonic(int mnemonic)
{
    return (mnemonic == JMP_MNEMONIC || mnemonic == RTS_MNEMONIC || mnemonic == RTI_MNEMONIC);
}

static int is_supported_index_addressing_mode(addressing_mode mode)
{
    return (mode == ABSOLUTE_X_MODE
         || mode == ABSOLUTE_Y_MODE
         || mode == ZEROPAGE_X_MODE
         || mode == ZEROPAGE_Y_MODE);
}

static int is_supported_direct_data_addressing_mode(addressing_mode mode)
{
    return (mode == ABSOLUTE_MODE
         || mode == ZEROPAGE_MODE
         || mode == ABSOLUTE_X_MODE
         || mode == ABSOLUTE_Y_MODE
         || mode == ZEROPAGE_X_MODE
         || mode == ZEROPAGE_Y_MODE);
}

static char index_register_for_mode(addressing_mode mode)
{
    switch (mode) {
        case ABSOLUTE_X_MODE:
        case ZEROPAGE_X_MODE:
            return 'X';
        case ABSOLUTE_Y_MODE:
        case ZEROPAGE_Y_MODE:
            return 'Y';
        default:
            return '\0';
    }
}

static int extract_symbol_and_displacement_recursive(astnode *expr,
                                                     const char **symbol,
                                                     long *displacement,
                                                     int sign)
{
    if (expr == NULL) {
        return 0;
    }
    if (astnode_is_type(expr, IDENTIFIER_NODE)) {
        if (*symbol != NULL && strcmp(*symbol, expr->ident) != 0) {
            return 0;
        }
        *symbol = expr->ident;
        return 1;
    }
    if (astnode_is_type(expr, INTEGER_NODE)) {
        *displacement += (long)sign * expr->integer;
        return 1;
    }
    if (astnode_is_type(expr, ARITHMETIC_NODE)) {
        if (expr->oper == PLUS_OPERATOR) {
            return extract_symbol_and_displacement_recursive(LHS(expr), symbol, displacement, sign)
                && extract_symbol_and_displacement_recursive(RHS(expr), symbol, displacement, sign);
        }
        if (expr->oper == MINUS_OPERATOR) {
            return extract_symbol_and_displacement_recursive(LHS(expr), symbol, displacement, sign)
                && extract_symbol_and_displacement_recursive(RHS(expr), symbol, displacement, -sign);
        }
    }
    return 0;
}

static int extract_symbol_and_displacement(astnode *expr, const char **symbol, int *displacement)
{
    const char *resolved_symbol = NULL;
    long resolved_disp = 0;
    if (!extract_symbol_and_displacement_recursive(expr, &resolved_symbol, &resolved_disp, 1)) {
        return 0;
    }
    if (resolved_symbol == NULL || resolved_disp < -32768 || resolved_disp > 32767) {
        return 0;
    }
    *symbol = resolved_symbol;
    *displacement = (int)resolved_disp;
    return 1;
}

static int writes_register(const index_instr *instr, char reg)
{
    switch (reg) {
        case 'A':
            return (instr->mnemonic == LDA_MNEMONIC
                 || instr->mnemonic == ADC_MNEMONIC
                 || instr->mnemonic == AND_MNEMONIC
                 || instr->mnemonic == ASL_MNEMONIC
                 || instr->mnemonic == EOR_MNEMONIC
                 || instr->mnemonic == LSR_MNEMONIC
                 || instr->mnemonic == ORA_MNEMONIC
                 || instr->mnemonic == PLA_MNEMONIC
                 || instr->mnemonic == ROL_MNEMONIC
                 || instr->mnemonic == ROR_MNEMONIC
                 || instr->mnemonic == SBC_MNEMONIC
                 || instr->mnemonic == TXA_MNEMONIC
                 || instr->mnemonic == TYA_MNEMONIC);
        case 'X':
            return (instr->mnemonic == LDX_MNEMONIC
                 || instr->mnemonic == DEX_MNEMONIC
                 || instr->mnemonic == INX_MNEMONIC
                 || instr->mnemonic == TAX_MNEMONIC
                 || instr->mnemonic == TSX_MNEMONIC);
        case 'Y':
            return (instr->mnemonic == LDY_MNEMONIC
                 || instr->mnemonic == DEY_MNEMONIC
                 || instr->mnemonic == INY_MNEMONIC
                 || instr->mnemonic == TAY_MNEMONIC);
        default:
            return 0;
    }
}

static const char *index_access_kind_name(int access_kind)
{
    switch (access_kind) {
        case INDEX_ACCESS_READ: return "read";
        case INDEX_ACCESS_WRITE: return "write";
        default: return "unknown";
    }
}

static const char *index_source_kind_name(int source_kind)
{
    switch (source_kind) {
        case INDEX_VALUE_SOURCE_REGISTER: return "register";
        case INDEX_VALUE_SOURCE_IMMEDIATE: return "immediate";
        case INDEX_VALUE_SOURCE_SCALED_ACCUMULATOR: return "scaled_accumulator";
        case INDEX_VALUE_SOURCE_SCALED_REGISTER: return "scaled_register";
        default: return "unknown";
    }
}

static const char *index_access_pattern_name(int pattern)
{
    switch (pattern) {
        case INDEX_PATTERN_BASE: return "base";
        case INDEX_PATTERN_BASE_PLUS_CONST: return "base_plus_const";
        case INDEX_PATTERN_BASE_MINUS_CONST: return "base_minus_const";
        case INDEX_PATTERN_PAIRED_BYTE_READS: return "paired_byte_reads";
        case INDEX_PATTERN_SCALED_INDEX_STRIDE_2: return "scaled_index_stride_2";
        case INDEX_PATTERN_SCALED_INDEX_STRIDE_4: return "scaled_index_stride_4";
        case INDEX_PATTERN_SPLIT_LO_HI_TABLES: return "split_lo_hi_tables";
        default: return "base";
    }
}

static int index_visit_dataseg(astnode *n, void *arg, astnode **next)
{
    index_analysis_context *ctx = (index_analysis_context *)arg;
    (void)n;
    (void)next;
    in_dataseg = 1;
    if (!start_index_segment(ctx)) {
        ctx->failed = 1;
        return 0;
    }
    if (!add_index_event(ctx, INDEX_EVENT_BARRIER, 0)) {
        return 0;
    }
    return 1;
}

static int index_visit_codeseg(astnode *n, void *arg, astnode **next)
{
    index_analysis_context *ctx = (index_analysis_context *)arg;
    (void)n;
    (void)next;
    in_dataseg = 0;
    if (!start_index_segment(ctx)) {
        ctx->failed = 1;
        return 0;
    }
    if (!add_index_event(ctx, INDEX_EVENT_BARRIER, 0)) {
        return 0;
    }
    return 1;
}

static int index_visit_org(astnode *org, void *arg, astnode **next)
{
    index_analysis_context *ctx = (index_analysis_context *)arg;
    int addr = get_current_pc();
    (void)next;
    if (eval_expression_int(LHS(org), &addr, 0)) {
        set_current_pc(addr);
    }
    if (!start_index_segment(ctx)) {
        ctx->failed = 1;
        return 0;
    }
    if (!add_index_event(ctx, INDEX_EVENT_BARRIER, 0)) {
        return 0;
    }
    return 0;
}

static int index_visit_label(astnode *label, void *arg, astnode **next)
{
    index_analysis_context *ctx = (index_analysis_context *)arg;
    const char *kind;
    const char *scope;
    index_label *out;
    (void)kind;
    (void)next;
    classify_symbol_name(label->label, &kind, &scope);
    if (!ensure_index_label_capacity(ctx)) {
        ctx->failed = 1;
        return 0;
    }
    out = &ctx->labels[ctx->label_count];
    memset(out, 0, sizeof(*out));
    out->name = xstrdup(label->label);
    out->scope = xstrdup(scope);
    out->cpu_address = get_current_pc();
    out->is_dataseg = in_dataseg;
    out->segment_id = ctx->current_segment_id;
    out->event_index = ctx->event_count;
    if (out->name == NULL || out->scope == NULL) {
        free(out->name);
        free(out->scope);
        out->name = NULL;
        out->scope = NULL;
        ctx->failed = 1;
        return 0;
    }
    if (!add_index_event(ctx, INDEX_EVENT_LABEL, ctx->label_count)) {
        free(out->name);
        free(out->scope);
        out->name = NULL;
        out->scope = NULL;
        return 0;
    }
    ctx->label_count++;
    return 0;
}

static int index_visit_instruction(astnode *instr, void *arg, astnode **next)
{
    index_analysis_context *ctx = (index_analysis_context *)arg;
    index_instr *out;
    int addr = get_current_pc();
    int len = opcode_length(instr->instr.opcode);
    const char *symbol = NULL;
    int displacement = 0;
    const char *scope;
    const char *kind;
    const char *access;
    (void)next;

    if (len < 1) {
        len = 1;
    }
    if (!ensure_index_instr_capacity(ctx)) {
        ctx->failed = 1;
        return 0;
    }
    out = &ctx->instrs[ctx->instr_count];
    memset(out, 0, sizeof(*out));
    out->loc = instr->loc;
    out->cpu_address = addr;
    out->is_dataseg = in_dataseg;
    out->mnemonic = instr->instr.mnemonic.value;
    out->mode = instr->instr.mode;
    out->length = len;
    out->event_index = ctx->event_count;
    out->segment_id = ctx->current_segment_id;

    access = classify_instruction_access(opcode_to_string(instr->instr.opcode), LHS(instr), instr->instr.mode);
    if (strcmp(access, "read") == 0) {
        out->access_kind = INDEX_ACCESS_READ;
    } else if (strcmp(access, "write") == 0) {
        out->access_kind = INDEX_ACCESS_WRITE;
    } else {
        out->access_kind = INDEX_ACCESS_NONE;
    }

    if (out->access_kind != INDEX_ACCESS_NONE
        && is_supported_direct_data_addressing_mode(instr->instr.mode)
        && LHS(instr) != NULL
        && extract_symbol_and_displacement(LHS(instr), &symbol, &displacement)) {
        classify_symbol_name(symbol, &kind, &scope);
        out->has_symbol_access = 1;
        out->is_indexed_access = is_supported_index_addressing_mode(instr->instr.mode);
        out->index_register = index_register_for_mode(instr->instr.mode);
        out->base_symbol = xstrdup(symbol);
        out->base_scope = xstrdup(scope);
        out->displacement = displacement;
        out->supported_access = out->is_indexed_access;
        if (out->base_symbol == NULL || out->base_scope == NULL) {
            free(out->base_symbol);
            free(out->base_scope);
            out->base_symbol = NULL;
            out->base_scope = NULL;
            ctx->failed = 1;
            return 0;
        }
    }

    if (instr->instr.mode == IMMEDIATE_MODE && LHS(instr) != NULL) {
        int immediate_value;
        if (eval_expression_int(LHS(instr), &immediate_value, 0)) {
            out->immediate_value_known = 1;
            out->immediate_value = immediate_value;
        }
    }

    if (is_conditional_branch_mnemonic(instr->instr.mnemonic.value) && LHS(instr) != NULL) {
        if (extract_symbol_and_displacement(LHS(instr), &symbol, &displacement)) {
            out->branch_target_symbol = xstrdup(symbol);
            if (out->branch_target_symbol == NULL) {
                ctx->failed = 1;
                return 0;
            }
        }
    }

    if (!add_index_event(ctx, INDEX_EVENT_INSTR, ctx->instr_count)) {
        return 0;
    }
    ctx->instr_count++;
    add_current_pc(len);
    update_index_segment_end(ctx, get_current_pc());
    return 0;
}

static int index_visit_data(astnode *data, void *arg, astnode **next)
{
    index_analysis_context *ctx = (index_analysis_context *)arg;
    astnode *expr;
    int bytes_per_item;
    int count = 0;
    (void)next;
    switch (LHS(data)->datatype) {
        case BYTE_DATATYPE:
        case CHAR_DATATYPE:
            bytes_per_item = 1;
            break;
        case WORD_DATATYPE:
            bytes_per_item = 2;
            break;
        case DWORD_DATATYPE:
            bytes_per_item = 4;
            break;
        default:
            bytes_per_item = 1;
            break;
    }
    for (expr = RHS(data); expr != NULL; expr = astnode_get_next_sibling(expr)) {
        count++;
    }
    if (!record_index_data_range(ctx, get_current_pc(), get_current_pc() + (count * bytes_per_item))) {
        return 0;
    }
    add_current_pc(count * bytes_per_item);
    return 0;
}

static int index_visit_storage(astnode *storage, void *arg, astnode **next)
{
    index_analysis_context *ctx = (index_analysis_context *)arg;
    int count = 0;
    (void)next;
    if (eval_expression_int(RHS(storage), &count, 0) && count > 0) {
        if (!record_index_data_range(ctx, get_current_pc(), get_current_pc() + count)) {
            return 0;
        }
        add_current_pc(count);
    }
    return 0;
}

static int index_visit_binary(astnode *node, void *arg, astnode **next)
{
    index_analysis_context *ctx = (index_analysis_context *)arg;
    (void)next;
    if (!record_index_data_range(ctx, get_current_pc(), get_current_pc() + node->binary.size)) {
        return 0;
    }
    add_current_pc(node->binary.size);
    return 0;
}

static int label_is_window_barrier(const index_label *label)
{
    if (strcmp(label->scope, "global") == 0) {
        return 1;
    }
    return label->is_branch_target;
}

static int instr_is_window_barrier(const index_instr *instr)
{
    return is_conditional_branch_mnemonic(instr->mnemonic)
        || instr->mnemonic == JSR_MNEMONIC
        || is_unconditional_transfer_mnemonic(instr->mnemonic);
}

static int collect_window_instruction_indexes(const index_analysis_context *ctx,
                                              int event_index,
                                              int direction,
                                              int *out_indexes,
                                              int max_indexes)
{
    int count = 0;
    int pos;
    for (pos = event_index + direction;
         pos >= 0 && pos < ctx->event_count;
         pos += direction) {
        const index_event *ev = &ctx->events[pos];
        if (ev->kind == INDEX_EVENT_BARRIER) {
            break;
        }
        if (ev->kind == INDEX_EVENT_LABEL) {
            const index_label *label = &ctx->labels[ev->index];
            if (label_is_window_barrier(label)) {
                break;
            }
            continue;
        }
        if (ev->kind == INDEX_EVENT_INSTR) {
            const index_instr *instr = &ctx->instrs[ev->index];
            if (instr_is_window_barrier(instr)) {
                break;
            }
            if (count < max_indexes) {
                out_indexes[count++] = ev->index;
            } else {
                break;
            }
        }
    }
    return count;
}

static const index_label *find_label_record(const index_analysis_context *ctx, const char *name)
{
    int idx = find_index_label_by_name(ctx, name);
    if (idx < 0) {
        return NULL;
    }
    return &ctx->labels[idx];
}

static int has_instruction_at_address(const index_analysis_context *ctx, int addr, int segment_id)
{
    int i;
    for (i = 0; i < ctx->instr_count; i++) {
        if (ctx->instrs[i].cpu_address == addr && ctx->instrs[i].segment_id == segment_id) {
            return 1;
        }
    }
    return 0;
}

static const char *find_index_routine_owner(const index_analysis_context *ctx,
                                            int event_index,
                                            int site_addr,
                                            int is_dataseg,
                                            int segment_id)
{
    int pos;
    for (pos = event_index - 1; pos >= 0; pos--) {
        const index_event *ev = &ctx->events[pos];
        if (ev->kind != INDEX_EVENT_LABEL) {
            continue;
        }
        if (ctx->labels[ev->index].is_dataseg != is_dataseg) {
            continue;
        }
        if (ctx->labels[ev->index].segment_id != segment_id) {
            continue;
        }
        if (strcmp(ctx->labels[ev->index].scope, "global") != 0) {
            continue;
        }
        if (!ctx->labels[ev->index].is_code) {
            continue;
        }
        if (ctx->labels[ev->index].cpu_address <= site_addr) {
            return ctx->labels[ev->index].name;
        }
    }
    return NULL;
}

static int count_contiguous_asl_a_before_transfer(const index_analysis_context *ctx, int transfer_event_index)
{
    int indexes[6];
    int count;
    int i;
    count = collect_window_instruction_indexes(ctx, transfer_event_index, -1, indexes, 6);
    for (i = 0; i < count; i++) {
        const index_instr *instr = &ctx->instrs[indexes[i]];
        if (instr->mnemonic == ASL_MNEMONIC && instr->mode == ACCUMULATOR_MODE) {
            continue;
        }
        return i;
    }
    return count;
}

static int determine_index_source_kind(const index_analysis_context *ctx,
                                       const index_instr *instr,
                                       int *shift_count_out)
{
    int prev_indexes[6];
    int prev_count;
    int i;
    char active_reg = instr->index_register;
    *shift_count_out = 0;

    if (active_reg == '\0') {
        return INDEX_VALUE_SOURCE_UNKNOWN;
    }

    prev_count = collect_window_instruction_indexes(ctx, instr->event_index, -1, prev_indexes, 6);
    for (i = 0; i < prev_count; i++) {
        const index_instr *prev = &ctx->instrs[prev_indexes[i]];
        if (!writes_register(prev, active_reg)) {
            continue;
        }
        if (active_reg == 'X' && prev->mnemonic == LDX_MNEMONIC && prev->mode == IMMEDIATE_MODE) {
            return INDEX_VALUE_SOURCE_IMMEDIATE;
        }
        if (active_reg == 'Y' && prev->mnemonic == LDY_MNEMONIC && prev->mode == IMMEDIATE_MODE) {
            return INDEX_VALUE_SOURCE_IMMEDIATE;
        }
        if ((active_reg == 'X' && prev->mnemonic == TAX_MNEMONIC)
            || (active_reg == 'Y' && prev->mnemonic == TAY_MNEMONIC)) {
            int shift_count = count_contiguous_asl_a_before_transfer(ctx, prev->event_index);
            if (shift_count > 0) {
                *shift_count_out = shift_count;
                return INDEX_VALUE_SOURCE_SCALED_ACCUMULATOR;
            }
        }
        return INDEX_VALUE_SOURCE_REGISTER;
    }
    return INDEX_VALUE_SOURCE_REGISTER;
}

static int find_forward_pair_candidate(const index_analysis_context *ctx,
                                       const index_instr *anchor,
                                       int anchor_source_kind)
{
    int next_indexes[6];
    int next_count;
    int i;
    next_count = collect_window_instruction_indexes(ctx, anchor->event_index, 1, next_indexes, 6);
    for (i = 0; i < next_count; i++) {
        const index_instr *cand = &ctx->instrs[next_indexes[i]];
        int cand_shift_count = 0;
        if (writes_register(cand, anchor->index_register)) {
            break;
        }
        if (!cand->supported_access || cand->access_kind != INDEX_ACCESS_READ) {
            continue;
        }
        if (cand->base_symbol == NULL || anchor->base_symbol == NULL) {
            continue;
        }
        if (strcmp(cand->base_symbol, anchor->base_symbol) != 0) {
            continue;
        }
        if (cand->index_register != anchor->index_register) {
            continue;
        }
        if (determine_index_source_kind(ctx, cand, &cand_shift_count) != anchor_source_kind) {
            continue;
        }
        if ((cand->displacement - anchor->displacement == 1)
            || (cand->displacement - anchor->displacement == -1)) {
            return next_indexes[i];
        }
    }
    return -1;
}

static int match_split_pair_labels(const char *lhs,
                                   const char *rhs,
                                   const index_suffix_pair *pairs,
                                   int pair_count,
                                   char **shared_stem_out,
                                   const char **lo_out,
                                   const char **hi_out)
{
    int i;
    for (i = 0; i < pair_count; i++) {
        size_t lhs_len = strlen(lhs);
        size_t rhs_len = strlen(rhs);
        size_t lo_len = strlen(pairs[i].lo);
        size_t hi_len = strlen(pairs[i].hi);
        if (lhs_len > lo_len && rhs_len > hi_len
            && strcmp(lhs + lhs_len - lo_len, pairs[i].lo) == 0
            && strcmp(rhs + rhs_len - hi_len, pairs[i].hi) == 0
            && lhs_len - lo_len == rhs_len - hi_len
            && strncmp(lhs, rhs, lhs_len - lo_len) == 0) {
            *shared_stem_out = (char *)malloc(lhs_len - lo_len + 1);
            if (*shared_stem_out == NULL) {
                return -1;
            }
            memcpy(*shared_stem_out, lhs, lhs_len - lo_len);
            (*shared_stem_out)[lhs_len - lo_len] = '\0';
            *lo_out = lhs;
            *hi_out = rhs;
            return 1;
        }
        if (lhs_len > hi_len && rhs_len > lo_len
            && strcmp(lhs + lhs_len - hi_len, pairs[i].hi) == 0
            && strcmp(rhs + rhs_len - lo_len, pairs[i].lo) == 0
            && lhs_len - hi_len == rhs_len - lo_len
            && strncmp(lhs, rhs, lhs_len - hi_len) == 0) {
            *shared_stem_out = (char *)malloc(lhs_len - hi_len + 1);
            if (*shared_stem_out == NULL) {
                return -1;
            }
            memcpy(*shared_stem_out, lhs, lhs_len - hi_len);
            (*shared_stem_out)[lhs_len - hi_len] = '\0';
            *lo_out = rhs;
            *hi_out = lhs;
            return 1;
        }
    }
    return 0;
}

static int find_forward_split_candidate(const index_analysis_context *ctx,
                                        const index_instr *anchor,
                                        int anchor_source_kind,
                                        const index_suffix_pair *pairs,
                                        int pair_count,
                                        char **shared_stem_out,
                                        const char **lo_out,
                                        const char **hi_out)
{
    int next_indexes[6];
    int next_count;
    int i;
    next_count = collect_window_instruction_indexes(ctx, anchor->event_index, 1, next_indexes, 6);
    for (i = 0; i < next_count; i++) {
        const index_instr *cand = &ctx->instrs[next_indexes[i]];
        int cand_shift_count = 0;
        int matched;
        if (writes_register(cand, anchor->index_register)) {
            break;
        }
        if (!cand->supported_access || cand->access_kind != INDEX_ACCESS_READ) {
            continue;
        }
        if (cand->base_symbol == NULL || anchor->base_symbol == NULL) {
            continue;
        }
        if (cand->index_register != anchor->index_register) {
            continue;
        }
        if (cand->displacement != anchor->displacement) {
            continue;
        }
        if (determine_index_source_kind(ctx, cand, &cand_shift_count) != anchor_source_kind) {
            continue;
        }
        matched = match_split_pair_labels(anchor->base_symbol,
                                          cand->base_symbol,
                                          pairs,
                                          pair_count,
                                          shared_stem_out,
                                          lo_out,
                                          hi_out);
        if (matched < 0) {
            return -2;
        }
        if (matched > 0) {
            return next_indexes[i];
        }
    }
    return -1;
}

static int parse_index_suffix_pairs(const char *value,
                                    index_suffix_pair **pairs_out,
                                    int *pair_count_out)
{
    static const char *default_pairs = "Lo:Hi,Low:High,_lo:_hi,_low:_high";
    char *buf;
    char *token;
    int capacity = 0;
    int count = 0;
    index_suffix_pair *pairs = NULL;
    const char *src = (value != NULL && value[0] != '\0') ? value : default_pairs;

    buf = xstrdup(src);
    if (buf == NULL) {
        return 0;
    }
    token = strtok(buf, ",");
    while (token != NULL) {
        char *sep = strchr(token, ':');
        index_suffix_pair *tmp;
        if (sep == NULL || sep == token || sep[1] == '\0') {
            int j;
            free(buf);
            for (j = 0; j < count; j++) {
                free(pairs[j].lo);
                free(pairs[j].hi);
            }
            free(pairs);
            return -1;
        }
        *sep = '\0';
        if (count >= capacity) {
            int new_capacity = (capacity == 0) ? 8 : (capacity * 2);
            tmp = (index_suffix_pair *)realloc(pairs, (size_t)new_capacity * sizeof(index_suffix_pair));
            if (tmp == NULL) {
                int j;
                free(buf);
                for (j = 0; j < count; j++) {
                    free(pairs[j].lo);
                    free(pairs[j].hi);
                }
                free(pairs);
                return 0;
            }
            pairs = tmp;
            capacity = new_capacity;
        }
        pairs[count].lo = xstrdup(token);
        pairs[count].hi = xstrdup(sep + 1);
        if (pairs[count].lo == NULL || pairs[count].hi == NULL) {
            int j;
            free(buf);
            for (j = 0; j <= count; j++) {
                free(pairs[j].lo);
                free(pairs[j].hi);
            }
            free(pairs);
            return 0;
        }
        count++;
        token = strtok(NULL, ",");
    }
    free(buf);
    *pairs_out = pairs;
    *pair_count_out = count;
    return 1;
}

static int resolve_exact_indexed_offset(const index_analysis_context *ctx,
                                        const index_instr *instr,
                                        int *resolved_displacement_out)
{
    int prev_indexes[3];
    int prev_count;
    int i;
    char active_reg = instr->index_register;

    if (!instr->is_indexed_access || active_reg == '\0') {
        return 0;
    }

    prev_count = collect_window_instruction_indexes(ctx, instr->event_index, -1, prev_indexes, 3);
    for (i = 0; i < prev_count; i++) {
        const index_instr *prev = &ctx->instrs[prev_indexes[i]];
        if (!writes_register(prev, active_reg)) {
            continue;
        }
        if (((active_reg == 'X' && prev->mnemonic == LDX_MNEMONIC)
             || (active_reg == 'Y' && prev->mnemonic == LDY_MNEMONIC))
            && prev->mode == IMMEDIATE_MODE
            && prev->immediate_value_known) {
            *resolved_displacement_out = instr->displacement + prev->immediate_value;
            return 1;
        }
        return 0;
    }
    return 0;
}

static int analyze_index_site_pattern(const index_analysis_context *ctx,
                                      const index_instr *instr,
                                      const index_suffix_pair *pairs,
                                      int pair_count,
                                      int *source_kind_out,
                                      int *access_pattern_out,
                                      int *estimated_width_out,
                                      char **split_stem_out,
                                      const char **split_lo_out,
                                      const char **split_hi_out)
{
    int source_kind;
    int shift_count;
    int pair_candidate;
    int split_candidate;

    *split_stem_out = NULL;
    *split_lo_out = NULL;
    *split_hi_out = NULL;
    *estimated_width_out = 0;

    source_kind = determine_index_source_kind(ctx, instr, &shift_count);
    pair_candidate = -1;
    split_candidate = -1;

    if (instr->access_kind == INDEX_ACCESS_READ && instr->is_indexed_access) {
        split_candidate = find_forward_split_candidate(ctx,
                                                       instr,
                                                       source_kind,
                                                       pairs,
                                                       pair_count,
                                                       split_stem_out,
                                                       split_lo_out,
                                                       split_hi_out);
        if (split_candidate == -2) {
            return 0;
        }
        pair_candidate = find_forward_pair_candidate(ctx, instr, source_kind);
    }

    if (split_candidate >= 0) {
        *access_pattern_out = INDEX_PATTERN_SPLIT_LO_HI_TABLES;
    } else if (pair_candidate >= 0) {
        *access_pattern_out = INDEX_PATTERN_PAIRED_BYTE_READS;
        *estimated_width_out = 2;
    } else if ((source_kind == INDEX_VALUE_SOURCE_SCALED_ACCUMULATOR
                || source_kind == INDEX_VALUE_SOURCE_SCALED_REGISTER)
               && shift_count >= 2) {
        *access_pattern_out = INDEX_PATTERN_SCALED_INDEX_STRIDE_4;
        *estimated_width_out = 4;
    } else if ((source_kind == INDEX_VALUE_SOURCE_SCALED_ACCUMULATOR
                || source_kind == INDEX_VALUE_SOURCE_SCALED_REGISTER)
               && shift_count >= 1) {
        *access_pattern_out = INDEX_PATTERN_SCALED_INDEX_STRIDE_2;
        *estimated_width_out = 2;
    } else if (instr->displacement < 0) {
        *access_pattern_out = INDEX_PATTERN_BASE_MINUS_CONST;
    } else if (instr->displacement > 0) {
        *access_pattern_out = INDEX_PATTERN_BASE_PLUS_CONST;
    } else {
        *access_pattern_out = INDEX_PATTERN_BASE;
    }

    *source_kind_out = source_kind;
    return 1;
}

static int build_index_analysis_context(astnode *root, index_analysis_context *ctx)
{
    static astnodeprocmap map[] = {
        { DATASEG_NODE, index_visit_dataseg },
        { CODESEG_NODE, index_visit_codeseg },
        { ORG_NODE, index_visit_org },
        { LABEL_NODE, index_visit_label },
        { INSTRUCTION_NODE, index_visit_instruction },
        { DATA_NODE, index_visit_data },
        { STORAGE_NODE, index_visit_storage },
        { BINARY_NODE, index_visit_binary },
        { STRUC_DECL_NODE, list_noop },
        { UNION_DECL_NODE, list_noop },
        { ENUM_DECL_NODE, list_noop },
        { RECORD_DECL_NODE, list_noop },
        { MACRO_NODE, list_noop },
        { MACRO_DECL_NODE, list_noop },
        { PROC_NODE, list_noop },
        { REPT_NODE, list_noop },
        { WHILE_NODE, list_noop },
        { DO_NODE, list_noop },
        { EXITM_NODE, list_noop },
        { PUSH_MACRO_BODY_NODE, list_noop },
        { POP_MACRO_BODY_NODE, list_noop },
        { PUSH_BRANCH_SCOPE_NODE, list_noop },
        { POP_BRANCH_SCOPE_NODE, list_noop },
        { UNDEF_NODE, list_noop },
        { TOMBSTONE_NODE, list_noop },
        { 0, NULL }
    };
    int i;

    memset(ctx, 0, sizeof(*ctx));
    ctx->current_segment_id = -1;

    in_dataseg = 0;
    dataseg_pc = 0;
    codeseg_pc = 0;
    if (!start_index_segment(ctx)) {
        return 0;
    }
    astproc_walk(root, ctx, map);
    if (ctx->failed) {
        return 0;
    }

    for (i = 0; i < ctx->instr_count; i++) {
        if (ctx->instrs[i].branch_target_symbol != NULL) {
            int idx = find_index_label_by_name(ctx, ctx->instrs[i].branch_target_symbol);
            if (idx >= 0) {
                ctx->labels[idx].is_branch_target = 1;
            }
        }
    }
    for (i = 0; i < ctx->label_count; i++) {
        ctx->labels[i].is_code = has_instruction_at_address(ctx,
                                                            ctx->labels[i].cpu_address,
                                                            ctx->labels[i].segment_id);
    }
    return 1;
}

static int index_pattern_record_compare(const void *a, const void *b)
{
    const index_pattern_record *lhs = (const index_pattern_record *)a;
    const index_pattern_record *rhs = (const index_pattern_record *)b;
    int cmp;
    if (lhs->site_addr != rhs->site_addr) {
        return lhs->site_addr - rhs->site_addr;
    }
    cmp = strcmp(lhs->table_label, rhs->table_label);
    if (cmp != 0) {
        return cmp;
    }
    cmp = strcmp(index_access_pattern_name(lhs->access_pattern), index_access_pattern_name(rhs->access_pattern));
    if (cmp != 0) {
        return cmp;
    }
    if (lhs->routine == NULL && rhs->routine != NULL) {
        return -1;
    }
    if (lhs->routine != NULL && rhs->routine == NULL) {
        return 1;
    }
    if (lhs->routine == NULL && rhs->routine == NULL) {
        return 0;
    }
    return strcmp(lhs->routine, rhs->routine);
}

static void free_index_analysis_context(index_analysis_context *ctx)
{
    int i;
    for (i = 0; i < ctx->label_count; i++) {
        free(ctx->labels[i].name);
        free(ctx->labels[i].scope);
    }
    free(ctx->labels);
    for (i = 0; i < ctx->instr_count; i++) {
        free(ctx->instrs[i].base_symbol);
        free(ctx->instrs[i].base_scope);
        free(ctx->instrs[i].branch_target_symbol);
    }
    free(ctx->instrs);
    free(ctx->events);
    free(ctx->segments);
    free(ctx->data_ranges);
}

static void free_index_pattern_record(index_pattern_record *record)
{
    free(record->table_label);
    free(record->routine);
    free(record->table_label_lo);
    free(record->table_label_hi);
}

static int emit_index_pattern_record_json(FILE *fp, const index_pattern_record *record, int first)
{
    int flags_emitted = 0;
    fprintf(fp, "%s  {", first ? "" : ",\n");
    fprintf(fp, "\"table_label\":");
    print_json_string(fp, record->table_label);
    if (record->routine != NULL) {
        fprintf(fp, ",\"routine\":");
        print_json_string(fp, record->routine);
    }
    fprintf(fp, ",\"site_addr\":\"0x%04X\"", record->site_addr & 0xFFFF);
    fprintf(fp, ",\"access_kind\":");
    print_json_string(fp, index_access_kind_name(record->access_kind));
    fprintf(fp, ",\"access_pattern\":");
    print_json_string(fp, index_access_pattern_name(record->access_pattern));
    fprintf(fp, ",\"index_register\":\"%c\"", record->index_register);
    fprintf(fp, ",\"displacement\":%d", record->displacement);
    fprintf(fp, ",\"index_value_source_kind\":");
    print_json_string(fp, index_source_kind_name(record->source_kind));
    if (record->estimated_record_width > 0) {
        fprintf(fp, ",\"estimated_record_width\":%d", record->estimated_record_width);
    }
    if (record->table_label_lo != NULL) {
        fprintf(fp, ",\"table_label_lo\":");
        print_json_string(fp, record->table_label_lo);
    }
    if (record->table_label_hi != NULL) {
        fprintf(fp, ",\"table_label_hi\":");
        print_json_string(fp, record->table_label_hi);
    }
    if (record->negative_displacement || record->adjacent_read_pair || record->scaled_index
        || record->split_named_lo_hi || record->write_access) {
        fprintf(fp, ",\"evidence_flags\":[");
        if (record->negative_displacement) {
            print_json_string(fp, "negative_displacement");
            flags_emitted = 1;
        }
        if (record->adjacent_read_pair) {
            fprintf(fp, "%s", flags_emitted ? "," : "");
            print_json_string(fp, "adjacent_read_pair");
            flags_emitted = 1;
        }
        if (record->scaled_index) {
            fprintf(fp, "%s", flags_emitted ? "," : "");
            print_json_string(fp, "scaled_index");
            flags_emitted = 1;
        }
        if (record->split_named_lo_hi) {
            fprintf(fp, "%s", flags_emitted ? "," : "");
            print_json_string(fp, "split_named_lo_hi");
            flags_emitted = 1;
        }
        if (record->write_access) {
            fprintf(fp, "%s", flags_emitted ? "," : "");
            print_json_string(fp, "write_access");
        }
        fprintf(fp, "]");
    }
    fprintf(fp, "}");
    return 1;
}

static void emit_index_patterns_json(FILE *fp, index_pattern_record *records, int count)
{
    int i;
    fprintf(fp, "[\n");
    for (i = 0; i < count; i++) {
        emit_index_pattern_record_json(fp, &records[i], i == 0);
    }
    if (count > 0) {
        fprintf(fp, "\n");
    }
    fprintf(fp, "]\n");
}

static void emit_index_patterns_ndjson(FILE *fp, index_pattern_record *records, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        emit_index_pattern_record_json(fp, &records[i], 1);
        fprintf(fp, "\n");
    }
}

static void emit_index_patterns_text(FILE *fp, index_pattern_record *records, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        fprintf(fp, "%s %s @ 0x%04X",
                records[i].table_label,
                index_access_kind_name(records[i].access_kind),
                records[i].site_addr & 0xFFFF);
        if (records[i].routine != NULL) {
            fprintf(fp, " in %s", records[i].routine);
        }
        fprintf(fp, "\n");
        fprintf(fp, "  pattern=%s index=%c displacement=%d origin=%s\n",
                index_access_pattern_name(records[i].access_pattern),
                records[i].index_register,
                records[i].displacement,
                index_source_kind_name(records[i].source_kind));
        if (records[i].estimated_record_width > 0) {
            fprintf(fp, "  estimated_record_width=%d\n", records[i].estimated_record_width);
        }
        if (records[i].table_label_lo != NULL || records[i].table_label_hi != NULL) {
            fprintf(fp, "  split_tables=%s/%s\n",
                    records[i].table_label_lo != NULL ? records[i].table_label_lo : "",
                    records[i].table_label_hi != NULL ? records[i].table_label_hi : "");
        }
        if (records[i].negative_displacement || records[i].adjacent_read_pair || records[i].scaled_index
            || records[i].split_named_lo_hi || records[i].write_access) {
            int first = 1;
            fprintf(fp, "  evidence_flags=");
            if (records[i].negative_displacement) {
                fprintf(fp, "negative_displacement");
                first = 0;
            }
            if (records[i].adjacent_read_pair) {
                fprintf(fp, "%sadjacent_read_pair", first ? "" : ", ");
                first = 0;
            }
            if (records[i].scaled_index) {
                fprintf(fp, "%sscaled_index", first ? "" : ", ");
                first = 0;
            }
            if (records[i].split_named_lo_hi) {
                fprintf(fp, "%ssplit_named_lo_hi", first ? "" : ", ");
                first = 0;
            }
            if (records[i].write_access) {
                fprintf(fp, "%swrite_access", first ? "" : ", ");
            }
            fprintf(fp, "\n");
        }
    }
}

int generate_index_patterns(astnode *root,
                            const char *output_path,
                            index_patterns_format format,
                            int include_locals,
                            int include_anon,
                            const char *split_pairs,
                            int pure_binary)
{
    index_analysis_context ctx;
    index_suffix_pair *pairs = NULL;
    index_pattern_record *records = NULL;
    int record_count = 0;
    int record_capacity = 0;
    FILE *fp = NULL;
    int ok = 1;
    int i;
    int parse_result = 0;
    int pair_count = 0;
    (void)pure_binary;

    parse_result = parse_index_suffix_pairs(split_pairs, &pairs, &pair_count);
    if (parse_result == -1) {
        fprintf(stderr, "error: invalid value for --index-patterns-split-pairs\n");
        return 0;
    }
    if (!parse_result) {
        return 0;
    }

    if (!build_index_analysis_context(root, &ctx)) {
        if (pairs != NULL) {
            for (i = 0; i < pair_count; i++) {
                free(pairs[i].lo);
                free(pairs[i].hi);
            }
            free(pairs);
        }
        return 0;
    }

    for (i = 0; ok && i < ctx.instr_count; i++) {
        const index_instr *instr = &ctx.instrs[i];
        const index_label *base_label;
        index_pattern_record *record;
        const char *routine_name;
        int source_kind;
        int access_pattern;
        int estimated_width;
        char *split_stem = NULL;
        const char *split_lo = NULL;
        const char *split_hi = NULL;

        if (!instr->supported_access || instr->base_symbol == NULL) {
            continue;
        }
        base_label = find_label_record(&ctx, instr->base_symbol);
        if (base_label == NULL) {
            continue;
        }
        if (!scope_allowed(base_label->scope, include_locals, include_anon)) {
            continue;
        }
        if (base_label->is_code) {
            continue;
        }

        if (!analyze_index_site_pattern(&ctx,
                                        instr,
                                        pairs,
                                        pair_count,
                                        &source_kind,
                                        &access_pattern,
                                        &estimated_width,
                                        &split_stem,
                                        &split_lo,
                                        &split_hi)) {
            ok = 0;
            break;
        }

        if (record_count >= record_capacity) {
            int new_capacity = (record_capacity == 0) ? 16 : (record_capacity * 2);
            index_pattern_record *tmp = (index_pattern_record *)realloc(
                records, (size_t)new_capacity * sizeof(index_pattern_record));
            if (tmp == NULL) {
                ok = 0;
                break;
            }
            records = tmp;
            record_capacity = new_capacity;
        }

        record = &records[record_count];
        memset(record, 0, sizeof(*record));
        routine_name = find_index_routine_owner(&ctx,
                                                instr->event_index,
                                                instr->cpu_address,
                                                instr->is_dataseg,
                                                instr->segment_id);
        record->table_label = xstrdup(split_stem != NULL ? split_stem : instr->base_symbol);
        record->routine = routine_name != NULL ? xstrdup(routine_name) : NULL;
        record->site_addr = instr->cpu_address;
        record->access_kind = instr->access_kind;
        record->access_pattern = access_pattern;
        record->index_register = instr->index_register;
        record->displacement = instr->displacement;
        record->source_kind = source_kind;
        record->estimated_record_width = estimated_width;
        if (split_lo != NULL) {
            record->table_label_lo = xstrdup(split_lo);
        }
        if (split_hi != NULL) {
            record->table_label_hi = xstrdup(split_hi);
        }
        record->negative_displacement = (instr->displacement < 0);
        record->adjacent_read_pair = (access_pattern == INDEX_PATTERN_PAIRED_BYTE_READS);
        record->scaled_index = (access_pattern == INDEX_PATTERN_SCALED_INDEX_STRIDE_2
                             || access_pattern == INDEX_PATTERN_SCALED_INDEX_STRIDE_4);
        record->split_named_lo_hi = (access_pattern == INDEX_PATTERN_SPLIT_LO_HI_TABLES);
        record->write_access = (instr->access_kind == INDEX_ACCESS_WRITE);
        if (record->table_label == NULL
            || (routine_name != NULL && record->routine == NULL)
            || (split_lo != NULL && record->table_label_lo == NULL)
            || (split_hi != NULL && record->table_label_hi == NULL)) {
            ok = 0;
            free_index_pattern_record(record);
            free(split_stem);
            break;
        }
        record_count++;
        free(split_stem);
    }

    if (ok && record_count > 1) {
        int out = 0;
        qsort(records, (size_t)record_count, sizeof(index_pattern_record), index_pattern_record_compare);
        for (i = 0; i < record_count; i++) {
            if (out > 0
                && records[i].site_addr == records[out - 1].site_addr
                && strcmp(records[i].table_label, records[out - 1].table_label) == 0
                && records[i].access_pattern == records[out - 1].access_pattern
                && ((records[i].routine == NULL && records[out - 1].routine == NULL)
                    || (records[i].routine != NULL && records[out - 1].routine != NULL
                        && strcmp(records[i].routine, records[out - 1].routine) == 0))) {
                free_index_pattern_record(&records[i]);
                continue;
            }
            if (out != i) {
                records[out] = records[i];
            }
            out++;
        }
        record_count = out;
    }

    if (ok) {
        if (output_path != NULL) {
            fp = fopen(output_path, "w");
            if (fp == NULL) {
                fprintf(stderr, "error: could not open `%s' for writing\n", output_path);
                ok = 0;
            }
        } else {
            fp = stdout;
        }
    }

    if (ok) {
        if (format == INDEX_PATTERNS_FORMAT_JSON) {
            emit_index_patterns_json(fp, records, record_count);
        } else if (format == INDEX_PATTERNS_FORMAT_NDJSON) {
            emit_index_patterns_ndjson(fp, records, record_count);
        } else {
            emit_index_patterns_text(fp, records, record_count);
        }
    }

    if (output_path != NULL && fp != NULL) {
        fclose(fp);
    }
    if (pairs != NULL) {
        for (i = 0; i < pair_count; i++) {
            free(pairs[i].lo);
            free(pairs[i].hi);
        }
        free(pairs);
    }
    for (i = 0; i < record_count; i++) {
        free_index_pattern_record(&records[i]);
    }
    free(records);
    free_index_analysis_context(&ctx);
    return ok;
}

typedef struct tag_data_consumer_span {
    char *label;
    int start;
    int end_exclusive;
    int segment_id;
    int label_index;
} data_consumer_span;

typedef struct tag_data_consumer_site {
    char *routine;
    int site_addr;
    int displacement;
    char *addressing_mode;
} data_consumer_site;

typedef struct tag_data_consumer_range {
    int start;
    int end_exclusive;
} data_consumer_range;

typedef struct tag_data_consumer_record {
    char *label;
    int declared_start;
    int declared_end_exclusive;
    int declared_size;
    data_consumer_site *read_sites;
    int read_site_count;
    int read_site_capacity;
    data_consumer_site *write_sites;
    int write_site_count;
    int write_site_capacity;
    int *observed_constant_displacements;
    int observed_count;
    int observed_capacity;
    data_consumer_range *covered_ranges;
    int covered_count;
    int covered_capacity;
    int has_indexed_accesses_without_exact_coverage;
    unsigned char access_pattern_seen[INDEX_PATTERN_SPLIT_LO_HI_TABLES + 1];
} data_consumer_record;

static int data_consumer_span_compare(const void *a, const void *b)
{
    const data_consumer_span *lhs = (const data_consumer_span *)a;
    const data_consumer_span *rhs = (const data_consumer_span *)b;
    if (lhs->start != rhs->start) {
        return lhs->start - rhs->start;
    }
    return lhs->label_index - rhs->label_index;
}

static int data_consumer_site_compare(const void *a, const void *b)
{
    const data_consumer_site *lhs = (const data_consumer_site *)a;
    const data_consumer_site *rhs = (const data_consumer_site *)b;
    int cmp;
    if (lhs->site_addr != rhs->site_addr) {
        return lhs->site_addr - rhs->site_addr;
    }
    if (lhs->routine == NULL && rhs->routine != NULL) {
        return -1;
    }
    if (lhs->routine != NULL && rhs->routine == NULL) {
        return 1;
    }
    if (lhs->routine != NULL && rhs->routine != NULL) {
        cmp = strcmp(lhs->routine, rhs->routine);
        if (cmp != 0) {
            return cmp;
        }
    }
    if (lhs->displacement != rhs->displacement) {
        return lhs->displacement - rhs->displacement;
    }
    return strcmp(lhs->addressing_mode, rhs->addressing_mode);
}

static int data_consumer_range_compare(const void *a, const void *b)
{
    const data_consumer_range *lhs = (const data_consumer_range *)a;
    const data_consumer_range *rhs = (const data_consumer_range *)b;
    if (lhs->start != rhs->start) {
        return lhs->start - rhs->start;
    }
    return lhs->end_exclusive - rhs->end_exclusive;
}

static int int_compare(const void *a, const void *b)
{
    const int *lhs = (const int *)a;
    const int *rhs = (const int *)b;
    return *lhs - *rhs;
}

static void format_data_consumer_addr(char *buf, int buf_size, int addr)
{
    if (buf_size <= 0) {
        return;
    }
    if (addr >= 0 && addr <= 0xFFFF) {
        snprintf(buf, (size_t)buf_size, "0x%04X", addr & 0xFFFF);
    } else {
        snprintf(buf, (size_t)buf_size, "0x%X", addr);
    }
}

static int label_has_data_address(const index_analysis_context *ctx, const index_label *label)
{
    int i;
    for (i = 0; i < ctx->data_range_count; i++) {
        if (ctx->data_ranges[i].segment_id != label->segment_id) {
            continue;
        }
        if (label->cpu_address >= ctx->data_ranges[i].start
            && label->cpu_address < ctx->data_ranges[i].end_exclusive) {
            return 1;
        }
    }
    return 0;
}

static int find_next_non_local_symbol_addr(const index_analysis_context *ctx, const index_label *label)
{
    int i;
    int best = -1;
    for (i = 0; i < ctx->label_count; i++) {
        const index_label *cand = &ctx->labels[i];
        if (strcmp(cand->scope, "global") != 0) {
            continue;
        }
        if (cand->segment_id != label->segment_id) {
            continue;
        }
        if (cand->cpu_address <= label->cpu_address) {
            continue;
        }
        if (best < 0 || cand->cpu_address < best) {
            best = cand->cpu_address;
        }
    }
    if (best >= 0) {
        return best;
    }
    if (label->segment_id >= 0 && label->segment_id < ctx->segment_count) {
        return ctx->segments[label->segment_id].end_exclusive;
    }
    return label->cpu_address;
}

static int build_data_consumer_spans(const index_analysis_context *ctx,
                                     int include_overlaps,
                                     data_consumer_span **spans_out,
                                     int *count_out)
{
    data_consumer_span *spans;
    int count;
    int capacity;
    int i;

    spans = NULL;
    count = 0;
    capacity = 0;
    for (i = 0; i < ctx->label_count; i++) {
        const index_label *label;
        int is_first_at_address;
        int j;
        int include_label;
        int end_exclusive;
        data_consumer_span *tmp;

        label = &ctx->labels[i];
        if (strcmp(label->scope, "global") != 0) {
            continue;
        }
        if (label->is_code) {
            continue;
        }
        if (!label_has_data_address(ctx, label)) {
            continue;
        }

        is_first_at_address = 1;
        for (j = 0; j < i; j++) {
            if (strcmp(ctx->labels[j].scope, "global") != 0) {
                continue;
            }
            if (ctx->labels[j].segment_id != label->segment_id) {
                continue;
            }
            if (ctx->labels[j].cpu_address == label->cpu_address) {
                is_first_at_address = 0;
                break;
            }
        }
        include_label = is_first_at_address || include_overlaps;
        if (!include_label) {
            continue;
        }

        end_exclusive = find_next_non_local_symbol_addr(ctx, label);
        if (count >= capacity) {
            int new_capacity = (capacity == 0) ? 16 : (capacity * 2);
            tmp = (data_consumer_span *)realloc(spans, (size_t)new_capacity * sizeof(data_consumer_span));
            if (tmp == NULL) {
                int k;
                for (k = 0; k < count; k++) {
                    free(spans[k].label);
                }
                free(spans);
                return 0;
            }
            spans = tmp;
            capacity = new_capacity;
        }
        spans[count].label = xstrdup(label->name);
        spans[count].start = label->cpu_address;
        spans[count].end_exclusive = end_exclusive;
        spans[count].segment_id = label->segment_id;
        spans[count].label_index = i;
        if (spans[count].label == NULL) {
            int k;
            for (k = 0; k < count; k++) {
                free(spans[k].label);
            }
            free(spans);
            return 0;
        }
        count++;
    }

    if (count > 1) {
        qsort(spans, (size_t)count, sizeof(data_consumer_span), data_consumer_span_compare);
    }

    *spans_out = spans;
    *count_out = count;
    return 1;
}

static int find_data_consumer_span_by_label(const data_consumer_span *spans,
                                            int span_count,
                                            const char *label)
{
    int i;
    for (i = 0; i < span_count; i++) {
        if (strcmp(spans[i].label, label) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_data_consumer_span_for_addr(const data_consumer_span *spans,
                                            int span_count,
                                            int segment_id,
                                            int addr)
{
    int i;
    int best = -1;
    for (i = 0; i < span_count; i++) {
        if (spans[i].segment_id != segment_id) {
            continue;
        }
        if (spans[i].start > addr || addr >= spans[i].end_exclusive) {
            continue;
        }
        if (best < 0 || spans[i].start >= spans[best].start) {
            best = i;
        }
    }
    return best;
}

static int ensure_data_consumer_site_capacity(data_consumer_record *record, int is_write)
{
    data_consumer_site **sites;
    int *count;
    int *capacity;
    data_consumer_site *tmp;
    int new_capacity;

    if (is_write) {
        sites = &record->write_sites;
        count = &record->write_site_count;
        capacity = &record->write_site_capacity;
    } else {
        sites = &record->read_sites;
        count = &record->read_site_count;
        capacity = &record->read_site_capacity;
    }
    if (*count < *capacity) {
        return 1;
    }
    new_capacity = (*capacity == 0) ? 8 : (*capacity * 2);
    tmp = (data_consumer_site *)realloc(*sites, (size_t)new_capacity * sizeof(data_consumer_site));
    if (tmp == NULL) {
        return 0;
    }
    *sites = tmp;
    *capacity = new_capacity;
    return 1;
}

static int ensure_data_consumer_int_capacity(int **values, int *count, int *capacity)
{
    int *tmp;
    int new_capacity;
    if (*count < *capacity) {
        return 1;
    }
    new_capacity = (*capacity == 0) ? 8 : (*capacity * 2);
    tmp = (int *)realloc(*values, (size_t)new_capacity * sizeof(int));
    if (tmp == NULL) {
        return 0;
    }
    *values = tmp;
    *capacity = new_capacity;
    return 1;
}

static int ensure_data_consumer_range_capacity(data_consumer_record *record)
{
    data_consumer_range *tmp;
    int new_capacity;
    if (record->covered_count < record->covered_capacity) {
        return 1;
    }
    new_capacity = (record->covered_capacity == 0) ? 8 : (record->covered_capacity * 2);
    tmp = (data_consumer_range *)realloc(record->covered_ranges,
                                         (size_t)new_capacity * sizeof(data_consumer_range));
    if (tmp == NULL) {
        return 0;
    }
    record->covered_ranges = tmp;
    record->covered_capacity = new_capacity;
    return 1;
}

static int add_data_consumer_site(data_consumer_record *record,
                                  int is_write,
                                  const char *routine,
                                  int site_addr,
                                  int displacement,
                                  const char *addressing_mode)
{
    data_consumer_site *site;
    if (!ensure_data_consumer_site_capacity(record, is_write)) {
        return 0;
    }
    if (is_write) {
        site = &record->write_sites[record->write_site_count++];
    } else {
        site = &record->read_sites[record->read_site_count++];
    }
    memset(site, 0, sizeof(*site));
    site->routine = routine != NULL ? xstrdup(routine) : NULL;
    site->site_addr = site_addr;
    site->displacement = displacement;
    site->addressing_mode = xstrdup(addressing_mode);
    if ((routine != NULL && site->routine == NULL) || site->addressing_mode == NULL) {
        return 0;
    }
    return 1;
}

static int add_data_consumer_displacement(data_consumer_record *record, int displacement)
{
    if (!ensure_data_consumer_int_capacity(&record->observed_constant_displacements,
                                           &record->observed_count,
                                           &record->observed_capacity)) {
        return 0;
    }
    record->observed_constant_displacements[record->observed_count++] = displacement;
    return 1;
}

static int add_data_consumer_covered_byte(data_consumer_record *record, int addr)
{
    if (!ensure_data_consumer_range_capacity(record)) {
        return 0;
    }
    record->covered_ranges[record->covered_count].start = addr;
    record->covered_ranges[record->covered_count].end_exclusive = addr + 1;
    record->covered_count++;
    return 1;
}

static void free_data_consumer_site(data_consumer_site *site)
{
    free(site->routine);
    free(site->addressing_mode);
}

static void free_data_consumer_record(data_consumer_record *record)
{
    int i;
    free(record->label);
    for (i = 0; i < record->read_site_count; i++) {
        free_data_consumer_site(&record->read_sites[i]);
    }
    free(record->read_sites);
    for (i = 0; i < record->write_site_count; i++) {
        free_data_consumer_site(&record->write_sites[i]);
    }
    free(record->write_sites);
    free(record->observed_constant_displacements);
    free(record->covered_ranges);
}

static void dedupe_data_consumer_sites(data_consumer_site *sites, int *count)
{
    int i;
    int out;
    if (*count <= 1) {
        return;
    }
    qsort(sites, (size_t)(*count), sizeof(data_consumer_site), data_consumer_site_compare);
    out = 0;
    for (i = 0; i < *count; i++) {
        if (out > 0 && data_consumer_site_compare(&sites[i], &sites[out - 1]) == 0) {
            free_data_consumer_site(&sites[i]);
            continue;
        }
        if (out != i) {
            sites[out] = sites[i];
        }
        out++;
    }
    *count = out;
}

static void finalize_data_consumer_record(data_consumer_record *record)
{
    int i;
    int out;

    dedupe_data_consumer_sites(record->read_sites, &record->read_site_count);
    dedupe_data_consumer_sites(record->write_sites, &record->write_site_count);

    if (record->observed_count > 1) {
        qsort(record->observed_constant_displacements,
              (size_t)record->observed_count,
              sizeof(int),
              int_compare);
        out = 0;
        for (i = 0; i < record->observed_count; i++) {
            if (out > 0
                && record->observed_constant_displacements[i]
                   == record->observed_constant_displacements[out - 1]) {
                continue;
            }
            record->observed_constant_displacements[out++] =
                record->observed_constant_displacements[i];
        }
        record->observed_count = out;
    }

    if (record->covered_count > 1) {
        qsort(record->covered_ranges,
              (size_t)record->covered_count,
              sizeof(data_consumer_range),
              data_consumer_range_compare);
        out = 0;
        for (i = 0; i < record->covered_count; i++) {
            if (out > 0
                && record->covered_ranges[i].start <= record->covered_ranges[out - 1].end_exclusive) {
                if (record->covered_ranges[i].end_exclusive > record->covered_ranges[out - 1].end_exclusive) {
                    record->covered_ranges[out - 1].end_exclusive = record->covered_ranges[i].end_exclusive;
                }
                continue;
            }
            record->covered_ranges[out++] = record->covered_ranges[i];
        }
        record->covered_count = out;
    }
}

static int count_data_consumer_distinct_routines(const data_consumer_record *record)
{
    char **seen;
    int seen_count;
    int seen_capacity;
    int i;
    int j;

    seen = NULL;
    seen_count = 0;
    seen_capacity = 0;

    for (i = 0; i < record->read_site_count + record->write_site_count; i++) {
        const data_consumer_site *site;
        int found;
        if (i < record->read_site_count) {
            site = &record->read_sites[i];
        } else {
            site = &record->write_sites[i - record->read_site_count];
        }
        if (site->routine == NULL) {
            continue;
        }
        found = 0;
        for (j = 0; j < seen_count; j++) {
            if (strcmp(seen[j], site->routine) == 0) {
                found = 1;
                break;
            }
        }
        if (found) {
            continue;
        }
        if (seen_count >= seen_capacity) {
            int new_capacity = (seen_capacity == 0) ? 8 : (seen_capacity * 2);
            char **tmp = (char **)realloc(seen, (size_t)new_capacity * sizeof(char *));
            if (tmp == NULL) {
                free(seen);
                return -1;
            }
            seen = tmp;
            seen_capacity = new_capacity;
        }
        seen[seen_count++] = site->routine;
    }
    free(seen);
    return seen_count;
}

static int build_uncovered_ranges(const data_consumer_record *record,
                                  data_consumer_range **ranges_out,
                                  int *count_out)
{
    data_consumer_range *ranges;
    int count;
    int pos;
    int i;

    ranges = NULL;
    count = 0;
    pos = record->declared_start;
    for (i = 0; i < record->covered_count; i++) {
        int start = record->covered_ranges[i].start;
        int end_exclusive = record->covered_ranges[i].end_exclusive;
        data_consumer_range *tmp;
        if (start > pos) {
            tmp = (data_consumer_range *)realloc(ranges, (size_t)(count + 1) * sizeof(data_consumer_range));
            if (tmp == NULL) {
                free(ranges);
                return 0;
            }
            ranges = tmp;
            ranges[count].start = pos;
            ranges[count].end_exclusive = start;
            count++;
        }
        if (end_exclusive > pos) {
            pos = end_exclusive;
        }
    }
    if (pos < record->declared_end_exclusive) {
        data_consumer_range *tmp = (data_consumer_range *)realloc(
            ranges, (size_t)(count + 1) * sizeof(data_consumer_range));
        if (tmp == NULL) {
            free(ranges);
            return 0;
        }
        ranges = tmp;
        ranges[count].start = pos;
        ranges[count].end_exclusive = record->declared_end_exclusive;
        count++;
    }
    *ranges_out = ranges;
    *count_out = count;
    return 1;
}

static int sum_data_consumer_ranges(const data_consumer_range *ranges, int count)
{
    int total = 0;
    int i;
    for (i = 0; i < count; i++) {
        total += ranges[i].end_exclusive - ranges[i].start;
    }
    return total;
}

static int data_consumer_access_count(const data_consumer_record *record)
{
    return record->read_site_count + record->write_site_count;
}

static void free_data_consumer_records(data_consumer_record *records, int count)
{
    int i;
    if (records == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        free_data_consumer_record(&records[i]);
    }
    free(records);
}

static int build_data_consumer_records(astnode *root,
                                       int include_overlaps,
                                       const char *split_pairs,
                                       data_consumer_record **records_out,
                                       int *record_count_out)
{
    index_analysis_context ctx;
    index_suffix_pair *pairs = NULL;
    int pair_count = 0;
    int parse_result;
    data_consumer_span *spans = NULL;
    int span_count = 0;
    data_consumer_record *records = NULL;
    int ok = 1;
    int i;

    memset(&ctx, 0, sizeof(ctx));

    parse_result = parse_index_suffix_pairs(split_pairs, &pairs, &pair_count);
    if (parse_result == -1) {
        fprintf(stderr, "error: invalid value for --index-patterns-split-pairs\n");
        return 0;
    }
    if (!parse_result) {
        return 0;
    }
    if (!build_index_analysis_context(root, &ctx)) {
        ok = 0;
        goto cleanup;
    }
    if (!build_data_consumer_spans(&ctx, include_overlaps, &spans, &span_count)) {
        ok = 0;
        goto cleanup;
    }

    records = (data_consumer_record *)calloc((size_t)(span_count > 0 ? span_count : 1),
                                             sizeof(data_consumer_record));
    if (records == NULL) {
        ok = 0;
        goto cleanup;
    }
    for (i = 0; i < span_count; i++) {
        records[i].label = xstrdup(spans[i].label);
        records[i].declared_start = spans[i].start;
        records[i].declared_end_exclusive = spans[i].end_exclusive;
        records[i].declared_size = spans[i].end_exclusive - spans[i].start;
        if (records[i].label == NULL) {
            ok = 0;
            goto cleanup;
        }
    }

    for (i = 0; i < ctx.instr_count; i++) {
        const index_instr *instr;
        const index_label *base_label;
        int record_index;
        int constant_addr;
        int aggregate_displacement;
        const char *routine_name;
        int access_pattern;
        int estimated_width;
        int source_kind;
        char *split_stem;
        const char *split_lo;
        const char *split_hi;

        instr = &ctx.instrs[i];
        if (!instr->has_symbol_access || instr->base_symbol == NULL || instr->access_kind == INDEX_ACCESS_NONE) {
            continue;
        }
        base_label = find_label_record(&ctx, instr->base_symbol);
        if (base_label == NULL || base_label->is_code) {
            continue;
        }

        record_index = find_data_consumer_span_by_label(spans, span_count, instr->base_symbol);
        constant_addr = base_label->cpu_address + instr->displacement;
        if (record_index < 0) {
            record_index = find_data_consumer_span_for_addr(spans,
                                                            span_count,
                                                            base_label->segment_id,
                                                            constant_addr);
        }
        if (record_index < 0) {
            continue;
        }

        aggregate_displacement = constant_addr - records[record_index].declared_start;
        routine_name = find_index_routine_owner(&ctx,
                                                instr->event_index,
                                                instr->cpu_address,
                                                instr->is_dataseg,
                                                instr->segment_id);
        if (!add_data_consumer_site(&records[record_index],
                                    instr->access_kind == INDEX_ACCESS_WRITE,
                                    routine_name,
                                    instr->cpu_address,
                                    aggregate_displacement,
                                    addressing_mode_name(instr->mode))) {
            ok = 0;
            goto cleanup;
        }
        if (!add_data_consumer_displacement(&records[record_index], aggregate_displacement)) {
            ok = 0;
            goto cleanup;
        }

        if (instr->is_indexed_access && base_label->cpu_address == records[record_index].declared_start) {
            split_stem = NULL;
            split_lo = NULL;
            split_hi = NULL;
            if (!analyze_index_site_pattern(&ctx,
                                            instr,
                                            pairs,
                                            pair_count,
                                            &source_kind,
                                            &access_pattern,
                                            &estimated_width,
                                            &split_stem,
                                            &split_lo,
                                            &split_hi)) {
                ok = 0;
                free(split_stem);
                goto cleanup;
            }
            free(split_stem);
        } else if (aggregate_displacement < 0) {
            access_pattern = INDEX_PATTERN_BASE_MINUS_CONST;
        } else if (aggregate_displacement > 0) {
            access_pattern = INDEX_PATTERN_BASE_PLUS_CONST;
        } else {
            access_pattern = INDEX_PATTERN_BASE;
        }
        records[record_index].access_pattern_seen[access_pattern] = 1;

        if (instr->is_indexed_access) {
            int resolved_disp;
            int resolved_addr;
            if (resolve_exact_indexed_offset(&ctx, instr, &resolved_disp)) {
                resolved_addr = base_label->cpu_address + resolved_disp;
                if (resolved_addr >= records[record_index].declared_start
                    && resolved_addr < records[record_index].declared_end_exclusive) {
                    if (!add_data_consumer_covered_byte(&records[record_index], resolved_addr)) {
                        ok = 0;
                        goto cleanup;
                    }
                }
            } else {
                records[record_index].has_indexed_accesses_without_exact_coverage = 1;
            }
        } else if (constant_addr >= records[record_index].declared_start
                   && constant_addr < records[record_index].declared_end_exclusive) {
            if (!add_data_consumer_covered_byte(&records[record_index], constant_addr)) {
                ok = 0;
                goto cleanup;
            }
        }
    }

    for (i = 0; i < span_count; i++) {
        finalize_data_consumer_record(&records[i]);
    }

    *records_out = records;
    *record_count_out = span_count;
    records = NULL;

cleanup:
    free_data_consumer_records(records, span_count);
    if (spans != NULL) {
        for (i = 0; i < span_count; i++) {
            free(spans[i].label);
        }
    }
    free(spans);
    if (pairs != NULL) {
        for (i = 0; i < pair_count; i++) {
            free(pairs[i].lo);
            free(pairs[i].hi);
        }
    }
    free(pairs);
    if (ctx.labels != NULL || ctx.instrs != NULL || ctx.events != NULL
        || ctx.segments != NULL || ctx.data_ranges != NULL) {
        free_index_analysis_context(&ctx);
    }
    return ok;
}

static int emit_data_consumer_record_json(FILE *fp,
                                          const data_consumer_record *record,
                                          int first)
{
    data_consumer_range *uncovered_ranges;
    int uncovered_count;
    int distinct_routine_count;
    int i;
    int first_pattern;
    char addr_buf[16];

    uncovered_ranges = NULL;
    uncovered_count = 0;
    if (!build_uncovered_ranges(record, &uncovered_ranges, &uncovered_count)) {
        return 0;
    }
    distinct_routine_count = count_data_consumer_distinct_routines(record);
    if (distinct_routine_count < 0) {
        free(uncovered_ranges);
        return 0;
    }

    fprintf(fp, "%s  {", first ? "" : ",\n");
    fprintf(fp, "\"label\":");
    print_json_string(fp, record->label);
    format_data_consumer_addr(addr_buf, sizeof(addr_buf), record->declared_start);
    fprintf(fp, ",\"declared_start\":");
    print_json_string(fp, addr_buf);
    format_data_consumer_addr(addr_buf, sizeof(addr_buf), record->declared_end_exclusive);
    fprintf(fp, ",\"declared_end_exclusive\":");
    print_json_string(fp, addr_buf);
    fprintf(fp, ",\"declared_size\":%d", record->declared_size);
    fprintf(fp, ",\"read_site_count\":%d", record->read_site_count);
    fprintf(fp, ",\"write_site_count\":%d", record->write_site_count);
    fprintf(fp, ",\"distinct_routine_count\":%d", distinct_routine_count);
    fprintf(fp, ",\"observed_constant_displacements\":[");
    for (i = 0; i < record->observed_count; i++) {
        fprintf(fp, "%s%d", i > 0 ? "," : "", record->observed_constant_displacements[i]);
    }
    fprintf(fp, "]");
    fprintf(fp, ",\"covered_ranges\":[");
    for (i = 0; i < record->covered_count; i++) {
        fprintf(fp, "%s{\"start\":", i > 0 ? "," : "");
        format_data_consumer_addr(addr_buf, sizeof(addr_buf), record->covered_ranges[i].start);
        print_json_string(fp, addr_buf);
        fprintf(fp, ",\"end_exclusive\":");
        format_data_consumer_addr(addr_buf, sizeof(addr_buf), record->covered_ranges[i].end_exclusive);
        print_json_string(fp, addr_buf);
        fprintf(fp, "}");
    }
    fprintf(fp, "]");
    fprintf(fp, ",\"uncovered_ranges\":[");
    for (i = 0; i < uncovered_count; i++) {
        fprintf(fp, "%s{\"start\":", i > 0 ? "," : "");
        format_data_consumer_addr(addr_buf, sizeof(addr_buf), uncovered_ranges[i].start);
        print_json_string(fp, addr_buf);
        fprintf(fp, ",\"end_exclusive\":");
        format_data_consumer_addr(addr_buf, sizeof(addr_buf), uncovered_ranges[i].end_exclusive);
        print_json_string(fp, addr_buf);
        fprintf(fp, "}");
    }
    fprintf(fp, "]");
    fprintf(fp, ",\"has_indexed_accesses_without_exact_coverage\":%s",
            record->has_indexed_accesses_without_exact_coverage ? "true" : "false");
    fprintf(fp, ",\"access_patterns\":[");
    first_pattern = 1;
    for (i = 0; i <= INDEX_PATTERN_SPLIT_LO_HI_TABLES; i++) {
        if (!record->access_pattern_seen[i]) {
            continue;
        }
        fprintf(fp, "%s", first_pattern ? "" : ",");
        print_json_string(fp, index_access_pattern_name(i));
        first_pattern = 0;
    }
    fprintf(fp, "]");
    fprintf(fp, ",\"read_sites\":[");
    for (i = 0; i < record->read_site_count; i++) {
        fprintf(fp, "%s{", i > 0 ? "," : "");
        if (record->read_sites[i].routine != NULL) {
            fprintf(fp, "\"routine\":");
            print_json_string(fp, record->read_sites[i].routine);
            fprintf(fp, ",");
        }
        fprintf(fp, "\"site_addr\":\"0x%04X\",\"displacement\":%d,\"addressing_mode\":",
                record->read_sites[i].site_addr & 0xFFFF,
                record->read_sites[i].displacement);
        print_json_string(fp, record->read_sites[i].addressing_mode);
        fprintf(fp, "}");
    }
    fprintf(fp, "]");
    fprintf(fp, ",\"write_sites\":[");
    for (i = 0; i < record->write_site_count; i++) {
        fprintf(fp, "%s{", i > 0 ? "," : "");
        if (record->write_sites[i].routine != NULL) {
            fprintf(fp, "\"routine\":");
            print_json_string(fp, record->write_sites[i].routine);
            fprintf(fp, ",");
        }
        fprintf(fp, "\"site_addr\":\"0x%04X\",\"displacement\":%d,\"addressing_mode\":",
                record->write_sites[i].site_addr & 0xFFFF,
                record->write_sites[i].displacement);
        print_json_string(fp, record->write_sites[i].addressing_mode);
        fprintf(fp, "}");
    }
    fprintf(fp, "]}");

    free(uncovered_ranges);
    return 1;
}

static int emit_data_consumers_json(FILE *fp, data_consumer_record *records, int count)
{
    int i;
    fprintf(fp, "[\n");
    for (i = 0; i < count; i++) {
        if (!emit_data_consumer_record_json(fp, &records[i], i == 0)) {
            return 0;
        }
    }
    if (count > 0) {
        fprintf(fp, "\n");
    }
    fprintf(fp, "]\n");
    return 1;
}

static int emit_data_consumers_ndjson(FILE *fp, data_consumer_record *records, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        if (!emit_data_consumer_record_json(fp, &records[i], 1)) {
            return 0;
        }
        fprintf(fp, "\n");
    }
    return 1;
}

static int emit_data_consumers_text(FILE *fp, data_consumer_record *records, int count)
{
    int i;
    int j;
    for (i = 0; i < count; i++) {
        data_consumer_range *uncovered_ranges;
        int uncovered_count;
        int distinct_routine_count;
        int first_pattern;
        char addr_buf[16];
        if (!build_uncovered_ranges(&records[i], &uncovered_ranges, &uncovered_count)) {
            return 0;
        }
        distinct_routine_count = count_data_consumer_distinct_routines(&records[i]);
        if (distinct_routine_count < 0) {
            free(uncovered_ranges);
            return 0;
        }
        format_data_consumer_addr(addr_buf, sizeof(addr_buf), records[i].declared_start);
        fprintf(fp, "%s @ %s size=%d\n", records[i].label, addr_buf, records[i].declared_size);
        fprintf(fp, "  reads=%d writes=%d routines=%d\n",
                records[i].read_site_count,
                records[i].write_site_count,
                distinct_routine_count);
        fprintf(fp, "  observed_constant_displacements:");
        for (j = 0; j < records[i].observed_count; j++) {
            fprintf(fp, "%s%d", j > 0 ? ", " : " ", records[i].observed_constant_displacements[j]);
        }
        if (records[i].observed_count == 0) {
            fprintf(fp, " (none)");
        }
        fprintf(fp, "\n");
        fprintf(fp, "  covered_ranges:");
        for (j = 0; j < records[i].covered_count; j++) {
            format_data_consumer_addr(addr_buf, sizeof(addr_buf), records[i].covered_ranges[j].start);
            fprintf(fp, "%s[%s,", j > 0 ? ", " : " ", addr_buf);
            format_data_consumer_addr(addr_buf, sizeof(addr_buf), records[i].covered_ranges[j].end_exclusive);
            fprintf(fp, "%s)", addr_buf);
        }
        if (records[i].covered_count == 0) {
            fprintf(fp, " (none)");
        }
        fprintf(fp, "\n");
        fprintf(fp, "  uncovered_ranges:");
        for (j = 0; j < uncovered_count; j++) {
            format_data_consumer_addr(addr_buf, sizeof(addr_buf), uncovered_ranges[j].start);
            fprintf(fp, "%s[%s,", j > 0 ? ", " : " ", addr_buf);
            format_data_consumer_addr(addr_buf, sizeof(addr_buf), uncovered_ranges[j].end_exclusive);
            fprintf(fp, "%s)", addr_buf);
        }
        if (uncovered_count == 0) {
            fprintf(fp, " (none)");
        }
        fprintf(fp, "\n");
        fprintf(fp, "  access_patterns:");
        first_pattern = 1;
        for (j = 0; j <= INDEX_PATTERN_SPLIT_LO_HI_TABLES; j++) {
            if (!records[i].access_pattern_seen[j]) {
                continue;
            }
            fprintf(fp, "%s%s", first_pattern ? " " : ", ", index_access_pattern_name(j));
            first_pattern = 0;
        }
        if (first_pattern) {
            fprintf(fp, " (none)");
        }
        fprintf(fp, "\n");
        free(uncovered_ranges);
    }
    return 1;
}

static int emit_data_coverage_record_json(FILE *fp,
                                          const data_consumer_record *record,
                                          int first)
{
    data_consumer_range *uncovered_ranges = NULL;
    int uncovered_count = 0;
    int covered_size;
    int uncovered_size;
    int i;
    char addr_buf[16];

    if (!build_uncovered_ranges(record, &uncovered_ranges, &uncovered_count)) {
        return 0;
    }
    covered_size = sum_data_consumer_ranges(record->covered_ranges, record->covered_count);
    uncovered_size = sum_data_consumer_ranges(uncovered_ranges, uncovered_count);

    fprintf(fp, "%s  {", first ? "" : ",\n");
    fprintf(fp, "\"label\":");
    print_json_string(fp, record->label);
    format_data_consumer_addr(addr_buf, sizeof(addr_buf), record->declared_start);
    fprintf(fp, ",\"declared_start\":");
    print_json_string(fp, addr_buf);
    format_data_consumer_addr(addr_buf, sizeof(addr_buf), record->declared_end_exclusive);
    fprintf(fp, ",\"declared_end_exclusive\":");
    print_json_string(fp, addr_buf);
    fprintf(fp, ",\"declared_size\":%d", record->declared_size);
    fprintf(fp, ",\"covered_ranges\":[");
    for (i = 0; i < record->covered_count; i++) {
        fprintf(fp, "%s{\"start\":", i > 0 ? "," : "");
        format_data_consumer_addr(addr_buf, sizeof(addr_buf), record->covered_ranges[i].start);
        print_json_string(fp, addr_buf);
        fprintf(fp, ",\"end_exclusive\":");
        format_data_consumer_addr(addr_buf, sizeof(addr_buf), record->covered_ranges[i].end_exclusive);
        print_json_string(fp, addr_buf);
        fprintf(fp, "}");
    }
    fprintf(fp, "]");
    fprintf(fp, ",\"covered_size\":%d", covered_size);
    fprintf(fp, ",\"uncovered_ranges\":[");
    for (i = 0; i < uncovered_count; i++) {
        fprintf(fp, "%s{\"start\":", i > 0 ? "," : "");
        format_data_consumer_addr(addr_buf, sizeof(addr_buf), uncovered_ranges[i].start);
        print_json_string(fp, addr_buf);
        fprintf(fp, ",\"end_exclusive\":");
        format_data_consumer_addr(addr_buf, sizeof(addr_buf), uncovered_ranges[i].end_exclusive);
        print_json_string(fp, addr_buf);
        fprintf(fp, "}");
    }
    fprintf(fp, "]");
    fprintf(fp, ",\"uncovered_size\":%d", uncovered_size);
    fprintf(fp, ",\"access_count\":%d", data_consumer_access_count(record));
    fprintf(fp, ",\"has_indexed_accesses_without_exact_coverage\":%s",
            record->has_indexed_accesses_without_exact_coverage ? "true" : "false");
    fprintf(fp, "}");

    free(uncovered_ranges);
    return 1;
}

static int emit_data_coverage_json(FILE *fp, data_consumer_record *records, int count)
{
    int i;
    fprintf(fp, "[\n");
    for (i = 0; i < count; i++) {
        if (!emit_data_coverage_record_json(fp, &records[i], i == 0)) {
            return 0;
        }
    }
    if (count > 0) {
        fprintf(fp, "\n");
    }
    fprintf(fp, "]\n");
    return 1;
}

static int emit_data_coverage_ndjson(FILE *fp, data_consumer_record *records, int count)
{
    int i;
    for (i = 0; i < count; i++) {
        if (!emit_data_coverage_record_json(fp, &records[i], 1)) {
            return 0;
        }
        fprintf(fp, "\n");
    }
    return 1;
}

static int emit_data_coverage_text(FILE *fp, data_consumer_record *records, int count)
{
    int i;
    int j;
    char addr_buf[16];
    for (i = 0; i < count; i++) {
        data_consumer_range *uncovered_ranges = NULL;
        int uncovered_count = 0;
        if (!build_uncovered_ranges(&records[i], &uncovered_ranges, &uncovered_count)) {
            return 0;
        }
        format_data_consumer_addr(addr_buf, sizeof(addr_buf), records[i].declared_start);
        fprintf(fp, "%s @ %s size=%d\n", records[i].label, addr_buf, records[i].declared_size);
        fprintf(fp, "  covered_ranges:");
        for (j = 0; j < records[i].covered_count; j++) {
            format_data_consumer_addr(addr_buf, sizeof(addr_buf), records[i].covered_ranges[j].start);
            fprintf(fp, "%s[%s,", j > 0 ? ", " : " ", addr_buf);
            format_data_consumer_addr(addr_buf, sizeof(addr_buf), records[i].covered_ranges[j].end_exclusive);
            fprintf(fp, "%s)", addr_buf);
        }
        if (records[i].covered_count == 0) {
            fprintf(fp, " (none)");
        }
        fprintf(fp, "\n");
        fprintf(fp, "  uncovered_ranges:");
        for (j = 0; j < uncovered_count; j++) {
            format_data_consumer_addr(addr_buf, sizeof(addr_buf), uncovered_ranges[j].start);
            fprintf(fp, "%s[%s,", j > 0 ? ", " : " ", addr_buf);
            format_data_consumer_addr(addr_buf, sizeof(addr_buf), uncovered_ranges[j].end_exclusive);
            fprintf(fp, "%s)", addr_buf);
        }
        if (uncovered_count == 0) {
            fprintf(fp, " (none)");
        }
        fprintf(fp, "\n");
        fprintf(fp, "  access_count: %d\n", data_consumer_access_count(&records[i]));
        fprintf(fp, "  has_indexed_accesses_without_exact_coverage: %s\n",
                records[i].has_indexed_accesses_without_exact_coverage ? "true" : "false");
        free(uncovered_ranges);
    }
    return 1;
}

int generate_data_consumers(astnode *root,
                            const char *output_path,
                            data_consumers_format format,
                            int include_overlaps,
                            const char *split_pairs,
                            int pure_binary)
{
    data_consumer_record *records = NULL;
    int span_count = 0;
    FILE *fp = NULL;
    int ok;

    (void)pure_binary;

    ok = build_data_consumer_records(root,
                                     include_overlaps,
                                     split_pairs,
                                     &records,
                                     &span_count);
    if (!ok) {
        return 0;
    }

    if (output_path != NULL) {
        fp = fopen(output_path, "w");
        if (fp == NULL) {
            fprintf(stderr, "error: could not open `%s' for writing\n", output_path);
            free_data_consumer_records(records, span_count);
            return 0;
        }
    } else {
        fp = stdout;
    }

    if (format == DATA_CONSUMERS_FORMAT_JSON) {
        if (!emit_data_consumers_json(fp, records, span_count)) {
            ok = 0;
            goto cleanup;
        }
    } else if (format == DATA_CONSUMERS_FORMAT_NDJSON) {
        if (!emit_data_consumers_ndjson(fp, records, span_count)) {
            ok = 0;
            goto cleanup;
        }
    } else {
        if (!emit_data_consumers_text(fp, records, span_count)) {
            ok = 0;
            goto cleanup;
        }
    }

cleanup:
    if (output_path != NULL && fp != NULL) {
        fclose(fp);
    }
    free_data_consumer_records(records, span_count);
    return ok;
}

int generate_data_coverage(astnode *root,
                           const char *output_path,
                           data_coverage_format format,
                           int include_overlaps,
                           const char *split_pairs,
                           int pure_binary)
{
    data_consumer_record *records = NULL;
    int record_count = 0;
    FILE *fp = NULL;
    int ok;

    (void)pure_binary;

    ok = build_data_consumer_records(root,
                                     include_overlaps,
                                     split_pairs,
                                     &records,
                                     &record_count);
    if (!ok) {
        return 0;
    }

    if (output_path != NULL) {
        fp = fopen(output_path, "w");
        if (fp == NULL) {
            fprintf(stderr, "error: could not open `%s' for writing\n", output_path);
            free_data_consumer_records(records, record_count);
            return 0;
        }
    } else {
        fp = stdout;
    }

    if (format == DATA_COVERAGE_FORMAT_JSON) {
        ok = emit_data_coverage_json(fp, records, record_count);
    } else if (format == DATA_COVERAGE_FORMAT_NDJSON) {
        ok = emit_data_coverage_ndjson(fp, records, record_count);
    } else {
        ok = emit_data_coverage_text(fp, records, record_count);
    }

    if (output_path != NULL && fp != NULL) {
        fclose(fp);
    }
    free_data_consumer_records(records, record_count);
    return ok;
}

/* ---- end analyze-index-patterns ---- */

int generate_xref(astnode *root,
                  const char *filename,
                  xref_format format,
                  int include_data,
                  int include_owner,
                  int include_locals,
                  int include_anon,
                  const char *source_file,
                  const char *output_file,
                  int pure_binary)
{
    static astnodeprocmap map[] = {
        { DATASEG_NODE, xref_visit_dataseg },
        { CODESEG_NODE, xref_visit_codeseg },
        { ORG_NODE, xref_visit_org },
        { LABEL_NODE, xref_visit_label },
        { INSTRUCTION_NODE, xref_visit_instruction },
        { DATA_NODE, xref_visit_data },
        { STORAGE_NODE, xref_visit_storage },
        { BINARY_NODE, xref_visit_binary },
        { STRUC_DECL_NODE, list_noop },
        { UNION_DECL_NODE, list_noop },
        { ENUM_DECL_NODE, list_noop },
        { RECORD_DECL_NODE, list_noop },
        { MACRO_NODE, list_noop },
        { MACRO_DECL_NODE, list_noop },
        { PROC_NODE, list_noop },
        { REPT_NODE, list_noop },
        { WHILE_NODE, list_noop },
        { DO_NODE, list_noop },
        { EXITM_NODE, list_noop },
        { PUSH_MACRO_BODY_NODE, list_noop },
        { POP_MACRO_BODY_NODE, list_noop },
        { PUSH_BRANCH_SCOPE_NODE, list_noop },
        { POP_BRANCH_SCOPE_NODE, list_noop },
        { UNDEF_NODE, list_noop },
        { TOMBSTONE_NODE, list_noop },
        { 0, NULL }
    };
    xref_build_context ctx;
    symbol_ident_list constants;
    int i;
    int ok = 1;

    memset(&ctx, 0, sizeof(ctx));
    ctx.include_locals = include_locals;
    ctx.include_anon = include_anon;
    ctx.include_data = include_data;
    ctx.include_owner = include_owner;
    ctx.pure_binary = pure_binary;
    ctx.output_offset = 0;
    ctx.current_segment_id = 0;
    ctx.next_segment_id = 1;

    in_dataseg = 0;
    dataseg_pc = 0;
    codeseg_pc = 0;
    close_source_cache();
    astproc_walk(root, &ctx, map);
    if (ctx.failed) {
        ok = 0;
    }

    if (ok) {
        symtab_list_type(CONSTANT_SYMBOL, &constants);
        for (i = 0; i < constants.size; ++i) {
            symtab_entry *e = symtab_lookup(constants.idents[i]);
            int value = 0;
            if (e == NULL) {
                continue;
            }
            if (!(e->flags & EQU_FLAG) || (e->flags & VOLATILE_FLAG)) {
                continue;
            }
            if (e->def != NULL) {
                if (eval_expression_int(e->def, (int *)&value, 0)) {
                    if (!add_or_update_xref_symbol(&ctx,
                                                   e->id,
                                                   "equ",
                                                   "global",
                                                   1,
                                                   &e->def->loc,
                                                   0,
                                                   0,
                                                   -1,
                                                   0,
                                                   0,
                                                   1,
                                                   (long)value)) {
                        ok = 0;
                        break;
                    }
                } else {
                    if (!add_or_update_xref_symbol(&ctx,
                                                   e->id,
                                                   "equ",
                                                   "global",
                                                   1,
                                                   &e->def->loc,
                                                   0,
                                                   0,
                                                   -1,
                                                   0,
                                                   0,
                                                   0,
                                                   0)) {
                        ok = 0;
                        break;
                    }
                }
            }
        }
        symtab_list_finalize(&constants);
    }

    if (ok) {
        qsort(ctx.symbols, (size_t)ctx.symbol_count, sizeof(xref_symbol), xref_symbol_compare);
        qsort(ctx.refs, (size_t)ctx.ref_count, sizeof(xref_ref), xref_ref_compare);

        if (format == XREF_FORMAT_JSON) {
            ok = emit_xref_json(filename, &ctx, include_data, source_file, output_file, pure_binary);
        } else if (format == XREF_FORMAT_TEXT) {
            ok = emit_xref_text(filename, &ctx);
        } else if (format == XREF_FORMAT_CSV) {
            ok = emit_xref_csv(filename, &ctx);
        } else {
            ok = 0;
        }
    }

    close_source_cache();
    free_xref_context(&ctx);
    return ok;
}

typedef struct tag_audit_label {
    char *name;
    int address;
} audit_label;

typedef struct tag_audit_equ {
    char *name;
    int value;
} audit_equ;

typedef struct tag_audit_finding {
    char *code;
    char *severity;
    location loc;
    int has_cpu_address;
    int cpu_address;
    char *expression;
    char *suggested_symbol;
    char *message;
} audit_finding;

typedef struct tag_audit_context {
    audit_label *labels;
    int label_count;
    int label_capacity;
    audit_equ *equs;
    int equ_count;
    int equ_capacity;
    audit_finding *findings;
    int finding_count;
    int finding_capacity;
    int level_error;
    int output_json;
    int rom_range_set;
    long rom_lo;
    long rom_hi;
} audit_context;

static int parse_hex_literal_strict(const char *s, int *out)
{
    const char *p;
    int v = 0;
    if (s == NULL || out == NULL) {
        return 0;
    }
    while (isspace((unsigned char)*s)) {
        s++;
    }
    if (*s != '$') {
        return 0;
    }
    p = s + 1;
    if (!isxdigit((unsigned char)*p)) {
        return 0;
    }
    while (*p != '\0' && !isspace((unsigned char)*p) && *p != ',' && *p != ';') {
        int c = (unsigned char)*p;
        if (!isxdigit(c)) {
            return 0;
        }
        if (c >= '0' && c <= '9') {
            v = (v * 16) + (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            v = (v * 16) + (c - 'a' + 10);
        } else {
            v = (v * 16) + (c - 'A' + 10);
        }
        p++;
    }
    while (isspace((unsigned char)*p)) {
        p++;
    }
    if (*p != '\0' && *p != ',' && *p != ';') {
        return 0;
    }
    *out = v;
    return 1;
}

static int parse_pointer_split_literal(const char *s, int *is_lo, int *target)
{
    const char *p = s;
    if (s == NULL || is_lo == NULL || target == NULL) {
        return 0;
    }
    while (isspace((unsigned char)*p)) {
        p++;
    }
    if (*p != '#') {
        return 0;
    }
    p++;
    while (isspace((unsigned char)*p)) {
        p++;
    }
    if (*p == '<') {
        *is_lo = 1;
    } else if (*p == '>') {
        *is_lo = 0;
    } else {
        return 0;
    }
    p++;
    while (isspace((unsigned char)*p)) {
        p++;
    }
    return parse_hex_literal_strict(p, target);
}

static int split_csv_operands(const char *s, char tokens[][128], int max_tokens)
{
    int count = 0;
    const char *p = s;
    while (p != NULL && *p != '\0' && count < max_tokens) {
        const char *start;
        const char *end;
        int len;
        while (isspace((unsigned char)*p)) {
            p++;
        }
        if (*p == '\0' || *p == ';') {
            break;
        }
        start = p;
        while (*p != '\0' && *p != ',' && *p != ';') {
            p++;
        }
        end = p;
        while (end > start && isspace((unsigned char)*(end - 1))) {
            end--;
        }
        len = (int)(end - start);
        if (len > 0) {
            if (len >= 128) {
                len = 127;
            }
            memcpy(tokens[count], start, (size_t)len);
            tokens[count][len] = '\0';
            count++;
        }
        if (*p == ',') {
            p++;
        } else if (*p == ';') {
            break;
        }
    }
    return count;
}

static int ensure_audit_label_capacity(audit_context *ctx)
{
    audit_label *tmp;
    int new_capacity;
    if (ctx->label_count < ctx->label_capacity) {
        return 1;
    }
    new_capacity = (ctx->label_capacity == 0) ? 32 : (ctx->label_capacity * 2);
    tmp = (audit_label *)realloc(ctx->labels, (size_t)new_capacity * sizeof(audit_label));
    if (tmp == NULL) {
        return 0;
    }
    ctx->labels = tmp;
    ctx->label_capacity = new_capacity;
    return 1;
}

static int ensure_audit_equ_capacity(audit_context *ctx)
{
    audit_equ *tmp;
    int new_capacity;
    if (ctx->equ_count < ctx->equ_capacity) {
        return 1;
    }
    new_capacity = (ctx->equ_capacity == 0) ? 32 : (ctx->equ_capacity * 2);
    tmp = (audit_equ *)realloc(ctx->equs, (size_t)new_capacity * sizeof(audit_equ));
    if (tmp == NULL) {
        return 0;
    }
    ctx->equs = tmp;
    ctx->equ_capacity = new_capacity;
    return 1;
}

static int ensure_audit_finding_capacity(audit_context *ctx)
{
    audit_finding *tmp;
    int new_capacity;
    if (ctx->finding_count < ctx->finding_capacity) {
        return 1;
    }
    new_capacity = (ctx->finding_capacity == 0) ? 32 : (ctx->finding_capacity * 2);
    tmp = (audit_finding *)realloc(ctx->findings, (size_t)new_capacity * sizeof(audit_finding));
    if (tmp == NULL) {
        return 0;
    }
    ctx->findings = tmp;
    ctx->finding_capacity = new_capacity;
    return 1;
}

static int add_audit_label(audit_context *ctx, const char *name, int address)
{
    audit_label *l;
    if (!ensure_audit_label_capacity(ctx)) {
        return 0;
    }
    l = &ctx->labels[ctx->label_count++];
    l->name = xstrdup(name);
    l->address = address;
    return l->name != NULL;
}

static int add_audit_equ(audit_context *ctx, const char *name, int value)
{
    audit_equ *e;
    if (!ensure_audit_equ_capacity(ctx)) {
        return 0;
    }
    e = &ctx->equs[ctx->equ_count++];
    e->name = xstrdup(name);
    e->value = value;
    return e->name != NULL;
}

static const char *audit_find_label_by_address(const audit_context *ctx, int address)
{
    int i;
    for (i = 0; i < ctx->label_count; ++i) {
        if ((ctx->labels[i].address & 0xFFFF) == (address & 0xFFFF)) {
            return ctx->labels[i].name;
        }
    }
    return NULL;
}

static const char *audit_find_equ_by_value(const audit_context *ctx, int value)
{
    int i;
    for (i = 0; i < ctx->equ_count; ++i) {
        if (ctx->equs[i].value >= 0 && ctx->equs[i].value < 0x100
            && ctx->equs[i].value == value) {
            return ctx->equs[i].name;
        }
    }
    return NULL;
}

static int audit_line_has_ignore_code(const char *comment, const char *code)
{
    const char *p;
    if (comment == NULL || code == NULL) {
        return 0;
    }
    p = strstr(comment, "xasm:audit-ignore");
    if (p == NULL) {
        return 0;
    }
    p += strlen("xasm:audit-ignore");
    while (*p != '\0') {
        char token[16];
        int n = 0;
        while (*p != '\0' && (isspace((unsigned char)*p) || *p == ',')) {
            p++;
        }
        while (*p != '\0' && !isspace((unsigned char)*p) && *p != ',') {
            if (n < (int)sizeof(token) - 1) {
                token[n++] = *p;
            }
            p++;
        }
        token[n] = '\0';
        if (n > 0 && strcmp(token, code) == 0) {
            return 1;
        }
        if (*p == '\0') {
            break;
        }
    }
    return 0;
}

static int audit_is_suppressed(location loc, const char *code)
{
    FILE *fp;
    char line[1024];
    int line_no = 0;
    int disabled = 0;
    if (loc.file == NULL || loc.first_line <= 0) {
        return 0;
    }
    fp = fopen(loc.file, "r");
    if (fp == NULL) {
        return 0;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        const char *comment = strchr(line, ';');
        line_no++;
        if (line_no == loc.first_line) {
            if (comment != NULL && audit_line_has_ignore_code(comment + 1, code)) {
                fclose(fp);
                return 1;
            }
            if (disabled) {
                fclose(fp);
                return 1;
            }
            break;
        }
        if (comment != NULL) {
            if (strstr(comment + 1, "xasm:audit-disable") != NULL) {
                disabled = 1;
            }
            if (strstr(comment + 1, "xasm:audit-enable") != NULL) {
                disabled = 0;
            }
        }
    }
    fclose(fp);
    return 0;
}

static int add_audit_finding(audit_context *ctx,
                             const char *code,
                             location loc,
                             int has_cpu_address,
                             int cpu_address,
                             const char *expression,
                             const char *suggested_symbol,
                             const char *message)
{
    audit_finding *f;
    if (audit_is_suppressed(loc, code)) {
        return 1;
    }
    if (!ensure_audit_finding_capacity(ctx)) {
        return 0;
    }
    f = &ctx->findings[ctx->finding_count++];
    memset(f, 0, sizeof(*f));
    f->code = xstrdup(code);
    f->severity = xstrdup(ctx->level_error ? "error" : "warning");
    f->loc = loc;
    f->has_cpu_address = has_cpu_address;
    f->cpu_address = cpu_address;
    f->expression = xstrdup(expression != NULL ? expression : "");
    f->suggested_symbol = xstrdup(suggested_symbol != NULL ? suggested_symbol : "");
    f->message = xstrdup(message != NULL ? message : "");
    return f->code != NULL && f->severity != NULL && f->expression != NULL
        && f->suggested_symbol != NULL && f->message != NULL;
}

static void build_audit_symbol_maps(audit_context *ctx)
{
    symbol_ident_list labels;
    symbol_ident_list constants;
    int i;
    symtab_list_type(LABEL_SYMBOL, &labels);
    for (i = 0; i < labels.size; ++i) {
        symtab_entry *e = symtab_lookup(labels.idents[i]);
        if (e != NULL && (e->flags & ADDR_FLAG)) {
            add_audit_label(ctx, e->id, e->address);
        }
    }
    symtab_list_finalize(&labels);

    symtab_list_type(CONSTANT_SYMBOL, &constants);
    for (i = 0; i < constants.size; ++i) {
        symtab_entry *e = symtab_lookup(constants.idents[i]);
        int value;
        if (e == NULL || !(e->flags & EQU_FLAG) || e->def == NULL) {
            continue;
        }
        if (eval_expression_int(e->def, &value, 0)) {
            add_audit_equ(ctx, e->id, value);
        }
    }
    symtab_list_finalize(&constants);
}

static int audit_visit_dataseg(astnode *n, void *arg, astnode **next)
{
    (void)n;
    (void)arg;
    (void)next;
    in_dataseg = 1;
    return 0;
}

static int audit_visit_codeseg(astnode *n, void *arg, astnode **next)
{
    (void)n;
    (void)arg;
    (void)next;
    in_dataseg = 0;
    return 0;
}

static int audit_visit_org(astnode *org, void *arg, astnode **next)
{
    int addr = get_current_pc();
    (void)arg;
    (void)next;
    if (eval_expression_int(LHS(org), &addr, 0)) {
        set_current_pc(addr);
    }
    return 0;
}

static int audit_visit_instruction(astnode *instr, void *arg, astnode **next)
{
    audit_context *ctx = (audit_context *)arg;
    const char *opcode = opcode_to_string(instr->instr.opcode);
    int len = opcode_length(instr->instr.opcode);
    int addr = get_current_pc();
    char operand[256];
    int value;
    const char *label_name;
    const char *equ_name;
    int is_lo;
    int target;
    astnode *operand_expr = LHS(instr);
    (void)next;

    if (len < 1) {
        len = 1;
    }
    extract_operand_from_line(instr->loc, operand, sizeof(operand));

    if ((strcmp(opcode, "JSR") == 0 || strcmp(opcode, "JMP") == 0)
        && parse_hex_literal_strict(operand, &value)) {
        label_name = audit_find_label_by_address(ctx, value);
        if (label_name != NULL) {
            add_audit_finding(ctx,
                              "A100",
                              instr->loc,
                              1,
                              addr,
                              operand,
                              label_name,
                              "raw control-flow target");
        }
    }

    if (parse_pointer_split_literal(operand, &is_lo, &target)) {
        label_name = audit_find_label_by_address(ctx, target);
        if (label_name != NULL) {
            add_audit_finding(ctx,
                              "A110",
                              instr->loc,
                              1,
                              addr,
                              operand,
                              label_name,
                              "raw pointer immediate split");
        }
    }

    if (instr->instr.mode != IMMEDIATE_MODE
        && (strncmp(opcode, "B", 1) != 0)
        && parse_hex_literal_strict(operand, &value)
        && value >= 0 && value < 0x100) {
        equ_name = audit_find_equ_by_value(ctx, value);
        if (equ_name != NULL) {
            location use_loc = instr->loc;
            if (operand_expr != NULL) {
                use_loc = operand_expr->loc;
            }
            add_audit_finding(ctx,
                              "A120",
                              use_loc,
                              1,
                              addr,
                              operand,
                              equ_name,
                              "raw low-address operand");
        }
    }

    add_current_pc(len);
    return 0;
}

static int audit_visit_data(astnode *data, void *arg, astnode **next)
{
    audit_context *ctx = (audit_context *)arg;
    astnode *expr;
    int addr = get_current_pc();
    int bytes_per_item;
    int expr_index = 0;
    char operands[32][128];
    int operand_count;
    char operand_text[512];
    (void)next;

    switch (LHS(data)->datatype) {
        case BYTE_DATATYPE:
        case CHAR_DATATYPE:
            bytes_per_item = 1;
            break;
        case WORD_DATATYPE:
            bytes_per_item = 2;
            break;
        case DWORD_DATATYPE:
            bytes_per_item = 4;
            break;
        default:
            bytes_per_item = 1;
            break;
    }

    extract_operand_from_line(data->loc, operand_text, sizeof(operand_text));
    operand_count = split_csv_operands(operand_text, operands, 32);

    if (LHS(data)->datatype == WORD_DATATYPE) {
        for (expr = RHS(data); expr != NULL; expr = astnode_get_next_sibling(expr), ++expr_index) {
            int value;
            const char *label_name;
            if (expr_index >= operand_count) {
                continue;
            }
            if (!parse_hex_literal_strict(operands[expr_index], &value)) {
                continue;
            }
            label_name = audit_find_label_by_address(ctx, value);
            if (label_name != NULL) {
                add_audit_finding(ctx,
                                  "A130",
                                  expr->loc,
                                  1,
                                  addr + (expr_index * 2),
                                  operands[expr_index],
                                  label_name,
                                  "raw .DW target");
            }
        }
    } else if (LHS(data)->datatype == BYTE_DATATYPE || LHS(data)->datatype == CHAR_DATATYPE) {
        int byte_index = 0;
        astnode *expr_list[64];
        int expr_count = 0;
        for (expr = RHS(data); expr != NULL && expr_count < 64; expr = astnode_get_next_sibling(expr)) {
            expr_list[expr_count++] = expr;
        }
        for (byte_index = 0; byte_index + 1 < expr_count && (byte_index + 1) < operand_count; byte_index += 2) {
            int lo;
            int hi;
            int target;
            const char *label_name;
            char pair_expr[300];
            if (!parse_hex_literal_strict(operands[byte_index], &lo)
                || !parse_hex_literal_strict(operands[byte_index + 1], &hi)) {
                continue;
            }
            if (lo < 0 || lo > 0xFF || hi < 0 || hi > 0xFF) {
                continue;
            }
            target = lo | (hi << 8);
            if (ctx->rom_range_set && ((long)target < ctx->rom_lo || (long)target > ctx->rom_hi)) {
                continue;
            }
            label_name = audit_find_label_by_address(ctx, target);
            if (label_name == NULL) {
                continue;
            }
            snprintf(pair_expr, sizeof(pair_expr), "%s,%s", operands[byte_index], operands[byte_index + 1]);
            add_audit_finding(ctx,
                              "A131",
                              expr_list[byte_index]->loc,
                              1,
                              addr + byte_index,
                              pair_expr,
                              label_name,
                              "raw .DB pointer pair");
        }
    }

    add_current_pc((astnode_get_child_count(data) - 1) * bytes_per_item);
    return 0;
}

static int audit_visit_storage(astnode *storage, void *arg, astnode **next)
{
    int count = 0;
    (void)arg;
    (void)next;
    if (eval_expression_int(RHS(storage), &count, 0) && count > 0) {
        add_current_pc(count);
    }
    return 0;
}

static int audit_visit_binary(astnode *node, void *arg, astnode **next)
{
    (void)arg;
    (void)next;
    add_current_pc(node->binary.size);
    return 0;
}

static void free_audit_context(audit_context *ctx)
{
    int i;
    for (i = 0; i < ctx->label_count; ++i) {
        free(ctx->labels[i].name);
    }
    free(ctx->labels);
    ctx->labels = NULL;
    ctx->label_count = 0;
    ctx->label_capacity = 0;

    for (i = 0; i < ctx->equ_count; ++i) {
        free(ctx->equs[i].name);
    }
    free(ctx->equs);
    ctx->equs = NULL;
    ctx->equ_count = 0;
    ctx->equ_capacity = 0;

    for (i = 0; i < ctx->finding_count; ++i) {
        free(ctx->findings[i].code);
        free(ctx->findings[i].severity);
        free(ctx->findings[i].expression);
        free(ctx->findings[i].suggested_symbol);
        free(ctx->findings[i].message);
    }
    free(ctx->findings);
    ctx->findings = NULL;
    ctx->finding_count = 0;
    ctx->finding_capacity = 0;
}

static void emit_audit_text(const audit_context *ctx)
{
    int i;
    for (i = 0; i < ctx->finding_count; ++i) {
        const audit_finding *f = &ctx->findings[i];
        fprintf(stderr, "%s:%d:%d: %s %s: %s '%s' can use symbol '%s'\n",
                f->loc.file != NULL ? f->loc.file : "",
                f->loc.first_line,
                f->loc.first_column,
                f->severity,
                f->code,
                f->message,
                f->expression,
                f->suggested_symbol);
    }
}

static void emit_audit_json(const audit_context *ctx)
{
    int i;
    printf("{\n");
    printf("  \"version\": \"1\",\n");
    printf("  \"findings\": [");
    for (i = 0; i < ctx->finding_count; ++i) {
        const audit_finding *f = &ctx->findings[i];
        if (i > 0) {
            printf(",");
        }
        printf("\n    {");
        printf("\"code\":");
        print_json_string(stdout, f->code);
        printf(",\"severity\":");
        print_json_string(stdout, f->severity);
        printf(",\"file\":");
        print_json_string(stdout, f->loc.file != NULL ? f->loc.file : "");
        printf(",\"line\":%d", f->loc.first_line);
        printf(",\"column\":%d", f->loc.first_column);
        printf(",\"cpu_address\":");
        if (f->has_cpu_address) {
            char hex[16];
            snprintf(hex, sizeof(hex), "0x%04X", f->cpu_address & 0xFFFF);
            print_json_string(stdout, hex);
        } else {
            printf("null");
        }
        printf(",\"expression\":");
        print_json_string(stdout, f->expression);
        printf(",\"suggested_symbol\":");
        if (f->suggested_symbol != NULL && f->suggested_symbol[0] != '\0') {
            print_json_string(stdout, f->suggested_symbol);
        } else {
            printf("null");
        }
        printf(",\"message\":");
        print_json_string(stdout, f->message);
        printf("}");
    }
    if (ctx->finding_count > 0) {
        printf("\n");
    }
    printf("  ]\n");
    printf("}\n");
}

int run_raw_address_audit(astnode *root,
                          int level_error,
                          int output_json,
                          int rom_range_set,
                          long rom_lo,
                          long rom_hi)
{
    static astnodeprocmap map[] = {
        { DATASEG_NODE, audit_visit_dataseg },
        { CODESEG_NODE, audit_visit_codeseg },
        { ORG_NODE, audit_visit_org },
        { INSTRUCTION_NODE, audit_visit_instruction },
        { DATA_NODE, audit_visit_data },
        { STORAGE_NODE, audit_visit_storage },
        { BINARY_NODE, audit_visit_binary },
        { STRUC_DECL_NODE, list_noop },
        { UNION_DECL_NODE, list_noop },
        { ENUM_DECL_NODE, list_noop },
        { RECORD_DECL_NODE, list_noop },
        { MACRO_NODE, list_noop },
        { MACRO_DECL_NODE, list_noop },
        { PROC_NODE, list_noop },
        { REPT_NODE, list_noop },
        { WHILE_NODE, list_noop },
        { DO_NODE, list_noop },
        { EXITM_NODE, list_noop },
        { PUSH_MACRO_BODY_NODE, list_noop },
        { POP_MACRO_BODY_NODE, list_noop },
        { PUSH_BRANCH_SCOPE_NODE, list_noop },
        { POP_BRANCH_SCOPE_NODE, list_noop },
        { UNDEF_NODE, list_noop },
        { TOMBSTONE_NODE, list_noop },
        { 0, NULL }
    };
    audit_context ctx;

    if (root == NULL) {
        return 0;
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.level_error = level_error ? 1 : 0;
    ctx.output_json = output_json ? 1 : 0;
    ctx.rom_range_set = rom_range_set ? 1 : 0;
    ctx.rom_lo = rom_lo;
    ctx.rom_hi = rom_hi;

    build_audit_symbol_maps(&ctx);

    in_dataseg = 0;
    dataseg_pc = 0;
    codeseg_pc = 0;
    close_source_cache();
    astproc_walk(root, &ctx, map);
    close_source_cache();

    if (ctx.output_json) {
        emit_audit_json(&ctx);
    } else {
        emit_audit_text(&ctx);
    }

    {
        int findings = ctx.finding_count;
        free_audit_context(&ctx);
        return findings;
    }
}
