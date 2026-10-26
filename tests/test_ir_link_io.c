/* Test-only faults in the same staged writer used by the v2 linker. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static const char *destination;
static int fault(const char *operation, const char *path)
{
    const char *selected = getenv("XLNK_TEST_FAULT"), *target = getenv("XLNK_TEST_OUTPUT");
    if (!selected || strcmp(selected, operation) || (target && (!path || strcmp(target, path))))
        return 0;
    fprintf(stderr, "INJECT %s\n", operation);
    errno = EIO;
    return 1;
}
static int stage_create(char *path) { return fault("create", destination) ? -1 : mkstemp(path); }
static int stage_flush(FILE *f) { return fault("flush", destination) ? EOF : fflush(f); }
static int stage_permissions(int fd, mode_t mode)
{
    return fault("permissions", destination) ? -1 : fchmod(fd, mode);
}
static int stage_close(FILE *f)
{
    int result = fclose(f);
    return fault("close", destination) ? EOF : result;
}
static int stage_rename(const char *a, const char *b)
{
    return fault("rename", b) ? -1 : rename(a, b);
}
#define mkstemp stage_create
#define fflush stage_flush
#define fchmod stage_permissions
#define fclose stage_close
#define rename stage_rename
#define output_file_open output_file_open_impl
#include "../output_file.c"
#undef output_file_open
int output_file_open(output_writer *output, const char *path)
{
    destination = path;
    return output_file_open_impl(output, path);
}
