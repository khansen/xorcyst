/* Isolated source frontend for the approved I3 prototype. GPL-2.0-or-later. */
#ifndef XORCYST_V2_FRONTEND_H
#define XORCYST_V2_FRONTEND_H
#include "../astnode.h"
#include <stdio.h>
void v2_frontend_process(astnode *root);
void v2_frontend_finish(void);
void v2_frontend_write(FILE *stream);
void v2_frontend_clear(void);
#endif
