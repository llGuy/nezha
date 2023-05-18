#pragma once

#include <nezha/render_pass.hpp>
#include <nezha/compute_pass.hpp>
#include <nezha/transfer.hpp>

namespace nz
{
  
/* Reserved for internal use. Pseudo polymorphic structure which encapsulates
 * a stage in the graph. */
class graph_pass 
{
public:
  enum type 
  {
    graph_compute_pass, graph_render_pass, graph_transfer_pass, graph_present, none
  };

  graph_pass();
  graph_pass(const render_pass &);
  graph_pass(const compute_pass &);
  graph_pass(const transfer_operation &);

  ~graph_pass() 
  {
    switch (type_) 
    {
    case graph_compute_pass: cp_.~compute_pass(); break;
    case graph_render_pass: rp_.~render_pass(); break;
    case graph_transfer_pass: tr_.~transfer_operation(); break;
    default: break;
    }
  };

  graph_pass &operator=(graph_pass &&other) 
  {
    type_ = other.type_;
    switch (type_) 
    {
    case graph_compute_pass: cp_ = std::move(other.cp_); break;
    case graph_render_pass: rp_ = std::move(other.rp_); break;
    case graph_transfer_pass: tr_ = std::move(other.tr_); break;
    default: break;
    }

    return *this;
  }

  graph_pass(graph_pass &&other) 
  {
    type_ = other.type_;
    switch (type_) 
    {
    case graph_compute_pass: cp_ = std::move(other.cp_); break;
    case graph_render_pass: rp_ = std::move(other.rp_); break;
    case graph_transfer_pass: tr_ = std::move(other.tr_); break;
    default: break;
    }
  }

  type get_type();
  render_pass &get_render_pass();
  compute_pass &get_compute_pass();
  transfer_operation &get_transfer_operation();

  binding &get_binding(uint32_t idx);

  bool is_initialized();

private:
  // For uninitialized graph passes, type_ is none
  type type_;

  union 
  {
    compute_pass cp_;
    render_pass rp_;
    transfer_operation tr_;
  };

  friend class render_graph;
};

}
