#ifndef _GRAPH_COMPUTE_H_
#define _GRAPH_COMPUTE_H_

#include <nezha/int.h>
#include <vulkan/vulkan.h>
#include <nezha/heap_vector.h>

struct nz_pass_compute
{
  VkPipeline pipeline;
  VkPipelineLayout layout;

  struct nz_heap_vector *bindings;

  const char *src_path;
  void *push_constant;
  uint32_t push_constant_size;

  struct nz_graph *graph;

  struct
  {
    /* These are all different ways to specify how to dispatch the shader. */
    u32 x, y, z;
    u32 binding_res;
    b8 is_waves;
  } dispatch_params;
};

void pass_compute_reset(struct nz_pass_compute *);
void pass_compute_init(struct nz_pass_compute *);
void pass_compute_submit(VkCommandBuffer cmdbuf);

#endif
