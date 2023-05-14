#ifndef _GRAPH_TRANSFER_H_
#define _GRAPH_TRANSFER_H_

#include <nezha/int.h>

enum graph_transfer_operation_type 
{
  JOB_TRANSFER_OPERATION_TYPE_BUFFER_UPDATE, 
  JOB_TRANSFER_OPERATION_TYPE_BUFFER_COPY, 
  JOB_TRANSFER_OPERATION_TYPE_IMAGE_COPY, 
  JOB_TRANSFER_OPERATION_TYPE_IMAGE_BLIT, 
  JOB_TRANSFER_OPERATION_TYPE_MAX
};

struct graph_transfer_operation
{
  enum graph_transfer_operation_type type;
  struct nz_graph *graph;

  union 
  {
    struct 
    {
      void *data;
      u32 offset;
      u32 size;
    } buffer_update_state;

    /* TODO: */
    struct 
    {
    } buffer_copy_state;

    struct 
    {
    } image_copy_state;

    struct 
    {
    } image_blit_state;
  };
};

#endif
