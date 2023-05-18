#pragma once

#include <vulkan/vulkan.h>

namespace nz
{


class render_graph;


/* JOBs are returned as a result of recording/scheduling commands into the
 * computation graph. They can be submitted, resulting in a PENDING_WORKLOAD. */
class job
{
public:
  /* No public facing API. This is basically an opaque object as far as
   * the user is concerned. */
  ~job();

private:
  job(VkCommandBuffer cmdbuf, 
      VkPipelineStageFlags end_stage, 
      render_graph *builder);
  
  void submit_();

private:
  /* All the recorded commands go in here! (as a result of the end() function
   * in render_graph). */
  VkCommandBuffer cmdbuf_;

  /* This is the semaphore that will get signaled when this command buffer
   * is finished. */
  VkSemaphore finished_semaphore_;

  int submission_idx_;

  VkPipelineStageFlags end_stage_;

  /* Use here for recycling command buffers. */
  render_graph *builder_;

  friend class render_graph;
};


/* PENDING_WORKLOAD comes as a result of submitting JOBs for the GPU to
 * execute.*/
class pending_workload
{
public:
  /* Causes the CPU side runtime to wait for a submitted workload (group of JOBs)
   * to finish execution. Warning, do not recommend using this. */
  void wait();

private:
  VkFence fence_;

  friend class render_graph;
};


/* For internal use: */
struct cmdbuf_info 
{
  uint32_t swapchain_idx;
  uint32_t frame_idx;
  VkCommandBuffer cmdbuf;
};


}
