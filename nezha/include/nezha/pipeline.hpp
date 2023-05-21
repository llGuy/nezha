#pragma once

#include <nezha/heap_array.hpp>
#include <nezha/gpu_context.hpp>

#include <vulkan/vulkan.h>

namespace nz
{

enum class pso_shader_type 
{
  vertex = VK_SHADER_STAGE_VERTEX_BIT,
  geometry = VK_SHADER_STAGE_GEOMETRY_BIT,
  fragment = VK_SHADER_STAGE_FRAGMENT_BIT
};

class pso_shader 
{
public:
  pso_shader() = default;
  pso_shader(const char *path);

private:
  VkShaderModule module_;
  VkShaderStageFlags stage_;

  friend class pso_config;
};

struct pso_descriptor 
{
  VkDescriptorType type;
  size_t count;
};

class pso_config 
{
public:
  template <typename ...T>
    pso_config(T ...shaders)
    : input_assembly_{},
    vertex_input_{},
    viewport_{},
    rasterization_{},
    multisample_{},
    blending_{},
    rendering_info_{},
    dynamic_state_{},
    depth_stencil_{},
    layouts_{},
    shader_stages_(sizeof...(shaders)),
    push_constant_size_(0),
    viewport_info_{},
    depth_format_{(VkFormat)0},
    create_info_{} 
  {
    shader_stages_.zero();
    set_shader_stages_(
      heap_array<pso_shader>({ shaders... }));

    set_default_values_();
  }

  void enable_blending_same(uint32_t attachment_idx,
    VkBlendOp op, VkBlendFactor src, VkBlendFactor dst);

  void enable_depth_testing();

  template <typename ...T>
  void configure_layouts(size_t push_constant_size, T ...layouts) 
  {
    push_constant_size_ = push_constant_size;
    layouts_ = heap_array<VkDescriptorSetLayout>(
      {get_descriptor_set_layout(layouts.type, layouts.count)...});
  }

  void configure_vertex_input(uint32_t attrib_count, uint32_t binding_count);
  void set_binding(
    uint32_t binding, uint32_t stride, VkVertexInputRate input_rate);
  void set_binding_attribute(
    uint32_t location, uint32_t binding, VkFormat format, uint32_t offset);
  void set_topology(VkPrimitiveTopology topology);
  void set_to_wireframe();

  void add_color_attachment(
    VkFormat format, VkBlendOp op = VK_BLEND_OP_MAX_ENUM,
    VkBlendFactor src = VK_BLEND_FACTOR_MAX_ENUM,
    VkBlendFactor dst = VK_BLEND_FACTOR_MAX_ENUM);

  void add_depth_attachment(VkFormat format);

private:
  void set_default_values_();
  void set_shader_stages_(const heap_array<pso_shader> &sources);
  void finish_configuration_();

private:
  VkPipelineInputAssemblyStateCreateInfo input_assembly_;
  VkPipelineVertexInputStateCreateInfo vertex_input_;
  heap_array<VkVertexInputAttributeDescription> attributes_;
  heap_array<VkVertexInputBindingDescription> bindings_;
  VkViewport viewport_;
  VkRect2D rect_;
  VkPipelineViewportStateCreateInfo viewport_info_;
  VkPipelineRasterizationStateCreateInfo rasterization_;
  VkPipelineMultisampleStateCreateInfo multisample_;
  VkPipelineColorBlendStateCreateInfo blending_;
  VkPipelineDynamicStateCreateInfo dynamic_state_;
  heap_array<VkDynamicState> dynamic_states_;
  VkPipelineDepthStencilStateCreateInfo depth_stencil_;
  heap_array<VkDescriptorSetLayout> layouts_;
  heap_array<VkPipelineShaderStageCreateInfo> shader_stages_;

  VkFormat depth_format_;
  std::vector<VkFormat> attachment_formats_;
  VkPipelineRenderingCreateInfo rendering_info_;
  std::vector<VkPipelineColorBlendAttachmentState> blend_states_;

  size_t push_constant_size_;

  VkGraphicsPipelineCreateInfo create_info_;

  VkPipelineLayout pso_layout_;

  friend class pso;
};

class pso 
{
public:
  pso() = default;
  pso(pso_config &config);

  void bind(VkCommandBuffer cmdbuf);
  void push_constant(VkCommandBuffer cmdbuf, void *data, uint32_t size);

  template <typename ...T>
  void bind_descriptors(VkCommandBuffer cmdbuf, T ...t_sets)
  {
    VkDescriptorSet sets[] = { t_sets... };
    bind_descriptors_(cmdbuf, sets, sizeof...(T));
  }

  inline VkPipelineLayout pso_layout() 
  {
    return layout_;
  }

private:
  void bind_descriptors_(
    VkCommandBuffer cmdbuf, VkDescriptorSet *sets, u32 count);

private:
  VkPipeline pipeline_;
  VkPipelineLayout layout_;
};

}
