/* Fail a real list-node allocation; CLI fixtures target the parser root. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../astnode.h"

static void *test_node_calloc(size_t count, size_t size, astnode_type type)
{
    static int instruction_allocations;
    const char *instruction_failure = getenv("XASM_TEST_INSTRUCTION_ALLOC_FAIL");
    if ((type == STRING_NODE || type == INTEGER_NODE) && getenv("XASM_TEST_VALUE_NODE_FAIL")) {
        fprintf(stderr, "INJECT_VALUE_NODE\n");
        return NULL;
    }
    if (type == LIST_NODE && getenv("XASM_TEST_LIST_ALLOC_FAIL") != NULL) {
        fprintf(stderr, "INJECT_LIST_ALLOC\n");
        return NULL;
    }
    if (type == INSTRUCTION_NODE && instruction_failure != NULL
        && instruction_allocations++ == atoi(instruction_failure)) {
        fprintf(stderr, "INJECT_INSTRUCTION_ALLOC\n");
        return NULL;
    }
    return calloc(count, size);
}

static void *test_node_malloc(size_t size, const char *function)
{
    if (getenv("XASM_TEST_STRING_TEXT_FAIL") && !strcmp(function, "astnode_create_string")) {
        fprintf(stderr, "INJECT_STRING_TEXT\n");
        return NULL;
    }
    return malloc(size);
}

#define malloc(size) test_node_malloc(size, __func__)
#define calloc(count, size) test_node_calloc(count, size, type)
#include "../astnode.c"
