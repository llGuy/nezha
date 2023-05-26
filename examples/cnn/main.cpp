/* CNN applied to a single image. */

#include <random>
#include <nezha/time.hpp>
#include <nezha/graph.hpp>
#include <nezha/gpu_context.hpp>

template <typename T>
T collapse_shape(T *shape, uint32_t n)
{
  T result = 1;
  for (int i = 0; i < n; ++i)
    result *= shape[i];
  return result;
}

struct graph_state
{
  /* H x W x C */
  uint32_t input_shape[3];

  /* K x R x S x C */
  uint32_t weights_shape[4];

  /* H x W x K */
  uint32_t output_shape[3];

  /* Resources. */
  nz::gpu_buffer_ref input_data;
  nz::gpu_buffer_ref weight_data;
  nz::gpu_buffer_ref output_data;

  /* CNN kernel. */
  nz::compute_kernel cnn_kernel;
};

void initialize_input_and_weights(graph_state &state, nz::render_graph &graph)
{
  /* Random number generator. */
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<float> dis(0.0f, 1.0f);

  nz::memory_mapping input_map = graph.get_buffer(state.input_data).map();
  float *input_data = (float *)input_map.data();

  for (int i = 0; i < collapse_shape(state.input_shape, 3); ++i)
    input_data[i] = dis(gen);

  nz::memory_mapping weight_map = graph.get_buffer(state.weight_data).map();
  float *weight_data = (float *)weight_map.data();

  for (int i = 0; i < collapse_shape(state.weights_shape, 4); ++i)
    weight_data[i] = dis(gen);
}

nz::job record_cnn(graph_state &state, nz::render_graph &graph)
{
  graph.begin();
  {
    struct send_data
    {
      int32_t p;
      int32_t q;
      int32_t k;
      int32_t c;
      int32_t r;
      int32_t s;
      int32_t h;
      int32_t w;

      int32_t stride_h;
      int32_t pad_h;
      int32_t stride_w;
      int32_t pad_w;
    };

    send_data sd = {
      640, 640, 32, 3, 3, 3, 640, 640, 1, 1, 1, 1
    };

    graph.add_compute_pass()
      .set_kernel(state.cnn_kernel)
      .add_storage_buffer(state.input_data)
      .add_storage_buffer(state.weight_data)
      .add_storage_buffer(state.output_data)
      .dispatch(640 * 640, 32, 1)
      .send_data(sd);
  }
  nz::job job = graph.end();

  return job;
}

int main(int argc, char **argv)
{
  nz::init_gpu_context({ .create_surface = false });
  nz::render_graph graph;

  graph_state state = {
    .input_shape = { 640, 640, 3 },
    .weights_shape = { 32, 3, 3, 3 },
    .output_shape = { 640, 640, 32 },
  };

  state.input_data = graph.register_buffer(
    { .size = (uint32_t)sizeof(float) * collapse_shape(state.input_shape, 3),
      .type = nz::binding::type::storage_buffer });

  state.weight_data = graph.register_buffer(
    { .size = (uint32_t)sizeof(float) * collapse_shape(state.weights_shape, 4),
      .type = nz::binding::type::storage_buffer });

  state.output_data = graph.register_buffer(
    { .size = (uint32_t)sizeof(float) * collapse_shape(state.output_shape, 3),
      .host_visible = true });

  state.cnn_kernel = graph.register_compute_kernel("kernel_cnn_mn");

  initialize_input_and_weights(state, graph);

  nz::job job = record_cnn(state, graph);

  nz::time_stamp start = nz::current_time();
  nz::pending_workload workload = graph.submit(job);
  workload.wait();
  nz::time_stamp end = nz::current_time();

  nz::log_info("Work finished in %f seconds", nz::time_difference(end, start));

  nz::memory_mapping output_map = graph.get_buffer(state.output_data).map();
  float *output_data = (float *)output_map.data();

  for (int i = 0; i < 20; ++i)
  {
    nz::log_info("%f", output_data[i]);
  }

  return 0;
}
