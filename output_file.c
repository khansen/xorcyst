#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "output_file.h"

static void output_error(const output_writer *output)
{
    fprintf(stderr, "error: could not write output `%s'\n",
            output->path != NULL ? output->path : "stdout");
}

static int replaceable_destination(const char *path)
{
    struct stat info;
    if (lstat(path, &info) != 0) return errno == ENOENT;
    /* Rename replaces a symlink itself. Never replace a FIFO, socket, device,
       or directory with a regular sidecar file. */
    return S_ISREG(info.st_mode) || S_ISLNK(info.st_mode);
}

void output_file_discard(output_writer *output)
{
    if (output->stream != NULL && output->stream != stdout) fclose(output->stream);
    if (output->temporary != NULL) unlink(output->temporary);
    free(output->temporary);
    memset(output, 0, sizeof(*output));
}

int output_file_open(output_writer *output, const char *path)
{
    const char *slash;
    size_t directory_length;
    const char suffix[] = ".xasm-XXXXXX";
    int fd;
    memset(output, 0, sizeof(*output));
    output->path = path;
    if (path == NULL) {
        output->stream = stdout;
        return 1;
    }
    if (!replaceable_destination(path)) goto fail;
    slash = strrchr(path, '/');
    directory_length = slash != NULL ? (size_t)(slash - path) + 1 : 0;
    if (directory_length > SIZE_MAX - sizeof(suffix)) goto fail;
    output->temporary = malloc(directory_length + sizeof(suffix));
    if (output->temporary == NULL) goto fail;
    /* A fixed basename also supports destinations near NAME_MAX. */
    memcpy(output->temporary, path, directory_length);
    memcpy(output->temporary + directory_length, suffix, sizeof(suffix));
    fd = mkstemp(output->temporary);
    if (fd < 0) {
        /* No file was created: do not unlink a pathname we do not own. */
        free(output->temporary);
        output->temporary = NULL;
        goto fail;
    }
    output->stream = fdopen(fd, "w");
    if (output->stream == NULL) {
        close(fd);
        goto fail;
    }
    return 1;
fail:
    output_error(output);
    output_file_discard(output);
    return 0;
}

int output_file_close(output_writer *output, int success)
{
    if (output->stream == NULL) return 0;
    if (ferror(output->stream)) success = 0;
    if (output->path == NULL) {
        if (fflush(output->stream) != 0) success = 0;
    } else if (fclose(output->stream) != 0) {
        success = 0;
    }
    output->stream = NULL;
    output->ready = success;
    if (!success) output_error(output);
    return success;
}

int output_file_publish(output_writer *output)
{
    int success = output->ready;
    if (success && output->path != NULL
        && (!replaceable_destination(output->path) || rename(output->temporary, output->path) != 0)) {
        output_error(output);
        success = 0;
    }
    /* A successful rename consumes the temporary pathname. */
    if (success) {
        free(output->temporary);
        output->temporary = NULL;
    }
    output_file_discard(output);
    return success;
}

int output_file_finish(output_writer *output, int success)
{
    if (!output_file_close(output, success)) {
        output_file_discard(output);
        return 0;
    }
    return output_file_publish(output);
}
