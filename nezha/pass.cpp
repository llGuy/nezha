#include "pass.hpp"

namespace nz
{

graph_pass::graph_pass()
: type_(graph_pass::type::none) 
{
}

graph_pass::graph_pass(const render_pass &pass) 
: rp_(pass), type_(graph_render_pass) 
{
}

graph_pass::graph_pass(const compute_pass &pass)
: cp_(pass), type_(graph_compute_pass) 
{
}

graph_pass::graph_pass(const transfer_operation &op)
: tr_(op), type_(graph_transfer_pass)
{
}

graph_pass::type graph_pass::get_type() 
{
  return type_;
}

render_pass &graph_pass::get_render_pass() 
{
  return rp_;
}

compute_pass &graph_pass::get_compute_pass() 
{
  return cp_;
}

transfer_operation &graph_pass::get_transfer_operation()
{
  return tr_;
}

bool graph_pass::is_initialized() 
{
  return (type_ != graph_pass::type::none);
}

binding &graph_pass::get_binding(uint32_t idx) 
{
  switch (type_) 
  {
  case type::graph_compute_pass: 
  {
    return cp_.bindings_[idx];
  } break;

  case type::graph_render_pass: 
  {
    return rp_.bindings_[idx];
  } break;

  case type::graph_transfer_pass:
  {
    return tr_.get_binding(idx);
  } break;

  default: 
    assert(false);
    return cp_.bindings_[idx];
  }
}
  
}
