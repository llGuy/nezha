#ifndef _GRAPH_H_
#define _GRAPH_H_

#include <nezha/bpool.h>
#include <nezha/graph.h>

#define GRAPH_INVALID_REFERENCE 0xDEADBABE

typedef int graph_resource_reference;
struct graph_stage_reference
{
  u32 stage_type : 3;
  u32 stage_idx : 29;
};

/* Used in GRAPH_STAGE_REFERENCE structure. */
enum graph_stage_type
{
  GRAPH_STAGE_TYPE_TRANSFER,
  GRAPH_STAGE_TYPE_COMPUTE,
  /* GRAPH_STAGE_TYPE_GRAPHICS, */
  GRAPH_STAGE_TYPE_MAX
};

/* This is used by bindings to keep track of where the binding is being used. */
struct graph_res_usage_node
{
  int stage_idx;
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

  /* These are all the temporary things that are used in the recording
   * of a job. */
  struct nz_heap_vector *recorded_stages;
  struct nz_heap_vector *used_resources;
  struct nz_heap_vector *transfers;
};

#endif
