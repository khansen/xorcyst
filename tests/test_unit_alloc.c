#include <stdio.h>
#include <stdlib.h>
static int allocation_index;
static int fail_unit_allocation(void)
{
    const char *index = getenv("XLNK_TEST_UNIT_FAIL");
    if (index == NULL || allocation_index++ != atoi(index)) return 0;
    fprintf(stderr, "INJECT_UNIT\n");
    return 1;
}
static void *unit_malloc(size_t size) { return fail_unit_allocation() ? NULL : malloc(size); }
static void *unit_calloc(size_t count, size_t size) { return fail_unit_allocation() ? NULL : calloc(count, size); }
#define malloc unit_malloc
#define calloc unit_calloc
#include "../unit.c"
#undef malloc
#undef calloc
#undef SAFE_FREE

static void *hash_malloc(size_t size)
{
    static int index;
    const char *fail = getenv("XLNK_TEST_HASH_FAIL");
    if (fail != NULL && index++ == atoi(fail)) {
        fprintf(stderr, "INJECT_HASH\n");
        return NULL;
    }
    return malloc(size);
}
#define malloc hash_malloc
#include "../hashtab.c"
