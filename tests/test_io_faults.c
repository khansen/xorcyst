/* Compile the real CLI/exporter with failures confined to output operations. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int io_fault(const char *operation)
{
    static int occurrence;
    const char *selected = getenv("XASM_TEST_IO_FAILURE");
    const char *index = getenv("XASM_TEST_IO_AT");
    if (selected == NULL || strcmp(selected, operation) != 0) return 0;
    if (occurrence++ != (index == NULL ? 0 : atoi(index))) return 0;
    fprintf(stderr, "INJECT_IO %s\n", operation);
    errno = EIO;
    return 1;
}

static FILE *binary_stream;

static FILE *binary_fdopen(int fd, const char *mode)
{
    binary_stream = io_fault("binary_fdopen") ? NULL : fdopen(fd, mode);
    return binary_stream;
}
static int binary_ferror(FILE *fp)
{
    return fp == binary_stream && io_fault("binary_ferror") ? 1 : ferror(fp);
}
static int binary_fclose(FILE *fp)
{
    int is_binary = fp == binary_stream;
    int result;
    if (is_binary) binary_stream = NULL;
    result = fclose(fp);
    return is_binary && io_fault("binary_fclose") ? EOF : result;
}
static int binary_rename(const char *from, const char *to)
{
    return io_fault("binary_rename") ? -1 : rename(from, to);
}
#define fdopen binary_fdopen
#define ferror binary_ferror
#define fclose binary_fclose
#define rename binary_rename
#include "../xasm.c"
#undef fdopen
#undef ferror
#undef fclose
#undef rename

static FILE *nl_fdopen(int fd, const char *mode)
{
    return io_fault("nl_fdopen") ? NULL : fdopen(fd, mode);
}
static int nl_ferror(FILE *fp)
{
    return io_fault("nl_ferror") ? 1 : ferror(fp);
}
static int nl_fclose(FILE *fp)
{
    int result = fclose(fp);
    return io_fault("nl_fclose") ? EOF : result;
}
static int nl_rename(const char *from, const char *to)
{
    return io_fault("nl_rename") ? -1 : rename(from, to);
}
#define fdopen nl_fdopen
#define ferror nl_ferror
#define fclose nl_fclose
#define rename nl_rename
#include "../fceux_nl.c"
