#include <stdio.h>
#include <stdlib.h>
#include <nezha/util.h>

void nz_panic_and_exit(const char *msg)
{
  printf("panic - stopping session at \"%s\"\n", msg);
  assert(0);
}

uint32_t nz_pop_count(uint32_t bits) 
{
#ifndef __GNUC__
    return __popcnt(bits);
#else
    return __builtin_popcount(bits);
#endif
}

void nz_zero_memory(void *ptr, uint32_t size)
{
  memset(ptr, 0, size);
}

void nz_set_breakpoint(void) 
{
#if _WIN32
  __debugbreak();
#elif defined(__APPLE__)
  raise(SIGTRAP);
#else
  asm("int $3");
#endif
}
