#pragma once

#include <nezha/job.hpp>
#include <nezha/pass.hpp>
#include <nezha/types.hpp>
#include <nezha/surface.hpp>
#include <nezha/binding.hpp>
#include <nezha/resource.hpp>
#include <nezha/transfer.hpp>
#include <nezha/gpu_image.hpp>
#include <nezha/gpu_buffer.hpp>
#include <nezha/render_pass.hpp>
#include <nezha/compute_pass.hpp>
#include <nezha/dynamic_array.hpp>

#include <set>

namespace nz
{


/* RENDER_GRAPH is the main class through which everything goes through. This is 
 * responsible for managing GPU resources, as well as recording commands into the 
 * computation graph.
 *
 * Usage typically looks like this:
 * - Register resources and compute kernels using the REGISTER_# functions like
 *   REGISTER_BUFFER() for registering a GPU-side buffer of memory. This functions
 *   will give you a reference that you can use when adding bindings to various
 *   stages in the computation graph.
 * - Once things have been registered, one can proceed with recording commands
 *   into a JOB. To do this, call BEGIN. Then call whichever functions you like
 *   for recording commands into the graph These are all functions which start with
 *   ADD_#. For instance, ADD_COMPUTE_PASS() will add a compute kernel into the JOB.
 *   Once you have recorded all commands, call END(). This will return a JOB. 
 * - Now that you have a JOB (or multiple), you can submit it/them with SUBMIT().
 *   This will give you a PENDING_WORKLOAD which is currently being executed on the
 *   GPU.
 * - With a PENDING_WORKLOAD, you can if you want, call its WAIT function to wait
 *   for the workload to finish although this is not advisable for maximum GPU
 *   execution throughput. */
class render_graph 
{
public:
  /* REGISTER_# functions. */
  gpu_buffer_ref register_buffer(const buffer_info &cfg);
  gpu_image_ref  register_image(const image_info &cfg);
  compute_kernel register_compute_kernel(const char *src);
  void           register_swapchain(const surface &, gpu_image_ref *dst);


  /* GET_# functions. Gives you direct access to the registered resources. */
  gpu_buffer &get_buffer(gpu_buffer_ref);
  gpu_image  &get_image(gpu_image_ref);


  /* ADD_# functions. These add stages into the computation graph. Must be called
   * after BEGIN() and before END(). */
  render_pass  &add_render_pass();
  compute_pass &add_compute_pass();
  void          add_buffer_update(gpu_buffer_ref, void *data, u32 offset = 0, u32 size = 0);
  void          add_buffer_copy_to_cpu(gpu_buffer_ref dst, gpu_buffer_ref src, u32 dst_offset, const range &rng);
  void          add_image_blit(gpu_image_ref src, gpu_image_ref dst);
  void          add_present_ready(gpu_image_ref img);


  /* BEGIN() function. This puts the GRAPH into a state of recording commands. */
  void begin();


  /* END() funciton. This stops recording and gives you a JOB which is ready for SUBMIT(). */
  job end();
  job placeholder_job();


  /* SUBMIT() function(s). This submits a JOB which may have other JOB dependencies.
   * The GRAPH will make sure to schedule all the JOBs appropriately taking into
   * account dependencies. We provide helper overloads of the SUBMIT() function
   * for convenience sake. The final overload which takes pointers, is the most
   * fundamental and is the one that the other ones call.*/
  template <typename ...T>
  inline pending_workload submit(job &job, T &&...dependencies);
  inline pending_workload submit(job &job);
  pending_workload        submit(job *jobs, int count, job *dependencies, int dependency_count);
  pending_workload        placeholder_workload();


public:
  render_graph();

private:
  class submission
  {
  public:
    void wait();

  private:
    VkFence fence_;

    uint32_t ref_count_;

    // All the semaphores that will get freed up
    std::vector<VkSemaphore> semaphores_;

    // All the command buffers that will get freed up
    std::vector<VkCommandBuffer> cmdbufs_;

    friend class render_graph;
    friend class job;
    friend class pending_workload;
  };

  /* All internal things that can be ignored! */
  graph_resource_tracker get_resource_tracker();
  void recycle_submissions_();
  submission *get_successful_submission_();
  VkFence get_fence_();
  VkSemaphore get_semaphore_();
  VkCommandBuffer get_command_buffer_();

  void prepare_pass_graph_stage_(graph_stage_ref ref);
  void prepare_transfer_graph_stage_(transfer_operation &op);

  void execute_pass_graph_stage_(
    graph_stage_ref ref, VkPipelineStageFlags &last,
    const cmdbuf_info &info);

  void execute_transfer_graph_stage_(
    transfer_operation &op, const cmdbuf_info &info);

  inline compute_pass &get_compute_pass_(graph_stage_ref stg) 
    { return recorded_stages_[stg].get_compute_pass(); }
  inline render_pass &get_render_pass_(graph_stage_ref stg) 
    { return recorded_stages_[stg].get_render_pass(); }

  inline graph_resource &get_resource_(u32 ref)
    { return resources_[ref]; }
  inline gpu_buffer &get_buffer_(u32 ref)
    { return resources_[ref].get_buffer(); }
  inline gpu_image &get_image_(u32 ref)
    { return resources_[ref].get_image(); }

  inline binding &get_binding_(graph_stage_ref stg, uint32_t binding_idx) 
  {
    return recorded_stages_[stg].get_binding(binding_idx); 
  }

private:
  static constexpr uint32_t max_resources = 1000;

  dynamic_array<graph_resource> resources_;

  std::vector<graph_pass> recorded_stages_;
  std::vector<graph_resource_ref> used_resources_;

  std::vector<VkCommandBuffer> free_cmdbufs_;
  std::vector<VkSemaphore> free_semaphores_;
  std::set<VkFence> free_fences_;
  std::vector<submission> submissions_;
  std::vector<compute_kernel_state> kernels_;

  VkCommandBuffer current_cmdbuf_;

  friend class compute_pass;
  friend class render_pass;
  friend class gpu_image;
  friend class gpu_buffer;
  friend class transfer_operation;
  friend class graph_resource_tracker;
  friend class job;
  friend class surface;
  friend class pending_workload;
};


template <typename ...T>
inline pending_workload render_graph::submit(job &job, T &&...dependencies)
{
  class job deps[] = { std::forward<T>(dependencies)... };
  return submit(&job, 1, deps, sizeof...(T));
}

inline pending_workload render_graph::submit(job &job)
{
  return submit(&job, 1, nullptr, 0);
}

std::string make_shader_src_path(const char *path, VkShaderStageFlags stage);

}
