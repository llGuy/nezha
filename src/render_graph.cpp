#include <string>
#include "bump_alloc.h"
#include "file.hpp"
#include <filesystem>
#include "log.hpp"
#include "render_graph.hpp"
#include "gpu_context.hpp"
#include "vulkan/vulkan_core.h"

namespace nz
{

// Some dumb string helpers
std::string make_shader_src_path(const char *path, VkShaderStageFlags stage) 
{
  const char *extension = "";

  switch (stage) 
  {
  case VK_SHADER_STAGE_COMPUTE_BIT: extension = ".comp.spv"; break;
  case VK_SHADER_STAGE_VERTEX_BIT: extension = ".vert.spv"; break;
  case VK_SHADER_STAGE_FRAGMENT_BIT: extension = ".frag.spv"; break;
  default: break;
  }

  std::string str_path = path;
  str_path = std::string(NEZHA_PROJECT_ROOT) +
    (char)std::filesystem::path::preferred_separator +
    std::string("res") + 
    (char)std::filesystem::path::preferred_separator +
    std::string("spv") + 
    (char)std::filesystem::path::preferred_separator +
    str_path +
    extension;

  return str_path;
}

/************************* Render Pass ****************************/
// TODO:
render_pass::render_pass(render_graph *builder, const uid_string &uid) 
  : builder_(builder), uid_(uid), depth_index_(-1), rect_{}, 
    prepare_commands_proc_(nullptr)
{
}

render_pass &render_pass::add_color_attachment(
  const uid_string &uid, clear_color color, const image_info &info) 
{
  uint32_t binding_id = bindings_.size();

  binding b = { (uint32_t)bindings_.size(), binding::type::color_attachment, 
    uid.id, color };

  bindings_.push_back(b);

  // Get image will allocate space for the image struct if it hasn't 
  // been created yet
  gpu_image &img = builder_->get_image_(uid.id);
  img.add_usage_node_(uid_.id, binding_id);
  img.configure(info);

  return *this;
}

render_pass &render_pass::add_depth_attachment(
  const uid_string &uid, clear_color color, const image_info &info) 
{
  uint32_t binding_id = bindings_.size();

  depth_index_ = binding_id;

  binding b = { (uint32_t)bindings_.size(), binding::type::depth_attachment,
    uid.id, color };

  bindings_.push_back(b);

  // Get image will allocate space for the image struct if it hasn't been 
  // created yet
  gpu_image &img = builder_->get_image_(uid.id);
  img.add_usage_node_(uid_.id, binding_id);

  image_info info_tmp = info;
  info_tmp.is_depth = true;

  img.configure(info_tmp);

  return *this;
}

render_pass &render_pass::set_render_area(VkRect2D rect) 
{
  rect_ = rect;
  return *this;
}

void render_pass::reset_() 
{
  bindings_.clear();
  depth_index_ = -1;
  rect_ = {};
  prepare_commands_proc_ = nullptr;
}

void render_pass::issue_commands_(VkCommandBuffer cmdbuf) 
{
  if (prepare_commands_proc_)
    prepare_commands_proc_({ cmdbuf, builder_, prepare_commands_aux_ });

  uint32_t color_attachment_count = (uint32_t)(bindings_.size() - 
    (depth_index_ == -1 ? 0 : 1));

  auto *color_attachments = bump_mem_alloc<VkRenderingAttachmentInfoKHR>(
    color_attachment_count);
  auto *depth_attachment = (depth_index_ == -1 ? 
    nullptr : bump_mem_alloc<VkRenderingAttachmentInfoKHR>());

  auto *img_barriers = bump_mem_alloc<VkImageMemoryBarrier>(bindings_.size());

  for (int b_idx = 0, c_idx = 0; b_idx < bindings_.size(); ++b_idx) 
  {
    if (b_idx == depth_index_) 
    {
      binding &b = bindings_[b_idx];

      gpu_image &img = builder_->get_image_(b.rref);

      VkClearDepthStencilValue clear_value;
      clear_value.depth = b.clear.r;

      depth_attachment->sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
      depth_attachment->clearValue.depthStencil = clear_value;
      depth_attachment->imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
      depth_attachment->imageView = img.get_().image_view_;
      depth_attachment->loadOp = (b.clear.r < 0.0f ? 
        VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR);
      depth_attachment->storeOp = VK_ATTACHMENT_STORE_OP_STORE;

      // Issue memory barrier
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
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 
        0, 0, NULL, 0, NULL, 1, &barrier);

      // Update image data
      img.get_().current_layout_ = b.get_image_layout();
      img.get_().current_access_ = b.get_image_access();
      img.get_().last_used_ = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else 
    {
      binding &b = bindings_[b_idx];

      gpu_image &img = builder_->get_image_(b.rref);

      VkClearColorValue clear_value;
      clear_value.float32[0] = b.clear.r;
      clear_value.float32[1] = b.clear.g;
      clear_value.float32[2] = b.clear.b;
      clear_value.float32[3] = b.clear.a;

      color_attachments[c_idx].sType = 
        VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
      color_attachments[c_idx].clearValue.color = clear_value;
      color_attachments[c_idx].imageLayout = 
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      color_attachments[c_idx].imageView = img.get_().image_view_;
      color_attachments[c_idx].loadOp = (b.clear.r < 0.0f ? 
         VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR);
      color_attachments[c_idx].storeOp = VK_ATTACHMENT_STORE_OP_STORE;

      // Issue memory barrier
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
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0, 0, NULL, 0, NULL, 1, &barrier);

      // Update image data
      img.get_().current_layout_ = b.get_image_layout();
      img.get_().current_access_ = b.get_image_access();
      img.get_().last_used_ = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

      ++c_idx;
    }
  }

  if (rect_.extent.width == 0) 
  {
    gpu_image &img = builder_->get_image_(bindings_[0].rref);

    rect_.offset = {};
    rect_.extent = { img.get_().extent_.width, img.get_().extent_.height };
  }

  VkRenderingInfoKHR rendering_info = 
  {
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
    .renderArea = rect_,
    // TODO: Don't hardcode 1 layer count
    .layerCount = 1,
    .colorAttachmentCount = color_attachment_count,
    .pColorAttachments = color_attachments,
    .pDepthAttachment = depth_attachment
  };

  vkCmdBeginRenderingKHR_proc(cmdbuf, &rendering_info);

  // Issue draw calls
  VkViewport viewport = 
  {
    .width = (float)rect_.extent.width, 
    .height = (float)rect_.extent.height, .maxDepth = 1.0f, .minDepth = 0.0f
  };

  vkCmdSetViewport(cmdbuf, 0, 1, &viewport);
  vkCmdSetScissor(cmdbuf, 0, 1, &rect_);
  draw_commands_proc_({ cmdbuf, rect_, builder_, draw_commands_aux_ });

  vkCmdEndRenderingKHR_proc(cmdbuf);
}

void render_pass::draw_commands_(draw_commands_proc draw_proc, void *aux) 
{
  draw_commands_proc_ = draw_proc;
  draw_commands_aux_ = aux;
}

render_pass &render_pass::prepare_commands(
  prepare_commands_proc prepare_proc, void *aux)
{
  prepare_commands_proc_ = prepare_proc;
  prepare_commands_aux_ = aux;

  return *this;
}



/************************* Compute Pass ***************************/
compute_pass::compute_pass(render_graph *builder, const uid_string &uid) 
  : builder_(builder), uid_(uid),
  push_constant_(nullptr),
  push_constant_size_(0),
  pipeline_(VK_NULL_HANDLE) 
{
}

compute_pass &compute_pass::set_source(const char *src_path) 
{
  src_path_ = src_path;
  return *this;
}

compute_pass &compute_pass::add_sampled_image(const uid_string &uid) 
{
  uint32_t binding_id = bindings_.size();

  binding b = { 
    (uint32_t)bindings_.size(), binding::type::sampled_image, uid.id 
  };

  bindings_.push_back(b);

  // Get image will allocate space for the image struct if it hasn't 
  // been created yet
  gpu_image &img = builder_->get_image_(uid.id);
  img.add_usage_node_(uid_.id, binding_id);

  return *this;
}

compute_pass &compute_pass::add_storage_image(const uid_string &uid, const image_info &i) 
{
  uint32_t binding_id = bindings_.size();

  binding b = {
    (uint32_t)bindings_.size(), binding::type::storage_image, uid.id 
  };

  bindings_.push_back(b);

  gpu_image &img = builder_->get_image_(uid.id);
  img.add_usage_node_(uid_.id, binding_id);
  img.configure(i);

  return *this;
}

compute_pass &compute_pass::add_storage_buffer(const uid_string &uid) 
{
  uint32_t binding_id = bindings_.size();

  binding b = {
    (uint32_t)bindings_.size(), binding::type::storage_buffer, uid.id 
  };

  bindings_.push_back(b);

  gpu_buffer &buf = builder_->get_buffer_(uid.id);
  buf.add_usage_node_(uid_.id, binding_id);

  return *this;
}

compute_pass &compute_pass::add_uniform_buffer(const uid_string &uid) 
{
  uint32_t binding_id = bindings_.size();

  binding b = {
    (uint32_t)bindings_.size(), binding::type::uniform_buffer, uid.id 
  };

  bindings_.push_back(b);

  gpu_buffer &buf = builder_->get_buffer_(uid.id);
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
  uint32_t wave_x, uint32_t wave_y, uint32_t wave_z, const uid_string &s) 
{
  dispatch_params_.x = wave_x;
  dispatch_params_.y = wave_y;
  dispatch_params_.z = wave_z;
  dispatch_params_.is_waves = true;

  // Works for images
  dispatch_params_.binding_res = s.id;

  return *this;
}

void compute_pass::reset_() 
{
  // Should keep capacity the same to no reallocs after the first time 
  // adding the compute pass
  bindings_.clear();
}

void compute_pass::create_() 
{
  VkPushConstantRange push_constant_range = {};
  push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
  push_constant_range.offset = 0;
  push_constant_range.size = push_constant_size_;

  // Pipeline layout TODO: Support descriptors with count>1
  VkDescriptorSetLayout *layouts = stack_alloc(
    VkDescriptorSetLayout, bindings_.size());
  for (u32 i = 0; i < bindings_.size(); ++i)
    layouts[i] = get_descriptor_set_layout(
      bindings_[i].get_descriptor_type(), 1);

  VkPipelineLayoutCreateInfo pipeline_layout_info = {};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount = bindings_.size();
  pipeline_layout_info.pSetLayouts = layouts;

  if (push_constant_size_) 
  {
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;
  }

  VK_CHECK(vkCreatePipelineLayout(
    gctx->device, &pipeline_layout_info, nullptr, &layout_));

  // Shader stage
  heap_array<u8> src_bytes = file(
    make_shader_src_path(src_path_, VK_SHADER_STAGE_COMPUTE_BIT), 
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
  compute_pipeline_info.layout = layout_;

  VK_CHECK(vkCreateComputePipelines(gctx->device, VK_NULL_HANDLE, 1, 
    &compute_pipeline_info, nullptr, &pipeline_));
}

// This also needs to issue all synchronization stuff that may be needed
void compute_pass::issue_commands_(VkCommandBuffer cmdbuf) 
{
  // Loop through all bindings (make sure images and buffers have proper 
  // barriers issued for them) This is a very rough estimate - TODO: Make sure 
  // to have exact number of image bindings
  auto *img_barriers = bump_mem_alloc<VkImageMemoryBarrier>(bindings_.size());
  auto *buf_barriers = bump_mem_alloc<VkBufferMemoryBarrier>(bindings_.size());

  u32 img_barrier_count = 0, buf_barrier_count = 0;

  VkDescriptorSet *descriptor_sets = bump_mem_alloc<VkDescriptorSet>(
    bindings_.size());

  int i = 0;
  // Once all barriers have been issued, we can dispatch the pipeline!
  for (auto b : bindings_) 
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

  vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_);
  vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, layout_, 
    0, bindings_.size(), descriptor_sets, 0, nullptr);

  if (push_constant_size_)
    vkCmdPushConstants(cmdbuf, layout_, VK_SHADER_STAGE_COMPUTE_BIT, 
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



/************************* Graph Pass *****************************/
graph_pass::graph_pass()
: type_(graph_pass::type::none) 
{
}

graph_pass::graph_pass(const render_pass &pass) 
: rp_(pass), type_(graph_render_pass) 
{
}

graph_pass::graph_pass(const compute_pass &pass)
: cp_(pass), type_(graph_compute_pass) 
{
}

graph_pass::type graph_pass::get_type() 
{
  return type_;
}

render_pass &graph_pass::get_render_pass() 
{
  return rp_;
}

compute_pass &graph_pass::get_compute_pass() 
{
  return cp_;
}

bool graph_pass::is_initialized() 
{
  return (type_ != graph_pass::type::none);
}

binding &graph_pass::get_binding(uint32_t idx) 
{
  switch (type_) 
  {
  case type::graph_compute_pass: 
  {
    return cp_.bindings_[idx];
  } break;

  case type::graph_render_pass: 
  {
    return rp_.bindings_[idx];
  } break;

  default: 
    assert(false);
    return cp_.bindings_[idx];
  }
}



/************************* Resource *******************************/
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
  else if (action_ == action_flag::to_present) 
  {
    reference_ = builder_->get_current_swapchain_ref_();

    // The image to present needs to also have descriptors created for it
    builder_->get_image_(reference_).create_descriptors_(usage_);
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
  // This means that we inherit swapchain resolution
  if (info.extent.width == 0 && info.extent.height == 0 && 
    info.extent.depth == 0) 
  {
    gpu_image &swapchain_img = builder_->get_image_(
      render_graph::swapchain_uids[0].id);
    extent_ = swapchain_img.extent_;
  }
  else 
  {
    extent_ = info.extent;
  }

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
      format_ = gctx->swapchain_format;
    }
  }
  else 
  {
    format_ = info.format;
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
  VkBufferCreateInfo buffer_create_info = 
  {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
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



graph_resource::graph_resource()
: type_(graph_resource::type::none), was_used_(false) 
{
}

graph_resource::graph_resource(const gpu_image &img) 
: type_(type::graph_image), img_(img), was_used_(false) 
{
}

graph_resource::graph_resource(const gpu_buffer &buf) 
: type_(type::graph_buffer), buf_(buf), was_used_(false) 
{
}

/************************* Transfer *******************************/
transfer_operation::transfer_operation()
: type_(type::none) 
{
}

transfer_operation::transfer_operation(
  graph_stage_ref ref, render_graph *builder) 
: type_(type::none), builder_(builder), stage_ref_(ref) 
{
}

void transfer_operation::init_as_buffer_update(
  graph_resource_ref buf_ref, void *data, uint32_t offset, uint32_t size) 
{
  type_ = type::buffer_update;
  binding b = { 0, binding::type::buffer_transfer_dst, buf_ref };

  bindings_ = bump_mem_alloc<binding>();
  *bindings_ = b;

  buffer_update_state_.data = data;
  buffer_update_state_.offset = offset;
  buffer_update_state_.size = size;

  gpu_buffer &buf = builder_->get_buffer_(buf_ref);
  buf.add_usage_node_(stage_ref_, 0);

  if (size == 0) 
  {
    buffer_update_state_.size = buf.size_;
  }
}

void transfer_operation::init_as_buffer_copy_to_cpu(
  graph_resource_ref dst, graph_resource_ref src)
{
  type_ = type::buffer_copy_to_cpu;
  binding b0 = { 0, binding::type::buffer_transfer_dst, dst };
  binding b1 = { 1, binding::type::buffer_transfer_dst, src };

  bindings_ = bump_mem_alloc<binding>(2);
  bindings_[0] = b0;
  bindings_[1] = b1;

  buffer_copy_to_cpu_.dst = dst;
  buffer_copy_to_cpu_.dst = src;

  gpu_buffer &buf0 = builder_->get_buffer_(dst);
  buf0.add_usage_node_(stage_ref_, 0);

  // Make sure the dst buffer is host visible
  buf0.configure({.host_visible = true});

  gpu_buffer &buf1 = builder_->get_buffer_(src);
  buf1.add_usage_node_(stage_ref_, 1);
}

void transfer_operation::init_as_image_blit(
  graph_resource_ref src, graph_resource_ref dst) 
{
  type_ = type::image_blit;
  binding b_src = { 0, binding::type::image_transfer_src, src };
  binding b_dst = { 1, binding::type::image_transfer_dst, dst };

  bindings_ = bump_mem_alloc<binding>(2);
  bindings_[0] = b_src;
  bindings_[1] = b_dst;

  gpu_image &src_img = builder_->get_image_(src);
  src_img.add_usage_node_(stage_ref_, 0);

  gpu_image &dst_img = builder_->get_image_(dst);
  dst_img.add_usage_node_(stage_ref_, 1);
}

binding &transfer_operation::get_binding(uint32_t idx) 
{
  return bindings_[idx];
}



/************************* Resource Synchronizatin ****************/
graph_resource_tracker::graph_resource_tracker(
  VkCommandBuffer cmdbuf, render_graph *builder)
: builder_(builder), cmdbuf_(cmdbuf) 
{
}

void graph_resource_tracker::prepare_buffer_for(
  const uid_string &uid, binding::type type, VkPipelineStageFlags stage) 
{
  gpu_buffer &buf = builder_->get_buffer_(uid.id);

  binding b = { .utype = type };

  VkBufferMemoryBarrier barrier = 
  {
    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    .size = buf.size_,
    .buffer = buf.buffer_,
    .offset = 0,
    .srcAccessMask = buf.current_access_,
    .dstAccessMask = b.get_buffer_access(),
  };

  vkCmdPipelineBarrier(
    cmdbuf_, buf.last_used_, stage, 0, 0, nullptr, 1, &barrier, 0, nullptr);

  buf.current_access_ = b.get_buffer_access();
  buf.last_used_ = stage;
}

gpu_buffer &graph_resource_tracker::get_buffer(const uid_string &uid) 
{
  return builder_->get_buffer_(uid.id);
}



/************************* Graph Builder **************************/
render_graph::render_graph() 
: present_generator_(2) 
{
}

uint32_t render_graph::get_res_uid()
{
  return rdg_name_id_counter++;
}

uint32_t render_graph::get_stg_uid()
{
  return stg_name_id_counter++;
}

void render_graph::setup_swapchain(const swapchain_info &swapchain) 
{
  // Setup swapchain targets (we assume that there won't be more than 5 
  // swapchain targets)
  swapchain_uids[0] = RES("swapchain-target-0");
  swapchain_uids[1] = RES("swapchain-target-1");
  swapchain_uids[2] = RES("swapchain-target-2");
  swapchain_uids[3] = RES("swapchain-target-3");
  swapchain_uids[4] = RES("swapchain-target-4");

  for (int i = 0; i < 5; ++i) 
  {
    gpu_image &img = get_image_(swapchain_uids[i].id);
    img.image_ = swapchain.images[i];
    img.image_view_ = swapchain.image_views[i];
    img.extent_ = { swapchain.extent.width, swapchain.extent.height, 1 };
    img.aspect_ = VK_IMAGE_ASPECT_COLOR_BIT;
    img.format_ = gctx->swapchain_format;
  }

  get_image_(swapchain_uids[0].id);
  get_image_(swapchain_uids[1].id);
  get_image_(swapchain_uids[2].id);
  get_image_(swapchain_uids[3].id);
  get_image_(swapchain_uids[4].id);
}

void render_graph::register_swapchain(const graph_swapchain_info &swp) 
{
  // resources_.push_back();
}

gpu_buffer &render_graph::register_buffer(const uid_string &uid) 
{
  return get_buffer_(uid.id);
}

render_pass &render_graph::add_render_pass(const uid_string &uid) 
{
  if (passes_.size() <= uid.id) 
  {
    // Reallocate the vector such that the new pass will fit
    passes_.resize(uid.id + 1);
  }

  if (!passes_[uid.id].is_initialized())
    new (&passes_[uid.id]) graph_pass(render_pass(this, uid));

  recorded_stages_.push_back(uid.id);

  return get_render_pass_(uid.id);
}

compute_pass &render_graph::add_compute_pass(const uid_string &uid) 
{
  if (passes_.size() <= uid.id) 
  {
    // Reallocate the vector such that the new pass will fit
    passes_.resize(uid.id + 1);
  }

  if (!passes_[uid.id].is_initialized())
    new (&passes_[uid.id]) graph_pass(compute_pass(this, uid));

  recorded_stages_.push_back(uid.id);

  return get_compute_pass_(uid.id);
}

void render_graph::add_buffer_update(
  const uid_string &uid, void *data, u32 offset, u32 size) 
{
  graph_stage_ref ref (graph_stage_ref::type::transfer, transfers_.size());

  transfers_.push_back(transfer_operation(ref, this));

  transfer_operation &transfer = transfers_[transfers_.size() - 1];
  transfer.init_as_buffer_update(uid.id, data, offset, size);

  recorded_stages_.push_back(ref);
}

void render_graph::add_buffer_copy_to_cpu(
  const uid_string &dst, const uid_string &src)
{
  graph_stage_ref ref (graph_stage_ref::type::transfer, transfers_.size());

  transfers_.push_back(transfer_operation(ref, this));

  transfer_operation &transfer = transfers_[transfers_.size() - 1];
  transfer.init_as_buffer_copy_to_cpu(dst.id, src.id);

  recorded_stages_.push_back(ref);
}

void render_graph::add_image_blit(const uid_string &src, const uid_string &dst) 
{
  graph_stage_ref ref (graph_stage_ref::type::transfer, transfers_.size());

  transfers_.push_back(transfer_operation(ref, this));

  transfer_operation &transfer = transfers_[transfers_.size() - 1];
  transfer.init_as_image_blit(src.id, dst.id);

  recorded_stages_.push_back(ref);
}

void render_graph::begin() 
{
  present_info_.is_active = false;

  // Looping through resource IDs (ID of resources that were used in the frame)
  for (auto r : used_resources_) 
  {
    resources_[r].was_used_ = false;

    switch (resources_[r].get_type()) 
    {
    case graph_resource::type::graph_image: 
    {
      resources_[r].get_image().head_node_ =
        { invalid_graph_ref, invalid_graph_ref };
      resources_[r].get_image().tail_node_ =
        { invalid_graph_ref, invalid_graph_ref };
    } break;

    case graph_resource::type::graph_buffer: 
    {
      resources_[r].get_buffer().head_node_ =
        { invalid_graph_ref, invalid_graph_ref };
      resources_[r].get_buffer().tail_node_ =
        { invalid_graph_ref, invalid_graph_ref };
    } break;

    default: break;
    }
  }

  for (auto stg : recorded_stages_) 
  {
    if (stg.stage_type == graph_stage_ref::type::pass) 
    {
      if (stg == graph_stage_ref_present) 
        continue;

      switch (passes_[stg].get_type()) 
      {
        case graph_pass::graph_compute_pass: 
        {
          compute_pass &cp = passes_[stg].get_compute_pass();
          cp.bindings_.clear();
        } break;

        case graph_pass::graph_render_pass: 
        {
          render_pass &rp = passes_[stg].get_render_pass();
          rp.bindings_.clear();
          rp.depth_index_ = -1;
        } break;

        default: break;
      }
    }
    else if (stg.stage_type == graph_stage_ref::type::transfer) 
    {
      transfers_[stg].bindings_ = nullptr;
    }
  }

  recorded_stages_.clear();
  used_resources_.clear();
  recorded_stages_.clear();
  transfers_.clear();

  present_info_.is_active = false;
}

void render_graph::prepare_pass_graph_stage_(graph_stage_ref stg) 
{
  if (stg == graph_stage_ref_present) 
  {
    // Handle present case
    gpu_image &swapchain_target = get_image_(present_info_.to_present);

    // This may override the action that was previously set from the looping 
    // through bindings
    swapchain_target.action_ = gpu_image::action_flag::to_present;

    // Very unlikely that this will pass but here just in case we want to 
    // present a disk-loaded texture to the screen
    if (!get_resource_(present_info_.to_present).was_used_) 
    {
      used_resources_.push_back(present_info_.to_present);
      get_resource_(present_info_.to_present).was_used_ = true;
    }
  }
  else 
  {
    // Handle render pass / compute pass case
    switch (passes_[stg].get_type()) 
    {
    case graph_pass::graph_compute_pass: 
    {
      compute_pass &cp = passes_[stg].get_compute_pass();

      // Loop through each binding
      for (auto &bind : cp.bindings_) {
        auto &res = get_resource_(bind.rref);
        switch (res.get_type()) {
          case graph_resource::type::graph_image: 
            res.get_image().update_action_(bind); break;
          case graph_resource::type::graph_buffer: 
            res.get_buffer().update_action_(bind); break;
          default: break;
        }

        if (!res.was_used_) {
          res.was_used_ = true;
          used_resources_.push_back(bind.rref);
        }
      }
    } break;

    case graph_pass::graph_render_pass: 
    {
      render_pass &rp = passes_[stg].get_render_pass();

      // Loop through each binding
      for (auto &bind : rp.bindings_) {
        auto &res = get_resource_(bind.rref);
        switch (res.get_type()) {
          case graph_resource::type::graph_image: 
            res.get_image().update_action_(bind); break;
          default: assert(false); break;
        }

        if (!res.was_used_) {
          res.was_used_ = true;
          used_resources_.push_back(bind.rref);
        }
      }
    } break;

    default: break;
    }
  }
}

void render_graph::prepare_transfer_graph_stage_(graph_stage_ref ref) 
{
  transfer_operation &op = transfers_[ref.stage_idx];

  switch (op.type_) 
  {
  case transfer_operation::type::buffer_update: {
    auto &bind = op.get_binding(0);
    auto &res = get_resource_(bind.rref);
    res.get_buffer().update_action_(bind);

    if (!res.was_used_) 
    {
      res.was_used_ = true;
      used_resources_.push_back(bind.rref);
    }
  } break;

  case transfer_operation::type::buffer_copy_to_cpu: {
    { // Dst
      auto &bind = op.get_binding(0);

      auto &res = get_resource_(bind.rref);
      res.get_buffer().update_action_(bind);

      if (!res.was_used_) 
      {
        res.was_used_ = true;
        used_resources_.push_back(bind.rref);
      }
    }

    { // Src
      auto &bind = op.get_binding(1);

      auto &res = get_resource_(bind.rref);
      res.get_buffer().update_action_(bind);

      if (!res.was_used_) 
      {
        res.was_used_ = true;
        used_resources_.push_back(bind.rref);
      }
    }
  } break;

  case transfer_operation::type::image_blit: 
  {
    { // Src
      auto &bind = op.get_binding(0);

      auto &res = get_resource_(bind.rref);
      res.get_image().update_action_(bind);

      if (!res.was_used_) 
      {
        res.was_used_ = true;
        used_resources_.push_back(bind.rref);
      }
    }

    { // Dst
      auto &bind = op.get_binding(1);

      auto &res = get_resource_(bind.rref);
      res.get_image().update_action_(bind);

      if (!res.was_used_) 
      {
        res.was_used_ = true;
        used_resources_.push_back(bind.rref);
      }
    }
  } break;

  default: break;
  }
}

void render_graph::execute_pass_graph_stage_(
  graph_stage_ref stg, VkPipelineStageFlags &last_stage,
  const cmdbuf_generator::cmdbuf_info &info) 
{
  if (stg == graph_stage_ref_present) 
  {
    gpu_image &img = get_image_(present_info_.to_present);

    // Handle present stage - transition image layout
    VkImageMemoryBarrier barrier = 
    {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .image = img.get_().image_,
      .oldLayout = img.get_().current_layout_,
      .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      .srcAccessMask = img.get_().current_access_,
      .dstAccessMask = 0,
      .subresourceRange.aspectMask = img.get_().aspect_,
      .subresourceRange.baseArrayLayer = 0,
      .subresourceRange.baseMipLevel = 0,
      .subresourceRange.layerCount = 1,
      .subresourceRange.levelCount = 1
    };

    vkCmdPipelineBarrier(info.cmdbuf, img.get_().last_used_,
      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    // Update image data
    img.get_().current_layout_ = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    img.get_().current_access_ = 0;
    img.get_().last_used_ = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  }
  else 
  {
    // Handle compute / render passes
    switch (passes_[stg].get_type()) 
    {
    case graph_pass::graph_compute_pass: 
    {
      last_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

      compute_pass &cp = passes_[stg].get_compute_pass();

      if (cp.pipeline_ == VK_NULL_HANDLE) 
      {
        // Actually initialize the compute pipeline
        cp.create_();
      }

      // Issue the commands
      cp.issue_commands_(info.cmdbuf);
    } break;

    case graph_pass::graph_render_pass: 
    {
      last_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

      render_pass &rp = passes_[stg].get_render_pass();

      rp.issue_commands_(info.cmdbuf);
    } break;

    default: break;
    }
  }
}

void render_graph::execute_transfer_graph_stage_(
  graph_stage_ref ref, const cmdbuf_generator::cmdbuf_info &info) 
{
  transfer_operation &op = transfers_[ref.stage_idx];
  switch (op.type_) 
  {
  case transfer_operation::type::buffer_update: 
  {
    binding &b = op.bindings_[0];
    gpu_buffer &buf = get_buffer_(b.rref);

    VkBufferMemoryBarrier barrier = 
    {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .buffer = buf.buffer_,
      .srcAccessMask = buf.current_access_,
      .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .offset = op.buffer_update_state_.offset,
      .size = op.buffer_update_state_.size,
    };

    assert(buf.buffer_ != VK_NULL_HANDLE);

    vkCmdPipelineBarrier(info.cmdbuf, buf.last_used_,
      VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);

    vkCmdUpdateBuffer(
      info.cmdbuf, buf.buffer_, op.buffer_update_state_.offset, 
      op.buffer_update_state_.size, op.buffer_update_state_.data);

    buf.last_used_ = VK_PIPELINE_STAGE_TRANSFER_BIT;
    buf.current_access_ = VK_ACCESS_TRANSFER_WRITE_BIT;
  } break;

  case transfer_operation::type::buffer_copy_to_cpu:
  {
    binding &dst_binding = op.bindings_[0];
    binding &src_binding = op.bindings_[1];

    gpu_buffer &dst = get_buffer_(dst_binding.rref);
    gpu_buffer &src = get_buffer_(src_binding.rref);

    VkBufferMemoryBarrier dst_barrier =
    {
      .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
      .buffer = dst.buffer_,
      .srcAccessMask = dst.current_access_,
      .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .offset = 0,
      .size = dst.size_
    };

    auto src_barrier = dst_barrier;
    src_barrier.buffer = src.buffer_;
    src_barrier.srcAccessMask = src.current_access_;
    src_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(info.cmdbuf, dst.last_used_,
      VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &dst_barrier, 0, nullptr);
    vkCmdPipelineBarrier(info.cmdbuf, src.last_used_,
      VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &src_barrier, 0, nullptr);

    VkBufferCopy region = {
      .size = dst.size_,
      .srcOffset = 0,
      .dstOffset = 0
    };

    vkCmdCopyBuffer(info.cmdbuf, src.buffer_, dst.buffer_, 1, &region);

    dst.last_used_ = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst.current_access_ = VK_ACCESS_TRANSFER_WRITE_BIT;

    src.last_used_ = VK_PIPELINE_STAGE_TRANSFER_BIT;
    src.current_access_ = VK_ACCESS_TRANSFER_READ_BIT;
  } break;

  case transfer_operation::type::image_blit: 
  {
    gpu_image &src = get_image_(op.bindings_[0].rref);
    gpu_image &dst = get_image_(op.bindings_[1].rref);

    // Transition layouts
    VkImageMemoryBarrier barrier = 
    {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .image = src.get_().image_,
      .oldLayout = src.get_().current_layout_,
      .newLayout = op.bindings_[0].get_image_layout(),
      .srcAccessMask = src.get_().current_access_,
      .dstAccessMask = op.bindings_[0].get_image_access(),
      .subresourceRange.aspectMask = src.get_().aspect_,
      // TODO: Support non-hardcoded values for this
      .subresourceRange.baseArrayLayer = 0,
      .subresourceRange.baseMipLevel = 0,
      .subresourceRange.layerCount = 1,
      .subresourceRange.levelCount = 1
    };

    vkCmdPipelineBarrier(info.cmdbuf, src.get_().last_used_, 
      VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    barrier.image = dst.get_().image_;
    barrier.oldLayout = dst.get_().current_layout_;
    barrier.newLayout = op.bindings_[1].get_image_layout();
    barrier.srcAccessMask = dst.get_().current_access_;
    barrier.dstAccessMask = op.bindings_[1].get_image_access();

    vkCmdPipelineBarrier(info.cmdbuf, dst.get_().last_used_,
      VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

    VkImageBlit region = 
    {
      .srcOffsets = { {}, { (int32_t)src.get_().extent_.width, 
        (int32_t)src.get_().extent_.height, (int32_t)src.get_().extent_.depth}},
      .dstOffsets = { {}, { (int32_t)dst.get_().extent_.width, 
        (int32_t)dst.get_().extent_.height, (int32_t)dst.get_().extent_.depth}},
      .srcSubresource = 
      {
        .layerCount = 1, .baseArrayLayer = 0,
        .aspectMask = src.get_().aspect_, .mipLevel = 0
      },
      .dstSubresource = 
      {
        .layerCount = 1, .baseArrayLayer = 0,
        .aspectMask = dst.get_().aspect_, .mipLevel = 0
      }
    };

    src.get_().current_layout_ = op.bindings_[0].get_image_layout();
    src.get_().current_access_ = op.bindings_[0].get_image_access();
    src.get_().last_used_ = VK_PIPELINE_STAGE_TRANSFER_BIT;

    dst.get_().current_layout_ = op.bindings_[1].get_image_layout();
    dst.get_().current_access_ = op.bindings_[1].get_image_access();
    dst.get_().last_used_ = VK_PIPELINE_STAGE_TRANSFER_BIT;

    vkCmdBlitImage(
      info.cmdbuf, 
      src.get_().image_, src.get_().current_layout_, 
      dst.get_().image_, dst.get_().current_layout_, 
      1, &region, VK_FILTER_LINEAR);
  } break;

  default: break;
  }
}

job render_graph::end(cmdbuf_generator *generator) 
{
#if 0
  // Determine which generator to use
  if (!generator) 
  {
    if (present_info_.is_active) 
      generator = &present_generator_;
    else 
      generator = &single_generator_;
  }

  // Generate the command buffer
  cmdbuf_generator::cmdbuf_info info = generator->get_command_buffer();
  current_cmdbuf_ = info.cmdbuf;
#endif
  cmdbuf_generator::cmdbuf_info info;
  info.cmdbuf = current_cmdbuf_ = get_command_buffer_();

  VkCommandBufferBeginInfo begin_info = 
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
  };

  vkBeginCommandBuffer(current_cmdbuf_, &begin_info);

  // swapchain_img_idx_ = info.swapchain_idx;

  // First traverse through all stages in order to figure out resources to use
  for (auto stg : recorded_stages_) 
  {
    if (stg.stage_type == graph_stage_ref::type::pass) 
      prepare_pass_graph_stage_(stg);
    else if (stg.stage_type == graph_stage_ref::type::transfer) 
      prepare_transfer_graph_stage_(stg);
  }

  // Loop through all used resources
  for (auto rref : used_resources_) 
  {
    graph_resource &res = resources_[rref];

    switch (res.get_type()) 
    {
    case graph_resource::type::graph_image:
      res.get_image().apply_action_();
      break;

    case graph_resource::type::graph_buffer:
      res.get_buffer().apply_action_();
      break;

    default:
      break;
    }
  }

  VkPipelineStageFlags last_stage = 0;

  // Now loop through the passes and actually issue the commands!
  for (auto stg : recorded_stages_) 
  {
    if (stg.stage_type == graph_stage_ref::type::pass) 
      execute_pass_graph_stage_(stg, last_stage, info);
    else if (stg.stage_type == graph_stage_ref::type::transfer) 
      execute_transfer_graph_stage_(stg, info);
  }

  vkEndCommandBuffer(current_cmdbuf_);
  // generator->submit_command_buffer(info, last_stage);

  return job(info.cmdbuf, last_stage, this);
}

submission *render_graph::get_successful_submission_()
{
  for (auto &sub : submissions_)
  {
    if (sub.ref_count_ == 0)
    {
      VkResult res = vkGetFenceStatus(gctx->device, sub.fence_);
      if (res == VK_SUCCESS)
        return &sub;
    }
  }

  return nullptr;
}

void render_graph::recycle_submissions_()
{
  auto *sub = get_successful_submission_();

  if (sub)
  {
    // Recycle all the stuff
    free_fences_.insert(sub->fence_);
    free_semaphores_.insert(free_semaphores_.end(), sub->semaphores_.begin(), sub->semaphores_.end());
    free_cmdbufs_.insert(free_cmdbufs_.end(), sub->cmdbufs_.begin(), sub->cmdbufs_.end());
  }

  if (submissions_.size() > 1)
  {
    int index = sub - submissions_.data();
    std::iter_swap(submissions_.begin() + index, submissions_.end() - 1);
    submissions_.pop_back();
  }
  else
  {
    submissions_.clear();
  }
}

VkFence render_graph::get_fence_()
{
  recycle_submissions_();

  if (free_fences_.size())
  {
    auto iter = free_fences_.begin();
    VkFence ret = *iter;
    free_fences_.erase(iter);
    return ret;
  }
  else
  {
    VkFence fence;
    VkFenceCreateInfo fence_info = {};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Set to signaled because we reset the fence when submitting commands
    vkCreateFence(gctx->device, &fence_info, nullptr, &fence);
    return fence;
  }
}

VkSemaphore render_graph::get_semaphore_()
{
  recycle_submissions_();

  if (free_semaphores_.size())
  {
    VkSemaphore ret = free_semaphores_.back();
    free_semaphores_.pop_back();
    return ret;
  }
  else
  {
    VkSemaphore semaphore;
    VkSemaphoreCreateInfo semaphore_info = {};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    vkCreateSemaphore(gctx->device, &semaphore_info, nullptr, &semaphore);
    return semaphore;
  }
}

VkCommandBuffer render_graph::get_command_buffer_()
{
  recycle_submissions_();

  if (free_cmdbufs_.size())
  {
    VkCommandBuffer ret = free_cmdbufs_.back();
    free_cmdbufs_.pop_back();
    return ret;
  }
  else
  {
    VkCommandBuffer command_buffer;
    VkCommandBufferAllocateInfo command_buffer_info = {};
    command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_info.commandBufferCount = 1;
    command_buffer_info.commandPool = gctx->command_pool;
    command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    vkAllocateCommandBuffers(
      gctx->device, &command_buffer_info, &command_buffer);
    return command_buffer;
  }
}

pending_workload render_graph::submit(job *jobs, int count,
  job *dependencies, int dependency_count)
{
  VkCommandBuffer *jobs_raw = stack_alloc(VkCommandBuffer, count);
  VkSemaphore *signal_raw = stack_alloc(VkSemaphore, count);
  for (int i = 0; i < count; ++i)
    (jobs_raw[i] = jobs[i].cmdbuf_), (signal_raw[i] = jobs[i].finished_semaphore_);

  uint32_t wait_count = 0;
  VkSemaphore *wait_raw = stack_alloc(VkSemaphore, dependency_count);
  VkPipelineStageFlags *end_stages = stack_alloc(VkPipelineStageFlags, dependency_count);

  for (int i = 0; i < dependency_count; ++i)
  {
    if (dependencies[i].submission_idx_ >= 0)
    {
      submission &s = submissions_[dependencies[i].submission_idx_];
      if (VK_SUCCESS == vkGetFenceStatus(gctx->device, s.fence_))
      {
        submissions_[dependencies[i].submission_idx_].ref_count_--;
        dependencies[i].submission_idx_ = -1;
      }
      else
      {
        ++wait_count;
        (wait_raw[i] = dependencies[i].finished_semaphore_), (end_stages[i] = dependencies[i].end_stage_);
      }
    }
  }

  VkSubmitInfo info = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = (uint32_t)count,
    .pCommandBuffers = jobs_raw,
    .waitSemaphoreCount = (uint32_t)wait_count,
    .pWaitSemaphores = wait_raw,
    .pWaitDstStageMask = end_stages,
    .signalSemaphoreCount = (uint32_t)count,
    .pSignalSemaphores = signal_raw
  };

  VkFence fence = get_fence_();
  vkResetFences(gctx->device, 1, &fence);

  vkQueueSubmit(gctx->graphics_queue, 1, &info, fence);

  submission workload;
  workload.fence_ = fence;
  workload.semaphores_.resize(count);
  workload.cmdbufs_.resize(count);
  workload.ref_count_ = count;
  for (int i = 0; i < count; ++i)
  {
    workload.semaphores_[i] = signal_raw[i];
    workload.cmdbufs_[i] = jobs_raw[i];
    jobs[i].submission_idx_ = submissions_.size();
  }

  submissions_.push_back(std::move(workload));

  pending_workload ret;
  ret.fence_ = fence;
  return ret;
}

gpu_buffer &render_graph::get_buffer(const uid_string &uid)
{
  return get_buffer_(uid.id);
}

void render_graph::present(const uid_string &res_uid) 
{
  present_info_.to_present = res_uid.id;
  present_info_.is_active = true;

  gpu_image &img = get_image_(res_uid.id);
  img.reference_ = graph_stage_ref_present;
  img.add_usage_node_(graph_stage_ref_present, 0);

  recorded_stages_.push_back(graph_stage_ref_present);
}

graph_resource_tracker render_graph::get_resource_tracker() 
{
  return graph_resource_tracker(current_cmdbuf_, this);
}

// Cmdbuf generators...
present_cmdbuf_generator::present_cmdbuf_generator(u32 frames_in_flight)
: max_frames_in_flight_(frames_in_flight), 
image_ready_semaphores_(frames_in_flight),
render_finished_semaphores_(frames_in_flight),
fences_(frames_in_flight),
command_buffers_(gctx->images.size()),
current_frame_(0) 
{
#if 0
  VkSemaphoreCreateInfo semaphore_info = {};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence_info = {};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (u32 i = 0; i < max_frames_in_flight_; ++i) 
  {
    vkCreateSemaphore(
      gctx->device, &semaphore_info, nullptr, &image_ready_semaphores_[i]);
    vkCreateSemaphore(
      gctx->device, &semaphore_info, nullptr, &render_finished_semaphores_[i]);
    vkCreateFence(gctx->device, &fence_info, nullptr, &fences_[i]);
  }

  // Command buffers
  VkCommandBufferAllocateInfo command_buffer_info = {};
  command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_buffer_info.commandBufferCount = command_buffers_.size();
  command_buffer_info.commandPool = gctx->command_pool;
  command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  vkAllocateCommandBuffers(
    gctx->device, &command_buffer_info, command_buffers_.data());
#endif
}

cmdbuf_generator::cmdbuf_info present_cmdbuf_generator::get_command_buffer() 
{
  u32 swapchain_image_idx = acquire_next_swapchain_image(
    image_ready_semaphores_[current_frame_]);
  vkWaitForFences(gctx->device, 1, &fences_[current_frame_], true, UINT64_MAX);
  vkResetFences(gctx->device, 1, &fences_[current_frame_]);

  VkCommandBuffer current_command_buffer =
    command_buffers_[swapchain_image_idx];

  VkCommandBufferBeginInfo begin_info = 
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
  };

  vkBeginCommandBuffer(current_command_buffer, &begin_info);

  return 
  { 
    .swapchain_idx = swapchain_image_idx,
    .cmdbuf = current_command_buffer,
    .frame_idx = current_frame_ 
  };
}

void present_cmdbuf_generator::submit_command_buffer(
  const cmdbuf_generator::cmdbuf_info &cmd, VkPipelineStageFlags stage) 
{
  vkEndCommandBuffer(cmd.cmdbuf);

  VkSubmitInfo info = {};
  info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  info.commandBufferCount = 1;
  info.pCommandBuffers = &cmd.cmdbuf;
  info.waitSemaphoreCount = 1;
  info.pWaitSemaphores = &image_ready_semaphores_[current_frame_];
  info.signalSemaphoreCount = 1;
  info.pSignalSemaphores = &render_finished_semaphores_[current_frame_];
  info.pWaitDstStageMask = &stage;
  vkQueueSubmit(gctx->graphics_queue, 1, &info, fences_[current_frame_]);

  // Present to screen
  present_swapchain_image(
    render_finished_semaphores_[current_frame_], cmd.swapchain_idx);
  current_frame_ = (current_frame_ + 1) % max_frames_in_flight_;
}

cmdbuf_generator::cmdbuf_info single_cmdbuf_generator::get_command_buffer() 
{
  VkCommandBuffer cmdbuf;

  VkCommandBufferAllocateInfo command_buffer_info = {};
  command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  command_buffer_info.commandBufferCount = 1;
  command_buffer_info.commandPool = gctx->command_pool;
  command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  vkAllocateCommandBuffers(gctx->device, &command_buffer_info, &cmdbuf);

  VkCommandBufferBeginInfo begin_info = 
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
  };

  vkBeginCommandBuffer(cmdbuf, &begin_info);

  return {.cmdbuf = cmdbuf};
}

void single_cmdbuf_generator::submit_command_buffer(
  const cmdbuf_info &info, VkPipelineStageFlags stage) 
{
  vkEndCommandBuffer(info.cmdbuf);

  VkSubmitInfo submit_info = 
  {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &info.cmdbuf,
    .waitSemaphoreCount = 0,
    .signalSemaphoreCount = 0,
    .pWaitDstStageMask = nullptr
  };

  vkQueueSubmit(gctx->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
}

job::job(VkCommandBuffer cmdbuf, VkPipelineStageFlags end_stage, render_graph *builder)
: builder_(builder), cmdbuf_(cmdbuf), end_stage_(end_stage)
{
  finished_semaphore_ = builder_->get_semaphore_();
}

job::~job()
{
  if (submission_idx_ != -1)
  {
    builder_->submissions_[submission_idx_].ref_count_--;
  }
}

void pending_workload::wait()
{
  vkWaitForFences(gctx->device, 1, &fence_, VK_TRUE, UINT64_MAX);
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
