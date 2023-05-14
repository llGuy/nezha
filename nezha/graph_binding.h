#ifndef _GRAPH_BINDING_H_
#define _GRAPH_BINDING_H_

#include <vulkan/vulkan.h>

#include "graph.h"

enum graph_binding_type
{
  /* Image binding types. */
  GRAPH_BINDING_TYPE_SAMPLED_IMAGE, 
  GRAPH_BINDING_TYPE_STORAGE_IMAGE, 
  GRAPH_BINDING_TYPE_COLOR_ATTACHMENT, 
  GRAPH_BINDING_TYPE_DEPTH_ATTACHMENT, 
  GRAPH_BINDING_TYPE_IMAGE_TRANSFER_SRC, 
  GRAPH_BINDING_TYPE_IMAGE_TRANSFER_DST, 
  GRAPH_BINDING_TYPE_MAX_IMAGE, 

  /* Buffer binding types. */
  GRAPH_BINDING_TYPE_STORAGE_BUFFER, 
  GRAPH_BINDING_TYPE_UNIFORM_BUFFER, 
  GRAPH_BINDING_TYPE_BUFFER_TRANSFER_SRC, 
  GRAPH_BINDING_TYPE_BUFFER_TRANSFER_DST, 
  GRAPH_BINDING_TYPE_VERTEX_BUFFER, 
  GRAPH_BINDING_TYPE_MAX_BUFFER,
};

struct graph_binding
{
  /* Multiple things can be bound to the same stage. This is the index
   * of this binding in that group. */
  int idx;

  /* The binding type. */
  enum graph_binding_type type;

  /* This is the ID of the USED_RESOURCE which corresponds to an index into the
   * USED_RESOURCES array in the NZ_JOB structure. */
  int id;

  struct graph_res_usage_node next;

  /* Other data that may be needed (TODO: For render passes, clear color...) */
};

/* Some helper functions. */
VkDescriptorType translate_descriptor_type(enum graph_binding_type);
VkImageLayout translate_image_layout(enum graph_binding_type);
VkAccessFlags translate_buffer_access(enum graph_binding_type);
VkAccessFlags translate_image_access(enum graph_binding_type);

#endif
