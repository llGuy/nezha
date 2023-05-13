#ifndef _NZ_SURFACE_H_
#define _NZ_SURFACE_H_

#include <nezha/int.h>

struct nz_surface_cfg
{
  int width;
  int height;
  const char *name;

  /* This should always be 0. Only set to one if you already called 
   * NZ_SURFACE_CREATE with IS_API_INITIALIZED set to 0. */
  b8 is_api_initialized;
};

/* You don't just directly create a surface. First you prepare, then
 * you pass the surface to a call to NZ_GPU_CREATE. */
struct nz_surface *nz_surface_prepare(const struct nz_surface_cfg *);

#endif
