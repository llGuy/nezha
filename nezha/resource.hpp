#pragma once

#include "gpu_image.hpp"
#include "gpu_buffer.hpp"

namespace nz
{

/* This class is a "pseudo" polymorphic class which encapsulates the idea
 * of a resource. Could be an image or a buffer. This is purely for internal
 * use by the graph structure. */
class graph_resource 
{
public:
  enum type { graph_image, graph_buffer, none };

  graph_resource();
  graph_resource(const gpu_image &img);
  graph_resource(const gpu_buffer &buf);

  ~graph_resource() 
  {
    switch (type_) 
    {
    case graph_image: img_.~gpu_image(); break;
    case graph_buffer: buf_.~gpu_buffer(); break;
    default: break;
    }
  };

  graph_resource &operator=(graph_resource &&other) 
  {
    type_ = other.type_;
    was_used_ = other.was_used_;
    switch (type_) 
    {
    case graph_image: img_ = std::move(other.img_); break;
    case graph_buffer: buf_ = std::move(other.buf_); break;
    default: break;
    }

    return *this;
  }

  graph_resource(graph_resource &&other) 
  {
    type_ = other.type_;
    was_used_ = other.was_used_;
    switch (type_) 
    {
    case graph_image: img_ = std::move(other.img_); break;
    case graph_buffer: buf_ = std::move(other.buf_); break;
    default: break;
    }
  }

  // Given a binding, update the action that should be done on the resource
  inline void update_action(const binding &b) 
  {
    switch (type_) 
    {
    case graph_image: img_.update_action_(b); break;
    case graph_buffer: buf_.update_action_(b); break;
    default: break;
    }
  }

  inline type get_type() { return type_; }
  inline gpu_buffer &get_buffer() { return buf_; }
  inline gpu_image &get_image() { return img_; }

private:
  type type_;
  bool was_used_;

  union 
  {
    gpu_buffer buf_;
    gpu_image img_;
  };

  friend class render_graph;
};

/* Resource tracker for more fine grained resource synchronization when we
 * deal with render passes. Render passes have the ability to record custom
 * render commands where we will need direct access to resources. This means
 * that the graph won't have the ability to know how to synchronize the resources
 * which warrants the need for the PREPARE_##_FOR*/
class graph_resource_tracker 
{
public:
  graph_resource_tracker(VkCommandBuffer cmdbuf, render_graph *builder);

  // Prepare resources for certain usages
  void prepare_buffer_for(
    gpu_buffer_ref, binding::type type, VkPipelineStageFlags stage);

  // Access the resources directly
  gpu_buffer &get_buffer(gpu_buffer_ref);

private:
  render_graph *builder_;
  VkCommandBuffer cmdbuf_;
};
  
}
