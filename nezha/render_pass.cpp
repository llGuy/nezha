#include "graph.hpp"
#include "bump_alloc.h"
#include "render_pass.hpp"

#include "gpu_context.hpp"

namespace nz
{

render_pass::render_pass(render_graph *builder, u32 idx) 
  : builder_(builder), idx_(idx), depth_index_(-1), rect_{}, 
    prepare_commands_proc_(nullptr)
{
}

render_pass &render_pass::add_color_attachment(
  gpu_image_ref ref, clear_color color, const image_info &info) 
{
  uint32_t binding_id = bindings_.size();

  binding b = { (uint32_t)bindings_.size(), binding::type::color_attachment, 
    ref, color };

  bindings_.push_back(b);

  // Get image will allocate space for the image struct if it hasn't 
  // been created yet
  gpu_image &img = builder_->get_image_(ref);
  img.add_usage_node_(idx_, binding_id);
  img.configure(info);

  return *this;
}

render_pass &render_pass::add_depth_attachment(
  gpu_image_ref ref, clear_color color, const image_info &info) 
{
  uint32_t binding_id = bindings_.size();

  depth_index_ = binding_id;

  binding b = { (uint32_t)bindings_.size(), binding::type::depth_attachment,
    ref, color };

  bindings_.push_back(b);

  // Get image will allocate space for the image struct if it hasn't been 
  // created yet
  gpu_image &img = builder_->get_image_(ref);
  img.add_usage_node_(idx_, binding_id);

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
  
}
