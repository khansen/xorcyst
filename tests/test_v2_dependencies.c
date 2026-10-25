/* Exercise v2 output protection without requiring a failed include to assemble.
 * The fault is confined to this harness; production never reads test flags. */
#include <stdlib.h>
#ifdef __APPLE__
#include <dlfcn.h>
static void *test_dlopen(const char *path, int mode)
{
    return getenv("TEST_V2_UNICODE_UNAVAILABLE") ? NULL : dlopen(path, mode);
}
#define dlopen test_dlopen
#endif
#include "../dependencies.c"
int main(int argc, char **argv)
{
    int ok;
    if (argc != 3) return 2;
    ok = dependencies_start_output_protection() && dependencies_protect_source(argv[1]) &&
         dependencies_output(argv[2]);
    dependencies_clear();
    return ok ? 0 : 3;
}
