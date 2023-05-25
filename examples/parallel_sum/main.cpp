#include <nezha/time.hpp>
#include <nezha/graph.hpp>
#include <nezha/gpu_context.hpp>

/* This sums an array in log(n) time. It continually splits the array by 32
 * and sums each group together until the number of elements left is 1. */

struct graph_state
{
  /* Compute kernels that we will use. */
  nz::compute_kernel init_kernel;
  nz::compute_kernel sum_kernel;

  /* Buffer containing all the elements we want to sum. Will use this buffer
   * as temporary storage for all temporary summations. */
  nz::gpu_buffer_ref buffer;
  nz::gpu_buffer_ref output;
};

/* CPU benchmark to replicate what the GPU is doing. */
void cpu_benchmark(uint32_t input_size)
{
  uint32_t input_count = input_size / sizeof(float);

  float *buffer = new float[input_count];
  float total = 0.0f;
  
  nz::log_info("CPU started work!");
  nz::time_stamp start = nz::current_time();
  {
    for (int i = 0; i < input_count; ++i)
      buffer[i] = (float)i;

    for (int i = 0; i < input_count; ++i)
      total += buffer[i];
  }
  nz::time_stamp end = nz::current_time();
  nz::log_info("CPU finished work in %f seconds!", nz::time_difference(end, start));
  nz::log_info("CPU got %f", total);
}

int main(int argc, char **argv)
{
  nz::init_gpu_context({ .create_surface = false });
  nz::render_graph graph;

  const uint32_t INPUT_SIZE = (1 << 25) * sizeof(float);

  /* Register all the state that we may need. */
  graph_state state = {
    .init_kernel = graph.register_compute_kernel("kernel_iota"),
    .sum_kernel = graph.register_compute_kernel("kernel_sum32"),

    .buffer = graph.register_buffer({ .size = INPUT_SIZE, .type = nz::binding::type::buffer_transfer_dst }),
    .output = graph.register_buffer({ .size = sizeof(float), .host_visible = true })
  };

  /* Figure out how many iterations of this loop we need to run. Amount of
   * iterations is just log_32(input_count)*/
  uint32_t input_count = INPUT_SIZE / sizeof(float);
  uint32_t iter_count = (uint32_t)std::ceil(std::log2f((float)input_count) / 5.0f);

  /* Record the actual commands. */
  graph.begin();
  {
    graph.add_compute_pass()
      .set_kernel(state.init_kernel)
      .add_storage_buffer(state.buffer)
      .dispatch(input_count/32, 1, 1);

    for (int i = 0, exp = 1; i < iter_count; ++i, exp *= 32)
    {
      struct {
        uint32_t input_count;
        uint32_t depth;
        uint32_t depth_exp;
      } sum_info;

      sum_info.input_count = input_count;
      sum_info.depth = i;
      sum_info.depth_exp = exp;

      graph.add_compute_pass()
        .set_kernel(state.sum_kernel)
        .add_storage_buffer(state.buffer)
        .send_data(sum_info)
        .dispatch(input_count / (32*exp), 1, 1);
    }

    graph.add_buffer_copy(
      state.output, state.buffer, 
      0, { .offset = 0, .size = sizeof(float) });
  }
  nz::job job = graph.end();

  /* After submitting, we get a pending workload and wait for it. */
  nz::log_info("GPU started work!");
  nz::time_stamp start = nz::current_time();
  {
    nz::pending_workload workload = graph.submit(job);
    workload.wait();
  }
  nz::time_stamp end = nz::current_time();
  nz::log_info("GPU finished work in %f seconds!", nz::time_difference(end, start));

  /* Verify the result of the summation. */
  nz::memory_mapping map = graph.get_buffer(state.output).map();

  float *result = (float *)map.data();
  nz::log_info("GPU got %f", *result);

  cpu_benchmark(INPUT_SIZE);

  return 0;
}
