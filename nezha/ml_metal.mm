#include "ml_metal.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_metal.h>

#import <MetalPerformanceShaders/MetalPerformanceShaders.h>

namespace nz
{

struct acc_matrix_descriptor
{
  MPSMatrixDescriptor *descriptor;
  MPSMatrix *matrix;
};


struct acc_kernel
{
  ml_kernel kernel_type;

  union
  {
    MPSMatrixMultiplication *multiplication;
  };
};

static id<MTLDevice> get_mtl_device()
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

  return export_device.mtlDevice;
}

static id<MTLCommandQueue> get_mtl_command_queue()
{
  VkDevice vk_device = nz::gctx->device;
  VkQueue vk_queue = nz::gctx->graphics_queue;

  VkExportMetalCommandQueueInfoEXT export_queue = {
    .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_COMMAND_QUEUE_INFO_EXT,
    .queue = vk_queue,
    .mtlCommandQueue = nullptr,
    .pNext = nullptr
  };

  VkExportMetalObjectsInfoEXT export_object = {
    .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT,
    .pNext = &export_queue
  };

  vkExportMetalObjectsEXT(vk_device, &export_object);

  return export_queue.mtlCommandQueue;
}

static id<MTLSharedEvent> get_mtl_shared_event(VkSemaphore sema)
{
  VkDevice vk_device = nz::gctx->device;

  VkExportMetalSharedEventInfoEXT export_event = {
    .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_SHARED_EVENT_INFO_EXT,
    .semaphore = sema,
    .event = VK_NULL_HANDLE,
    .mtlSharedEvent = nullptr,
    .pNext = nullptr
  };

  VkExportMetalObjectsInfoEXT export_object = {
    .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT,
    .pNext = &export_event
  };

  vkExportMetalObjectsEXT(vk_device, &export_object);

  return export_event.mtlSharedEvent;
}

static id<MTLBuffer> get_mtl_buffer(VkDeviceMemory mem)
{
  VkDevice vk_device = nz::gctx->device;

  VkExportMetalBufferInfoEXT export_buffer = {
    .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_BUFFER_INFO_EXT,
    .memory = mem,
    .mtlBuffer = nullptr,
    .pNext = nullptr
  };

  VkExportMetalObjectsInfoEXT export_object = {
    .sType = VK_STRUCTURE_TYPE_EXPORT_METAL_OBJECTS_INFO_EXT,
    .pNext = &export_buffer
  };

  vkExportMetalObjectsEXT(vk_device, &export_object);

  return export_buffer.mtlBuffer;
}

acc_matrix_descriptor *make_acc_matrix_descriptor(VkDeviceMemory mem, uint32_t rows, uint32_t cols)
{
  id<MTLBuffer> buf = get_mtl_buffer(mem);

  MPSMatrixDescriptor *desc = [MPSMatrixDescriptor matrixDescriptorWithDimensions:rows 
                              columns:cols rowBytes:rows*sizeof(float) dataType:MPSDataTypeFloat32];

  MPSMatrix *matrix = [[MPSMatrix alloc] initWithBuffer:buf descriptor:desc];

  acc_matrix_descriptor *acc_desc = (acc_matrix_descriptor *)malloc(sizeof(acc_matrix_descriptor));
  acc_desc->descriptor = desc;
  acc_desc->matrix = matrix;

  return acc_desc;
}

acc_kernel *make_acc_kernel(ml_kernel kernel_type, const ml_kernel_config &cfg)
{
  acc_kernel *k = (acc_kernel *)malloc(sizeof(acc_kernel));
  k->kernel_type = kernel_type;

  switch (kernel_type)
  {
    case ml_kernel::matrix_multiplication:
    {
      k->multiplication = [[MPSMatrixMultiplication alloc] initWithDevice:get_mtl_device()
        transposeLeft:false transposeRight:false resultRows:cfg.matrix_multiplication.result_rows 
        resultColumns:cfg.matrix_multiplication.result_columns
        interiorColumns:cfg.matrix_multiplication.interior_columns alpha:1 beta:0];
    } break;

    default: break;
  }
}

void encode_matrix_multiplication(VkSemaphore wait_semaphore, VkSemaphore signal_semaphore,
                                  acc_kernel *kernel,
                                  acc_matrix_descriptor *a, acc_matrix_descriptor *b,
                                  acc_matrix_descriptor *result)
{
  id<MTLCommandQueue> cmd_queue = get_mtl_command_queue();
  id<MTLSharedEvent> wait_event = get_mtl_shared_event(wait_semaphore);
  id<MTLSharedEvent> signal_event = get_mtl_shared_event(signal_semaphore);

  id<MTLCommandBuffer> cmdbuf = [cmd_queue commandBuffer];
  cmdbuf.label = @"ML Kernel Command Buffer";

  [cmdbuf encodeWaitForEvent:wait_event value:(wait_event.signaledValue+1)];

  [kernel->multiplication encodeToCommandBuffer:cmdbuf leftMatrix:a->matrix 
    rightMatrix:b->matrix resultMatrix:result->matrix];

  [cmdbuf encodeSignalEvent:signal_event value:(signal_event.signaledValue+1)];

  [cmdbuf commit];
}

}
