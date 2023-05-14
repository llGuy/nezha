#ifndef _GRAPH_RESOURCE_H_
#define _GRAPH_RESOURCE_H_

#include "graph_image.h"
#include "graph_buffer.h"

enum graph_resource_type
{
  GRAPH_RESOURCE_TYPE_IMAGE,
  GRAPH_RESOURCE_TYPE_BUFFER
};

/* Basically union where the usage depends on the value of the
 * GRAPH_RESOURCE_TYPE enum field. */
struct graph_resource
{
  b8 was_used;
  enum graph_resource_type type;
  struct graph_res_usage_node head_node;
  struct graph_res_usage_node tail_node;

  union
  {
    struct graph_image img;
    struct graph_buffer buf;
  };
};

void graph_resource_add_usage_node(struct graph_resource *, 
                                   struct graph_stage_reference *, u32);

#endif
