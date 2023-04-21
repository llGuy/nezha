#include <dlfcn.h>
#include <assert.h>
#include <stdlib.h>
#include <nezha/load_backend.h>

struct nz_backend nz_load_backend(const char *name)
{
  struct nz_backend ret = { .handle = NULL };

  void *lib_handle = dlopen(name, RTLD_NOW);
  if (!lib_handle)
    return ret;

  ret.initialize_proc = dlsym(lib_handle, "nz_backend_initialize");
  ret.run_frame_proc = dlsym(lib_handle, "nz_backend_run_frame");
  ret.shutdown_proc = dlsym(lib_handle, "nz_backend_shutdown");

  assert(ret.initialize_proc);
  assert(ret.run_frame_proc);
  assert(ret.shutdown_proc);

  return ret;
}

void nz_close_backend(struct nz_backend *backend)
{
  dlclose(backend->handle);
}
