#include <stdio.h>
#include <nezha/gpu.h>
#include <nezha/backend.h>

b8 nz_backend_initialize(struct nz_backend_ctx_init *ctx)
{
  printf("In SDF backend!\n");

  nz_greet();

  return 1;
}

void nz_backend_run_frame(struct nz_backend_frame *frame)
{
  
}

void nz_backend_shutdown(void)
{
  
}
