#ifndef _NZ_GPU_H_
#define _NZ_GPU_H_

#include <nezha/int.h>

struct nz_gpu_cfg
{
  struct nz_surface *surface;
};

struct nz_gpu *nz_gpu_create(struct nz_gpu_cfg *);

#endif
