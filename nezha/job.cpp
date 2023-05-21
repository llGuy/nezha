#include <nezha/job.hpp>
#include <nezha/graph.hpp>
#include <nezha/gpu_context.hpp>

namespace nz
{

job::job()
  : submission_idx_(-1)
{
}

job::job(const job &other)
{
  cmdbuf_ = other.cmdbuf_;
  finished_semaphore_ = other.finished_semaphore_;
  submission_idx_ = other.submission_idx_;
  fence_ = other.fence_;
  end_stage_ = other.end_stage_;
  builder_ = other.builder_;

  if (submission_idx_ != -1)
  {
    builder_->submissions_[submission_idx_].ref_count_++;
  }
}

job::job(job &&other)
{
  cmdbuf_ = other.cmdbuf_;
  finished_semaphore_ = other.finished_semaphore_;
  submission_idx_ = other.submission_idx_;
  fence_ = other.fence_;
  end_stage_ = other.end_stage_;
  builder_ = other.builder_;

  other.submission_idx_ = -1;
}

job::job(VkCommandBuffer cmdbuf, VkPipelineStageFlags end_stage, render_graph *builder)
: builder_(builder), cmdbuf_(cmdbuf), end_stage_(end_stage), submission_idx_(-1)
{
  finished_semaphore_ = builder_->get_semaphore_();
}

job::~job()
{
  if (submission_idx_ != -1)
  {
    assert(builder_->submissions_[submission_idx_].ref_count_ > 0);
    builder_->submissions_[submission_idx_].ref_count_--;
    submission_idx_ = -1;
  }
}

job &job::operator=(const job &other)
{
  if (submission_idx_ != -1)
  {
    assert(builder_->submissions_[submission_idx_].ref_count_ > 0);
    builder_->submissions_[submission_idx_].ref_count_--;
    submission_idx_ = -1;
  }

  cmdbuf_ = other.cmdbuf_;
  finished_semaphore_ = other.finished_semaphore_;
  submission_idx_ = other.submission_idx_;
  fence_ = other.fence_;
  end_stage_ = other.end_stage_;
  builder_ = other.builder_;

  if (submission_idx_ != -1)
    builder_->submissions_[submission_idx_].ref_count_++;

  return *this;
}

job &job::operator=(job &&other)
{
  cmdbuf_ = other.cmdbuf_;
  finished_semaphore_ = other.finished_semaphore_;
  submission_idx_ = other.submission_idx_;
  fence_ = other.fence_;
  end_stage_ = other.end_stage_;
  builder_ = other.builder_;

  other.submission_idx_ = -1;

  return *this;
}

void job::wait()
{
  vkWaitForFences(gctx->device, 1, &fence_, true, UINT64_MAX);

  if (submission_idx_ != -1)
  {
    assert(builder_->submissions_[submission_idx_].ref_count_ > 0);
    builder_->submissions_[submission_idx_].ref_count_--;
    submission_idx_ = -1;
  }
}

pending_workload::pending_workload()
  : submission_idx_(-1)
{
}

pending_workload::pending_workload(const pending_workload &other)
  : submission_idx_(other.submission_idx_), fence_(other.fence_), builder_(other.builder_)
{
  if (submission_idx_ != -1)
    builder_->submissions_[submission_idx_].ref_count_++;
}

pending_workload::pending_workload(pending_workload &&other)
  : fence_(other.fence_), builder_(other.builder_)
{
  if (submission_idx_ != -1)
  {
    assert(builder_->submissions_[submission_idx_].ref_count_ > 0);
    builder_->submissions_[submission_idx_].ref_count_--;
    submission_idx_ = -1;
  }

  submission_idx_ = other.submission_idx_;
}

pending_workload &pending_workload::operator=(const pending_workload &other)
{
  if (submission_idx_ != -1)
  {
    assert(builder_->submissions_[submission_idx_].ref_count_ > 0);
    builder_->submissions_[submission_idx_].ref_count_--;
    submission_idx_ = -1;
  }

  submission_idx_ = other.submission_idx_;
  fence_ = other.fence_; 
  builder_ = other.builder_;

  if (submission_idx_ != -1)
    builder_->submissions_[submission_idx_].ref_count_++;

  return *this;
}

pending_workload &pending_workload::operator=(pending_workload &&other)
{
  fence_ = other.fence_;
  builder_ = other.builder_;

  if (submission_idx_ != -1)
  {
    assert(builder_->submissions_[submission_idx_].ref_count_ > 0);
    builder_->submissions_[submission_idx_].ref_count_--;
    submission_idx_ = -1;
  }

  submission_idx_ = other.submission_idx_;

  return *this;
}

pending_workload::~pending_workload()
{
  if (submission_idx_ != -1)
  {
    assert(builder_->submissions_[submission_idx_].ref_count_ > 0);
    builder_->submissions_[submission_idx_].ref_count_--;
  }
}

void pending_workload::wait()
{
  vkWaitForFences(gctx->device, 1, &fence_, VK_TRUE, UINT64_MAX);

  /* Make it so that WAIT() release the submission. */
  if (submission_idx_ != -1)
  {
    assert(builder_->submissions_[submission_idx_].ref_count_ > 0);
    builder_->submissions_[submission_idx_].ref_count_--;
    submission_idx_ = -1;
  }
}

}
