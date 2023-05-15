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

  /* Record computation. */
  graph.begin();
  {
    graph.add_compute_pass(STG("compute_random"))
      .set_source("compute_random_kernel")
      .add_storage_buffer(RES("input"))
      .dispatch(input_size/32, 1, 1);
  }
  graph.end();

  return 0;
}
