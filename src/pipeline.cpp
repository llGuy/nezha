#include <filesystem>
#include "memory.hpp"
#include "pipeline.hpp"
#include "log.hpp"
#include "file.hpp"

namespace nz
{

std::string make_shader_src_path(const char *path) 
{
  std::string str_path = path;
  str_path = std::string(NEZHA_PROJECT_ROOT) +
    (char)std::filesystem::path::preferred_separator +
    std::string("res") + 
    (char)std::filesystem::path::preferred_separator +
    std::string("spv") + 
    (char)std::filesystem::path::preferred_separator +
    std::string(str_path);

  return str_path;
}

pso_shader::pso_shader(const char *path) 
{
  size_t pathLen = strlen(path);
  const char *ext = &path[pathLen - 8];
  if (!strcmp(ext, "vert.spv"))
    stage_ = VK_SHADER_STAGE_VERTEX_BIT;
  else if (!strcmp(ext, "geom.spv"))
    stage_ = VK_SHADER_STAGE_GEOMETRY_BIT;
  else if (!strcmp(ext, "frag.spv"))
    stage_ = VK_SHADER_STAGE_FRAGMENT_BIT;
  else 
  {
    log_error("Unknown shader file extention: %s", path);
    panic_and_exit();
  }

  heap_array<u8> src_bytes = file(
    make_shader_src_path(path), 
    file_type_bin | file_type_in).read_binary();

  VkShaderModuleCreateInfo shaderInfo = {};
  shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaderInfo.codeSize = src_bytes.size();
  shaderInfo.pCode = (uint32_t *)src_bytes.data();

  VK_CHECK(
    vkCreateShaderModule(
      gctx->device,
      &shaderInfo,
      nullptr,
      &module_));
}

void pso_config::enable_blending_same(
  uint32_t attachment_index, VkBlendOp op, VkBlendFactor src, VkBlendFactor dst) 
{
  auto &blend_state = blend_states_[attachment_index];

  blend_state.blendEnable = VK_TRUE;
  blend_state.colorBlendOp = op;
  blend_state.alphaBlendOp = op;
  blend_state.srcColorBlendFactor = src;
  blend_state.srcAlphaBlendFactor = src;
  blend_state.dstColorBlendFactor = dst;
  blend_state.dstAlphaBlendFactor = dst;

  // Color write mask is already set
}

void pso_config::enable_depth_testing() 
{
  depth_stencil_.depthTestEnable = VK_TRUE;
  depth_stencil_.depthWriteEnable = VK_TRUE;
  depth_stencil_.depthCompareOp = VK_COMPARE_OP_LESS;
  depth_stencil_.minDepthBounds = 0.0f;
  depth_stencil_.maxDepthBounds = 1.0f;
}

void pso_config::configure_vertex_input(
  uint32_t attrib_count, uint32_t binding_count) 
{
  attributes_ = heap_array<VkVertexInputAttributeDescription>(attrib_count);
  bindings_ = heap_array<VkVertexInputBindingDescription>(binding_count);

  vertex_input_.pVertexBindingDescriptions = bindings_.data();
  vertex_input_.vertexBindingDescriptionCount = binding_count;
  vertex_input_.pVertexAttributeDescriptions = attributes_.data();
  vertex_input_.vertexAttributeDescriptionCount = attrib_count;
}

void pso_config::set_binding(
  uint32_t binding_idx, uint32_t stride, VkVertexInputRate input_rate) 
{
  auto &binding = bindings_[binding_idx];
  binding.binding = binding_idx;
  binding.stride = stride;
  binding.inputRate = input_rate;
}

void pso_config::set_binding_attribute(
  uint32_t location, uint32_t binding, VkFormat format, uint32_t offset) 
{
  auto &attribute = attributes_[location];
  attribute.location = location;
  attribute.binding = binding;
  attribute.format = format;
  attribute.offset = offset;
}

void pso_config::set_to_wireframe() 
{
  rasterization_.polygonMode = VK_POLYGON_MODE_LINE;
}

void pso_config::set_default_values_() 
{
  blending_.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blending_.logicOpEnable = VK_FALSE;
  blending_.attachmentCount = 0;
  blending_.pAttachments = nullptr;
  blending_.logicOp = VK_LOGIC_OP_COPY;

  /* Vertex input (empty by default) */
  vertex_input_.sType =
    VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  /* Input assembly (triangle strip by default) */
  input_assembly_.sType =
    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly_.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  /* Viewport */
  viewport_.width = 1;
  viewport_.height = 1;
  viewport_.maxDepth = 1.0f;

  rect_.extent = {1, 1};

  viewport_info_.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_info_.viewportCount = 1;
  viewport_info_.pViewports = &viewport_;
  viewport_info_.scissorCount = 1;
  viewport_info_.pScissors = &rect_;

  /* Rasterization */
  rasterization_.sType =
    VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterization_.polygonMode = VK_POLYGON_MODE_FILL;
  rasterization_.cullMode = VK_CULL_MODE_NONE;
  rasterization_.frontFace = VK_FRONT_FACE_CLOCKWISE;
  rasterization_.lineWidth = 1.0f;

  /* Multisampling */
  multisample_.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample_.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multisample_.minSampleShading = 1.0f;

  /* Dynamic states */
  dynamic_states_ = heap_array<VkDynamicState>({ VK_DYNAMIC_STATE_VIEWPORT, 
    VK_DYNAMIC_STATE_SCISSOR });

  dynamic_state_.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state_.dynamicStateCount = dynamic_states_.size();
  dynamic_state_.pDynamicStates = dynamic_states_.data();

  /* Depth stencil - off by default */
  depth_stencil_.sType =
    VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil_.depthTestEnable = VK_FALSE;
  depth_stencil_.depthWriteEnable = VK_FALSE;
}

void pso_config::set_topology(VkPrimitiveTopology topology) 
{
  input_assembly_.topology = topology;
}

void pso_config::set_shader_stages_(const heap_array<pso_shader> &shaders) 
{
  for (int i = 0; i < shaders.size(); ++i) 
  {
    VkPipelineShaderStageCreateInfo *info = &shader_stages_[i];
    info->sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info->pName = "main";
    info->stage = (VkShaderStageFlagBits)shaders[i].stage_;
    info->module = shaders[i].module_;
  }
}

void pso_config::add_color_attachment(
    VkFormat format, 
    VkBlendOp op,
    VkBlendFactor src,
    VkBlendFactor dst) 
{
  blend_states_.push_back({});
  auto &blend_state = blend_states_[blend_states_.size() - 1];

  switch (format) {
  // TODO: Add other formats as they come up
  case VK_FORMAT_B8G8R8A8_UNORM:
  case VK_FORMAT_R8G8B8A8_UNORM:
  case VK_FORMAT_R32G32B32A32_SFLOAT:
  case VK_FORMAT_R16G16B16A16_SFLOAT: 
  {
    blend_state.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  } break;

  case VK_FORMAT_R32G32_SFLOAT:
  case VK_FORMAT_R16G16_SFLOAT: 
  {
    blend_state.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;
  } break;

  case VK_FORMAT_R16_SFLOAT: 
  {
    blend_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
  } break;

  default: 
  {
    log_error("Handling unsupported format for color blending!");
    panic_and_exit();
  } break;
  }

  if (op != VK_BLEND_OP_MAX_ENUM) 
  {
    // Enable blending on this attachment
    blend_state.blendEnable = VK_TRUE;
    blend_state.colorBlendOp = op;
    blend_state.alphaBlendOp = op;

    blend_state.srcColorBlendFactor = src;
    blend_state.srcAlphaBlendFactor = src;

    blend_state.dstColorBlendFactor = dst;
    blend_state.dstAlphaBlendFactor = dst;
  }

  attachment_formats_.push_back(format);
}

void pso_config::add_depth_attachment(VkFormat format) 
{
  depth_format_ = format;
}

void pso_config::finish_configuration_() 
{
  /* Create pipeline layout */
  VkPushConstantRange pushConstantRange = {};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_ALL;
  pushConstantRange.offset = 0;
  pushConstantRange.size = push_constant_size_;

  VkPipelineLayoutCreateInfo pipeline_layout_info = {};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount = layouts_.size();
  pipeline_layout_info.pSetLayouts = layouts_.data();

  if (push_constant_size_) 
  {
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &pushConstantRange;
  }

  VK_CHECK(
    vkCreatePipelineLayout(
      gctx->device,
      &pipeline_layout_info,
      NULL,
      &pso_layout_));

  rendering_info_.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  rendering_info_.pNext = nullptr;
  rendering_info_.colorAttachmentCount = attachment_formats_.size();
  rendering_info_.pColorAttachmentFormats = attachment_formats_.data();
  rendering_info_.depthAttachmentFormat = depth_format_;

  blending_.attachmentCount = blend_states_.size();
  blending_.pAttachments = blend_states_.data();

  create_info_.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  create_info_.pNext = &rendering_info_;
  create_info_.stageCount = shader_stages_.size();
  create_info_.pStages = shader_stages_.data();
  create_info_.pVertexInputState = &vertex_input_;
  create_info_.pInputAssemblyState = &input_assembly_;
  create_info_.pViewportState = &viewport_info_;
  create_info_.pRasterizationState = &rasterization_;
  create_info_.pMultisampleState = &multisample_;
  create_info_.pDepthStencilState = &depth_stencil_;
  create_info_.pColorBlendState = &blending_;
  create_info_.pDynamicState = &dynamic_state_;
  create_info_.layout = pso_layout_;
  create_info_.renderPass = VK_NULL_HANDLE;
  create_info_.subpass = 0;
  create_info_.basePipelineHandle = VK_NULL_HANDLE;
  create_info_.basePipelineIndex = -1;
}

pso::pso(pso_config &config) 
{
  config.finish_configuration_();

  VK_CHECK(
    vkCreateGraphicsPipelines(
      gctx->device,
      VK_NULL_HANDLE,
      1,
      &config.create_info_,
      NULL,
      &pipeline_));

  layout_ = config.pso_layout_;
}

void pso::bind(VkCommandBuffer cmdbuf) 
{
  vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
}

void pso::bind_descriptors_(
  VkCommandBuffer cmdbuf, VkDescriptorSet *sets, u32 count)
{
  vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, 
    layout_, 0, count, sets, 0, nullptr);
}

}
