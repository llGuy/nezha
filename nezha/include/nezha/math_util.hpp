#pragma once

#include <glm/glm.hpp>

#include <nezha/types.hpp>

namespace nz
{

template <typename T>
inline T floor_to(T x, typename T::value_type multiple) 
{
  return multiple * glm::floor(x / multiple);
}

template <typename T>
inline T ceil_to(T x, typename T::value_type multiple) 
{
  return multiple * glm::ceil(x / multiple);
}

template <typename T>
void apply_3d(iv3 dim, T pred)
{
  iv3 off = iv3(0);
  for (off.z = 0; off.z < dim.z; ++off.z) 
    for (off.y = 0; off.y < dim.y; ++off.y) 
      for (off.x = 0; off.x < dim.x; ++off.x) 
        pred(off);
}

}
