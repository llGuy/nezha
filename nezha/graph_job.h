#ifndef _GRAPH_JOB_H_
#define _GRAPH_JOB_H_

#include <vulkan/vulkan.h>

/* Because of the up front ID creating, these memory allocations get cached. */
struct nz_job
{
  /* This is state which gets reset at the end of every node recording. */
  VkSemaphore finished;
  VkFence fence;
};

#endif
