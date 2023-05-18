#pragma once

#include <utility>
#include <string.h>
#include <nezha/types.hpp>

namespace nz
{

inline constexpr u32 kilobytes(u32 kb) { return(kb * 1024); }
inline constexpr u32 megabytes(u32 mb) { return(kilobytes(mb * 1024)); }

// For now, these all invoke new/delete
template <typename T, typename ...Args>
T *mem_alloc(Args &&...args) 
{
  return new T(std::forward<Args>(args)...);
}

template <typename T, typename ...Args>
T *mem_allocv(u32 count, Args &&...args) 
{
  return new T[count] {std::forward<Args>(args)...};
}

template <typename T>
void mem_free(T *ptr) 
{
  delete ptr;
}

template <typename T>
void mem_freev(T *ptr) 
{
  delete[] ptr;
}

inline void zero_memory(void *ptr, u32 size) 
{
  memset(ptr, 0, size);
}

template <typename T>
inline void zero_memory(T *ptr) 
{
  memset(ptr, 0, sizeof(T));
}

template <typename T>
inline void zero_memory(u32 count, T *ptr) 
{
  memset(ptr, 0, sizeof(T) * count);
}

}

#define stack_alloc(type, count) (type *)alloca(sizeof(type) * (count))
