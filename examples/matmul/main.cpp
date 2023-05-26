#include <nezha/time.hpp>
#include <nezha/graph.hpp>
#include <nezha/gpu_context.hpp>


#define SHAPE_M       (640*640)
#define SHAPE_N       (32*32)
#define SHAPE_K       (320)

#define BLOCK_ITEMS_M (64)
#define BLOCK_ITEMS_N (32*2)
#define BLOCK_ITEMS_K (20)

// These are things which are derived from user defined values.
// We round up the block items constants.
#define BLOCK_COUNT_M ((SHAPE_M + BLOCK_ITEMS_M - 1) / BLOCK_ITEMS_M)
#define BLOCK_COUNT_N ((SHAPE_N + BLOCK_ITEMS_N - 1) / BLOCK_ITEMS_N)
#define BLOCK_COUNT_K ((SHAPE_K + BLOCK_ITEMS_K - 1) / BLOCK_ITEMS_K)

int main(int argc, char **argv)
{
  nz::init_gpu_context({ .create_surface = false });
  nz::render_graph graph;

  nz::compute_kernel kernel = graph.register_compute_kernel("kernel_matmul");
  nz::gpu_buffer_ref mat_a = graph.register_buffer(
    { .size = SHAPE_M * SHAPE_K * sizeof(float), .host_visible = true, .type = nz::binding::type::storage_buffer });
  nz::gpu_buffer_ref mat_b = graph.register_buffer(
    { .size = SHAPE_K * SHAPE_N * sizeof(float), .host_visible = true, .type = nz::binding::type::storage_buffer });
  nz::gpu_buffer_ref mat_out = graph.register_buffer(
    { .size = SHAPE_M * SHAPE_N * sizeof(float), .host_visible = true });

  { // Initialize mat a
    nz::memory_mapping map_a = graph.get_buffer(mat_a).map();
    float *data = (float *)map_a.data();
    for (int i = 0; i < SHAPE_M * SHAPE_K; ++i)
      data[i] = (float)i;

    nz::memory_mapping map_b = graph.get_buffer(mat_b).map();
    data = (float *)map_b.data();
    for (int i = 0; i < SHAPE_K * SHAPE_N; ++i)
      data[i] = (float)i;
  }

  graph.begin();
  {
    graph.add_compute_pass()
      .set_kernel(kernel)
      .add_storage_buffer(mat_a)
      .add_storage_buffer(mat_b)
      .add_storage_buffer(mat_out)
      .dispatch(BLOCK_COUNT_N, BLOCK_COUNT_M, 1);
  }
  nz::job job = graph.end();

  nz::time_stamp start = nz::current_time();
  auto workload = graph.submit(job);
  workload.wait();
  nz::time_stamp end = nz::current_time();

  nz::log_info("Time %f", nz::time_difference(end, start));

  return 0;
}
