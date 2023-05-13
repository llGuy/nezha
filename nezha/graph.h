#ifndef _GRAPH_H_
#define _GRAPH_H_

#include <nezha/bpool.h>
#include <nezha/graph.h>

typedef int graph_resource_id_t;
typedef int graph_stage_id_t;

/* This is used by bindings to keep track of where the binding is being used. */
struct graph_res_usage_node
{
  graph_stage_id_t stage;
  int binding_idx;
};

struct nz_graph
{
  nz_graph_hdl_t current_res_id;
  nz_graph_hdl_t current_stg_id;
  nz_graph_hdl_t current_job_id;

  /* These are all things which will persist during the lifetime of the
   * program. Declared upfront, etc... */
  struct nz_heap_vector *passes;
  struct nz_heap_vector *resources;
  struct nz_heap_vector *jobs;
};

#endif
