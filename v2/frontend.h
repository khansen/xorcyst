/* Isolated source frontend for the approved I3 prototype. GPL-2.0-or-later. */
#ifndef XORCYST_V2_FRONTEND_H
#define XORCYST_V2_FRONTEND_H
#include "../astnode.h"
#include <stdio.h>
void v2_frontend_process(astnode *root);
void v2_frontend_finish(void);
void v2_frontend_write(FILE *stream);
void v2_frontend_clear(void);
/* Read-only source rows over a successful layout. All payloads are borrowed
 * until frontend_clear; no parser AST or namespace is needed by this view. */
typedef struct {
    astnode_type kind;
    datatype data_type;
    location loc;
    unsigned address, output_offset, length;
    const unsigned char *bytes;
    const char *label;
    unsigned char opcode;
    int ram, scope_anchor;
} v2_analysis_row;
int v2_frontend_visit_rows(int (*visit)(const v2_analysis_row *, void *), void *);
#endif
