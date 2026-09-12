/* Exercise every allocation failure in the NL collector and writer. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int allocation_count;
static int fail_at;

static int fail_allocation(void)
{
    return allocation_count++ == fail_at;
}
static void *fault_malloc(size_t size)
{
    return fail_allocation() ? NULL : malloc(size);
}
static void *fault_calloc(size_t count, size_t size)
{
    return fail_allocation() ? NULL : calloc(count, size);
}
static void *fault_realloc(void *ptr, size_t size)
{
    return fail_allocation() ? NULL : realloc(ptr, size);
}

#define malloc fault_malloc
#define calloc fault_calloc
#define realloc fault_realloc
#include "../output_file.c"
#include "../fceux_nl.c"
#undef malloc
#undef calloc
#undef realloc

static int attempt_export(const char *prefix, const char *ram)
{
    fceux_nl_table table = {0};
    fceux_nl_output_plan plan = {0};
    int i, ok = 1;
    for (i = 0; i < 70 && ok; i++) {
        char *name = fault_malloc(32);
        if (name != NULL) snprintf(name, 32, "Symbol%d", i);
        ok = fceux_nl_add(&table, i % 3 - 1, i % 3 == 0 ? i : 0x8000 + i, name);
    }
    if (ok) ok = fceux_nl_plan_outputs(&plan, prefix, ram, 2);
    if (ok) ok = fceux_nl_write(&table, &plan);
    fceux_nl_free_outputs(&plan);
    fceux_nl_free(&table);
    return ok;
}

int main(int argc, char **argv)
{
    int total, failure;
    assert(argc == 3);
    fail_at = -1;
    allocation_count = 0;
    assert(attempt_export(argv[1], argv[2]));
    total = allocation_count;
    for (failure = 0; failure < total; failure++) {
        allocation_count = 0;
        fail_at = failure;
        assert(!attempt_export(argv[1], argv[2]));
    }
    printf("Checked %d allocation failure sites\n", total);
    return 0;
}
