#ifndef _GPU_H_
#define _GPU_H_

#include <nezha/int.h>
#include <nezha/util.h>
#include <vulkan/vulkan.h>

/* Encapsulates all necessary GPU state. */
struct nz_gpu
{
  /* Core Vulkan objects. */
  VkInstance instance;
  VkPhysicalDevice physical_device;
  VkDevice device;
  VkDebugUtilsMessengerEXT dbg;

  /* Queue information. */
  int graphics_family; 
  int present_family;
  int transfer_family;
  VkQueue graphics_queue;
  VkQueue present_queue;
  VkQueue transfer_queue;

  /* Hardware information. */
  VkFormat depth_format;
  u32 max_push_constant_size;

  /* Pools. */
  VkCommandPool command_pool;
  VkDescriptorPool descriptor_pool;
};

#define VK_CHECK(call) \
  if (call != VK_SUCCESS)\
  { nz_panic_and_exit(#call); }

/* Extension functions that are needed. */
extern PFN_vkDebugMarkerSetObjectTagEXT vkDebugMarkerSetObjectTag;
extern PFN_vkDebugMarkerSetObjectNameEXT vkDebugMarkerSetObjectName;
extern PFN_vkCmdDebugMarkerBeginEXT vkCmdDebugMarkerBegin;
extern PFN_vkCmdDebugMarkerEndEXT vkCmdDebugMarkerEnd;
extern PFN_vkCmdDebugMarkerInsertEXT vkCmdDebugMarkerInsert;
extern PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR_proc;
extern PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR_proc;

#endif
