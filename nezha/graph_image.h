#ifndef _GRAPH_IMAGE_H_
#define _GRAPH_IMAGE_H_

#include <nezha/int.h>
#include <vulkan/vulkan.h>

#include "graph.h"
#include "graph_binding.h"

struct graph_image
{
  /* Actual Vulkan object handles are stored here.  */
  VkImage image;
  VkImageView view;
  VkDeviceMemory memory;

  /* Information about the image. */
  VkExtent3D extent;
  VkImageAspectFlags aspect;
  VkFormat format;

  /* Fields used by the graph as it gets updated and executed. */
  VkImageLayout current_layout;
  VkAccessFlags current_access;
  VkPipelineStageFlags last_used;
  VkImageUsageFlags usage;

  /* Descriptor set cache. */
  VkDescriptorSet sets[GRAPH_BINDING_TYPE_MAX_IMAGE];

  /* Gets marked as true if the graph needs to create this resource. */
  b8 to_create;
};

#endif
