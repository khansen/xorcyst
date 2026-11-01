#ifndef XASM_DEPENDENCIES_H
#define XASM_DEPENDENCIES_H

#include <stdio.h>

enum dependency_role {
    DEP_SOURCE = 1, DEP_BINARY = 2, DEP_CHARMAP = 4,
    DEP_ANALYSIS_SOURCE = 8, DEP_COMPARISON = 16, DEP_PRODUCER = 32
};

/* Capture before getopt mutates or reorders argv; enable only after parsing. */
void dependencies_arguments(int argc, char **argv);
int dependencies_start(const char *version, const char *manifest);
/* Track input/output aliases without taking content snapshots or emitting a manifest. */
int dependencies_start_output_protection(void);
int dependencies_protect_source(const char *path);
int dependencies_output(const char *path);
FILE *dependencies_open(const char *path, const char *mode, unsigned role);
int dependencies_validate(void);
int dependencies_write(const char *path);
int dependencies_failed(void);
/* Borrow immutable consumed snapshots while the invocation is alive. */
typedef struct {
    const char *path;
    unsigned roles;
    int missing;
    const unsigned char *bytes;
    size_t size;
    const char *sha256;
} dependency_snapshot;
int dependencies_visit(int (*visit)(const dependency_snapshot *, void *), void *);
int dependencies_write_evidence(FILE *);
void dependencies_clear(void);

#endif
