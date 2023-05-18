#pragma once

#include <nezha/types.hpp>
#include <nezha/heap_array.hpp>

namespace nz
{

inline u32 pop_count(u32 bits) 
{
#ifndef __GNUC__
  return __popcnt(bits);
#else
  return __builtin_popcount(bits);
#endif
}

class bit_vector 
{
public:
  bit_vector() = default;

  void set_size(u32 size) 
  {
    bits_ = heap_array<u32>((size + 31) / 32);
    bits_.zero();
  }

  void set_bit(u32 idx) 
  {
    u32 &group = bits_[idx / 32];
    u32 bit_idx = idx % 32;
    group |= (1 << bit_idx);
  }

  u32 get_bit(u32 idx) 
  {
    u32 &group = bits_[idx / 32];
    u32 bit_idx = idx % 32;
    return (group >> bit_idx) & 0b1;
  }

  u32 get_set_count() 
  {
    u32 count = 0;
    for (int i = 0; i < bits_.size(); ++i) 
      count += pop_count(bits_[i]);

    return count;
  }

private:
  // Bits are grouped into uint32_t
  heap_array<u32> bits_;
};

}
