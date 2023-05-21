#pragma once

#include <stdint.h>
#include <cassert>
#include <vulkan/vulkan.h>


namespace nz
{

using graph_resource_ref = uint32_t;
using graph_stage_ref = uint32_t;

constexpr uint32_t invalid_graph_ref = 0xFFFFFFF;
constexpr uint32_t graph_stage_ref_present = 0xBADC0FE;
  
/* For internal use. */
struct resource_usage_node 
{
  inline void invalidate() 
  {
    stage = invalid_graph_ref;
    binding_idx = invalid_graph_ref;
  }

  inline bool is_invalid() 
  {
    return stage == invalid_graph_ref;
  }

  graph_stage_ref stage;

  // May need more data (optional for a lot of cases) to describe this
  uint32_t binding_idx;
};

struct clear_color 
{
  float r, g, b, a;
};

struct binding 
{
  enum type 
  {
    // Image types
    sampled_image, storage_image, color_attachment, depth_attachment, 
    image_transfer_src, image_transfer_dst, present_ready, max_image,

    // Buffer types
    storage_buffer, uniform_buffer, buffer_transfer_src,
    buffer_transfer_dst, vertex_buffer, max_buffer,
    none
  };

  // Index of this binding
  uint32_t idx;
  type utype;
  graph_resource_ref rref;

  // For render passes
  clear_color clear;

  // This is going to point to the next place that the given resource is used
  // Member is written to by the resource which is pointed to by the binding
  resource_usage_node next;

  VkDescriptorType get_descriptor_type() 
  {
    switch (utype) 
    {
    case sampled_image: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case storage_image: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case storage_buffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case uniform_buffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    default: return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
  }

  VkImageLayout get_image_layout() 
  {
    assert(utype < type::max_image);

    switch (utype) 
    {
    case type::sampled_image:
      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case type::storage_image:
      return VK_IMAGE_LAYOUT_GENERAL;
    case type::color_attachment:
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case type::depth_attachment:
      return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    case type::image_transfer_src:
      return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case type::image_transfer_dst:
      return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    default:
      assert(false);
      return VK_IMAGE_LAYOUT_MAX_ENUM;
    }
  }

  VkAccessFlags get_buffer_access() 
  {
    switch (utype) 
    {
    case type::uniform_buffer:
      return VK_ACCESS_MEMORY_READ_BIT;
    case type::storage_buffer:
      return VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    case type::buffer_transfer_src:
      return VK_ACCESS_MEMORY_READ_BIT;
    case type::buffer_transfer_dst:
      return VK_ACCESS_MEMORY_WRITE_BIT;
    case type::vertex_buffer:
      return VK_ACCESS_MEMORY_READ_BIT;
    default:
      assert(false);
      return (VkAccessFlags)0;
    }
  }

  VkAccessFlags get_image_access() 
  {
    assert(utype < type::max_image);

    switch (utype) 
    {
    case type::sampled_image:
      return VK_ACCESS_SHADER_READ_BIT;
    case type::image_transfer_src:
      return VK_ACCESS_MEMORY_READ_BIT;
    case type::image_transfer_dst:
      return VK_ACCESS_MEMORY_WRITE_BIT;
    case type::storage_image:
      return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    case type::color_attachment:
      return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case type::depth_attachment:
      return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    default:
      assert(false);
      return (VkAccessFlags)0;
    }
  }
};

}
