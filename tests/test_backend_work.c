/* Test-only wrappers around real AST operations and the assembler entry point. */
#if defined(TEST_AST_WORK)
#include <assert.h>
#include <stdlib.h>
#include "../astnode.h"

unsigned long test_child_index_nodes;
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

#else
#define main assembler_main
#include "../xasm.c"
#undef main

extern unsigned long test_child_index_nodes;
int test_remove_ast_nodes(int count);
int test_indexed_ast_removal(void);

int main(int argc, char **argv)
{
    int result;
    if (argc == 3 && strcmp(argv[1], "--test-ast-removal") == 0)
        result = test_remove_ast_nodes(atoi(argv[2]));
    else if (argc == 2 && strcmp(argv[1], "--test-indexed-removal") == 0)
        result = test_indexed_ast_removal();
    else
        result = assembler_main(argc, argv);
    fprintf(stderr, "BACKEND_WORK index_nodes=%lu\n", test_child_index_nodes);
    return result;
}
#endif
