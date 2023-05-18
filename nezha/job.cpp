#include "job.hpp"
#include "graph.hpp"
#include "gpu_context.hpp"

namespace nz
{

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

}
