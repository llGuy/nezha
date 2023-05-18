#pragma once

#include <nezha/types.hpp>

namespace nz
{

// Probably the stupidest bump allocator in the world
void init_bump_allocator(u32 max_size);
void *bump_alloc(u32 size);
void bump_clear();

template <typename T>
T *bump_mem_alloc(u32 count = 1) 
{
  return (T *)bump_alloc(sizeof(T) * count);
}

}
