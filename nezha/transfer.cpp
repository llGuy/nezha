#include <nezha/graph.hpp>
#include <nezha/transfer.hpp>
#include <nezha/gpu_context.hpp>
#include <nezha/bump_alloc.hpp>

namespace nz
{

transfer_operation::transfer_operation()
: type_(type::none) 
{
}

transfer_operation::transfer_operation(
  render_graph *builder, u32 idx) 
: type_(type::none), builder_(builder), idx_(idx)
{
}

void transfer_operation::init_as_buffer_update(
  graph_resource_ref buf_ref, void *data, uint32_t offset, uint32_t size) 
{
  type_ = type::buffer_update;
  binding b = { 0, binding::type::buffer_transfer_dst, buf_ref };

  bindings_->push_back(b);

  buffer_update_state_.data = data;
  buffer_update_state_.offset = offset;
  buffer_update_state_.size = size;

  gpu_buffer &buf = builder_->get_buffer_(buf_ref);
  buf.add_usage_node_(idx_, 0);

  if (size == 0) 
  {
    buffer_update_state_.size = buf.size_;
  }
}

void transfer_operation::init_as_buffer_copy_to_cpu(
  graph_resource_ref dst, graph_resource_ref src, uint32_t dst_base, const range &src_range)
{
  type_ = type::buffer_copy_to_cpu;
  binding b0 = { 0, binding::type::buffer_transfer_dst, dst };
  binding b1 = { 1, binding::type::buffer_transfer_dst, src };

  bindings_->push_back(b0);
  bindings_->push_back(b1);

  buffer_copy_to_cpu_.dst = dst;
  buffer_copy_to_cpu_.dst = src;
  buffer_copy_to_cpu_.dst_offset = dst_base;
  buffer_copy_to_cpu_.src_range = src_range;

  gpu_buffer &buf0 = builder_->get_buffer_(dst);
  buf0.add_usage_node_(idx_, 0);

  // Make sure the dst buffer is host visible
  buf0.configure({.host_visible = true});

  gpu_buffer &buf1 = builder_->get_buffer_(src);
  buf1.add_usage_node_(idx_, 1);
}

void transfer_operation::init_as_image_blit(
  graph_resource_ref src, graph_resource_ref dst) 
{
  type_ = type::image_blit;
  binding b_src = { 0, binding::type::image_transfer_src, src };
  binding b_dst = { 1, binding::type::image_transfer_dst, dst };

  bindings_->push_back(b_src);
  bindings_->push_back(b_dst);

  gpu_image &src_img = builder_->get_image_(src);
  src_img.add_usage_node_(idx_, 0);

  gpu_image &dst_img = builder_->get_image_(dst);
  dst_img.add_usage_node_(idx_, 1);
}

binding &transfer_operation::get_binding(uint32_t idx) 
{
  return (*bindings_)[idx];
}
  
}
