#pragma once

#include <nezha/graph.hpp>
#include <nezha/surface.hpp>
#include <nezha/gpu_context.hpp>

namespace nz
{

io_context::io_context(GLFWwindow *window)
  : window_(window)
{
}

bool io_context::is_window_open()
{
  return !glfwWindowShouldClose(window_);
}

void io_context::poll_input()
{
  glfwPollEvents();
}
  
surface::surface(surface_data &data)
  : ctx_(data.window), window_width_(data.window_width), window_height_(data.window_height), 
    surface_(data.surface), swapchain_(data.swapchain), swapchain_format_(data.swapchain_format),
    images_(data.images), image_views_(data.image_views), swapchain_extent_(data.swapchain_extent)
{
}

u32 surface::get_swapchain_image_count() const
{
  return images_.size();
}

VkFormat surface::get_swapchain_format() const
{
  return swapchain_format_;
}

nz::job surface::acquire_next_swapchain_image(render_graph &graph, u32 &idx)
{
  /* This will acquire a semaphore for us to signal with acquire. */
  nz::job ret = nz::job(VK_NULL_HANDLE, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, &graph);

  vkAcquireNextImageKHR(gctx->device, swapchain_, UINT64_MAX, 
                        ret.finished_semaphore_, VK_NULL_HANDLE, &idx);

  return ret;
}

void surface::present(const nz::job &render_job, u32 image_idx)
{
  VkPresentInfoKHR present_info = {};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &render_job.finished_semaphore_;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swapchain_;
  present_info.pImageIndices = &image_idx;

  vkQueuePresentKHR(gctx->present_queue, &present_info);
}

io_context &surface::io()
{
  return ctx_;
}

}
