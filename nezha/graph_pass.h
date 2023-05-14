#ifndef _GRAPH_PASS_H_
#define _GRAPH_PASS_H_

#include "graph_pass_compute.h"

enum graph_pass_type
{
  GRAPH_PASS_TYPE_COMPUTE,
  GRAPH_PASS_TYPE_GRAPHICS,
  GRAPH_PASS_TYPE_MAX
};

struct graph_pass
{
  enum graph_pass_type type;

  union
  {
    struct nz_pass_compute cp;
  };
};

#endif
