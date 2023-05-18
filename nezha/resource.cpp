#include "graph.hpp"
#include "resource.hpp"

namespace nz
{

graph_resource_tracker::graph_resource_tracker(
  VkCommandBuffer cmdbuf, render_graph *builder)
: builder_(builder), cmdbuf_(cmdbuf) 
{
}

void graph_resource_tracker::prepare_buffer_for(
  gpu_buffer_ref ref, binding::type type, VkPipelineStageFlags stage) 
{
  gpu_buffer &buf = builder_->get_buffer_(ref);

  binding b = { .utype = type };

  VkBufferMemoryBarrier barrier = 
  {
    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    .size = buf.size_,
    .buffer = buf.buffer_,
    .offset = 0,
    .srcAccessMask = buf.current_access_,
    .dstAccessMask = b.get_buffer_access(),
  };

  vkCmdPipelineBarrier(
    cmdbuf_, buf.last_used_, stage, 0, 0, nullptr, 1, &barrier, 0, nullptr);

  buf.current_access_ = b.get_buffer_access();
  buf.last_used_ = stage;
}

gpu_buffer &graph_resource_tracker::get_buffer(gpu_buffer_ref ref)
{
  return builder_->get_buffer_(ref);
}

graph_resource::graph_resource()
: type_(graph_resource::type::none), was_used_(false) 
{
}

graph_resource::graph_resource(const gpu_image &img) 
: type_(type::graph_image), img_(img), was_used_(false) 
{
}

graph_resource::graph_resource(const gpu_buffer &buf) 
: type_(type::graph_buffer), buf_(buf), was_used_(false) 
{
}

  
}
