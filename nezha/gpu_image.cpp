#include <nezha/graph.hpp>
#include <nezha/gpu_image.hpp>
#include <nezha/gpu_context.hpp>

namespace nz
{

gpu_image::gpu_image() 
  : head_node_{ invalid_graph_ref, invalid_graph_ref },
  tail_node_{ invalid_graph_ref, invalid_graph_ref },
  image_(VK_NULL_HANDLE),
  image_view_(VK_NULL_HANDLE),
  image_memory_(VK_NULL_HANDLE),
  current_layout_(VK_IMAGE_LAYOUT_UNDEFINED),
  current_access_(0),
  last_used_(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
  usage_(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
  descriptor_sets_{} 
{
}

gpu_image::gpu_image(render_graph *builder)
  : head_node_{ invalid_graph_ref, invalid_graph_ref },
  tail_node_{ invalid_graph_ref, invalid_graph_ref },
  builder_(builder),
  image_(VK_NULL_HANDLE),
  image_view_(VK_NULL_HANDLE),
  image_memory_(VK_NULL_HANDLE),
  current_layout_(VK_IMAGE_LAYOUT_UNDEFINED),
  current_access_(0),
  last_used_(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT),
  usage_(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT),
  descriptor_sets_{} 
{
}

void gpu_image::add_usage_node_(graph_stage_ref stg, uint32_t binding_idx) 
{
  if (!tail_node_.is_invalid()) 
  {
    binding &tail = builder_->get_binding_(tail_node_.stage, 
      tail_node_.binding_idx);

    tail.next.stage = stg;
    tail.next.binding_idx = binding_idx;
  }

  tail_node_.stage = stg;
  tail_node_.binding_idx = binding_idx;

  binding &tail = builder_->get_binding_(tail_node_.stage,
    tail_node_.binding_idx);

  // Just make sure that if we traverse through t=is node, we don't try to 
  // keep going
  tail.next.invalidate();

  if (head_node_.stage == invalid_graph_ref) 
  {
    head_node_.stage = stg;
    head_node_.binding_idx = binding_idx;
  }
}

void gpu_image::update_action_(const binding &b) 
{
  if (image_ == VK_NULL_HANDLE) 
  {
    // Lazily create the images
    action_ = action_flag::to_create;
  }
  else 
  {
    action_ = action_flag::none;
  }

  // This is going to determine which descriptor sets to create
  switch (b.utype) 
  {
  case binding::type::sampled_image: 
    usage_ |= VK_IMAGE_USAGE_SAMPLED_BIT; break;
  case binding::type::storage_image: 
    usage_ |= VK_IMAGE_USAGE_STORAGE_BIT; break;
  case binding::type::color_attachment: 
    usage_ |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; break;
  case binding::type::depth_attachment: 
    usage_ |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; break;
  default: break;
  }
}

void gpu_image::apply_action_() 
{
  // We need to create this image
  if (action_ == action_flag::to_create) 
  {
    // TODO: actual image creation
    alloc();

    create_descriptors_(usage_);
  }
  else 
  {
    // If the descriptors were already created, no need to do anything for them
    create_descriptors_(usage_);
  }

  // action_ = action_flag::none;
}

// These may or may not match with the usage_ member - could be invoked
// by another image (resource aliasing stuff)
void gpu_image::create_descriptors_(VkImageUsageFlags usage) 
{
  VkImageUsageFlagBits bits[] = {
    VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_USAGE_STORAGE_BIT };
  VkDescriptorType descriptor_types[] = {
    VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE };
  VkImageLayout expected_layouts[] = {
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL };

  for (int i = 0; i < sizeof(bits)/sizeof(bits[0]); ++i) 
  {
    if (usage & bits[i] && descriptor_sets_[i] == VK_NULL_HANDLE) 
    {
      VkDescriptorSetLayout layout = get_descriptor_set_layout(
        descriptor_types[i], 1);

      VkDescriptorSetAllocateInfo allocate_info = {};
      allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      allocate_info.descriptorPool = gctx->descriptor_pool;
      allocate_info.descriptorSetCount = 1;
      allocate_info.pSetLayouts = &layout;

      vkAllocateDescriptorSets(
        gctx->device, &allocate_info, &descriptor_sets_[i]);

      VkDescriptorImageInfo image_info = {};
      VkWriteDescriptorSet write = {};

      image_info.imageLayout = expected_layouts[i];
      image_info.imageView = image_view_;
      image_info.sampler = VK_NULL_HANDLE;

      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstSet = descriptor_sets_[i];
      write.dstBinding = 0;
      write.dstArrayElement = 0;
      write.descriptorCount = 1;
      write.descriptorType = descriptor_types[i];
      write.pImageInfo = &image_info;

      vkUpdateDescriptorSets(gctx->device, 1, &write, 0, nullptr);
    }
  }
}

VkDescriptorSet gpu_image::get_descriptor_set_(binding::type t) 
{
  return descriptor_sets_[t];
}

gpu_image &gpu_image::get_() 
{
  // Later, we can also add a to reference in the case of aliasing
  if (action_ == action_flag::to_present)
    return builder_->get_image_(reference_);

  return *this;
}

gpu_image &gpu_image::configure(const image_info &info) 
{
  /* If the image was created already, ignore */
  if (image_ == VK_NULL_HANDLE)
  {
    extent_ = info.extent;

    aspect_ = info.is_depth ?
      VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    if (info.format == VK_FORMAT_MAX_ENUM) 
    {
      if (aspect_ == VK_IMAGE_ASPECT_DEPTH_BIT) 
      {
        // Get default depth format
        format_ = gctx->depth_format;
      }
      else 
      {
        assert(false);
      }
    }
    else 
    {
      format_ = info.format;
    }
  }

  // TODO: Layer counts and stuff...
  return *this;
}

gpu_image &gpu_image::alloc() 
{
  VkImageCreateInfo image_create_info = 
  {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .flags = 0,
    .arrayLayers = 1,
    .extent = extent_,
    .format = format_,
    .imageType = extent_.depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .mipLevels = 1,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = usage_,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE
  };

  vkCreateImage(gctx->device, &image_create_info, nullptr, &image_);

  u32 allocated_size = 0;
  image_memory_ = allocate_image_memory(
    image_, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &allocated_size);

  VkImageViewCreateInfo view_create_info = 
  {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .image = image_,
    .viewType = extent_.depth > 1 ?
      VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D,
    .format = format_,
    .subresourceRange.aspectMask = aspect_,
    .subresourceRange.baseMipLevel = 0,
    .subresourceRange.levelCount = 1,
    .subresourceRange.baseArrayLayer = 0,
    .subresourceRange.layerCount = 1
  };

  vkCreateImageView(gctx->device, &view_create_info, nullptr, &image_view_);

  return *this;
}

}
