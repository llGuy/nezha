#pragma once

#include <vector>
#include <nezha/memory.hpp>
#include <nezha/types.hpp>
#include <initializer_list>

namespace nz
{

template <typename T>
class heap_array 
{
public:
  // Construction
  heap_array() 
  : buffer_(nullptr), count_(0) 
  {
  }

  heap_array(std::initializer_list<T> list) 
  {
    buffer_ = (T *)mem_allocv<T>(list.size());
    count_ = list.size();

    u32 i = 0;
    for (auto &item : list) 
    {
      new(&buffer_[i]) T(item);
      ++i;
    }
  }

  heap_array(const T *ptr, u32 size) 
  {
    buffer_ = (T *)mem_allocv<T>(size);
    count_ = size;

    for (int i = 0; i < size; ++i) 
    {
      buffer_[i] = ptr[i];
    }
  }

  heap_array(u32 size) 
  {
    buffer_ = (T *)mem_allocv<T>(size);
    count_ = size;
  }

  void zero() 
  {
    memset(buffer_, 0, sizeof(T) * count_);
  }

  ~heap_array() 
  {
    mem_freev(buffer_);
    count_ = 0;
  }

  heap_array<T> &operator=(const std::vector<T> &other) 
  {
    if (!buffer_)
      alloc(other.size());

    assert(other.size() == count_);

    for (int i = 0; i < count_; ++i)
      buffer_[i] = other[i];

    return *this;
  }

  heap_array<T> &operator=(const heap_array<T> &other) 
  {
    if (!buffer_)
      alloc(other.size());

    assert(other.size() == count_);

    for (int i = 0; i < count_; ++i)
      buffer_[i] = other[i];

    return *this;
  }

  heap_array<T> &operator=(std::initializer_list<T> list) 
  {
    buffer_ = (T *)mem_allocv<T>(list.size());
    count_ = list.size();

    u32 i = 0;
    for (auto &item : list) 
    {
      new(&buffer_[i]) T(item);
      ++i;
    }

    return *this;
  }

  // Helpers
  u32 size() const      { return count_; }
  T *data()             { return buffer_; }
  const T *data() const { return buffer_; }

  // Iteration
  T &operator[](u32 index)             { return buffer_[index]; }
  const T &operator[](u32 index) const { return buffer_[index]; }

private:
  void alloc(u32 size) 
  {
    count_ = size;
    buffer_ = mem_allocv<T>(count_);
  }

private:
    T *buffer_;
    u32 count_;
};

template <typename T>
struct buffer 
{
  T *data;
  u32 size;

  buffer(T *ptr, u32 size)
  : data(ptr), size(size) 
  {
  }

  buffer(const heap_array<T> &ha) 
  : data(ha.data()), size(ha.size())
  {
  }

  T &operator[](u32 index)             { return data[index]; }
  const T &operator[](u32 index) const { return data[index]; }
};

}
