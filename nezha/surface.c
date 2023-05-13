#include "gpu.h"
#include "surface.h"

#include <stdlib.h>
#include <nezha/util.h>
#include <nezha/surface.h>

struct nz_surface *nz_surface_prepare(const struct nz_surface_cfg *cfg)
{
  struct nz_surface *surface = (struct nz_surface *)
    malloc(sizeof(struct nz_surface));

  if (!cfg->is_api_initialized && !glfwInit())
    nz_panic_and_exit("Failed to initialize GLFW\n");

  surface->width = cfg->width;
  surface->height = cfg->height;
  surface->name = cfg->name;

  return surface;
}

void surface_make(struct nz_gpu *gpu, struct nz_surface *surface)
{
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWmonitor *primaryMonitor = glfwGetPrimaryMonitor();
  const GLFWvidmode *vidmode = glfwGetVideoMode(primaryMonitor);
  surface->width = vidmode->width;
  surface->height = vidmode->height;

  glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

  surface->window = glfwCreateWindow(
    surface->width, surface->height, surface->name, NULL, NULL);

  if (!surface->window) 
    nz_panic_and_exit("Failed to create window");

  VK_CHECK(glfwCreateWindowSurface(gpu->instance, 
                                   surface->window, NULL, &surface->surface));
}

void surface_create_swapchain(struct nz_gpu *gpu, struct nz_surface *surface)
{
  VkSurfaceCapabilitiesKHR surface_capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    gpu->physical_device, surface->surface, &surface_capabilities);

  VkSurfaceFormatKHR format = {};
  format.format = VK_FORMAT_B8G8R8A8_UNORM;
  format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

  VkExtent2D surface_extent = {};
  if (surface_capabilities.currentExtent.width != UINT32_MAX) 
  {
    surface_extent = surface_capabilities.currentExtent;
  }
  else 
  {
    surface_extent.width = (u32)surface->width;
    surface_extent.height = (u32)surface->height;

    surface_extent.width = NZ_CLAMP(
      surface_extent.width,
      surface_capabilities.minImageExtent.width,
      surface_capabilities.maxImageExtent.width);

    surface_extent.height = NZ_CLAMP(
      surface_extent.height,
      surface_capabilities.minImageExtent.height,
      surface_capabilities.maxImageExtent.height);
  }

  // Present mode
  VkPresentModeKHR present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;

  // add 1 to the minimum images supported in the swapchain
  u32 image_count = surface_capabilities.minImageCount + 1;

  if (image_count > surface_capabilities.maxImageCount && 
      surface_capabilities.maxImageCount)
    image_count = surface_capabilities.maxImageCount;

  u32 queue_family_indices[] = { 
    (u32)gpu->graphics_family,
    (u32)gpu->present_family 
  };

  VkSwapchainCreateInfoKHR swapchain_info = {};
  swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_info.surface = surface->surface;
  swapchain_info.minImageCount = image_count;
  swapchain_info.imageFormat = format.format;
  swapchain_info.imageColorSpace = format.colorSpace;
  swapchain_info.imageExtent = surface_extent;
  swapchain_info.imageArrayLayers = 1;
  swapchain_info.imageUsage = 
    VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
    VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  swapchain_info.imageSharingMode = 
    (gpu->graphics_family == gpu->present_family) ?
    VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT;

  swapchain_info.queueFamilyIndexCount =
    (gpu->graphics_family == gpu->present_family) ? 0 : 2;

  swapchain_info.pQueueFamilyIndices =
    (gpu->graphics_family == gpu->present_family) ? 
    NULL : queue_family_indices;

  swapchain_info.preTransform = surface_capabilities.currentTransform;
  swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain_info.presentMode = present_mode;
  swapchain_info.clipped = VK_TRUE;
  swapchain_info.oldSwapchain = VK_NULL_HANDLE;

  VK_CHECK(vkCreateSwapchainKHR(gpu->device, &swapchain_info, 
                                NULL, &surface->swapchain));

  vkGetSwapchainImagesKHR(gpu->device, surface->swapchain, &image_count, NULL);

  surface->image_count = image_count;

  VK_CHECK(vkGetSwapchainImagesKHR(gpu->device, surface->swapchain,
                                   &image_count, surface->images));

  surface->swapchain_extent = surface_extent;
  surface->swapchain_format = format.format;

  for (u32 i = 0; i < image_count; ++i) 
  {
    VkImageViewCreateInfo image_view_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = surface->images[i],
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = surface->swapchain_format,
      .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .subresourceRange.baseMipLevel = 0,
      .subresourceRange.levelCount = 1,
      .subresourceRange.baseArrayLayer = 0,
      .subresourceRange.layerCount = 1
    };

    VK_CHECK(vkCreateImageView(gpu->device, &image_view_info, 
                               NULL, &surface->image_views[i]));
  }
}
