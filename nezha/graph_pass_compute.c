#include <nezha/compute.h>
#include "graph_binding.h"
#include "graph_pass_compute.h"

void nz_compute_set_src(struct nz_pass_compute *c, const char *src)
{
  c->src_path = src;
}

void nz_compute_send_data(struct nz_pass_compute *, const void *, size_t)
{
  
}

void nz_compute_add_storage_buffer(struct nz_pass_compute *c, nz_graph_hdl_t hdl)
{
  u32 binding_id = nz_heap_vector_size(c->bindings);

  struct graph_binding b = {
    binding_id, GRAPH_BINDING_TYPE_STORAGE_BUFFER, hdl
  };

  *(struct graph_binding *)nz_heap_vector_push(c->bindings) = b;

  gpu_buffer &buf = builder_->get_buffer_(uid.id);
  buf.add_usage_node_(uid_.id, binding_id);
}

void nz_compute_add_uniform_buffer(struct nz_pass_compute *, nz_graph_hdl_t)
{
  
}

void nz_compute_dispatch(struct nz_pass_compute *, int x, int y, int z)
{
  
}

void nz_compute_dispatch_waves(struct nz_pass_compute *, int x, int y, int z)
{
  
}

void pass_compute_reset(struct nz_pass_compute *)
{
  
}

void pass_compute_init(struct nz_pass_compute *)
{
  
}

void pass_compute_submit(VkCommandBuffer cmdbuf)
{
  
}
