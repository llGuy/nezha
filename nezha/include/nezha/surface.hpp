#pragma once

#include <GLFW/glfw3.h>
#include <nezha/job.hpp>
#include <nezha/types.hpp>
#include <vulkan/vulkan.h>
#include <nezha/heap_array.hpp>


namespace nz
{


/* Provides interface for anything to do with input/output (keyboard, mouse, touch,
 * scroll, etc...) */
class io_context
{
public:
  /* User hasn't yet pressed the red X button to close the window. */
  bool is_window_open();

  /* Poll input from the user. Calls all input callbacks. */
  void poll_input();

private:
  io_context(GLFWwindow *window);

private:
  GLFWwindow *window_;

  friend class surface;
};


struct surface_data;


/* Provides interface for anything rendering related for windowing/surface things. */
class surface
{
public:
  /* How many images are there in the swapchain. */
  u32 get_swapchain_image_count() const;

  /* Swapchain format. */
  VkFormat get_swapchain_format() const;

  /* Acquire next swapchain image. Returns a JOB because submission for the next
   * rendered frame will depend on a swapchain image being acquired. This
   * will write the new image index into IMAGE_IDX. */
  nz::job acquire_next_swapchain_image(render_graph &graph, u32 &image_idx);

  /* Presents the image to the screen. */
  void present(const nz::job &render_job, u32 image_idx);

  /* Get the IO context of this surface such that the user can perform
   * keyboard/mouse input queries. */
  io_context &io();

public:
  surface(surface_data &data);

private:
  io_context ctx_;
  u32 window_width_, window_height_;
  VkSurfaceKHR surface_;
  VkSwapchainKHR swapchain_;
  VkFormat swapchain_format_;
  VkExtent2D swapchain_extent_;
  std::vector<VkImage> images_;
  std::vector<VkImageView> image_views_;

  friend class render_graph;
};


/* Reserved for internal use. */
struct surface_data
{
  GLFWwindow *window;
  u32 window_width, window_height;
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
  VkExtent2D swapchain_extent;
  VkFormat swapchain_format;
  std::vector<VkImage> images;
  std::vector<VkImageView> image_views;
};
  
}
