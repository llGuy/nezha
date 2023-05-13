#include <assert.h>
#include <nezha/util.h>
#include "graph_binding.h"

VkDescriptorType translate_descriptor_type(enum graph_binding_type t)
{
  switch (t) 
  {
    case GRAPH_BINDING_TYPE_SAMPLED_IMAGE: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case GRAPH_BINDING_TYPE_STORAGE_IMAGE: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case GRAPH_BINDING_TYPE_STORAGE_BUFFER: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case GRAPH_BINDING_TYPE_UNIFORM_BUFFER: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    default: return VK_DESCRIPTOR_TYPE_MAX_ENUM;
  }
}

VkImageLayout translate_image_layout(enum graph_binding_type t)
{
  assert(t < GRAPH_BINDING_TYPE_MAX_IMAGE);

  switch (t) 
  {
    case GRAPH_BINDING_TYPE_SAMPLED_IMAGE: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case GRAPH_BINDING_TYPE_STORAGE_IMAGE: return VK_IMAGE_LAYOUT_GENERAL;
    case GRAPH_BINDING_TYPE_COLOR_ATTACHMENT: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case GRAPH_BINDING_TYPE_DEPTH_ATTACHMENT: return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    case GRAPH_BINDING_TYPE_IMAGE_TRANSFER_SRC: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case GRAPH_BINDING_TYPE_IMAGE_TRANSFER_DST: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    default:
      nz_panic_and_exit("Invalid graph binding type!");
      return VK_IMAGE_LAYOUT_MAX_ENUM;
  }
}

VkAccessFlags translate_buffer_access(enum graph_binding_type t)
{
  switch (t)
  {
    case GRAPH_BINDING_TYPE_UNIFORM_BUFFER: return VK_ACCESS_MEMORY_READ_BIT;
    case GRAPH_BINDING_TYPE_STORAGE_BUFFER: return VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    case GRAPH_BINDING_TYPE_BUFFER_TRANSFER_SRC: return VK_ACCESS_MEMORY_READ_BIT;
    case GRAPH_BINDING_TYPE_BUFFER_TRANSFER_DST: return VK_ACCESS_MEMORY_WRITE_BIT;
    case GRAPH_BINDING_TYPE_VERTEX_BUFFER: return VK_ACCESS_MEMORY_READ_BIT;
    default:
      nz_panic_and_exit("Invalid graph binding type!");
      return (VkAccessFlags)0;
  }
}

VkAccessFlags translate_image_access(enum graph_binding_type t)
{
  switch (t) 
  {
    case GRAPH_BINDING_TYPE_SAMPLED_IMAGE: return VK_ACCESS_SHADER_READ_BIT;
    case GRAPH_BINDING_TYPE_IMAGE_TRANSFER_SRC: return VK_ACCESS_MEMORY_READ_BIT;
    case GRAPH_BINDING_TYPE_IMAGE_TRANSFER_DST: return VK_ACCESS_MEMORY_WRITE_BIT;
    case GRAPH_BINDING_TYPE_STORAGE_IMAGE: return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    case GRAPH_BINDING_TYPE_COLOR_ATTACHMENT: return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case GRAPH_BINDING_TYPE_DEPTH_ATTACHMENT: return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    default:
      nz_panic_and_exit("Invalid graph binding type!");
      return (VkAccessFlags)0;
  }
}
