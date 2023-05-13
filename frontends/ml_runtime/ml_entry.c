#include <stdio.h>
#include <stdlib.h>
#include <nezha/int.h>
#include <nezha/gpu.h>
#include <nezha/graph.h>
#include <nezha/surface.h>
#include <nezha/load_backend.h>

int main(int argc, char **argv)
{
  struct nz_gpu_cfg gpu_cfg = {
    .surface = NULL
  };

  struct nz_gpu *gpu = nz_gpu_create(&gpu_cfg);

  return 0;
}
