#include "graph_job.h"

#include <nezha/int.h>
#include <nezha/heap_vector.h>

#define INIT_VEC_SIZE 8

void job_init(struct nz_job *j, struct nz_graph *g)
{
  j->graph = g;

  j->recorded_stages = nz_heap_vector_init(INIT_VEC_SIZE, 
                                           sizeof(struct job_recorded_stage));

  j->used_resources = nz_heap_vector_init(INIT_VEC_SIZE, 
                                          sizeof(struct job_recorded_stage));

  j->transfers = nz_heap_vector_init(INIT_VEC_SIZE, 
                                     sizeof(struct job_recorded_stage));
}
