#pragma once

#include <nezha/types.hpp>
#include <vulkan/vulkan.h>

#include <nezha/gpu_image.hpp>
#include <nezha/gpu_buffer.hpp>

namespace nz
{


/* Returned to the user as a reference to a compute kernel that can be invoked
 * during a compute pass stage of the graph. */
using compute_kernel = u32;


/* For ML, here are kernels that can be used if hardware acceleration permits. */
enum ml_kernel
{
  matrix_multiplication
};


struct acc_kernel;


struct ml_kernel_config
{
  union
  {
    struct
    {
      uint32_t result_rows, result_columns, interior_columns;
    } matrix_multiplication;
  };
};


/* Internal use. This is used for representing actual compute kernel GPU-side
 * objects - pipelines and such. */
struct compute_kernel_state
{
  const char *src;
  VkPipeline pipeline;
  VkPipelineLayout layout;

  ml_kernel ml;
  acc_kernel *kernel;
  ml_kernel_config cfg;
};


class render_graph;


/* Encapsulates a compute pass that will get executed on the GPU. Translates
 * to a compute shader internally. */
class compute_pass 
{
public:
  /* Set the compute kernel to be invoked for this compute pass. */
  compute_pass &set_kernel(compute_kernel kernel);
  compute_pass &set_kernel(ml_kernel kernel);

  /* Send some constant data to the kernel (usually up to 256 bytes). */
  template <typename T>
  compute_pass &send_data(const T &data) { return send_data(&data, sizeof(T)); }
  compute_pass &send_data(const void *data, uint32_t size);

  /* Whenever we add a resource here, we need to update the linked list
   * of used nodes that start with the resource itself. */
  compute_pass &add_sampled_image(gpu_image_ref);
  compute_pass &add_storage_image(gpu_image_ref, const image_info &i = {});
  compute_pass &add_storage_buffer(gpu_image_ref);
  compute_pass &add_uniform_buffer(gpu_image_ref);

  /* Configures the dispatch with given dimensions */
  compute_pass &dispatch(uint32_t count_x, uint32_t count_y, uint32_t count_z);
  /* Configures the dispatch with the size of each wave - specify which image 
   * to get resolution from */
  compute_pass &dispatch_waves(
    uint32_t wave_x, uint32_t wave_y, uint32_t wave_z, gpu_image_ref);

public:
  compute_pass() = default;
  compute_pass(render_graph *, const uid_string &uid);
  ~compute_pass() = default;

private:
  void reset_();
  void create_(compute_kernel_state &);
  void create_compute_shader_(compute_kernel_state &);
  void create_ml_backend_(compute_kernel_state &);
  void issue_commands_(VkCommandBuffer cmdbuf, compute_kernel_state &state);

private:
  void *push_constant_;
  uint32_t push_constant_size_;

  compute_kernel kernel_;
  ml_kernel ml_kernel_;

  std::vector<binding> *bindings_;

  struct 
  {
    uint32_t x, y, z;
    uint32_t binding_res;
    bool is_waves;
  } dispatch_params_;

private:
  uid_string uid_;

  render_graph *builder_;

  friend class render_graph;
  friend class graph_pass;
};

  
}
