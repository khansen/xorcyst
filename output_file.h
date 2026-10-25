#ifndef OUTPUT_FILE_H
#define OUTPUT_FILE_H

#include <stdio.h>

/* A single output destination. The caller retains the path until completion.
   NULL selects stdout; files are staged beside their destination. Destination
   alias validation belongs to the invocation planner and precedes this API. */
typedef struct {
    FILE *stream;
    const char *path;
    char *temporary;
    int ready;
} output_writer;

int output_file_open(output_writer *output, const char *path);

/* Close/flush before publishing. Pass false if serialization failed. Splitting
   these steps lets multi-file formats finish all streams before any rename. */
int output_file_close(output_writer *output, int success);
int output_file_publish(output_writer *output);
int output_file_finish(output_writer *output, int success);

/* Release an incomplete output without replacing its destination. Safe after
   failed open/close/publish and after a previous discard. Never closes stdout. */
void output_file_discard(output_writer *output);

/* Optional invocation-wide publication. All file streams are closed first;
 * failed generation discards every stage. Commit retains old destinations
 * until every rename succeeds, and rolls back a failed rename. This is not
 * crash-atomic across files. Stdout cannot be rolled back. */
void output_batch_begin(void);
int output_batch_stage(const char *path, const char *temporary,
                       int (*replace)(const char *, const char *));
const char *output_batch_read_path(const char *path);
int output_batch_finish(int success);

#endif
