/* Faults are confined to the real linker's copy operations and staged writer. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fault(const char *operation, const char *path)
{
    const char *selected = getenv("XLNK_TEST_FAULT");
    const char *target = getenv("XLNK_TEST_OUTPUT");
    if (selected == NULL || strcmp(selected, operation) != 0) return 0;
    if (target != NULL && (path == NULL || strcmp(target, path) != 0)) return 0;
    fprintf(stderr, "INJECT %s\n", operation);
    errno = EIO;
    return 1;
}

static FILE *failed_input;
static size_t copy_fread(void *data, size_t size, size_t count, FILE *fp)
{
    if (fault("read", NULL)) { failed_input = fp; return 0; }
    return fread(data, size, count, fp);
}
static int copy_ferror(FILE *fp) { return fp == failed_input || ferror(fp); }
static int copy_fclose(FILE *fp, const char *function)
{
    int result;
    if (fp == failed_input) failed_input = NULL;
    result = fclose(fp);
    return strcmp(function, "copy_to_output") == 0 && fault("read_close", NULL) ? EOF : result;
}
#define fread copy_fread
#define ferror copy_ferror
#define fclose(fp) copy_fclose(fp, __func__)
#include "../xlnk.c"
#undef fread
#undef ferror
#undef fclose

static const char *output_path;
static int stage_mkstemp(char *path) { return fault("create", output_path) ? -1 : mkstemp(path); }
static int stage_fflush(FILE *fp) { return fault("flush", output_path) ? EOF : fflush(fp); }
static int stage_fclose(FILE *fp)
{
    int result = fclose(fp);
    return fault("close", output_path) ? EOF : result;
}
static int stage_rename(const char *from, const char *to)
{
    return fault("rename", to) ? -1 : rename(from, to);
}
#define mkstemp stage_mkstemp
#define fflush stage_fflush
#define fclose stage_fclose
#define rename stage_rename
#define output_file_open output_file_open_impl
#include "../output_file.c"
#undef mkstemp
#undef fflush
#undef fclose
#undef rename
#undef output_file_open
int output_file_open(output_writer *output, const char *path)
{
    output_path = path;
    return output_file_open_impl(output, path);
}
