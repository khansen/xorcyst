/* Fail a real list-node allocation; CLI fixtures target the parser root. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../astnode.h"

/* Optional live-node accounting catches ownership leaks even without LSan. */
static void **live_nodes;
static size_t live_count, live_capacity;
static int track_nodes = -1;

static void report_live_nodes(void)
{
    fprintf(stderr, "AST_NODES_LIVE=%zu\n", live_count);
    free(live_nodes);
}

static void remember_node(void *node)
{
    if (track_nodes < 0) {
        track_nodes = getenv("XASM_TEST_TRACK_NODES") != NULL;
        if (track_nodes) atexit(report_live_nodes);
    }
    if (!track_nodes || node == NULL) return;
    if (live_count == live_capacity) {
        size_t capacity = live_capacity ? live_capacity * 2 : 64;
        void **grown = realloc(live_nodes, capacity * sizeof(*grown));
        if (grown == NULL) abort();
        live_nodes = grown;
        live_capacity = capacity;
    }
    live_nodes[live_count++] = node;
}

static void test_node_free(void *pointer)
{
    size_t i;
    if (track_nodes > 0) {
        for (i = 0; i < live_count; i++) {
            if (live_nodes[i] == pointer) {
                live_nodes[i] = live_nodes[--live_count];
                break;
            }
        }
    }
    free(pointer);
}

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
    void *node = calloc(count, size);
    remember_node(node);
    return node;
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
#define free(pointer) test_node_free(pointer)
#include "../astnode.c"
