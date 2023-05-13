#include <nezha/def.h>
#include <nezha/util.h>
#include <nezha/graph.h>
#include <nezha/heap_vector.h>

#include "graph.h"
#include "graph_job.h"
#include "graph_resource.h"

nz_graph_hdl_t nz_graph_get_res_id(struct nz_graph *g)
{
  return g->current_res_id++;
}

nz_graph_hdl_t nz_graph_get_stg_id(struct nz_graph *g)
{
  return g->current_stg_id++;
}

nz_graph_hdl_t nz_graph_get_job_id(struct nz_graph *g)
{
  return g->current_job_id++;
}

struct nz_graph *nz_graph_init(struct nz_gpu *gpu)
{
  struct nz_graph *g = (struct nz_graph *)malloc(sizeof(struct nz_graph));
  nz_zero_memory(g, sizeof(struct nz_graph));

  g->resources = nz_heap_vector_init(NZ_DEFVAL, sizeof(struct graph_resource));
  g->jobs = nz_heap_vector_init(NZ_DEFVAL, sizeof(struct nz_job));

  return g;
}

struct nz_job *nz_graph_begin_job(struct nz_graph *g, nz_graph_hdl_t h)
{
  /* Make sure that the vector has enough space. */
  nz_heap_vector_guarantee(g->jobs, h+1);

  struct nz_job *j = (struct nz_job *)nz_heap_vector_get(g->jobs, h);

  return j;
}
