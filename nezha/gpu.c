#include <stdio.h>
#include <stdlib.h>
#include <nezha/gpu.h>

#include "gpu.h"
#include "surface.h"

struct queue_families 
{
  int graphics;
  int present;
  int transfer;
};

static VkInstance gpu_create_instance(const char **layers, int layer_count);
static b8 gpu_is_validation_supported(const char **layers, int count);
static VkDebugUtilsMessengerEXT gpu_create_messenger(VkInstance);
static VKAPI_ATTR VkBool32 VKAPI_PTR gpu_debug_messenger_callback(
  VkDebugUtilsMessageSeverityFlagBitsEXT severity,
  VkDebugUtilsMessageTypeFlagsEXT type,
  const VkDebugUtilsMessengerCallbackDataEXT *data,
  void *);
static b8 gpu_is_family_complete(struct queue_families *fam);
static VkFormat gpu_find_suitable_depth_format(struct nz_gpu *gpu, 
  VkFormat *formats, uint32_t format_count, 
  VkImageTiling tiling, VkFormatFeatureFlags features);
static VkDevice gpu_create_device(struct nz_gpu *gpu, struct nz_surface *surface,
                                  const char **layers, int layer_count);


struct nz_gpu *nz_gpu_create(struct nz_gpu_cfg *cfg)
{
  struct nz_gpu *gpu = (struct nz_gpu *)malloc(sizeof(struct nz_gpu));
  struct nz_surface *surface = cfg->surface;

  const char *layers[] = { "VK_LAYER_KHRONOS_validation" };
  int layer_count = sizeof(layers)/sizeof(layers[0]);

  gpu->instance = gpu_create_instance(layers, layer_count);
  gpu->dbg = gpu_create_messenger(gpu->instance);

  /* We create the surface before creating the device. */
  if (cfg->surface)
    surface_make(gpu, surface);

  gpu->device = gpu_create_device(gpu, surface, layers, layer_count);

  /* We create the swapchain after the device. Annoying double if statement
   * but whatever.*/
  if (cfg->surface)
    surface_create_swapchain(gpu, surface);

  return gpu;
}


static VkInstance gpu_create_instance(const char **layers, int layer_count)
{
  if (!gpu_is_validation_supported(layers, layer_count))
    layer_count = 0;

  const char *extensions[] = {
#if defined(_WIN32)
    "VK_KHR_win32_surface",
#elif defined(__ANDROID__)
    "VK_KHR_android_surface"
#elif defined(__APPLE__)
    "VK_EXT_metal_surface",
#else
    "VK_KHR_xcb_surface",
#endif
    "VK_KHR_surface",

#ifndef NDEBUG
    "VK_EXT_debug_utils",
    "VK_EXT_debug_report",
#endif

    "VK_KHR_portability_enumeration"
  };

  VkApplicationInfo app_info = {
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
    .pNext = NULL,
    .pApplicationName = "NULL",
    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
    .pEngineName = "",
    .engineVersion = VK_MAKE_VERSION(1, 0, 0),
    .apiVersion = VK_API_VERSION_1_1
  };

  VkInstanceCreateInfo instance_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pNext = NULL,
    .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
    .pApplicationInfo = &app_info,
    .enabledLayerCount = layer_count,
    .ppEnabledLayerNames = layers,
    .enabledExtensionCount = sizeof(extensions)/sizeof(extensions[0]),
    .ppEnabledExtensionNames = extensions,
  };

  VkInstance instance;
  VK_CHECK(vkCreateInstance(&instance_info, NULL, &instance));

  return instance;
}

static b8 gpu_is_validation_supported(const char **layers, int count)
{
  /* Get supported validation layers */
  uint32_t supported_layer_count = 0;
  vkEnumerateInstanceLayerProperties(&supported_layer_count, NULL);
  VkLayerProperties *supported_layers = NZ_STACK_ALLOC(VkLayerProperties, 
                                                       supported_layer_count);

  vkEnumerateInstanceLayerProperties(&supported_layer_count, supported_layers);

  uint32_t requested_verified_count = 0;

  for (uint32_t requested = 0; requested < count; ++requested) 
  {
    b8 found = 0;

    for (uint32_t avail = 0; avail < supported_layer_count; ++avail) 
    {
      if (!strcmp(supported_layers[avail].layerName,
                  layers[requested])) {
        found = 1;
        break;
      }
    }

    if (!found) 
      return 0;
  }

  return 1;
}

static VkDebugUtilsMessengerEXT gpu_create_messenger(VkInstance instance)
{
  VkDebugUtilsMessengerCreateInfoEXT messenger_info = {
   .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    .pNext = NULL,
    .flags = 0,
    .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
    .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
    .pfnUserCallback = &gpu_debug_messenger_callback,
    .pUserData = NULL
  };

  PFN_vkCreateDebugUtilsMessengerEXT ptr_vkCreateDebugUtilsMessenger =
    (PFN_vkCreateDebugUtilsMessengerEXT) 
    vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

  VkDebugUtilsMessengerEXT messenger;
  VK_CHECK(ptr_vkCreateDebugUtilsMessenger(instance,
                                           &messenger_info,
                                           NULL,
                                           &messenger));

  return messenger;
}

static VKAPI_ATTR VkBool32 VKAPI_PTR gpu_debug_messenger_callback(
  VkDebugUtilsMessageSeverityFlagBitsEXT severity,
  VkDebugUtilsMessageTypeFlagsEXT type,
  const VkDebugUtilsMessengerCallbackDataEXT *data,
  void *ptr)
{
  (void)ptr;
  if (type == VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT ||
      type == VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) 
    printf("Validation layer (%d;%d): %s\n", type, severity, data->pMessage);

  return 0;
}

static b8 gpu_is_family_complete(struct queue_families *fam) 
{
  return fam->graphics >= 0 && fam->present >= 0 && fam->transfer >= 0;
}

static VkFormat gpu_find_suitable_depth_format(struct nz_gpu *gpu, VkFormat *formats, 
  uint32_t format_count, VkImageTiling tiling, VkFormatFeatureFlags features)
{
  for (uint32_t i = 0; i < format_count; ++i) 
  {
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(gpu->physical_device, formats[i], &properties);

    if (tiling == VK_IMAGE_TILING_LINEAR && 
        (properties.linearTilingFeatures & features) == features) 
      return formats[i];
    else if (tiling == VK_IMAGE_TILING_OPTIMAL && 
            (properties.optimalTilingFeatures & features) == features) 
      return formats[i];
  }

  nz_panic_and_exit("Failed to find depth format");

  return 0;
}

static VkDevice gpu_create_device(struct nz_gpu *gpu, struct nz_surface *surface,
                                  const char **layers, int layer_count)
{
  const char *extensions[] = {
    VK_EXT_DEBUG_MARKER_EXTENSION_NAME,
#if defined (__APPLE__)
    "VK_KHR_portability_subset",
    "VK_EXT_shader_viewport_index_layer",
#endif
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME,
    VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
  };

  u32 extension_count = sizeof(extensions)/sizeof(extensions[0]);

  /* Get rid of all the ones to do with rendering to the screen. */
  if (!surface)
    extension_count -= 4;

  // Get physical devices
  VkPhysicalDevice *devices;
  u32 device_count = 0;
  {
    vkEnumeratePhysicalDevices(gpu->instance, &device_count, NULL);

    devices = (VkPhysicalDevice *)malloc(
      sizeof(VkPhysicalDevice) * device_count);
    vkEnumeratePhysicalDevices(gpu->instance, &device_count, devices);
  }

  u32 selected_physical_device = 0;

  for (u32 i = 0; i < device_count; ++i) 
  {
    VkPhysicalDeviceProperties device_properties;
    vkGetPhysicalDeviceProperties(devices[i], &device_properties);

    if (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
        device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) 
    {
      selected_physical_device = i;

      gpu->max_push_constant_size =
        device_properties.limits.maxPushConstantsSize;

      // Get queue families
      u32 queue_family_count = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(devices[i], 
                                               &queue_family_count, NULL);

      VkQueueFamilyProperties *queue_family_properties = (VkQueueFamilyProperties *)
        malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);

      vkGetPhysicalDeviceQueueFamilyProperties(
        devices[i], &queue_family_count, queue_family_properties);

      for (u32 f = 0; f < queue_family_count; ++f) 
      {
        if (queue_family_properties[f].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
            queue_family_properties[f].queueCount > 0) 
          gpu->graphics_family = f;

        if (queue_family_properties[f].queueFlags & VK_QUEUE_TRANSFER_BIT &&
            queue_family_properties[f].queueCount > 0)
          gpu->transfer_family = f;

        VkBool32 present_support = 0;

        if (surface)
        {
          vkGetPhysicalDeviceSurfaceSupportKHR(
            devices[i], f, surface->surface, &present_support);
        }
        else
        {
          /* We will just create the device anyway. */
          present_support = 1;
        }

        if (queue_family_properties[f].queueCount > 0 && present_support) 
          gpu->present_family = f;

        if (gpu->present_family >= 0 && gpu->graphics_family >= 0 && 
            gpu->transfer_family >= 0) 
          break;
      }
    }
  }

  gpu->physical_device = devices[selected_physical_device];

  u32 unique_queue_family_finder = 0;
  unique_queue_family_finder |= 1 << gpu->graphics_family;
  unique_queue_family_finder |= 1 << gpu->present_family;
  unique_queue_family_finder |= 1 << gpu->transfer_family;
  u32 unique_queue_family_count = nz_pop_count(unique_queue_family_finder);

  u32 queue_count = 0;
  u32 *unique_family_indices = (u32 *)malloc(
    sizeof(u32) * unique_queue_family_finder);
  VkDeviceQueueCreateInfo *unique_family_infos = (VkDeviceQueueCreateInfo *)
    malloc(sizeof(VkDeviceQueueCreateInfo) * unique_queue_family_count);

  for (u32 bit = 0, set_bits = 0;
    bit < 32 && set_bits < unique_queue_family_count; ++bit) 
  {
    if (unique_queue_family_finder & (1 << bit)) 
    {
      unique_family_indices[queue_count++] =bit;
      ++set_bits;
    }
  }

  f32 priority1 = 1.0f;
  for (u32 i = 0; i < unique_queue_family_count; ++i) 
  {
    VkDeviceQueueCreateInfo queue_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .queueFamilyIndex = unique_family_indices[i],
      .queueCount = 1,
      .pQueuePriorities = &priority1
    };

    unique_family_infos[i] = queue_info;
  }

  VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_feature = 
  {
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
    .dynamicRendering = VK_TRUE,
  };

  VkDeviceCreateInfo device_info = {
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext = &dynamic_rendering_feature,
    .flags = 0,
    .queueCreateInfoCount = unique_queue_family_count,
    .pQueueCreateInfos = unique_family_infos,
    .enabledLayerCount = layer_count,
    .ppEnabledLayerNames = layers,
    .enabledExtensionCount = extension_count,
    .ppEnabledExtensionNames = extensions
  };

  VkDevice device;
  VK_CHECK(vkCreateDevice(gpu->physical_device, &device_info, NULL, &device));

  vkGetDeviceQueue(device, gpu->graphics_family, 0, &gpu->graphics_queue);
  vkGetDeviceQueue(device, gpu->present_family, 0, &gpu->present_queue);
  vkGetDeviceQueue(device, gpu->transfer_family, 0, &gpu->transfer_queue);

  vkDebugMarkerSetObjectTag = (PFN_vkDebugMarkerSetObjectTagEXT)
    vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectTagEXT");
  vkDebugMarkerSetObjectName = (PFN_vkDebugMarkerSetObjectNameEXT)
    vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectNameEXT");
  vkCmdDebugMarkerBegin = (PFN_vkCmdDebugMarkerBeginEXT)
    vkGetDeviceProcAddr(device, "vkCmdDebugMarkerBeginEXT");
  vkCmdDebugMarkerEnd = (PFN_vkCmdDebugMarkerEndEXT)
    vkGetDeviceProcAddr(device, "vkCmdDebugMarkerEndEXT");
  vkCmdDebugMarkerInsert = (PFN_vkCmdDebugMarkerInsertEXT)
    vkGetDeviceProcAddr(device, "vkCmdDebugMarkerInsertEXT");
  vkCmdBeginRenderingKHR_proc = (PFN_vkCmdBeginRenderingKHR)
    vkGetDeviceProcAddr(device, "vkCmdBeginRenderingKHR");
  vkCmdEndRenderingKHR_proc = (PFN_vkCmdEndRenderingKHR)
    vkGetDeviceProcAddr(device, "vkCmdEndRenderingKHR");

  /* Find depth format */
  VkFormat formats[] =
  {
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D24_UNORM_S8_UINT
  };

  gpu->device = device;
  gpu->depth_format = gpu_find_suitable_depth_format(
    gpu, formats, 3, VK_IMAGE_TILING_OPTIMAL,
    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

  return device;
}

PFN_vkDebugMarkerSetObjectTagEXT vkDebugMarkerSetObjectTag;
PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectName;
PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBegin;
PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEnd;
PFN_vkCmdDebugMarkerInsertEXT vkCmdDebugMarkerInsert;
PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR_proc;
PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR_proc;
