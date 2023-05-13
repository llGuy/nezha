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
  enum graph_resource_type type;

  union
  {
    struct graph_image img;
    struct graph_buffer buf;
  };
};

#endif
