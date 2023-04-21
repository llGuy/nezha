#ifndef _NZ_LOAD_BACKEND_H_
#define _NZ_LOAD_BACKEND_H_

#include <nezha/int.h>
#include <nezha/backend.h>

/* Entry point functions to the backend: */
typedef b8 (*nz_backend_initialize_proc)(struct nz_backend_ctx_init *);
typedef void (*nz_backend_run_frame_proc)(struct nz_backend_frame *);
typedef void (*nz_backend_shutdown_proc)(void);

/* Structure used to describe a backend. If in mode where we use dynamic
 * linking, the handle will be an appropriate handle to the DLL based
 * on the platform. */
struct nz_backend
{
  /* Shared library handle. */
  void *handle;

  nz_backend_initialize_proc initialize_proc;
  nz_backend_run_frame_proc run_frame_proc;
  nz_backend_shutdown_proc shutdown_proc;
};

struct nz_backend nz_load_backend(const char *name);
void nz_close_backend(struct nz_backend *);

#endif
