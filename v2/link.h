/* Object binding and shared-backend integration. GPL-2.0-or-later. */
#ifndef XORCYST_V2_LINK_H
#define XORCYST_V2_LINK_H
#include "layout.h"
#include "heap.h"
#include "object.h"
typedef struct v2_link v2_link;
v2_link *v2_link_create(void);
/* Internal optional allocator: caller keeps heap alive through destroy. */
v2_link *v2_link_create_heap(v2_heap *);
void v2_link_destroy(v2_link *);
/* Takes ownership of the object, including on failure; copies its name. */
v2_id v2_link_add(v2_link *, v2_object *, const char *name);
/* Append consumes the wire tables into owned native values/fragments. Objects
 * may be added and appended one at a time in script order. After all appends,
 * prepare binds the complete namespace; finish calls it if not explicit. */
int v2_link_prepare(v2_link *);
int v2_link_append(v2_link *, v2_id unit);
int v2_link_finish(v2_link *);
v2_layout *v2_link_layout(v2_link *);
v2_unit *v2_link_values(v2_link *);
const char *v2_link_error(v2_link *);
v2_location v2_link_error_location(v2_link *);
/* Optional symbol context, owned by the link until destruction. */
const char *v2_link_error_symbol(v2_link *);
#endif
