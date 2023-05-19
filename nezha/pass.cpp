#include <nezha/pass.hpp>

namespace nz
{

graph_pass::graph_pass()
: bindings_(new std::vector<binding>), type_(graph_pass::type::none) 
{
}

graph_pass::graph_pass(const render_pass &pass) 
: bindings_(new std::vector<binding>), rp_(pass), type_(graph_render_pass) 
{
  rp_.bindings_ = bindings_;
}

graph_pass::graph_pass(const compute_pass &pass)
: bindings_(new std::vector<binding>), cp_(pass), type_(graph_compute_pass) 
{
  cp_.bindings_ = bindings_;
}

graph_pass::graph_pass(const transfer_operation &op)
: bindings_(new std::vector<binding>), tr_(op), type_(graph_transfer_pass)
{
  tr_.bindings_ = bindings_;
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
  return (*bindings_)[idx];
}
  
}
