#include <nezha/file.hpp>
#include <nezha/graph.hpp>
#include <nezha/memory.hpp>
#include <nezha/gpu_context.hpp>
#include <nezha/bump_alloc.hpp>
#include <nezha/compute_pass.hpp>
#include <nezha/descriptor_helper.hpp>

namespace nz
{

compute_pass::compute_pass(render_graph *builder, const uid_string &uid) 
  : builder_(builder), uid_(uid),
  push_constant_(nullptr),
  push_constant_size_(0)
{
}

compute_pass &compute_pass::set_kernel(compute_kernel kernel) 
{
  kernel_ = kernel;
  return *this;
}

compute_pass &compute_pass::add_sampled_image(gpu_image_ref ref) 
{
  uint32_t binding_id = bindings_->size();

  binding b = { 
    (uint32_t)bindings_->size(), binding::type::sampled_image, ref
  };

  bindings_->push_back(b);

  // Get image will allocate space for the image struct if it hasn't 
  // been created yet
  gpu_image &img = builder_->get_image_(ref);
  img.add_usage_node_(uid_.id, binding_id);

  return *this;
}

compute_pass &compute_pass::add_storage_image(gpu_image_ref ref, const image_info &i) 
{
  uint32_t binding_id = bindings_->size();

  binding b = {
    (uint32_t)bindings_->size(), binding::type::storage_image, ref
  };

  bindings_->push_back(b);

  gpu_image &img = builder_->get_image_(ref);
  img.add_usage_node_(uid_.id, binding_id);
  img.configure(i);

  return *this;
}

compute_pass &compute_pass::add_storage_buffer(gpu_buffer_ref ref) 
{
  uint32_t binding_id = bindings_->size();

  binding b = {
    (uint32_t)bindings_->size(), binding::type::storage_buffer, ref
  };

  bindings_->push_back(b);

  gpu_buffer &buf = builder_->get_buffer_(ref);
  buf.add_usage_node_(uid_.id, binding_id);

  return *this;
}

compute_pass &compute_pass::add_uniform_buffer(gpu_buffer_ref ref) 
{
  uint32_t binding_id = bindings_->size();

  binding b = {
    (uint32_t)bindings_->size(), binding::type::uniform_buffer, ref
  };

  bindings_->push_back(b);

  gpu_buffer &buf = builder_->get_buffer_(ref);
  buf.add_usage_node_(uid_.id, binding_id);

  return *this;
}

compute_pass &compute_pass::send_data(const void *data, uint32_t size) 
{
  if (!push_constant_)
    push_constant_ = malloc(size);

  memcpy(push_constant_, data, size);
  push_constant_size_ = size;

  return *this;
}

compute_pass &compute_pass::dispatch(
  uint32_t count_x, uint32_t count_y, uint32_t count_z) 
{
  dispatch_params_.x = count_x;
  dispatch_params_.y = count_y;
  dispatch_params_.z = count_z;
  dispatch_params_.is_waves = false;

  return *this;
}

compute_pass &compute_pass::dispatch_waves(
  uint32_t wave_x, uint32_t wave_y, uint32_t wave_z, gpu_image_ref ref)
{
  dispatch_params_.x = wave_x;
  dispatch_params_.y = wave_y;
  dispatch_params_.z = wave_z;
  dispatch_params_.is_waves = true;

  // Works for images
  dispatch_params_.binding_res = ref;

  return *this;
}

void compute_pass::reset_() 
{
  // Should keep capacity the same to no reallocs after the first time 
  // adding the compute pass
  bindings_->clear();
}

void compute_pass::create_(compute_kernel_state &state)
{
  VkPushConstantRange push_constant_range = {};
  push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  push_constant_range.offset = 0;
  push_constant_range.size = push_constant_size_;

  // Pipeline layout TODO: Support descriptors with count>1
  VkDescriptorSetLayout *layouts = stack_alloc(
    VkDescriptorSetLayout, bindings_->size());
  for (u32 i = 0; i < bindings_->size(); ++i)
    layouts[i] = get_descriptor_set_layout(
      (*bindings_)[i].get_descriptor_type(), 1);

  VkPipelineLayoutCreateInfo pipeline_layout_info = {};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount = bindings_->size();
  pipeline_layout_info.pSetLayouts = layouts;

  if (push_constant_size_) 
  {
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;
  }

  VK_CHECK(vkCreatePipelineLayout(
    gctx->device, &pipeline_layout_info, nullptr, &state.layout));

  // Shader stage
  heap_array<u8> src_bytes = file(
    make_shader_src_path(state.src, VK_SHADER_STAGE_COMPUTE_BIT), 
    file_type_bin | file_type_in).read_binary();

  VkShaderModule shader_module;
  VkShaderModuleCreateInfo shader_info = {};
  shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader_info.codeSize = src_bytes.size();
  shader_info.pCode = (u32 *)src_bytes.data();

  VK_CHECK(vkCreateShaderModule(
    gctx->device, &shader_info, NULL, &shader_module));

  VkPipelineShaderStageCreateInfo module_info = {};
  module_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  module_info.pName = "main";
  module_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  module_info.module = shader_module;

  VkComputePipelineCreateInfo compute_pipeline_info = {};
  compute_pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  compute_pipeline_info.stage = module_info;
  compute_pipeline_info.layout = state.layout;

  VK_CHECK(vkCreateComputePipelines(gctx->device, VK_NULL_HANDLE, 1, 
    &compute_pipeline_info, nullptr, &state.pipeline));
}

// This also needs to issue all synchronization stuff that may be needed
void compute_pass::issue_commands_(VkCommandBuffer cmdbuf, compute_kernel_state &state)
{
  // Loop through all bindings (make sure images and buffers have proper 
  // barriers issued for them) This is a very rough estimate - TODO: Make sure 
  // to have exact number of image bindings
  auto *img_barriers = bump_mem_alloc<VkImageMemoryBarrier>(bindings_->size());
  auto *buf_barriers = bump_mem_alloc<VkBufferMemoryBarrier>(bindings_->size());

  u32 img_barrier_count = 0, buf_barrier_count = 0;

  VkDescriptorSet *descriptor_sets = bump_mem_alloc<VkDescriptorSet>(
    bindings_->size());

  int i = 0;
  // Once all barriers have been issued, we can dispatch the pipeline!
  for (auto &b : *bindings_)
  {
    auto &res = builder_->get_resource_(b.rref);

    switch (res.get_type()) 
    {
    case graph_resource::type::graph_image: 
    {
      auto &img = res.get_image();
      VkImageMemoryBarrier barrier = 
      {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .image = img.get_().image_,
        .oldLayout = img.get_().current_layout_,
        .newLayout = b.get_image_layout(),
        .srcAccessMask = img.get_().current_access_,
        .dstAccessMask = b.get_image_access(),
        .subresourceRange.aspectMask = img.get_().aspect_,
        // TODO: Support non-hardcoded values for this
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.layerCount = 1,
        .subresourceRange.levelCount = 1
      };

      vkCmdPipelineBarrier(cmdbuf, img.get_().last_used_,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

      // Update image data
      img.get_().current_layout_ = b.get_image_layout();
      img.get_().current_access_ = b.get_image_access();
      img.get_().last_used_ = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

      descriptor_sets[i] = img.get_().get_descriptor_set_(b.utype);
      assert(descriptor_sets[i] != VK_NULL_HANDLE);
    } break;

    case graph_resource::type::graph_buffer: 
    {
      auto &buf = res.get_buffer();
      VkBufferMemoryBarrier barrier = 
      {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .size = buf.size_,
        .buffer = buf.buffer_,
        .offset = 0,
        .srcAccessMask = buf.current_access_,
        .dstAccessMask = b.get_buffer_access(),
      };

      vkCmdPipelineBarrier(cmdbuf, buf.last_used_, 
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
        0, 0, nullptr, 1, &barrier, 0, nullptr);

      buf.current_access_ = b.get_buffer_access();
      buf.last_used_ = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

      descriptor_sets[i] = buf.get_descriptor_set_(b.utype);
      assert(descriptor_sets[i] != VK_NULL_HANDLE);
    } break;

    default: break;
    }

    ++i;
  }

  vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, state.pipeline);
  vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, state.layout, 
    0, bindings_->size(), descriptor_sets, 0, nullptr);

  if (push_constant_size_)
    vkCmdPushConstants(cmdbuf, state.layout, VK_SHADER_STAGE_COMPUTE_BIT, 
      0, push_constant_size_, push_constant_);

  u32 gx = dispatch_params_.x, gy = dispatch_params_.y, gz = dispatch_params_.z;
  if (dispatch_params_.is_waves) 
  {
    gpu_image &ref = builder_->get_image_(dispatch_params_.binding_res);

    VkExtent3D extent = ref.get_().extent_;
    gx = (uint32_t)glm::ceil((float)extent.width / (float)gx);
    gy = (uint32_t)glm::ceil((float)extent.height / (float)gy);
    gz = (uint32_t)glm::ceil((float)extent.depth / (float)gz);
  }

  vkCmdDispatch(cmdbuf, gx, gy, gz);
}

}
