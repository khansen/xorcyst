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
} xref_meta;

typedef struct tag_xref_build_context {
    xref_symbol *symbols;
    int symbol_count;
    int symbol_capacity;
    xref_ref *refs;
    int ref_count;
    int ref_capacity;
    int include_locals;
    int include_anon;
    int pure_binary;
    long output_offset;
} xref_build_context;

static int starts_with(const char *s, const char *prefix)
{
    size_t n;
    if (s == NULL || prefix == NULL) {
        return 0;
    }
    n = strlen(prefix);
    return strncmp(s, prefix, n) == 0;
}

static void classify_symbol_name(const char *name, const char **kind, const char **scope)
{
    if (name != NULL) {
        if (name[0] == '+' || name[0] == '-') {
            *kind = "anonymous_label";
            *scope = "anonymous";
            return;
        }
        if (strstr(name, "@@") != NULL) {
            *kind = "local_label";
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
        return 0;
    }
    ctx->refs = tmp;
    ctx->ref_capacity = new_capacity;
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
    if (r->symbol == NULL || r->opcode == NULL || r->addressing_mode == NULL
        || r->access == NULL || r->expression == NULL) {
        return 0;
    }
    return 1;
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
    (void)n;
    (void)arg;
    (void)next;
    in_dataseg = 1;
    return 0;
}

static int xref_visit_codeseg(astnode *n, void *arg, astnode **next)
{
    (void)n;
    (void)arg;
    (void)next;
    in_dataseg = 0;
    return 0;
}

static int xref_visit_org(astnode *org, void *arg, astnode **next)
{
    int addr = get_current_pc();
    (void)arg;
    (void)next;
    if (eval_expression_int(LHS(org), &addr, 0)) {
        set_current_pc(addr);
    }
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
                                   ctx->pure_binary && !in_dataseg,
                                   ctx->output_offset,
                                   0,
                                   0)) {
        return 0;
    }
    return 0;
}

static int xref_visit_instruction(astnode *instr, void *arg, astnode **next)
{
    xref_build_context *ctx = (xref_build_context *)arg;
    int addr = get_current_pc();
    int len = opcode_length(instr->instr.opcode);
    xref_meta meta;
    char expr_buf[512];
    (void)next;

    if (len < 1) {
        len = 1;
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

static int xref_visit_data(astnode *data, void *arg, astnode **next)
{
    xref_build_context *ctx = (xref_build_context *)arg;
    astnode *expr;
    int bytes_per_item;
    int addr = get_current_pc();
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
        memset(&meta, 0, sizeof(meta));
        meta.has_cpu_address = 1;
        meta.cpu_address = addr + (index * bytes_per_item);
        meta.has_output_offset = ctx->pure_binary && !in_dataseg;
        meta.output_offset = ctx->output_offset + (index * bytes_per_item);
        meta.opcode = NULL;
        meta.addressing_mode = NULL;
        meta.access = "address_compute";
        meta.expression = expr_buf;
        if (!collect_expr_refs_recursive(expr, ctx, &meta)) {
            return 0;
        }
        index++;
    }

    add_current_pc(index * bytes_per_item);
    if (ctx->pure_binary && !in_dataseg) {
        ctx->output_offset += index * bytes_per_item;
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
    if (lhs->has_output_offset != rhs->has_output_offset) {
        return rhs->has_output_offset - lhs->has_output_offset;
    }
    if (lhs->has_output_offset && rhs->has_output_offset && lhs->output_offset != rhs->output_offset) {
        return (lhs->output_offset < rhs->output_offset) ? -1 : 1;
    }
    return strcmp(lhs->symbol, rhs->symbol);
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
                          const char *source_file,
                          const char *output_file,
                          int pure_binary)
{
    FILE *fp;
    int i;
    char ts[64];
    fp = fopen(filename, "w");
    if (fp == NULL) {
        fprintf(stderr, "error: could not open `%s' for writing\n", filename);
        return 0;
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
        fprintf(fp, "}");
    }
    if (ctx->ref_count > 0) {
        fprintf(fp, "\n  ");
    }
    fprintf(fp, "]\n");
    fprintf(fp, "}\n");
    fclose(fp);
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

int generate_xref(astnode *root,
                  const char *filename,
                  xref_format format,
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
        { 0, NULL }
    };
    xref_build_context ctx;
    symbol_ident_list constants;
    int i;
    int ok = 1;

    memset(&ctx, 0, sizeof(ctx));
    ctx.include_locals = include_locals;
    ctx.include_anon = include_anon;
    ctx.pure_binary = pure_binary;
    ctx.output_offset = 0;

    in_dataseg = 0;
    dataseg_pc = 0;
    codeseg_pc = 0;
    close_source_cache();
    astproc_walk(root, &ctx, map);

    symtab_list_type(CONSTANT_SYMBOL, &constants);
    for (i = 0; i < constants.size; ++i) {
        symtab_entry *e = symtab_lookup(constants.idents[i]);
        int value = 0;
        if (e == NULL) {
            continue;
        }
        if (e->flags & VOLATILE_FLAG) {
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

    if (ok) {
        qsort(ctx.symbols, (size_t)ctx.symbol_count, sizeof(xref_symbol), xref_symbol_compare);
        qsort(ctx.refs, (size_t)ctx.ref_count, sizeof(xref_ref), xref_ref_compare);

        if (format == XREF_FORMAT_JSON) {
            ok = emit_xref_json(filename, &ctx, source_file, output_file, pure_binary);
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
