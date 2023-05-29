#include "ml_metal.h"

#include <nezha/graph.hpp>
#include <nezha/gpu_buffer.hpp>
#include <nezha/gpu_context.hpp>

namespace nz
{

gpu_buffer::gpu_buffer(render_graph *graph) 
  : builder_(graph), size_(0), host_visible_(false),
  buffer_(VK_NULL_HANDLE),
  usage_(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
  descriptor_sets_{},
  current_access_(0), last_used_(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT)
{
  tail_node_.invalidate();
  head_node_.invalidate();
}

void gpu_buffer::update_action_(const binding &b) 
{
  if (buffer_ == VK_NULL_HANDLE) 
  {
    action_ = action_flag::to_create;
  }
  else 
  {
    action_ = action_flag::none;
  }

  switch (b.utype) 
  {
  case binding::type::storage_buffer: 
    usage_ |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; break;
  case binding::type::uniform_buffer: 
    usage_ |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; break;
  case binding::type::buffer_transfer_dst: 
    usage_ |= VK_BUFFER_USAGE_TRANSFER_DST_BIT; break;
  default: break;
  }
}

void gpu_buffer::apply_action_() 
{
  if (action_ == action_flag::to_create) 
  {
    alloc();

    create_descriptors_(usage_);
  }
  else 
  {
    create_descriptors_(usage_);
  }
}

gpu_buffer &gpu_buffer::configure(const buffer_info &info) 
{
  switch (info.type) 
  {
  case binding::type::storage_buffer: 
    usage_ |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT; break;
  case binding::type::uniform_buffer: 
    usage_ |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; break;
  default: break;
  }

  if (info.size)
    size_ = info.size;

  host_visible_ |= info.host_visible;

  return *this;
}

gpu_buffer &gpu_buffer::alloc() 
{
  void *pNext = nullptr;

#if 0
#if defined (__APPLE__)
  VkExportMetalObjectCreateInfoEXT export_metal_obj = {
    .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECT_CREATE_INFO_EXT,
    .exportObjectType = VK_EXPORT_METAL_OBJECT_TYPE_METAL_BUFFER_BIT_EXT
  };
#endif
#endif

  VkBufferCreateInfo buffer_create_info = 
  {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .pNext = pNext,
    .flags = 0,
    .size = size_,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    .usage = usage_
  };

  vkCreateBuffer(gctx->device, &buffer_create_info, nullptr, &buffer_);

  u32 allocated_size = 0;
  VkMemoryPropertyFlags mem_prop = host_visible_ ? 
    (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) : (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  buffer_memory_ = allocate_buffer_memory(buffer_, mem_prop);

  return *this;
}

memory_mapping gpu_buffer::map()
{
  if (buffer_ == VK_NULL_HANDLE)
  {
    // Set host visible to true and create the buffer
    host_visible_ = true;
    action_ = action_flag::to_create;
    apply_action_();
  }

  return memory_mapping(buffer_memory_, size_);
}

void gpu_buffer::add_usage_node_(graph_stage_ref stg, uint32_t binding_idx) 
{
  if (!tail_node_.is_invalid()) 
  {
    binding &tail = builder_->get_binding_(
      tail_node_.stage, tail_node_.binding_idx);

    tail.next.stage = stg;
    tail.next.binding_idx = binding_idx;
  }

  tail_node_.stage = stg;
  tail_node_.binding_idx = binding_idx;

  binding &tail = builder_->get_binding_(
    tail_node_.stage, tail_node_.binding_idx);

  // Just make sure that if we traverse through t=is node, we don't try to 
  // keep going
  tail.next.invalidate();

  if (head_node_.stage == invalid_graph_ref) 
  {
    head_node_.stage = stg;
    head_node_.binding_idx = binding_idx;
  }
}

void gpu_buffer::create_descriptors_(VkBufferUsageFlags usage) 
{
  VkBufferUsageFlagBits bits[] = 
  {
    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT 
  };

  VkDescriptorType descriptor_types[] = 
  {
    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER 
  };

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

      VkDescriptorBufferInfo buffer_info = {};
      VkWriteDescriptorSet write = {};

      buffer_info.offset = 0;
      buffer_info.buffer = buffer_;
      buffer_info.range = size_;

      write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write.dstSet = descriptor_sets_[i];
      write.dstBinding = 0;
      write.dstArrayElement = 0;
      write.descriptorCount = 1;
      write.descriptorType = descriptor_types[i];
      write.pBufferInfo = &buffer_info;

      vkUpdateDescriptorSets(gctx->device, 1, &write, 0, nullptr);
    }
  }
}

VkDescriptorSet gpu_buffer::get_descriptor_set_(binding::type utype) 
{
  return descriptor_sets_[utype - binding::type::storage_buffer];
}

memory_mapping::memory_mapping(VkDeviceMemory memory, size_t size)
  : memory_(memory), size_(size)
{
  vkMapMemory(gctx->device, memory, 0, size, 0, &data_);
}

memory_mapping::~memory_mapping()
{
  vkUnmapMemory(gctx->device, memory_);
}

void *memory_mapping::data()
{
  return data_;
}

size_t memory_mapping::size()
{
  return size_;
}
  
}
