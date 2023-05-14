#ifndef _NZ_GRAPH_H_
#define _NZ_GRAPH_H_

#include <stdlib.h>
#include <nezha/gpu.h>
#include <vulkan/vulkan.h>


/* Opaque handle to a graph resource. This could be to a buffer or image,
 * any kind of compute/render stage.
 *
 * Application will hold on to these in order to reference these objects in
 * command recording phase. */
typedef int nz_graph_hdl_t;


/* This is the object that encapsulates all the computation/render graph
 * infrastructure. */
struct nz_graph;


/* These functions create new IDs to be used to bind to graph objects
 * like buffers, images, compute passes, render passes, etc...
 *
 * Use NZ_GRAPH_GET_RES_ID for resources and NZ_GRAPH_GET_STG_ID for 
 * computation stages. */
nz_graph_hdl_t nz_graph_get_res_id(struct nz_graph *);
nz_graph_hdl_t nz_graph_get_stg_id(struct nz_graph *);


/* Create a computation graph. This allows for recording and synchronization 
 * of rendering and compute commands. This also allows for registering and
 * tracking of GPU resource. All buffers and images will get registered
 * through this object. This is perhaps the most important object in the
 * entire NEZHA suite. The main graph will be passed to the backend for it
 * to fill out with recording commands and the frontend will take that same
 * graph and finish the frame for presenting to the surface. Typically,
 * only one NZ_GRAPH will be created per application. */
struct nz_graph *nz_graph_init(struct nz_gpu *);


/* Start a computation graph. Translates to a commands will get recorded 
 * into a job (which will hold a reference to a graph). */
void nz_graph_begin_job(struct nz_graph *);


/* Command buffer generator is used to have finer grain control over how
 * command buffers get used to record commands in a graph job. For instance,
 * if the job is used for the recording of a frame which gets sent to the
 * swapchain, you might want to create a command buffer generator which
 * switches between the N command buffers you have with N being the amount
 * of frames in flight the application supports. */
struct nz_cmdbuf_generator
{
  /* Data that will get passed to the generate procedure. */
  void *user_data;

  /* Gets called in NZ_GRAPH_END_END function to get the command buffer
   * that commands will get recorded in.*/
  VkCommandBuffer (*generate_proc)(void *user_data);
};


/* Once all commands have been submitted to the NZ_JOB, Creates the actual
 * VkCommandBuffer which can then be submitted with inter-VkCommandBuffer
 * synchronization. */
struct nz_job *nz_graph_end_job(struct nz_cmdbuf_generator *);


/* Submits a graph job to be computed on the GPU. Takes in a list of
 * dependency jobs (uses synchronization using VkSemaphore). */
void nz_graph_submit(struct nz_job **dependencies, struct nz_job *job);


struct nz_gpu_buffer *nz_graph_register_buffer(struct nz_graph *, nz_graph_hdl_t);

/* Adding stages. */
struct nz_pass_graphics *nz_graph_add_pass_graphics(struct nz_graph *, nz_graph_hdl_t);
struct nz_pass_compute *nz_graph_add_pass_compute(struct nz_graph *, nz_graph_hdl_t);
void nz_graph_add_buffer_update(struct nz_graph *, int id, void *, 
                                size_t off, size_t size);
void nz_graph_add_image_blit(struct nz_graph *, nz_graph_hdl_t dst, 
                             nz_graph_hdl_t src);

#endif
