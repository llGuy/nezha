#ifndef _GRAPH_JOB_H_
#define _GRAPH_JOB_H_

/* Because of the up front ID creating, these memory allocations get cached. */
struct nz_job
{
  /* This is state which gets reset at the end of every node recording. */
  struct nz_heap_vector *recorded_stages;
  struct nz_heap_vector *used_resources;
  struct nz_heap_vector *transfers;
};

void job_init(struct nz_job *);

#endif
