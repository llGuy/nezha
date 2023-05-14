#include <nezha/def.h>
#include <nezha/util.h>
#include <nezha/graph.h>
#include <nezha/heap_vector.h>

#include "graph.h"
#include "graph_job.h"
#include "graph_resource.h"

void graph_clear_used_resources(struct nz_graph *);
void graph_clear_recorded_stages(struct nz_graph *);

nz_graph_hdl_t nz_graph_get_res_id(struct nz_graph *g)
{
  return g->current_res_id++;
}

nz_graph_hdl_t nz_graph_get_stg_id(struct nz_graph *g)
{
  return g->current_stg_id++;
}

struct nz_graph *nz_graph_init(struct nz_gpu *gpu)
{
  struct nz_graph *g = (struct nz_graph *)malloc(sizeof(struct nz_graph));
  nz_zero_memory(g, sizeof(struct nz_graph));

  g->passes = nz_heap_vector_init(NZ_DEFVAL, sizeof(struct graph_pass));
  g->resources = nz_heap_vector_init(NZ_DEFVAL, sizeof(struct graph_resource));
  g->jobs = nz_heap_vector_init(NZ_DEFVAL, sizeof(struct nz_job));

  g->recorded_stages = nz_heap_vector_init(NZ_DEFVAL, sizeof(struct graph_stage_reference));
  g->used_resources = nz_heap_vector_init(NZ_DEFVAL, sizeof(graph_resource_reference));

  return g;
}

void nz_graph_begin_job(struct nz_graph *g)
{
  graph_clear_used_resources(g);
  graph_clear_recorded_stages(g);

  nz_heap_vector_clear(g->recorded_stages);
  nz_heap_vector_clear(g->used_resources);
  nz_heap_vector_clear(g->transfers);
}

struct nz_compute_pass *
nz_graph_add_compute_pass(struct nz_graph *g, nz_graph_hdl_t h)
{

}

void graph_clear_used_resources(struct nz_graph *g)
{
  struct graph_res_usage_node invalid_node = {
    .stage_idx = GRAPH_INVALID_REFERENCE,
    .binding_idx = GRAPH_INVALID_REFERENCE
  };

  for (int i = 0; i < nz_heap_vector_size(g->used_resources); ++i)
  {
    graph_resource_reference *rref = nz_heap_vector_get(g->used_resources, i);
    struct graph_resource *r = nz_heap_vector_get(g->resources, *rref);
    r->head_node = invalid_node;
    r->tail_node = invalid_node;
  }
}

void graph_clear_recorded_stages(struct nz_graph *g)
{
  for (int i = 0; i < nz_heap_vector_size(g->recorded_stages); ++i)
  {
    struct graph_stage_reference *ref = nz_heap_vector_get(g->recorded_stages, i);

    switch (ref->stage_type)
    {
      
    }
  }
}
