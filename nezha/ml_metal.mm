#include "ml_metal.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>

void ml_metal_test()
{
  VkDevice vk_device = nz::gctx->device;

  VkExportMetalDeviceInfoEXT export_device = {
    .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_DEVICE_INFO_EXT,
    .mtlDevice = nullptr,
    .pNext = nullptr
  };

  VkExportMetalObjectsInfoEXT export_object = {
    .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT,
    .pNext = &export_device
  };

  vkExportMetalObjectsEXT(vk_device, &export_object);

  // id<MTLDevice> mtl_device = export_device.mtlDevice;

  int test_lib = (int)[export_device.mtlDevice maxBufferLength];

  nz::log_info("Hello there %p", (void *)export_device.mtlDevice);
}
