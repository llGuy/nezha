#pragma once

#include <nezha/types.hpp>
#include <nezha/binding.hpp>

namespace nz
{


/* Reference that's returned to the user. */
using gpu_buffer_ref = u32;


class render_graph;


/* Configure a buffer witih BUFFER_INFO structure. Need to set SIZE. TYPE can be
 * inferred during compilation of the graph JOB. */
struct buffer_info 
{
  /* Either use this, or the second batch of parameters for ML stuff. */
  u32 size = 0;
  binding::type type = binding::type::max_buffer;
  bool host_visible = false;

  /* For ML. */
  bool ml_accelerate = false;
  uint32_t rows;
  uint32_t columns;
};


/* MEMORY_MAPPING allows the user to have a CPU-side view of the bytes inside
 * a buffer (TODO: or image). To use, make sure that the GPU_BUFFER is
 * HOST_VISIBLE. Then call the MAP function of GPU_BUFFER/GPU_IMAGE. */
class memory_mapping
{
public:
  /* View into the data of the device memory. */
  void *data();

  /* Size of the allocated memory. */
  size_t size();

public:
  /* RAII style unmapping of memory happens when object gets destroyed. */
  ~memory_mapping();

private:
  memory_mapping(VkDeviceMemory memory, size_t size);

private:
  VkDeviceMemory memory_;
  void *data_;
  size_t size_;

  friend class render_graph;
  friend class gpu_buffer;
  friend class gpu_image;
};


  
/* GPU_BUFFER encapsulates a raw buffer of memory usable by compute kernels
 * and pipeline state objects. */
class gpu_buffer 
{
public:
  enum action_flag { to_create, none };

  gpu_buffer(render_graph *graph);

  /* Configures the buffer according to BUFFER_INFO. */
  gpu_buffer &configure(const buffer_info &i);

  /* Actually allocates the internal Vulkan GPU structures. */
  gpu_buffer &alloc();

  /* Mapping memory. */
  memory_mapping map();

public:
  /* For internal use. */
  inline VkBuffer buffer() { return buffer_; }
  inline VkDescriptorSet descriptor(binding::type t) { return get_descriptor_set_(t); }

private:
  void update_action_(const binding &b);
  void apply_action_();

  void add_usage_node_(graph_stage_ref stg, uint32_t binding_idx);

  VkDescriptorSet get_descriptor_set_(binding::type utype);
  void create_descriptors_(VkBufferUsageFlags usage);

private:
  resource_usage_node head_node_;
  resource_usage_node tail_node_;

  action_flag action_;

  render_graph *builder_;

  VkBuffer buffer_;
  VkDeviceMemory buffer_memory_;

  u32 size_;

  VkBufferUsageFlags usage_;

  VkDescriptorSet descriptor_sets_[
    binding::type::max_buffer - binding::type::max_image];

  VkAccessFlags current_access_;
  VkPipelineStageFlags last_used_;

  bool host_visible_;

  friend class render_graph;
  friend class compute_pass;
  friend class render_pass;
  friend class graph_resource;
  friend class transfer_operation;
  friend class graph_resource_tracker;
};
}
