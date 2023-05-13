#ifndef _SURFACE_H_
#define _SURFACE_H_

#define GLFW_INCLUDE_VULKAN

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include <nezha/gpu.h>
#include <nezha/surface.h>

#define NZ_MAX_SURFACE_IMAGES (5)

struct nz_surface
{
  GLFWwindow *window;

  int width;
  int height;
  const char *name;

  /* Vulkan surface/swapchain objects. */
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;

  /* Image data for the surface */
  int image_count;
  VkImage images[NZ_MAX_SURFACE_IMAGES];
  VkImageView image_views[NZ_MAX_SURFACE_IMAGES];
  VkExtent2D swapchain_extent;
  VkFormat swapchain_format;
};

/* Initializes the surface with appropriate objects. */
void surface_make(struct nz_gpu *gpu, struct nz_surface *);
void surface_create_swapchain(struct nz_gpu *gpu, struct nz_surface *surface);

#endif
