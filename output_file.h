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

#endif
