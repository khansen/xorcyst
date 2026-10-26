/* Object binding and shared-backend integration. GPL-2.0-or-later. */
#ifndef XORCYST_IR_LINK_H
#define XORCYST_IR_LINK_H
#include "layout.h"
#include "heap.h"
#include "object.h"
typedef struct ir_link ir_link;
ir_link *ir_link_create(void);
/* Internal optional allocator: caller keeps heap alive through destroy. */
ir_link *ir_link_create_heap(ir_heap *);
void ir_link_destroy(ir_link *);
/* Takes ownership of the object, including on failure; copies its name. */
ir_id ir_link_add(ir_link *, ir_object *, const char *name);
/* Append consumes the wire tables into owned native values/fragments. Objects
 * may be added and appended one at a time in script order. After all appends,
 * prepare binds the complete namespace; finish calls it if not explicit. */
int ir_link_prepare(ir_link *);
int ir_link_append(ir_link *, ir_id unit);
int ir_link_finish(ir_link *);
ir_layout *ir_link_layout(ir_link *);
ir_unit *ir_link_values(ir_link *);
const char *ir_link_error(ir_link *);
ir_location ir_link_error_location(ir_link *);
/* Optional symbol context, owned by the link until destruction. */
const char *ir_link_error_symbol(ir_link *);
#endif
