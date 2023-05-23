#include <nezha/graph.hpp>
#include <nezha/gpu_context.hpp>

struct graph_state
{
  int input_count;

  nz::gpu_buffer_ref vector_a;
  nz::gpu_buffer_ref vector_b;
  nz::gpu_buffer_ref output;

  nz::compute_kernel init_kernel;
  nz::compute_kernel multiply_kernel;
  nz::compute_kernel sum_kernel;
};

nz::job initialize(nz::render_graph &graph, graph_state &state)
{
  graph.begin();

  graph.add_compute_pass()
    .set_kernel(state.init_kernel)
    .add_storage_buffer(state.vector_a)
    .dispatch(state.input_count/32, 1, 1);

  graph.add_compute_pass()
    .set_kernel(state.init_kernel)
    .add_storage_buffer(state.vector_b)
    .dispatch(state.input_count/32, 1, 1);

  nz::job job = graph.end();

  return job;
}

nz::job multiply(nz::render_graph &graph, graph_state &state)
{
  graph.begin();

  graph.add_compute_pass()
    .set_kernel(state.multiply_kernel)
    .add_storage_buffer(state.vector_a)
    .add_storage_buffer(state.vector_b)
    .dispatch(state.input_count/32, 1, 1);

  nz::job job = graph.end();

  return job;
}

nz::job sum(nz::render_graph &graph, graph_state &state)
{
  graph.begin();

  uint32_t iter_count = (uint32_t)std::ceil(std::log2f((float)state.input_count) / 5.0f);

  for (int i = 0, exp = 1; i < iter_count; ++i, exp *= 32)
  {
    struct {
      uint32_t input_count;
      uint32_t depth;
      uint32_t depth_exp;
    } sum_info;

    sum_info.input_count = state.input_count;
    sum_info.depth = i;
    sum_info.depth_exp = exp;

    graph.add_compute_pass()
      .set_kernel(state.sum_kernel)
      .add_storage_buffer(state.vector_a)
      .send_data(sum_info)
      .dispatch(state.input_count / (32*exp), 1, 1);
  }

  graph.add_buffer_copy_to_cpu(
    state.output, state.vector_a, 
    0, { .offset = 0, .size = sizeof(float) });

  nz::job job = graph.end();

  return job;
}

int main(int argc, char **argv)
{

  nz::init_gpu_context({ .create_surface = false });
  nz::render_graph graph;

  const int INPUT_COUNT = (1 << 25);

  graph_state state = {
    .input_count = INPUT_COUNT,
    .vector_a = graph.register_buffer({ .size = INPUT_COUNT * sizeof(float) }),
    .vector_b = graph.register_buffer({ .size = INPUT_COUNT * sizeof(float) }),
    .output = graph.register_buffer({ .size = sizeof(float) }),
    .init_kernel = graph.register_compute_kernel("kernel_iota"),
    .sum_kernel = graph.register_compute_kernel("kernel_sum32"),
    .multiply_kernel = graph.register_compute_kernel("kernel_multiply")
  };

  nz::job init_job = initialize(graph, state);
  nz::job multiply_job = multiply(graph, state);
  nz::job sum_job = sum(graph, state);

  graph.submit(init_job);
  graph.submit(multiply_job, init_job);
  nz::pending_workload workload = graph.submit(sum_job, multiply_job);

  workload.wait();

  nz::memory_mapping map = graph.get_buffer(state.output).map();

  nz::log_info("hello world");

  float *result = (float *)map.data();
  nz::log_info("GPU got %f", *result);

  return 0;
}