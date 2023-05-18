#pragma once

#include <nezha/binding.hpp>
#include <nezha/gpu_image.hpp>

namespace nz
{


class render_graph;


/* Encapsulates a render pass. Performs all synchronization for
 * resources that are used by the render pass. */
class render_pass 
{
public:
  /* By default, the render pass won't clear the target. */
  render_pass &add_color_attachment(gpu_image_ref image_ref, clear_color color = {-1.0f}, 
                                    const image_info &info = {});
  render_pass &add_depth_attachment(gpu_image_ref image_ref, clear_color color = {-1.0f}, 
                                    const image_info &info = {});

  /* If this isn't set, it just inherits from first binding. */
  render_pass &set_render_area(VkRect2D rect);

  struct draw_package 
  {
    VkCommandBuffer cmdbuf;
    VkRect2D rect;
    render_graph *graph;
    void *user_ptr;
  };

  struct prepare_package
  {
    VkCommandBuffer cmdbuf;
    render_graph *graph;
    void *user_ptr;
  };

  using draw_commands_proc = void(*)(draw_package package);
  using prepare_commands_proc = void(*)(prepare_package package);

  render_pass &prepare_commands(prepare_commands_proc prepare_proc, void *aux);

  inline render_pass &draw_commands(draw_commands_proc draw_proc, void *aux) 
  {
    draw_commands_(draw_proc, aux);
    return *this;
  }

public:
  render_pass() = default;
  render_pass(render_graph *, u32 idx);
  ~render_pass() = default;

private:
  void reset_();

  void issue_commands_(VkCommandBuffer cmdbuf);

  void draw_commands_(draw_commands_proc draw_proc, void *aux);

private:
  render_graph *builder_;

  std::vector<binding> bindings_;
  // -1 if there is no depth attachment
  s32 depth_index_;

  u32 idx_;

  VkRect2D rect_;

  draw_commands_proc draw_commands_proc_;
  prepare_commands_proc prepare_commands_proc_;

  void *draw_commands_aux_;
  void *prepare_commands_aux_;

  friend class render_graph;
  friend class graph_pass;
};
  

}
