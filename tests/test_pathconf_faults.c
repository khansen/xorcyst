/* Compile output protection with controlled Darwin filesystem capabilities. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static long test_pathconf(const char *path, int name)
{
    const char *mode = getenv("XASM_TEST_CASE_SENSITIVITY");
    if (name != _PC_CASE_SENSITIVE || mode == NULL) return pathconf(path, name);
    fprintf(stderr, "INJECT_PATHCONF %s\n", mode);
    if (strcmp(mode, "error") == 0) { errno = EIO; return -1; }
    /* An indeterminate result leaves errno unchanged, including stale errors
       from earlier path resolution unless the caller explicitly clears it. */
    if (strcmp(mode, "unknown") == 0) return -1;
    return strcmp(mode, "sensitive") == 0;
}

#define pathconf test_pathconf
#include "../dependencies.c"
