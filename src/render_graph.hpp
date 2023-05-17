#pragma once


/* TODO:
 * - Recycle semaphores/fences/commandbuffers
 * - Make sure that we can add multiple instances of a compute shader. 
 * - Job synchronization
 * - More async stuff */


#include <set>
#include <array>
#include <vector>
#include <assert.h>
#include "string.hpp"
#include "heap_array.hpp"
#include <vulkan/vulkan.h>

namespace nz
{

// ID of a resource
using graph_resource_ref = uint32_t;

// ID of a pass stage
struct graph_stage_ref 
{
  enum type 
  {
    pass,
    transfer,
    none
  };

  uint32_t stage_type : 2;
  uint32_t stage_idx : 30;

  graph_stage_ref() = default;

  graph_stage_ref(uint32_t idx) 
  {
    stage_type = type::pass;
    stage_idx = idx;
  }

  graph_stage_ref(type t, uint32_t idx) 
  {
    stage_type = t;
    stage_idx = idx;
  }

  operator uint32_t() 
  {
    return stage_idx;
  }
};

constexpr uint32_t invalid_graph_ref = 0xFFFFFFF;
constexpr uint32_t graph_stage_ref_present = 0xBADC0FE;

class render_graph;

// This whole struct is basically like a pointer
struct resource_usage_node 
{
  inline void invalidate() 
  {
    stage = invalid_graph_ref;
    binding_idx = invalid_graph_ref;
  }

  inline bool is_invalid() 
  {
    return stage.stage_idx == invalid_graph_ref;
  }

  graph_stage_ref stage;

  // May need more data (optional for a lot of cases) to describe this
  uint32_t binding_idx;
};

struct clear_color 
{
  float r, g, b, a;
};

struct binding 
{
  enum type 
  {
    // Image types
    sampled_image, storage_image, color_attachment, depth_attachment, 
    image_transfer_src, image_transfer_dst, max_image, 

    // Buffer types
    storage_buffer, uniform_buffer, buffer_transfer_src,
    buffer_transfer_dst, vertex_buffer, max_buffer,
    none
  };

  // Index of this binding
  uint32_t idx;
  type utype;
  graph_resource_ref rref;

  // For render passes
  clear_color clear;

  // This is going to point to the next place that the given resource is used
  // Member is written to by the resource which is pointed to by the binding
  resource_usage_node next;

  VkDescriptorType get_descriptor_type() 
  {
    switch (utype) 
    {
    case sampled_image: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case storage_image: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case storage_buffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case uniform_buffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    default: return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
  }

  VkImageLayout get_image_layout() 
  {
    assert(utype < type::max_image);

    switch (utype) 
    {
    case type::sampled_image:
      return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case type::storage_image:
      return VK_IMAGE_LAYOUT_GENERAL;
    case type::color_attachment:
      return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case type::depth_attachment:
      return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    case type::image_transfer_src:
      return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case type::image_transfer_dst:
      return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    default:
      assert(false);
      return VK_IMAGE_LAYOUT_MAX_ENUM;
    }
  }

  VkAccessFlags get_buffer_access() 
  {
    switch (utype) 
    {
    case type::uniform_buffer:
      return VK_ACCESS_MEMORY_READ_BIT;
    case type::storage_buffer:
      return VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    case type::buffer_transfer_src:
      return VK_ACCESS_MEMORY_READ_BIT;
    case type::buffer_transfer_dst:
      return VK_ACCESS_MEMORY_WRITE_BIT;
    case type::vertex_buffer:
      return VK_ACCESS_MEMORY_READ_BIT;
    default:
      assert(false);
      return (VkAccessFlags)0;
    }
  }

  VkAccessFlags get_image_access() 
  {
    assert(utype < type::max_image);

    switch (utype) 
    {
    case type::sampled_image:
      return VK_ACCESS_SHADER_READ_BIT;
    case type::image_transfer_src:
      return VK_ACCESS_MEMORY_READ_BIT;
    case type::image_transfer_dst:
      return VK_ACCESS_MEMORY_WRITE_BIT;
    case type::storage_image:
      return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    case type::color_attachment:
      return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case type::depth_attachment:
      return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
    default:
      assert(false);
      return (VkAccessFlags)0;
    }
  }
};

struct buffer_info 
{
  u32 size = 0;
  binding::type type = binding::type::max_buffer;
  bool host_visible = false;
};

struct image_info 
{
  // If format is NULL, use swapchain image
  VkFormat format = VK_FORMAT_MAX_ENUM;

  // If 0 in all dimentions, inherit extent from swapchain
  VkExtent3D extent = {};

  // Default values
  bool is_depth = false;
  uint32_t layer_count = 1;
};

/************************* Render Pass ****************************/
class render_pass 
{
public:
  render_pass() = default;
  render_pass(render_graph *, const uid_string &uid);

  ~render_pass() = default;

  // By default, the render pass won't clear the target
  render_pass &add_color_attachment(
    const uid_string &uid, 
    clear_color color = {-1.0f}, 
    const image_info &info = {});

  render_pass &add_depth_attachment(
    const uid_string &uid,
    clear_color color = {-1.0f}, 
    const image_info &info = {});

  // If this isn't set, it just inherits from first binding
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

private:
  void reset_();

  void issue_commands_(VkCommandBuffer cmdbuf);

  void draw_commands_(draw_commands_proc draw_proc, void *aux);

private:
  render_graph *builder_;

  std::vector<binding> bindings_;
  // -1 if there is no depth attachment
  s32 depth_index_;

  uid_string uid_;

  VkRect2D rect_;

  draw_commands_proc draw_commands_proc_;
  prepare_commands_proc prepare_commands_proc_;

  void *draw_commands_aux_;
  void *prepare_commands_aux_;

  friend class render_graph;
  friend class graph_pass;
};



/************************* Compute Pass ***************************/
class compute_pass 
{
public:
  compute_pass() = default;
  compute_pass(render_graph *, const uid_string &uid);

  ~compute_pass() = default;

  compute_pass &set_source(const char *src_path);

  compute_pass &send_data(const void *data, uint32_t size);

  // Push constant
  template <typename T>
  compute_pass &send_data(const T &data) 
  {
    return send_data(&data, sizeof(T));
  }

  // Whenever we add a resource here, we need to update the linked list
  // of used nodes that start with the resource itself
  compute_pass &add_sampled_image(const uid_string &);
  compute_pass &add_storage_image(const uid_string &, const image_info &i = {});
  compute_pass &add_storage_buffer(const uid_string &);
  compute_pass &add_uniform_buffer(const uid_string &);

  // Configures the dispatch with given dimensions
  compute_pass &dispatch(uint32_t count_x, uint32_t count_y, uint32_t count_z);
  // Configures the dispatch with the size of each wave - specify which image 
  // to get resolution from
  compute_pass &dispatch_waves(
    uint32_t wave_x, uint32_t wave_y, uint32_t wave_z, const uid_string &);
  // TODO: Support indirect

private:
  // Gets called everytime you call add_compute_pass from render_graph
  void reset_();

  // Actually creates the compute pass
  void create_();

  // Issue the commands to command buffer
  void issue_commands_(VkCommandBuffer cmdbuf);

  // void add_image(binding::type type);

  // Configuration data
private:
  // Push constant information
  void *push_constant_;
  uint32_t push_constant_size_;

  // Shader source path
  const char *src_path_;

  // Uniform bindings
  std::vector<binding> bindings_;

  // Dispatch parameters
  struct 
  {
    uint32_t x, y, z;
    // Index of the binding for which we will bind our resolution with
    uint32_t binding_res;
    bool is_waves;
  } dispatch_params_;

  // Actual Vulkan objects
private:
  VkPipeline pipeline_;
  VkPipelineLayout layout_;

private:
  uid_string uid_;

  // If the compute pass is uninitialized, builder_ is nullptr
  render_graph *builder_;

  friend class render_graph;
  friend class graph_pass;
};

class graph_pass 
{
public:
  enum type 
  {
    graph_compute_pass, graph_render_pass, none
  };

  graph_pass();
  graph_pass(const render_pass &);
  graph_pass(const compute_pass &);

  ~graph_pass() 
  {
    switch (type_) 
    {
    case graph_compute_pass: cp_.~compute_pass(); break;
    case graph_render_pass: rp_.~render_pass(); break;
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
    default: break;
    }
  }

  type get_type();
  render_pass &get_render_pass();
  compute_pass &get_compute_pass();

  binding &get_binding(uint32_t idx);

  bool is_initialized();

private:
  // For uninitialized graph passes, type_ is none
  type type_;

  union 
  {
    compute_pass cp_;
    render_pass rp_;
  };
};



/************************* Resource *******************************/
class memory_mapping
{
public:
  ~memory_mapping();

  void *data();
  size_t size();

private:
  memory_mapping(VkDeviceMemory memory, size_t size);

private:
  VkDeviceMemory memory_;
  void *data_;
  size_t size_;

  friend class render_graph;
  friend class gpu_buffer;
  friend class gpu_image;
};

class gpu_buffer 
{ // TODO
public:
  enum action_flag { to_create, none };

  gpu_buffer(render_graph *graph);

  gpu_buffer &configure(const buffer_info &i);

  // Actually allocates the memory and creates the resource
  gpu_buffer &alloc();

  memory_mapping map();

  inline VkBuffer buffer() 
  {
    return buffer_;
  }

  inline VkDescriptorSet descriptor(binding::type t) 
  {
    return get_descriptor_set_(t);
  }

private:
  void update_action_(const binding &b);
  void apply_action_();

  void add_usage_node_(graph_stage_ref stg, uint32_t binding_idx);

  VkDescriptorSet get_descriptor_set_(binding::type utype);
  void create_descriptors_(VkBufferUsageFlags usage);

private:
  resource_usage_node head_node_;
  resource_usage_node tail_node_;

  action_flag action_;

  render_graph *builder_;

  VkBuffer buffer_;
  VkDeviceMemory buffer_memory_;

  u32 size_;

  VkBufferUsageFlags usage_;

  VkDescriptorSet descriptor_sets_[
    binding::type::max_buffer - binding::type::max_image];

  VkAccessFlags current_access_;
  VkPipelineStageFlags last_used_;

  bool host_visible_;

  friend class render_graph;
  friend class compute_pass;
  friend class render_pass;
  friend class graph_resource;
  friend class transfer_operation;
  friend class graph_resource_tracker;
};

class gpu_image 
{
public:
  // TODO: Be smarter about how we can alias this resource to another
  // for now, we only support aliasing to the swapchain image
  enum action_flag { to_create, to_present, none };

  gpu_image();

  gpu_image &configure(const image_info &i);
  gpu_image &alloc();

private:
  gpu_image(render_graph *);

  void add_usage_node_(graph_stage_ref stg, uint32_t binding_idx);

  // Update action but also its usage flags
  void update_action_(const binding &b);

  void apply_action_();

  // Create descriptors given a usage flag (if the descriptors were already 
  // created, don't do anything for that kind of descriptor).
  void create_descriptors_(VkImageUsageFlags usage);

  // This is necessary in the case of indirection (there is always at most one 
  // level of indirection)
  gpu_image &get_();

  VkDescriptorSet get_descriptor_set_(binding::type t);

private:
  resource_usage_node head_node_;
  resource_usage_node tail_node_;

  render_graph *builder_;
  action_flag action_;

private:
  // Are we referencing another image?
  //
  // void alloc();
  graph_resource_ref reference_;

  // Actual image data
  VkImage image_;
  VkImageView image_view_;
  VkDeviceMemory image_memory_;

  VkExtent3D extent_;

  VkImageAspectFlags aspect_;
  VkFormat format_;

  VkImageUsageFlags usage_;

  VkDescriptorSet descriptor_sets_[binding::type::none];

  // Keep track of current layout / usage stage
  VkImageLayout current_layout_;
  VkAccessFlags current_access_;
  VkPipelineStageFlags last_used_;

  friend class render_graph;
  friend class compute_pass;
  friend class render_pass;
  friend class graph_resource;
  friend class transfer_operation;
};

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

/************************* Transfer *******************************/
class transfer_operation 
{
public:
  enum type 
  {
    buffer_update, buffer_copy, buffer_copy_to_cpu, image_copy, image_blit, none
  };

  transfer_operation();
  transfer_operation(graph_stage_ref ref, render_graph *builder);

  void init_as_buffer_update(
    graph_resource_ref buf_ref, void *data, uint32_t offset, uint32_t size);

  void init_as_buffer_copy_to_cpu(
    graph_resource_ref dst, graph_resource_ref src);

  // For now, assume we blit the entire thing
  void init_as_image_blit(graph_resource_ref src, graph_resource_ref dst);

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
  graph_stage_ref stage_ref_;

  binding *bindings_;

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
    } buffer_copy_to_cpu_;

    // TODO:
    struct 
    {
    } buffer_copy_state;

    struct 
    {
    } image_copy_state;

    struct 
    {
    } image_blit_state;
  };

  friend class render_graph;
};



/************************* Graph Builder **************************/
struct graph_swapchain_info 
{
  uint32_t image_count;
  VkImage *images;
  VkImageView *image_views;
};

// This may be something that takes care of presenting to the screen
// Can create any custom generator you want if you want to custom set synch primitives
class cmdbuf_generator 
{
public:
  struct cmdbuf_info 
  {
    uint32_t swapchain_idx;
    uint32_t frame_idx;
    VkCommandBuffer cmdbuf;
  };

  virtual cmdbuf_info get_command_buffer() = 0;
  virtual void submit_command_buffer(
    const cmdbuf_info &info, VkPipelineStageFlags stage) = 0;
};

class present_cmdbuf_generator : public cmdbuf_generator 
{
public:
  present_cmdbuf_generator(u32 frames_in_flight);

  // TODO: Add input and output synchornization semaphores and fences
  cmdbuf_info get_command_buffer() override;
  void submit_command_buffer(
    const cmdbuf_info &info, VkPipelineStageFlags stage) override;

private:
  heap_array<VkSemaphore> image_ready_semaphores_;
  heap_array<VkSemaphore> render_finished_semaphores_;
  heap_array<VkFence> fences_;
  heap_array<VkCommandBuffer> command_buffers_;

  u32 current_frame_;
  u32 max_frames_in_flight_;
};

class single_cmdbuf_generator : public cmdbuf_generator 
{
public:
  cmdbuf_info get_command_buffer() override;
  void submit_command_buffer(
    const cmdbuf_info &info, VkPipelineStageFlags stage) override;
};

/* Resource tracker for more fine grained resource synchronization */
class graph_resource_tracker 
{
public:
  graph_resource_tracker(VkCommandBuffer cmdbuf, render_graph *builder);

  // Prepare resources for certain usages
  void prepare_buffer_for(
    const uid_string &, binding::type type, VkPipelineStageFlags stage);

  // Access the resources directly
  gpu_buffer &get_buffer(const uid_string &uid);

private:
  render_graph *builder_;
  VkCommandBuffer cmdbuf_;
};

class job
{
public:
  job(VkCommandBuffer cmdbuf, VkPipelineStageFlags end_stage, render_graph *builder);

  ~job();

private:
  void submit_();

private:
  /* All the recorded commands go in here! (as a result of the end() function
   * in render_graph). */
  VkCommandBuffer cmdbuf_;

  /* This is the semaphore that will get signaled when this command buffer
   * is finished. */
  VkSemaphore finished_semaphore_;

  int submission_idx_;

  VkPipelineStageFlags end_stage_;

  /* Use here for recycling command buffers. */
  render_graph *builder_;

  friend class render_graph;
};

class pending_workload
{
public:
  void wait();

private:
  VkFence fence_;

  friend class render_graph;
};

class submission
{
public:
  void wait();

private:
  VkFence fence_;

  uint32_t ref_count_;

  // All the semaphores that will get freed up
  std::vector<VkSemaphore> semaphores_;

  // All the command buffers that will get freed up
  std::vector<VkCommandBuffer> cmdbufs_;

  friend class render_graph;
  friend class job;
};

class render_graph 
{
public:
  render_graph();

  struct swapchain_info 
  {
    u32 swapchain_image_count;
    VkImage *images;
    VkImageView *image_views;
    VkExtent2D extent;
  };

  uint32_t get_res_uid();
  uint32_t get_stg_uid();

  void setup_swapchain(const swapchain_info &swapchain);

  // Register swapchain targets in the resources array
  void register_swapchain(const graph_swapchain_info &swp);
  // Usage of this buffer
  gpu_buffer &register_buffer(const uid_string &);
  gpu_buffer &get_buffer(const uid_string &);

  // This will schedule the passes and potentially allocate and create them
  render_pass &add_render_pass(const uid_string &);
  compute_pass &add_compute_pass(const uid_string &);
  void add_buffer_update(
    const uid_string &, void *data, u32 offset = 0, u32 size = 0);
  void add_buffer_copy_to_cpu(
    const uid_string &dst, const uid_string &src);
  void add_image_blit(const uid_string &src, const uid_string &dst);

  // Start recording a set of passes / commands
  void begin();

  // End the commands and submit - determines whether or not to create a 
  // command buffer on the fly If there is a present command, will use the 
  // present_cmdbuf_generator If not, will use single_cmdbuf_generator,
  // But can specify custom generator too
  job end(cmdbuf_generator *generator = nullptr);

  template <typename ...T>
  pending_workload submit(job &job, T &&...dependencies)
  {
    class job deps[] = { std::forward<T>(dependencies)... };
    return submit(&job, 1, deps, sizeof...(T));
  }

  pending_workload submit(job &job)
  {
    return submit(&job, 1, nullptr, 0);
  }

  pending_workload submit(job *jobs, int count,
    job *dependencies, int dependency_count);

  // Make sure that this image is what gets presented to the screen
  void present(const uid_string &);

  graph_resource_tracker get_resource_tracker();

private:
  void recycle_submissions_();
  submission *get_successful_submission_();
  VkFence get_fence_();
  VkSemaphore get_semaphore_();
  VkCommandBuffer get_command_buffer_();

  void prepare_pass_graph_stage_(graph_stage_ref ref);
  void prepare_transfer_graph_stage_(graph_stage_ref ref);

  void execute_pass_graph_stage_(
    graph_stage_ref ref, VkPipelineStageFlags &last,
    const cmdbuf_generator::cmdbuf_info &info);

  void execute_transfer_graph_stage_(
    graph_stage_ref ref, const cmdbuf_generator::cmdbuf_info &info);

  inline compute_pass &get_compute_pass_(graph_stage_ref stg) 
    { return passes_[stg].get_compute_pass(); }
  inline render_pass &get_render_pass_(graph_stage_ref stg) 
    { return passes_[stg].get_render_pass(); }

  // For resources, we need to allocate them on the fly - 
  // not the case with passes
  inline graph_resource &get_resource_(graph_resource_ref id) 
  {
    if (resources_.size() <= id)
      resources_.resize(id + 1);
    return resources_[id];
  }

  inline gpu_buffer &get_buffer_(graph_resource_ref id) 
  { 
    auto &res = get_resource_(id);
    if (res.get_type() == graph_resource::type::none)
      new (&res) graph_resource(gpu_buffer(this));
    return res.get_buffer();
  }

  inline gpu_image &get_image_(graph_resource_ref id) 
  {
    if (id == graph_stage_ref_present) 
    {
      return get_image_(get_current_swapchain_ref_());
    }

    auto &res = get_resource_(id);
    if (res.get_type() == graph_resource::type::none)
      new (&res) graph_resource(gpu_image(this));
    return res.get_image();
  }

  inline binding &get_binding_(graph_stage_ref stg, uint32_t binding_idx) 
  {
    if (stg.stage_type == graph_stage_ref::type::pass) 
    {
      if (stg == graph_stage_ref_present)
        return present_info_.b;
      else
        return passes_[stg].get_binding(binding_idx); 
    }
    else 
    {
      return transfers_[stg].get_binding(binding_idx);
    }
  }

  graph_resource_ref get_current_swapchain_ref_() 
  {
    return swapchain_uids[swapchain_img_idx_].id;
  }

public:
  static inline uint32_t rdg_name_id_counter = 0;
  static inline uint32_t stg_name_id_counter = 0;

private:
  static constexpr uint32_t swapchain_img_max_count = 5;
  static inline uid_string swapchain_uids[swapchain_img_max_count] = {};

  uint32_t swapchain_img_idx_;

  // All existing passes that could be used
  std::vector<graph_pass> passes_;
  // All existing resources that could be used
  std::vector<graph_resource> resources_;

  std::vector<graph_stage_ref> recorded_stages_;
  std::vector<graph_resource_ref> used_resources_;

  std::vector<transfer_operation> transfers_;

  std::vector<VkCommandBuffer> free_cmdbufs_;
  std::vector<VkSemaphore> free_semaphores_;
  std::set<VkFence> free_fences_;
  std::vector<submission> submissions_;

  struct present_info 
  {
    graph_resource_ref to_present;
    // Are we presenting with these commands?
    bool is_active;

    binding b;
  } present_info_;

  // Default cmdbuf generators
  present_cmdbuf_generator present_generator_;
  single_cmdbuf_generator single_generator_;

  VkCommandBuffer current_cmdbuf_;

  friend class compute_pass;
  friend class render_pass;
  friend class gpu_image;
  friend class gpu_buffer;
  friend class transfer_operation;
  friend class graph_resource_tracker;
  friend class job;
};

std::string make_shader_src_path(const char *path, VkShaderStageFlags stage);

}

#define RES(x) (uid_string{     \
  x, sizeof(x),               \
  get_id<crc32<sizeof(x)-2>(x)^0xFFFFFFFF>(nz::render_graph::rdg_name_id_counter)})

#define STG(x) (uid_string{     \
  x, sizeof(x),               \
  get_id<crc32<sizeof(x)-2>(x)^0xFFFFFFFF>(nz::render_graph::stg_name_id_counter)})
