#include <signal.h>
#include "log.hpp"
#include "bump_alloc.h"
#include "render_graph.hpp"
#include "gpu_context.hpp"

struct graph_state
{
  u32 input_size;

  // Buffers in use
  nz::gpu_buffer_ref input_buffer;
  nz::gpu_buffer_ref output_buffer;

  // This initializes a buffer with 0, 1, 2, 3...
  nz::compute_kernel init_kernel;

  // This doubles all values in a buffer
  nz::compute_kernel double_kernel;
};

nz::job record_first_job(nz::render_graph &graph, const graph_state &state)
{
  /* Record computation. */
  graph.begin();

  { /* These are all the compute workloads we will dispatch in this job. */
    graph.add_compute_pass()
      .set_kernel(state.init_kernel)
      .add_storage_buffer(state.input_buffer)
      .dispatch(state.input_size/32, 1, 1);

    graph.add_compute_pass()
      .set_kernel(state.double_kernel)
      .add_storage_buffer(state.input_buffer)
      .dispatch(state.input_size/32, 1, 1);
  }

  /* We get a JOB object after ending the graph recording. */
  nz::job job = graph.end();

  return job;
}

nz::job record_second_job(nz::render_graph &graph, const graph_state &state)
{
  graph.begin();

  graph.add_compute_pass()
    .set_kernel(state.double_kernel)
    .add_storage_buffer(state.input_buffer)
    .dispatch(state.input_size/32, 1, 1);

  graph.add_compute_pass()
    .set_kernel(state.double_kernel)
    .add_storage_buffer(state.input_buffer)
    .dispatch(state.input_size/32, 1, 1);

  graph.add_buffer_copy_to_cpu(state.output_buffer, state.input_buffer);

  nz::job job = graph.end();

  return job;
}

int main(int argc, char **argv) 
{
  init_bump_allocator(megabytes(10));

  /* Initialize API */
  nz::init_gpu_context({ .create_surface = false });
  nz::render_graph graph;
  
  /* Configure resources */
  const u32 INPUT_SIZE = (1<<10) * sizeof(float);

  graph_state state = {
    .input_size = INPUT_SIZE,
    .input_buffer = graph.register_buffer({ .size = INPUT_SIZE }),
    .output_buffer = graph.register_buffer({ .size = INPUT_SIZE }),
    .init_kernel = graph.register_compute_kernel("fill_random_kernel"),
    .double_kernel = graph.register_compute_kernel("compute_random_kernel")
  };

  nz::job job1 = record_first_job(graph, state);
  nz::job job2 = record_second_job(graph, state);

  /* After submitting, we get a pending workload and wait for it. */
  nz::log_info("GPU started work!");
  {
    nz::pending_workload workload1 = graph.submit(job1);
    nz::pending_workload workload2 = graph.submit(job2, job1);
    workload2.wait();
  }
  nz::log_info("GPU finished work!");

  /* Reading data from the result of compute kernel */
  nz::log_info("Verifying data...");
  nz::memory_mapping map = graph.get_buffer(state.output_buffer).map();

  float *result = (float *)map.data();
  nz::log_info("Got %f %f %f ...", result[0], result[1], result[2]);

  return 0;
}
