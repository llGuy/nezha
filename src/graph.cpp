#include "log.hpp"
#include "graph.hpp"
#include "gpu_context.hpp"

#include <filesystem>

namespace nz
{
  
render_graph::render_graph() 
: resources_(max_resources)
{
}

gpu_buffer_ref render_graph::register_buffer(const buffer_info &cfg) 
{
  gpu_buffer_ref ref = resources_.add();
  graph_resource &res = resources_[ref];

  new (&res) graph_resource(gpu_buffer(this));

  res.get_buffer().configure(cfg);

  return ref;
}

gpu_image_ref render_graph::register_image(const image_info &cfg)
{
  gpu_image_ref ref = resources_.add();
  graph_resource &res = resources_[ref];

  new (&res) graph_resource(gpu_image(this));

  res.get_image().configure(cfg);

  return ref;
}

compute_kernel render_graph::register_compute_kernel(const char *src)
{
  compute_kernel k = kernels_.size();
  kernels_.push_back({ src, VK_NULL_HANDLE, VK_NULL_HANDLE });
  return k;
}

render_pass &render_graph::add_render_pass() 
{
  recorded_stages_.emplace_back(graph_pass(render_pass(this, recorded_stages_.size())));
  return recorded_stages_.back().get_render_pass();
}

compute_pass &render_graph::add_compute_pass() 
{
  recorded_stages_.emplace_back(graph_pass(compute_pass(this, recorded_stages_.size())));
  return recorded_stages_.back().get_compute_pass();
}

void render_graph::add_buffer_update(
  gpu_buffer_ref ref, void *data, u32 offset, u32 size) 
{
  recorded_stages_.emplace_back(graph_pass(transfer_operation(this, recorded_stages_.size())));
  auto &transfer = recorded_stages_.back();

  transfer.get_transfer_operation().init_as_buffer_update(ref, data, offset, size);
}

void render_graph::add_buffer_copy_to_cpu(
  gpu_buffer_ref dst, gpu_buffer_ref src)
{
  recorded_stages_.emplace_back(graph_pass(transfer_operation(this, recorded_stages_.size())));
  auto &transfer = recorded_stages_.back();

  transfer.get_transfer_operation().init_as_buffer_copy_to_cpu(dst, src);
}

void render_graph::add_image_blit(gpu_image_ref dst, gpu_image_ref src) 
{
  recorded_stages_.emplace_back(graph_pass(transfer_operation(this, recorded_stages_.size())));
  auto &transfer = recorded_stages_.back();

  transfer.get_transfer_operation().init_as_image_blit(src, dst);
}

void render_graph::begin() 
{
  // Looping through resource IDs (ID of resources that were used in the frame)
  for (auto &r : used_resources_) 
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

  for (auto &stg : recorded_stages_) 
  {
    switch (stg.type_)
    {
      case graph_pass::type::graph_compute_pass:
      {
        compute_pass &cp = stg.get_compute_pass();
        cp.bindings_.clear();
      } break;

      case graph_pass::type::graph_render_pass:
      {
        render_pass &rp = stg.get_render_pass();
        rp.bindings_.clear();
      } break;

      case graph_pass::type::graph_transfer_pass:
      {
        stg.get_transfer_operation().bindings_ = nullptr;
      } break;

      default: break;
    }
  }

  recorded_stages_.clear();
  used_resources_.clear();
  recorded_stages_.clear();
}

void render_graph::prepare_pass_graph_stage_(graph_stage_ref stg) 
{
  // Handle render pass / compute pass case
  switch (recorded_stages_[stg].get_type()) 
  {
  case graph_pass::graph_compute_pass: 
  {
    compute_pass &cp = recorded_stages_[stg].get_compute_pass();

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
    render_pass &rp = recorded_stages_[stg].get_render_pass();

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

  case graph_pass::graph_transfer_pass:
  {
    transfer_operation &op = recorded_stages_[stg].get_transfer_operation();
    prepare_transfer_graph_stage_(op);
  } break;

  default: break;
  }
}

void render_graph::prepare_transfer_graph_stage_(transfer_operation &op) 
{
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
  const cmdbuf_info &info) 
{
  // Handle compute / render passes
  switch (recorded_stages_[stg].get_type()) 
  {
  case graph_pass::graph_compute_pass: 
  {
    last_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

    compute_pass &cp = recorded_stages_[stg].get_compute_pass();
    compute_kernel_state &cp_state = kernels_[cp.kernel_];

    if (cp_state.pipeline == VK_NULL_HANDLE) 
    {
      // Actually initialize the compute pipeline/layout
      // which is stored in the compute_kernel_state of the render graph
      nz::log_info("Created a compute pipeline!");
      cp.create_(cp_state);
    }

    // Issue the commands
    cp.issue_commands_(info.cmdbuf, cp_state);
  } break;

  case graph_pass::graph_render_pass: 
  {
    last_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    render_pass &rp = recorded_stages_[stg].get_render_pass();

    rp.issue_commands_(info.cmdbuf);
  } break;

  case graph_pass::graph_transfer_pass:
  {
    last_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

    transfer_operation &op = recorded_stages_[stg].get_transfer_operation();
    execute_transfer_graph_stage_(op, info);
  } break;

  default: break;
  }
}

void render_graph::execute_transfer_graph_stage_(
  transfer_operation &op, const cmdbuf_info &info) 
{
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

job render_graph::end() 
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
  cmdbuf_info info;
  info.cmdbuf = current_cmdbuf_ = get_command_buffer_();

  VkCommandBufferBeginInfo begin_info = 
  {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
  };

  vkBeginCommandBuffer(current_cmdbuf_, &begin_info);

  // swapchain_img_idx_ = info.swapchain_idx;

  // First traverse through all stages in order to figure out resources to use
  for (int i = 0; i < recorded_stages_.size(); ++i) 
    prepare_pass_graph_stage_(i);

  // Loop through all used resources
  for (auto &rref : used_resources_) 
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
  for (int i = 0; i < recorded_stages_.size(); ++i) 
  {
    execute_pass_graph_stage_(i, last_stage, info);
  }

  vkEndCommandBuffer(current_cmdbuf_);
  // generator->submit_command_buffer(info, last_stage);

  return job(info.cmdbuf, last_stage, this);
}

render_graph::submission *render_graph::get_successful_submission_()
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

gpu_buffer &render_graph::get_buffer(gpu_buffer_ref ref)
{
  return get_buffer_(ref);
}

graph_resource_tracker render_graph::get_resource_tracker() 
{
  return graph_resource_tracker(current_cmdbuf_, this);
}

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

}
