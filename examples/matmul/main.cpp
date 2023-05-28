#include <nezha/time.hpp>
#include <nezha/graph.hpp>
#include <nezha/gpu_context.hpp>


#if 0
#define SHAPE_M       (640*640*3)
#define SHAPE_N       (32)
#define SHAPE_K       (32*3)
#endif

#define SHAPE_M       (640*640*3)
#define SHAPE_N       (32)
#define SHAPE_K       (32*3)

#define BLOCK_ITEMS_M (64)
#define BLOCK_ITEMS_N (32)
#define BLOCK_ITEMS_K (8)

// These are things which are derived from user defined values.
// We round up the block items constants.
#define BLOCK_COUNT_M ((SHAPE_M + BLOCK_ITEMS_M - 1) / BLOCK_ITEMS_M)
#define BLOCK_COUNT_N ((SHAPE_N + BLOCK_ITEMS_N - 1) / BLOCK_ITEMS_N)
#define BLOCK_COUNT_K ((SHAPE_K + BLOCK_ITEMS_K - 1) / BLOCK_ITEMS_K)

struct graph_state
{
  nz::compute_kernel kernel;
  nz::gpu_buffer_ref a;
  nz::gpu_buffer_ref b;
  nz::gpu_buffer_ref out;
};

void dump_matrix(float *data, uint32_t h, uint32_t w)
{
  for (int y = 0; y < h; ++y)
  {
    for (int x = 0; x < w; ++x)
      printf("%d ", (int)data[x+y*w]);

    printf("\n");
  }

  printf("\n");
}

void initialize_matrices(nz::render_graph &graph, graph_state &state)
{
  nz::memory_mapping map_a = graph.get_buffer(state.a).map();
  float *data = (float *)map_a.data();
  for (int i = 0; i < SHAPE_M * SHAPE_K; ++i)
    data[i] = (float)(rand() % 1000);

  nz::memory_mapping map_b = graph.get_buffer(state.b).map();
  data = (float *)map_b.data();

  for (int i = 0; i < SHAPE_K * SHAPE_N; ++i)
    data[i] = (float)(rand() % 1000);
}

void show_output(nz::render_graph &graph, graph_state &state)
{
  nz::memory_mapping map_out = graph.get_buffer(state.out).map();
  float *data = (float *)map_out.data();

  dump_matrix(data, SHAPE_M, SHAPE_N);
}

void test_output(nz::render_graph &graph, graph_state &state)
{
  nz::memory_mapping map_a = graph.get_buffer(state.a).map();
  float *a_data = (float *)map_a.data();

  nz::memory_mapping map_b = graph.get_buffer(state.b).map();
  float *b_data = (float *)map_b.data();

  nz::memory_mapping map_out = graph.get_buffer(state.out).map();
  float *out_data = (float *)map_out.data();

  for (int y = 0; y < SHAPE_M; ++y)
  {
    for (int x = 0; x < SHAPE_N; ++x)
    {
      float output_from_gpu = out_data[x + y * SHAPE_N];

      // Calculate dot product
      float output_from_cpu = 0.0f;
      for (int k = 0; k < SHAPE_K; ++k)
      {
        output_from_cpu += a_data[k * SHAPE_M + y] * b_data[x + k * SHAPE_N];
      }

      if (fabs(output_from_cpu - output_from_gpu) > 0.01f)
      {
        nz::log_error("Matrix multiply failed! %f vs %f", output_from_cpu, output_from_gpu);
      }
    }
  }
}

int main(int argc, char **argv)
{
  nz::init_gpu_context({ .create_surface = false });
  nz::render_graph graph;

  graph_state state;

  state.kernel = graph.register_compute_kernel("kernel_matmul_4x_threads");
  state.a = graph.register_buffer(
    { .size = SHAPE_M * SHAPE_K * sizeof(float), .host_visible = true, .type = nz::binding::type::storage_buffer });
  state.b = graph.register_buffer(
    { .size = SHAPE_K * SHAPE_N * sizeof(float), .host_visible = true, .type = nz::binding::type::storage_buffer });
  state.out = graph.register_buffer(
    { .size = SHAPE_M * SHAPE_N * sizeof(float), .host_visible = true });

  initialize_matrices(graph, state);

  graph.begin();
  {
    graph.add_compute_pass()
      .set_kernel(state.kernel)
      .add_storage_buffer(state.a)
      .add_storage_buffer(state.b)
      .add_storage_buffer(state.out)
      .dispatch(BLOCK_COUNT_N, BLOCK_COUNT_M, 1);
  }
  nz::job job = graph.end();

  nz::time_stamp start = nz::current_time();
  auto workload = graph.submit(job);
  workload.wait();
  nz::time_stamp end = nz::current_time();

  nz::log_info("Time %f", nz::time_difference(end, start));

  // show_output(graph, state);
  nz::log_info("Verifying matrix multiplication result...");
  test_output(graph, state);
  nz::log_info("Passed matrix multiplication!");

  return 0;
}
