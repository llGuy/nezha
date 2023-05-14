#ifndef _NZ_COMPUTE_H_
#define _NZ_COMPUTE_H_

#include <nezha/int.h>
#include <nezha/graph.h>

struct nz_pass_compute;

/* Set the shader source file that the compute pass will execute. */
void nz_compute_set_src(struct nz_pass_compute *, const char *);

/* Send some uniform data (small - at most 256 bytes typically although
 * this is hardware dependent). */
void nz_compute_send_data(struct nz_pass_compute *, const void *, size_t);

/* Adding bindings to a compute pass: (TODO: images) */
void nz_compute_add_storage_buffer(struct nz_pass_compute *, nz_graph_hdl_t);
void nz_compute_add_uniform_buffer(struct nz_pass_compute *, nz_graph_hdl_t);

/* 2 different ways to configure dispatch parameters.
 * The first expects how many work groups in each direction we need
 * The second expects how large the waves are. */
void nz_compute_dispatch(struct nz_pass_compute *, int x, int y, int z);
void nz_compute_dispatch_waves(struct nz_pass_compute *, int x, int y, int z);

#endif
