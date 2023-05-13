#ifndef _GRAPH_BUFFER_H_
#define _GRAPH_BUFFER_H_

#include <vulkan/vulkan.h>

#include "graph_binding.h"

struct graph_buffer
{
  /* Vulkan objects / data */
  VkBuffer buffer;
  VkDeviceMemory memory;
  size_t size;

  /* Buffer information that gets used throughout graph building. */
  VkBufferUsageFlags usage;
  VkAccessFlags current_access;
  VkPipelineStageFlags last_used;

  VkDescriptorSet sets[
    GRAPH_BINDING_TYPE_MAX_BUFFER - GRAPH_BINDING_TYPE_MAX_IMAGE];
};

#endif
