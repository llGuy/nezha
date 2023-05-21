#pragma once

#include <vector>
#include <nezha/types.hpp>
#include <nezha/binding.hpp>

namespace nz
{


class render_graph;


struct range
{
  uint32_t offset;
  uint32_t size;
};


/* Encapsulates a transfer operation. For internal use. */
class transfer_operation 
{
public:
  enum type 
  {
    buffer_update, buffer_copy, buffer_copy_to_cpu, image_copy, image_blit, present_ready, none
  };

  transfer_operation();
  transfer_operation(render_graph *builder, u32 idx);

  void init_as_buffer_update(
    graph_resource_ref buf_ref, void *data, uint32_t offset, uint32_t size);

  void init_as_buffer_copy_to_cpu(
    graph_resource_ref dst, graph_resource_ref src, uint32_t dst_base, const range &src_range);

  // For now, assume we blit the entire thing
  void init_as_image_blit(graph_resource_ref src, graph_resource_ref dst);

  void init_as_present_ready(graph_resource_ref ref);

  binding &get_binding(uint32_t idx);

  /* TODO
  void init_as_buffer_copy(
    graph_resource_ref buf_ref, uint32_t offset, uint32_t size);
  void init_as_image_copy(
    graph_resource_ref buf_ref, uint32_t offset, uint32_t size);
  */

private:
  type type_;
  render_graph *builder_;

  // Index of this stage in the recorded stages vector in render_graph
  u32 idx_;

  std::vector<binding> *bindings_;

  union
  {
    struct 
    {
      void *data;
      uint32_t offset;
      uint32_t size;
    } buffer_update_state_;

    struct
    {
      graph_resource_ref dst, src;
      uint32_t dst_offset;
      range src_range;
    } buffer_copy_to_cpu_;

    // TODO:
    struct 
    {
    } buffer_copy_state_;

    struct 
    {
    } image_copy_state_;

    struct 
    {
    } image_blit_state_;

    struct
    {
      graph_resource_ref ref;
    } present_ready_;
  };

  friend class render_graph;
  friend class graph_pass;
};


}
