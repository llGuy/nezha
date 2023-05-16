#include "log.hpp"
#include "bump_alloc.h"
#include "render_graph.hpp"
#include "gpu_context.hpp"

int main(int argc, char **argv) 
{
  init_bump_allocator(megabytes(10));

  /* Initialize API */
  nz::init_gpu_context({ .create_surface = false });
  nz::render_graph graph;
  
  /* Configure resources */
  u32 input_size = (1<<20) * sizeof(float);

  graph.register_buffer(RES("input"))
    .configure({ .size = input_size });

  graph.register_buffer(RES("output"))
    .configure({ .size = input_size });

  /* Record computation. */
  graph.begin();

  { /* These are all the compute workloads we will dispatch in this job. */
    graph.add_compute_pass(STG("compute_random"))
      .set_source("compute_random_kernel")
      .add_storage_buffer(RES("input"))
      .dispatch(input_size/32, 1, 1);

    graph.add_buffer_copy_to_cpu(RES("output"), RES("input"));
  }

  /* We get a JOB object after ending the graph recording. */
  nz::job job = graph.end();

  /* After submitting, we get a pending workload and wait for it. */
  nz::log_info("GPU started work!");
  {
    nz::pending_workload workload = graph.submit(job);
    workload.wait();
  }
  nz::log_info("GPU finished work!");

  /* Reading data from the result of compute kernel */
  nz::log_info("Verifying data...");
  nz::memory_mapping map = graph.get_buffer(RES("output")).map();

  float *result = (float *)map.data();
  nz::log_info("Got %f %f %f ...", result[0], result[1], result[2]);

  return 0;
}





