/* Inject symbol-table allocation failures without changing the production API. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../symtab.h"
static void check_scope_cleanup(void)
{
    if (symtab_tos() != NULL) {
        fprintf(stderr, "active symbol scope at exit\n");
        abort();
    }
}
static int test_fail_index = -1, test_allocations, test_configured;
void test_symtab_fail_after(int index)
{
    test_configured = 1;
    test_fail_index = index;
    test_allocations = 0;
}
static int test_symtab_allocation_fails(const char *function)
{
    const char *filter = getenv("XASM_TEST_SYMTAB_FUNCTION");
    if (!test_configured) {
        if (getenv("XASM_TEST_EMPTY_STACK_AT_EXIT")) atexit(check_scope_cleanup);
        const char *index = getenv("XASM_TEST_SYMTAB_FAIL");
        test_symtab_fail_after(index == NULL ? -1 : atoi(index));
    }
    if (filter != NULL && strcmp(filter, function) != 0) return 0;
    if (test_allocations++ != test_fail_index) return 0;
    fprintf(stderr, "INJECT_SYMTAB %s\n", function);
    return 1;
}
static void *test_symtab_malloc(size_t size, const char *function)
{
    return test_symtab_allocation_fails(function) ? NULL : malloc(size);
}
static void *test_symtab_realloc(void *pointer, size_t size, const char *function)
{
    return test_symtab_allocation_fails(function) ? NULL : realloc(pointer, size);
}
#define malloc(size) test_symtab_malloc(size, __func__)
#define realloc(pointer, size) test_symtab_realloc(pointer, size, __func__)
#include "../symtab.c"
