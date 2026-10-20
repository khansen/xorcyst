/* Test-only wrappers around real AST operations, JSON writes, and main. */
#if defined(TEST_AST_WORK)
#include <assert.h>
#include <stdlib.h>
#include "../astnode.h"

unsigned long test_child_index_nodes;
unsigned long test_list_nodes;
int test_real_child_index(const astnode *parent, const astnode *child);

int astnode_get_child_index(const astnode *parent, const astnode *child)
{
    const astnode *node;
    /* Count the prefix requested by each real index lookup, including misses. */
    if (parent != NULL && child != NULL) {
        for (node = parent->first_child; node != NULL; node = node->next_sibling) {
            test_child_index_nodes++;
            if (node == child) break;
        }
    }
    return test_real_child_index(parent, child);
}

/* The test build renames only the index function's definition. Calls from
   production AST code still go through the counting wrapper above. */
#include TEST_AST_SOURCE

static void assert_children(astnode *parent, astnode **nodes, int count)
{
    astnode *actual = parent->first_child, *previous = NULL;
    int i;
    for (i = 0; i < count; i++) {
        if (nodes[i] == NULL) continue;
        assert(actual == nodes[i]);
        assert(actual->parent == parent && actual->prev_sibling == previous);
        previous = actual;
        actual = actual->next_sibling;
    }
    assert(actual == NULL);
}

int test_remove_ast_nodes(int count)
{
    location loc = {0};
    astnode **nodes = (astnode **)calloc((size_t)count, sizeof(*nodes));
    astnode *parent = astnode_create_list(NULL);
    int i;
    assert(nodes != NULL && parent != NULL);
    for (i = 0; i < count; i++) {
        nodes[i] = astnode_create_list(astnode_create_integer(i, loc));
        astnode_add_child(parent, nodes[i]);
    }
    astnode_remove(NULL);
    astnode_finalize(NULL); /* Missing expression results are safe to release. */
    astnode_remove(parent);
    assert_children(parent, nodes, count);
    test_child_index_nodes = 0;
    /* Multiplication by an odd number permutes a power-of-two-sized list. */
    for (i = 0; i < count; i++) {
        int index = (i * 73) % count;
        astnode *removed = nodes[index];
        astnode_remove(removed);
        assert(removed->parent == NULL && removed->prev_sibling == NULL
               && removed->next_sibling == NULL);
        assert(removed->first_child->parent == removed);
        assert(removed->first_child->integer == index);
        nodes[index] = NULL;
        assert_children(parent, nodes, count);
        astnode_remove(removed); /* Detached nodes are harmless to remove again. */
        astnode_finalize(removed);
    }
    astnode_finalize(parent);
    free(nodes);
    return 0;
}

int test_indexed_ast_removal(void)
{
    astnode *parent = astnode_create_list(NULL);
    astnode *other = astnode_create_list(NULL);
    astnode *nodes[5], *removed;
    int order[] = {2, 0, 4, 1, 3};
    int positions[] = {2, 0, 2, 0, 0};
    int i;
    for (i = 0; i < 5; i++) {
        nodes[i] = astnode_create_list(NULL);
        astnode_add_child(parent, nodes[i]);
    }
    assert(astnode_remove_child(NULL, nodes[0]) == -1);
    assert(astnode_remove_child(parent, NULL) == -1);
    assert(astnode_remove_child(other, nodes[2]) == -1);
    assert_children(parent, nodes, 5);
    for (i = 0; i < 5; i++) {
        removed = nodes[order[i]];
        if (i == 3)
            assert(astnode_remove_child_at(parent, positions[i]) == removed);
        else
            assert(astnode_remove_child(parent, removed) == positions[i]);
        nodes[order[i]] = NULL;
        assert(removed->parent == NULL && removed->prev_sibling == NULL
               && removed->next_sibling == NULL);
        assert(astnode_remove_child(parent, removed) == -1);
        assert_children(parent, nodes, 5);
        astnode_finalize(removed);
    }
    /* An unparented sibling chain is also unchanged by astnode_remove(). */
    astnode_add_sibling(parent, other);
    astnode_remove(other);
    assert(parent->next_sibling == other && other->prev_sibling == parent);
    parent->next_sibling = other->prev_sibling = NULL;
    astnode_finalize(parent);
    astnode_finalize(other);
    return 0;
}

int test_ast_splicing(void)
{
    location loc = {0};
    int position, length, operation;
    for (operation = 0; operation < 3; operation++) {
        for (position = 0; position < 3; position++) {
            for (length = 0; length <= 3; length++) {
                astnode *parent = astnode_create_list(NULL), *original[3], *expected[6];
                astnode *replacement = NULL, *added[3], *old;
                int i, used = 0;
                for (i = 0; i < 3; i++) {
                    original[i] = astnode_create_list(astnode_create_integer(i, loc));
                    original[i]->analysis_origin_id = 100 + i;
                    astnode_add_child(parent, original[i]);
                }
                for (i = 0; i < length; i++) {
                    added[i] = astnode_create_list(astnode_create_integer(10 + i, loc));
                    if (replacement == NULL) replacement = added[i];
                    else astnode_add_sibling(replacement, added[i]);
                }
                old = original[position];
                if (operation == 0) {
                    astnode_replace(old, old); /* Identity leaves the chain intact. */
                    assert_children(parent, original, 3);
                    astnode_replace(old, replacement);
                    assert(old->parent == NULL && old->next_sibling == NULL && old->prev_sibling == NULL);
                    assert(old->first_child->parent == old && old->first_child->integer == position);
                    if (replacement != NULL) assert(replacement->analysis_origin_id == old->analysis_origin_id);
                } else if (operation == 1) {
                    astnode_insert_child(parent, replacement, position);
                } else {
                    astnode_add_sibling_after(old, replacement);
                }
                for (i = 0; i < 3; i++) {
                    int j;
                    if (i == position && operation != 2)
                        for (j = 0; j < length; j++) expected[used++] = added[j];
                    if (i != position || operation != 0) expected[used++] = original[i];
                    if (i == position && operation == 2)
                        for (j = 0; j < length; j++) expected[used++] = added[j];
                }
                assert_children(parent, expected, used);
                for (i = 0; i < length; i++) {
                    assert(added[i]->first_child->parent == added[i]);
                    assert(added[i]->first_child->integer == 10 + i);
                }
                if (operation == 0) astnode_finalize(old);
                astnode_finalize(parent);
            }
        }
    }
    return 0;
}

#elif defined(TEST_JSON_WORK)
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

unsigned long test_json_stdio_calls;

static int counted_json_fputc(int c, FILE *fp)
{
    test_json_stdio_calls++;
    return fputc(c, fp);
}

static int counted_json_fputs(const char *s, FILE *fp)
{
    test_json_stdio_calls++;
    return fputs(s, fp);
}

static size_t counted_json_fwrite(const void *p, size_t size, size_t count, FILE *fp)
{
    test_json_stdio_calls++;
    return fwrite(p, size, count, fp);
}

static int counted_json_fprintf(FILE *fp, const char *format, ...)
{
    int result;
    va_list args;
    test_json_stdio_calls++;
    va_start(args, format);
    result = vfprintf(fp, format, args);
    va_end(args);
    return result;
}

#define fputc counted_json_fputc
#define fputs counted_json_fputs
#define fwrite counted_json_fwrite
#define fprintf counted_json_fprintf
#include "../listing.c"
#undef fputc
#undef fputs
#undef fwrite
#undef fprintf

int test_write_json_string(const char *length_arg)
{
    size_t length;
    char *bytes;
    if (strcmp(length_arg, "null") == 0) {
        print_json_string_n(stdout, NULL, 17);
        return 0;
    }
    length = (size_t)strtoul(length_arg, NULL, 10);
    /* Deliberately no NUL terminator: the serializer must respect length. */
    bytes = (char *)malloc(length != 0 ? length : 1);
    if (bytes == NULL || fread(bytes, 1, length, stdin) != length) {
        free(bytes);
        return 1;
    }
    print_json_string_n(stdout, bytes, length);
    free(bytes);
    return 0;
}

int test_xref_reference_order(void)
{
    xref_build_context ctx = {0};
    xref_meta meta = {0};
    xref_ref original[8];
    const char *files[] = {"b.asm", "a.asm", "a.asm", "a.asm", "a.asm", NULL, "", "a.asm"};
    const int lines[] = {1, 20, 2, 2, 2, 9, 9, 1};
    const int columns[] = {1, 3, 18, 11, 11, 1, 1, 1};
    const int expected[] = {5, 6, 7, 3, 4, 2, 1, 0};
    int i, j, shift, result = 1;
    meta.has_cpu_address = meta.has_output_offset = 1;
    meta.cpu_address = 0x8000;
    for (i = 0; i < 8; i++) {
        location loc = {0};
        char expression[16];
        loc.file = files[i];
        loc.first_line = lines[i];
        loc.first_column = columns[i];
        snprintf(expression, sizeof(expression), "use-%d", i);
        meta.expression = expression;
        if (!add_xref_reference(&ctx, "Target", &loc, &meta)) goto done;
    }
    memcpy(original, ctx.refs, sizeof(original));
    /* Compare every pair, including exact-location ties, independently of
     * the platform's qsort behavior. No internal ordinal is set by the test. */
    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
            int cmp = xref_ref_compare(&original[expected[i]], &original[expected[j]]);
            if ((cmp > 0) - (cmp < 0) != (i > j) - (i < j)) goto done;
        }
    }
    for (shift = 0; shift < 8; shift++) {
        for (i = 0; i < 8; i++) ctx.refs[i] = original[(15 - i + shift) % 8];
        qsort(ctx.refs, 8, sizeof(*ctx.refs), xref_ref_compare);
        for (i = 0; i < 8; i++)
            if (strcmp(ctx.refs[i].expression, original[expected[i]].expression) != 0) goto done;
    }
    result = 0;
done:
    if (result) fprintf(stderr, "xref reference tie ordering failed\n");
    free_xref_context(&ctx);
    return result;
}

#else
#define main assembler_main
#include "../xasm.c"
#undef main

extern unsigned long test_child_index_nodes;
extern unsigned long test_list_nodes;
extern unsigned long test_json_stdio_calls;
unsigned long test_constant_expansions;
int test_remove_ast_nodes(int count);
int test_indexed_ast_removal(void);
int test_ast_splicing(void);
int test_write_json_string(const char *length_arg);
int test_xref_reference_order(void);

int main(int argc, char **argv)
{
    int result;
    if (argc == 3 && strcmp(argv[1], "--test-ast-removal") == 0)
        result = test_remove_ast_nodes(atoi(argv[2]));
    else if (argc == 2 && strcmp(argv[1], "--test-indexed-removal") == 0)
        result = test_indexed_ast_removal();
    else if (argc == 2 && strcmp(argv[1], "--test-ast-splicing") == 0)
        result = test_ast_splicing();
    else if (argc == 3 && strcmp(argv[1], "--test-json-string") == 0)
        result = test_write_json_string(argv[2]);
    else if (argc == 2 && strcmp(argv[1], "--test-xref-reference-order") == 0)
        result = test_xref_reference_order();
    else
        result = assembler_main(argc, argv);
    fprintf(stderr, "BACKEND_WORK index_nodes=%lu json_calls=%lu\n",
            test_child_index_nodes, test_json_stdio_calls);
    fprintf(stderr, "CONSTANT_WORK expansions=%lu\n", test_constant_expansions);
    fprintf(stderr, "LIST_WORK nodes=%lu\n", test_list_nodes);
    return result;
}
#endif
