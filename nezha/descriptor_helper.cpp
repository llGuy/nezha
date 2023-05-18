#include <nezha/memory.hpp>
#include <nezha/gpu_context.hpp>
#include <nezha/descriptor_helper.hpp>

namespace nz
{

descriptor_set_layout_category::descriptor_set_layout_category() 
{
}

descriptor_set_layout_category::descriptor_set_layout_category(
  VkDescriptorType type) 
: type_(type), layouts_{VK_NULL_HANDLE} 
{
}

VkDescriptorSetLayout descriptor_set_layout_category::get_descriptor_set_layout(
  u32 count) 
{
  if (layouts_[count - 1] == VK_NULL_HANDLE) 
  {
    auto *bindings = stack_alloc(VkDescriptorSetLayoutBinding, count);
    zero_memory(count, bindings);

    for (int i = 0; i < count; ++i) 
    {
      bindings[i].binding = i;
      bindings[i].descriptorType = type_;
      bindings[i].descriptorCount = 1;
      bindings[i].stageFlags = VK_SHADER_STAGE_ALL;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = count;
    layoutInfo.pBindings = bindings;

    VK_CHECK(vkCreateDescriptorSetLayout(
      gctx->device,
      &layoutInfo,
      NULL,
      &layouts_[count - 1]))
  }

  return layouts_[count - 1];
}

}
