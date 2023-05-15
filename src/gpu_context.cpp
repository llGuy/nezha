#include "descriptor_helper.hpp"
#include "vulkan/vulkan_core.h"
#define GLFW_INCLUDE_VULKAN

#include "log.hpp"
#include "bits.hpp"
#include "memory.hpp"
#include "gpu_context.hpp"

#include <vector>

namespace nz
{

gpu_context *gctx;

static void verify_validation_support_(const std::vector<const char *> &layers) 
{
  // TODO
}

static void init_instance_(const gpu_config &config) 
{
  std::vector<const char*> layers = { "VK_LAYER_KHRONOS_validation" };

  if (gctx->is_validation_enabled)
    verify_validation_support_(layers);

  std::vector<const char *> extensions = 
  {
#ifndef NDEBUG
    "VK_EXT_debug_utils",
    "VK_EXT_debug_report",
#endif

    "VK_KHR_portability_enumeration"
  };

  if (config.create_surface)
  {
    const char *ext =
#if defined(_WIN32)
      "VK_KHR_win32_surface";
#elif defined(__ANDROID__)
      "VK_KHR_android_surface";
#elif defined(__APPLE__)
      "VK_EXT_metal_surface";
#else
      "VK_KHR_xcb_surface";
#endif

    extensions.push_back(ext);
    extensions.push_back("VK_KHR_surface");
  }

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pNext = nullptr;
  app_info.pApplicationName = "NULL";
  app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.pEngineName = "";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_1;

  VkInstanceCreateInfo instance_info = {};
  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.pNext = nullptr;
  instance_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
  instance_info.pApplicationInfo = &app_info;
  instance_info.enabledLayerCount = layers.size();
  instance_info.ppEnabledLayerNames = layers.data();
  instance_info.enabledExtensionCount = extensions.size();
  instance_info.ppEnabledExtensionNames = extensions.data();

  VK_CHECK(vkCreateInstance(&instance_info, nullptr, &gctx->instance));

  gctx->layers = layers;
}

VKAPI_ATTR VkBool32 VKAPI_PTR debug_messenger_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT *data,
    void *) 
{
  if (type == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT ||
      type == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) 
  {
    log_error(
      "Validation layer (%d;%d): %s\n",
      type,
      severity,
      data->pMessage);
  }

  return 0;
}

static void init_debug_messenger_() 
{
  VkDebugUtilsMessengerCreateInfoEXT messenger_info = {};
  messenger_info.sType = 
    VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

  messenger_info.pNext = NULL;
  messenger_info.flags = 0;

  messenger_info.messageSeverity =
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

  messenger_info.messageType =
    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

  messenger_info.pfnUserCallback = &debug_messenger_callback;
  messenger_info.pUserData = NULL;

  auto ptr_vkCreateDebugUtilsMessenger = (PFN_vkCreateDebugUtilsMessengerEXT)
    vkGetInstanceProcAddr(gctx->instance, "vkCreateDebugUtilsMessengerEXT");

  VK_CHECK (ptr_vkCreateDebugUtilsMessenger(
    gctx->instance,
    &messenger_info,
    NULL,
    &gctx->messenger));
}

static void init_surface_() 
{
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWmonitor *primaryMonitor = glfwGetPrimaryMonitor();
  const GLFWvidmode *vidmode = glfwGetVideoMode(primaryMonitor);
  gctx->window_width = vidmode->width;
  gctx->window_height = vidmode->height;

  glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

  gctx->window = glfwCreateWindow(
    gctx->window_width, gctx->window_height, "Mirage", nullptr, nullptr);

  if (!gctx->window) 
  {
    log_error("Failed to create window");
    panic_and_exit();
  }

  VK_CHECK(glfwCreateWindowSurface(
    gctx->instance, gctx->window, nullptr, &gctx->surface));
}

struct queue_families 
{
  s32 graphics, present;

  inline bool is_complete() 
  {
    return graphics >= 0 && present >= 0;
  }
};

VkFormat find_suitable_depth_format_(VkFormat *formats, uint32_t format_count,
  VkImageTiling tiling, VkFormatFeatureFlags features)
{
  for (uint32_t i = 0; i < format_count; ++i) 
  {
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(gctx->gpu, formats[i], &properties);

    if (tiling == VK_IMAGE_TILING_LINEAR && 
        (properties.linearTilingFeatures & features) == features) 
    {
      return formats[i];
    }
    else if (tiling == VK_IMAGE_TILING_OPTIMAL && 
             (properties.optimalTilingFeatures & features) == features) 
    {
      return formats[i];
    }
  }

  log_error("Found no depth formats!\n");
  panic_and_exit();

  return VkFormat(0);
}

static void init_device_(const gpu_config &config) 
{
  std::vector<const char *> extensions = 
  {
    VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
    VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
#if defined (__APPLE__)
    "VK_KHR_portability_subset",
    "VK_EXT_shader_viewport_index_layer"
#endif
  };

  if (config.create_surface)
  {
    extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  }

  // Get physical devices
  std::vector<VkPhysicalDevice> devices;
  {
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(gctx->instance, &device_count, NULL);

    devices.resize(device_count);
    vkEnumeratePhysicalDevices(gctx->instance, &device_count, devices.data());
  }

  u32 selected_physical_device = 0;
  for (u32 i = 0; i < devices.size(); ++i) 
  {
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(devices[i], &device_properties);

    if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
        device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) 
    {
      selected_physical_device = i;

      gctx->max_push_constant_size =
        device_properties.limits.maxPushConstantsSize;

      // Get queue families
      u32 queue_family_count = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(
        devices[i], &queue_family_count, nullptr);
      std::vector<VkQueueFamilyProperties> queue_properties;
      queue_properties.resize(queue_family_count);
      vkGetPhysicalDeviceQueueFamilyProperties(
        devices[i], &queue_family_count, queue_properties.data());

      for (u32 f = 0; f < queue_family_count; ++f) 
      {
        if (queue_properties[f].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
            queue_properties[f].queueCount > 0) 
          gctx->graphics_family = f;

        VkBool32 present_support = 0;

        if (config.create_surface)
        {
          vkGetPhysicalDeviceSurfaceSupportKHR(
              devices[i], f, gctx->surface, &present_support);
        }
        else
        {
          present_support = 1;
        }

        if (queue_properties[f].queueCount > 0 && present_support) 
          gctx->present_family = f;

        if (gctx->present_family >= 0 && gctx->graphics_family >= 0) 
          break;
      }
    }
  }

  gctx->gpu = devices[selected_physical_device];

  u32 unique_queue_family_finder = 0;
  unique_queue_family_finder |= 1 << gctx->graphics_family;
  unique_queue_family_finder |= 1 << gctx->present_family;
  u32 unique_queue_family_count = pop_count(unique_queue_family_finder);

  std::vector<u32> unique_family_indices;
  std::vector<VkDeviceQueueCreateInfo> unique_family_infos;

  for (u32 bit = 0, set_bits = 0;
    bit < 32 && set_bits < unique_queue_family_count; ++bit) 
  {
    if (unique_queue_family_finder & (1 << bit)) 
    {
      unique_family_indices.push_back(bit);
      ++set_bits;
    }
  }

  f32 priority1 = 1.0f;
  unique_family_infos.resize(unique_queue_family_count);
  for (u32 i = 0; i < unique_queue_family_count; ++i) 
  {
    VkDeviceQueueCreateInfo queue_info = {};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.pNext = NULL;
    queue_info.flags = 0;
    queue_info.queueFamilyIndex = unique_family_indices[i];
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority1;

    unique_family_infos[i] = queue_info;
  }

  VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_feature = 
  {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
    .dynamicRendering = VK_TRUE,
  };

  VkDeviceCreateInfo device_info = {};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.pNext = &dynamic_rendering_feature;
  device_info.flags = 0;
  device_info.queueCreateInfoCount = unique_queue_family_count;
  device_info.pQueueCreateInfos = unique_family_infos.data();
  device_info.enabledLayerCount = gctx->layers.size();
  device_info.ppEnabledLayerNames = gctx->layers.data();
  device_info.enabledExtensionCount = extensions.size();
  device_info.ppEnabledExtensionNames = extensions.data();
  // device_info.pEnabledFeatures = &requiredFeatures.features;

  VK_CHECK(vkCreateDevice(gctx->gpu, &device_info, nullptr, &gctx->device));

  vkGetDeviceQueue(
    gctx->device, gctx->graphics_family, 0, &gctx->graphics_queue);
  vkGetDeviceQueue(
    gctx->device, gctx->present_family, 0, &gctx->present_queue);

  vkDebugMarkerSetObjectTag = (PFN_vkDebugMarkerSetObjectTagEXT)
    vkGetDeviceProcAddr(gctx->device, "vkDebugMarkerSetObjectTagEXT");
  vkDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)
    vkGetDeviceProcAddr(gctx->device, "vkDebugMarkerSetObjectNameEXT");
  vkCmdDebugMarkerBegin = (PFN_vkCmdDebugMarkerBeginEXT)
    vkGetDeviceProcAddr(gctx->device, "vkCmdDebugMarkerBeginEXT");
  vkCmdDebugMarkerEnd = (PFN_vkCmdDebugMarkerEndEXT)
    vkGetDeviceProcAddr(gctx->device, "vkCmdDebugMarkerEndEXT");
  vkCmdDebugMarkerInsert = (PFN_vkCmdDebugMarkerInsertEXT)
    vkGetDeviceProcAddr(gctx->device, "vkCmdDebugMarkerInsertEXT");
  vkCmdBeginRenderingKHR_proc = (PFN_vkCmdBeginRenderingKHR)
    (vkGetDeviceProcAddr(gctx->device, "vkCmdBeginRenderingKHR"));
  vkCmdEndRenderingKHR_proc = (PFN_vkCmdEndRenderingKHR)
    (vkGetDeviceProcAddr(gctx->device, "vkCmdEndRenderingKHR"));

  // Find depth format
  VkFormat formats[] =
  {
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D24_UNORM_S8_UINT
  };

  gctx->depth_format = find_suitable_depth_format_(
    formats, 3, VK_IMAGE_TILING_OPTIMAL,
    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

static void init_swapchain_() 
{
  VkSurfaceCapabilitiesKHR surface_capabilities;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    gctx->gpu, gctx->surface, &surface_capabilities);

  // Format
  VkSurfaceFormatKHR format = {};
  format.format = VK_FORMAT_B8G8R8A8_UNORM;
  format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

  // Extent
  VkExtent2D surface_extent = {};
  if (surface_capabilities.currentExtent.width != UINT32_MAX) 
  {
    surface_extent = surface_capabilities.currentExtent;
  }
  else 
  {
    surface_extent = { (u32)gctx->window_width, (u32)gctx->window_height };

    surface_extent.width = std::clamp(
      surface_extent.width,
      surface_capabilities.minImageExtent.width,
      surface_capabilities.maxImageExtent.width);

    surface_extent.height = std::clamp(
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
    (u32)gctx->graphics_family, (u32)gctx->present_family };

  VkSwapchainCreateInfoKHR swapchain_info = {};
  swapchain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_info.surface = gctx->surface;
  swapchain_info.minImageCount = image_count;
  swapchain_info.imageFormat = format.format;
  swapchain_info.imageColorSpace = format.colorSpace;
  swapchain_info.imageExtent = surface_extent;
  swapchain_info.imageArrayLayers = 1;
  swapchain_info.imageUsage = 
    VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
    VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  swapchain_info.imageSharingMode = 
    (gctx->graphics_family == gctx->present_family) ?
    VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT;

  swapchain_info.queueFamilyIndexCount =
    (gctx->graphics_family == gctx->present_family) ? 0 : 2;

  swapchain_info.pQueueFamilyIndices =
    (gctx->graphics_family == gctx->present_family) ? 
    NULL : queue_family_indices;

  swapchain_info.preTransform = surface_capabilities.currentTransform;
  swapchain_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain_info.presentMode = present_mode;
  swapchain_info.clipped = VK_TRUE;
  swapchain_info.oldSwapchain = VK_NULL_HANDLE;

  VK_CHECK(vkCreateSwapchainKHR(
    gctx->device, &swapchain_info, NULL, &gctx->swapchain));

  vkGetSwapchainImagesKHR(gctx->device, gctx->swapchain, &image_count, NULL);

  gctx->images = heap_array<VkImage>(image_count);

  VK_CHECK(vkGetSwapchainImagesKHR(
    gctx->device, gctx->swapchain,
    &image_count, gctx->images.data()));

  gctx->swapchain_extent = surface_extent;
  gctx->swapchain_format = format.format;

  gctx->image_views = heap_array<VkImageView>(image_count);

  for (u32 i = 0; i < image_count; ++i) 
  {
    VkImageViewCreateInfo image_view_info = {};
    image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_info.image = gctx->images[i];
    image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_info.format = gctx->swapchain_format;
    image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_view_info.subresourceRange.baseMipLevel = 0;
    image_view_info.subresourceRange.levelCount = 1;
    image_view_info.subresourceRange.baseArrayLayer = 0;
    image_view_info.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(
      gctx->device, &image_view_info, nullptr, &gctx->image_views[i]));
  }
}

void init_command_pool_() 
{
  VkCommandPoolCreateInfo command_pool_info = {};
  command_pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_info.queueFamilyIndex = gctx->graphics_family;
  command_pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  VK_CHECK(vkCreateCommandPool(
    gctx->device, &command_pool_info, nullptr, &gctx->command_pool));
}

void init_descriptor_pool_() 
{
  constexpr u32 set_count = 1000;

  heap_array<VkDescriptorPoolSize> sizes = 
  {
    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLER },
    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER },
    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE },
    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE },
    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER },
    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER },
    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER },
    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER },
    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC },
    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC },
    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT }
  };

  for (u32 i = 0; i < sizes.size(); ++i)
    sizes[i].descriptorCount = set_count;

  u32 max_sets = set_count * sizes.size();

  VkDescriptorPoolCreateInfo descriptor_pool_info = {};
  descriptor_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptor_pool_info.flags =
    VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  descriptor_pool_info.maxSets = max_sets;
  descriptor_pool_info.poolSizeCount = sizes.size();
  descriptor_pool_info.pPoolSizes = sizes.data();

  VK_CHECK(vkCreateDescriptorPool(
    gctx->device, &descriptor_pool_info, nullptr, &gctx->descriptor_pool));
}

void init_descriptor_layout_helper_() 
{
  for (u32 i = 0; i < descriptor_set_layout_category::category_count; ++i)
    gctx->layout_categories[i] = 
      descriptor_set_layout_category((VkDescriptorType)i);
}

void init_gpu_context(const gpu_config &config) 
{
  gctx = mem_alloc<gpu_context>();
  zero_memory(gctx);

  // Set all flags
  gctx->is_validation_enabled = true;

  if (!glfwInit()) 
  {
    log_error("failed to initialize GLFW");
    panic_and_exit();
  }

  init_instance_(config);
  init_debug_messenger_();

  if (config.create_surface)
    init_surface_();
  init_device_(config);

  if (config.create_surface)
    init_swapchain_();

  init_command_pool_();
  init_descriptor_pool_();
  init_descriptor_layout_helper_();
}

VkDescriptorSetLayout get_descriptor_set_layout(
  VkDescriptorType type, u32 count) 
{
  return gctx->layout_categories[(int)type].get_descriptor_set_layout(count);
}

bool is_running() 
{
  return !glfwWindowShouldClose(gctx->window);
}

void poll_input() 
{
  glfwPollEvents();
}

u32 acquire_next_swapchain_image(VkSemaphore semaphore) 
{
  u32 idx = 0;
  vkAcquireNextImageKHR(
    gctx->device, gctx->swapchain, UINT64_MAX, semaphore, VK_NULL_HANDLE, &idx);
  return idx;
}

void present_swapchain_image(VkSemaphore to_wait, u32 image_idx) 
{
  VkPresentInfoKHR present_info = {};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &to_wait;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &gctx->swapchain;
  present_info.pImageIndices = &image_idx;

  vkQueuePresentKHR(gctx->present_queue, &present_info);
}

VkAccessFlags find_access_flags_for_stage(VkPipelineStageFlags stage) 
{
  switch (stage) 
  {
  case VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT:
    return VK_ACCESS_MEMORY_WRITE_BIT;

  case VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT:
  case VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT:
    return VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;

  case VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:
    return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

  case VK_PIPELINE_STAGE_VERTEX_INPUT_BIT:
    return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;

  case VK_PIPELINE_STAGE_VERTEX_SHADER_BIT:
  case VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT:
  case VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT:
    return VK_ACCESS_UNIFORM_READ_BIT;

  case VK_PIPELINE_STAGE_TRANSFER_BIT:
    return VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;

  case VK_PIPELINE_STAGE_ALL_COMMANDS_BIT:
    return VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;

  default: 
  {
    log_error("Didn't handle stage for finding access flags %d!", stage);
    panic_and_exit();
  } return 0;
  }
}

VkAccessFlags find_access_flags_for_layout(VkImageLayout layout) 
{
  switch (layout) 
  {
  case VK_IMAGE_LAYOUT_UNDEFINED: case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
    return 0;

  case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
    return VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT;

  case VK_IMAGE_LAYOUT_GENERAL:
    return VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;

  case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
    return VK_ACCESS_TRANSFER_WRITE_BIT;

  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
    return VK_ACCESS_SHADER_READ_BIT;

  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
    return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  default: 
  {
    log_error("Didn't handle image layout for finding access flags!");
    panic_and_exit();
  } return 0;
  }
}

u32 find_memory_type(
  VkMemoryPropertyFlags properties, VkMemoryRequirements &memory_requirements) 
{
  VkPhysicalDeviceMemoryProperties mem_properties;
  vkGetPhysicalDeviceMemoryProperties(gctx->gpu, &mem_properties);

  for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) 
  {
    if (memory_requirements.memoryTypeBits & (1 << i) &&
        (mem_properties.memoryTypes[i].propertyFlags & properties) == 
          properties)
      return i;
  }

  log_error("Unable to find memory type!");
  panic_and_exit();

  return 0;
}

VkDeviceMemory allocate_buffer_memory(
  VkBuffer buffer, VkMemoryPropertyFlags properties) 
{
  VkMemoryRequirements requirements = {};
  vkGetBufferMemoryRequirements(gctx->device, buffer, &requirements);

  VkMemoryAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = requirements.size;
  alloc_info.memoryTypeIndex = find_memory_type(properties, requirements);

  VkDeviceMemory memory;
  vkAllocateMemory(gctx->device, &alloc_info, nullptr, &memory);

  vkBindBufferMemory(gctx->device, buffer, memory, 0);

  return memory;
}

VkDeviceMemory allocate_image_memory(
  VkImage image, VkMemoryPropertyFlags properties, u32 *size) 
{
  VkMemoryRequirements requirements = {};
  vkGetImageMemoryRequirements(gctx->device, image, &requirements);

  VkMemoryAllocateInfo alloc_info = {};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = requirements.size;
  alloc_info.memoryTypeIndex = find_memory_type(properties, requirements);

  VkDeviceMemory memory;
  vkAllocateMemory(gctx->device, &alloc_info, nullptr, &memory);

  vkBindImageMemory(gctx->device, image, memory, 0);

  if (size)
    *size = requirements.size;

  return memory;
}

// Debug marker functions
PFN_vkDebugMarkerSetObjectTagEXT vkDebugMarkerSetObjectTag;
PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectName;
PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBegin;
PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEnd;
PFN_vkCmdDebugMarkerInsertEXT vkCmdDebugMarkerInsert;
PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR_proc;
PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR_proc;

}
