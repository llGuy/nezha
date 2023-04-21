#include <stdlib.h>
#include <nezha/int.h>
#include <nezha/gpu.h>
#include <nezha/load_backend.h>

#define BACKEND_PATH "/Users/lucrosenzweig/Development/nezha/build/backends/libsdf.dylib"

int main(int argc, char **argv)
{
  nz_greet();

  struct nz_backend backend = nz_load_backend(BACKEND_PATH);
  backend.initialize_proc(NULL);

  return 0;
}
