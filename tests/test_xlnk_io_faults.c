/* Faults are confined to the real linker's copy operations and staged writer. */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <limits.h>

static void *link_malloc(size_t size, const char *function);

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
static FILE *copy_fopen(const char *path, const char *mode, const char *function)
{
    if (strcmp(function, "copy_to_output") == 0) {
        const char *selected = getenv("XLNK_TEST_FAULT");
        if (selected != NULL && (strcmp(selected, "copy_grow") == 0
                                 || strcmp(selected, "copy_shrink") == 0)) {
            FILE *update = fopen(path, "r+b");
            long size;
            if (update == NULL || fseek(update, 0, SEEK_END) != 0
                || (size = ftell(update)) < 1) abort();
            if (strcmp(selected, "copy_grow") == 0) {
                if (fputc(0xA5, update) == EOF) abort();
            } else if (ftruncate(fileno(update), size - 1) != 0) abort();
            if (fclose(update) != 0) abort();
            fprintf(stderr, "INJECT %s\n", selected);
        }
    }
    return fopen(path, mode);
}
static void *link_malloc(size_t size, const char *function)
{
    return strcmp(function, "register_one_local") == 0 && fault("local_name", NULL)
        ? NULL : malloc(size);
}
static size_t copy_fread(void *data, size_t size, size_t count, FILE *fp)
{
    const char *selected = getenv("XLNK_TEST_FAULT");
    if (selected != NULL && strcmp(selected, "large_copy") == 0) {
        /* A small planned file now yields >INT_MAX bytes. Keep reads within
           the real buffer size and discard writes to avoid a multi-GB fixture. */
        static uintmax_t remaining = (uintmax_t)INT_MAX + 1;
        if (remaining == (uintmax_t)INT_MAX + 1) fprintf(stderr, "INJECT large_copy\n");
        if (count > remaining / size) count = (size_t)(remaining / size);
        remaining -= count * size;
        memset(data, 0, count * size);
        return count;
    }
    if (fault("read", NULL)) { failed_input = fp; return 0; }
    return fread(data, size, count, fp);
}
static size_t copy_fwrite(const void *data, size_t size, size_t count, FILE *fp)
{
    const char *selected = getenv("XLNK_TEST_FAULT");
    return selected != NULL && strcmp(selected, "large_copy") == 0
        ? count : fwrite(data, size, count, fp);
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
#define fopen(path, mode) copy_fopen(path, mode, __func__)
#define ferror copy_ferror
#define fclose(fp) copy_fclose(fp, __func__)
#define fwrite copy_fwrite
#define malloc(size) link_malloc(size, __func__)
#include "../xlnk.c"
#undef fread
#undef fopen
#undef ferror
#undef fclose
#undef fwrite
#undef malloc

static const char *output_path;
static int stage_mkstemp(char *path) { return fault("create", output_path) ? -1 : mkstemp(path); }
static int stage_fflush(FILE *fp) { return fault("flush", output_path) ? EOF : fflush(fp); }
static int stage_fchmod(int fd, mode_t mode) { return fault("permissions", output_path) ? -1 : fchmod(fd, mode); }
static int stage_fclose(FILE *fp)
{
    int fd = fileno(fp);
    int result = fclose(fp);
    if (fcntl(fd, F_GETFD) != -1 || errno != EBADF) abort();
    fprintf(stderr, "CLOSED_STAGE\n");
    return fault("close", output_path) ? EOF : result;
}
static int stage_rename(const char *from, const char *to)
{
    return fault("rename", to) ? -1 : rename(from, to);
}
#define mkstemp stage_mkstemp
#define fflush stage_fflush
#define fchmod stage_fchmod
#define fclose stage_fclose
#define rename stage_rename
#define output_file_open output_file_open_impl
#include "../output_file.c"
#undef mkstemp
#undef fflush
#undef fchmod
#undef fclose
#undef rename
#undef output_file_open
int output_file_open(output_writer *output, const char *path)
{
    output_path = path;
    return output_file_open_impl(output, path);
}
