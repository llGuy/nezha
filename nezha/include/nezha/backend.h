#ifndef _NZ_BACKEND_H_
#define _NZ_BACKEND_H_

#include <nezha/int.h>

/* Structure passes all state that is needed to create a Nezha backend. */
struct nz_backend_ctx_init
{
  /* Render graph which maintains state for the renderer and pipelines. */
  struct graph *graph;

  /* Pointer to specific kind of frontend extra data. */
  void *frontend_ptr;
};

/* Structure passes all state that is needed to run a Nezha frame. */
struct nz_backend_frame
{
  struct nz_graph *graph;
};

/* Used for initializing game state, rendering pipelines for the game's
 * renderer, loading assets, computing whatever is needed before any
 * calls to NZ_BACKEND_RUN_FRAME. */
extern b8 nz_backend_initialize(struct nz_backend_ctx_init *);

/* Deploy simulation computations, update game state and produce
 * a rendered output which needs to get rendered at the graph resource
 * passed through the NZ_DLL_RENDER_CTX_INIT structure. */
extern void nz_backend_run_frame(struct nz_backend_frame *);

/* Used for saving state, deinitialization of the game, etc... */
extern void nz_backend_shutdown(void);

#endif
