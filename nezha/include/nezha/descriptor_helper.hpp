#pragma once

#include <nezha/types.hpp>
#include <algorithm>
#include <vulkan/vulkan.hpp>

namespace nz
{

// Used for descriptor set layout tracker in render_context
class descriptor_set_layout_category 
{
public:
  descriptor_set_layout_category();
  descriptor_set_layout_category(VkDescriptorType type);

  VkDescriptorSetLayout get_descriptor_set_layout(u32 count);

  static constexpr u32 category_count = std::min(
    (u32)VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 10u);

private:
  static constexpr u32 max_descriptor_set_layouts_per_type = 20;

  VkDescriptorType type_;
  VkDescriptorSetLayout layouts_[max_descriptor_set_layouts_per_type];
};

}
