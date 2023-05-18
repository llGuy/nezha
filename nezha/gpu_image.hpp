#pragma once

#include "types.hpp"
#include "binding.hpp"
#include "string.hpp"

#include <vector>

namespace nz
{

class render_graph;

/* Reference that's returned to the user. */
using gpu_image_ref = u32;

/* In order to create an image, user needs to configure it with an IMAGE_INFO
 * structure. It already has some default values but the things that imperatively
 * need to be set are the FORMAT and the EXTENT. */
struct image_info 
{
  VkFormat format = VK_FORMAT_MAX_ENUM;
  VkExtent3D extent = {};

  /* Default values */
  bool is_depth = false;
  u32 layer_count = 1;
};

  
/* GPU_IMAGE encapsulates the image resource. This can be a texture which
 * binds to a compute kernel or a texture that the user uses during a
 * render pass. */
class gpu_image 
{
public:
  enum action_flag { to_create, to_present, none };

  gpu_image();

  gpu_image &configure(const image_info &i);
  gpu_image &alloc();

private:
  gpu_image(render_graph *);

  void add_usage_node_(graph_stage_ref stg, uint32_t binding_idx);

  /* Update action but also its usage flags. */
  void update_action_(const binding &b);

  void apply_action_();

  /* Create descriptors given a usage flag (if the descriptors were already 
   * created, don't do anything for that kind of descriptor). */
  void create_descriptors_(VkImageUsageFlags usage);

  /* This is necessary in the case of indirection (there is always at most one 
   * level of indirection). */
  gpu_image &get_();

  VkDescriptorSet get_descriptor_set_(binding::type t);

private:
  resource_usage_node head_node_;
  resource_usage_node tail_node_;

  render_graph *builder_;
  action_flag action_;

private:
  /* Deprecated. */
  graph_resource_ref reference_;

  VkImage image_;
  VkImageView image_view_;
  VkDeviceMemory image_memory_;
  VkExtent3D extent_;
  VkImageAspectFlags aspect_;
  VkFormat format_;
  VkImageUsageFlags usage_;
  VkDescriptorSet descriptor_sets_[binding::type::none];

  /* Keep track of current layout / usage stage */
  VkImageLayout current_layout_;
  VkAccessFlags current_access_;
  VkPipelineStageFlags last_used_;

  friend class render_graph;
  friend class compute_pass;
  friend class render_pass;
  friend class graph_resource;
  friend class transfer_operation;
};

}
