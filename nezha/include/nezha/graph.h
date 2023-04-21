#ifndef _NZ_GRAPH_H_
#define _NZ_GRAPH_H_

#include <vulkan/vulkan.h>


/* Opaque handle to a graph resource. This could be to a buffer or image,
 * any kind of compute/render stage.
 *
 * Application will hold on to these in order to reference these objects in
 * command recording phase. */
typedef int nz_graph_hdl_t;


/* These functions create new IDs to be used to bind to graph objects
 * like buffers, images, compute passes, render passes, etc...
 *
 * Use NZ_GRAPH_GET_RES_ID for resources and NZ_GRAPH_GET_STG_ID for 
 * computation stages. */
nz_graph_hdl_t nz_graph_get_res_id();
nz_graph_hdl_t nz_graph_get_stg_id();


/* Create a computation graph. This allows for recording and synchronization 
 * of rendering and compute commands. This also allows for registering and
 * tracking of GPU resource. All buffers and images will get registered
 * through this object. This is perhaps the most important object in the
 * entire NEZHA suite. The main graph will be passed to the backend for it
 * to fill out with recording commands and the frontend will take that same
 * graph and finish the frame for presenting to the surface. Typically,
 * only one NZ_GRAPH will be created per application. */
struct nz_graph *nz_graph_init();


/* Start a computation graph. Translates to a Commands will get recorded 
 * into a node (which will hold a reference to a graph). */
struct nz_node *nz_graph_begin_node(struct nz_graph *);


/* Once all commands have been submitted to the NZ_NODE, Creates the actual
 * VkCommandBuffer which can then be submitted with inter-VkCommandBuffer
 * synchronization. */
void nz_graph_end_node(struct nz_node *, struct nz_cmdbuf_generator *);


/* Submits a graph node to be computed on the GPU. Takes in a list of
 * dependency nodes (uses synchronization using VkSemaphore). */
void nz_graph_submit(struct nz_node **dependencies, struct nz_node *node);


struct nz_gpu_buffer *nz_graph_register_buffer(struct nz_graph *, nz_graph_hdl_t);

/* Adding stages. */
struct nz_render_pass *nz_graph_add_render_pass(struct nz_graph *, nz_graph_hdl_t);
struct nz_compute_pass *nz_graph_add_compute_pass(struct nz_graph *, nz_graph_hdl_t);
void nz_graph_add_buffer_update(struct nz_graph *, int id, void *, 
                                size_t off, size_t size);
void nz_graph_add_image_blit(struct nz_graph *, nz_graph_hdl_t dst, 
                             nz_graph_hdl_t src);

#endif
