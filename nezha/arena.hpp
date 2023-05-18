#pragma once

#include "types.hpp"
#include "memory.hpp"

template <typename T>
class arena_pool 
{
public:
  arena_pool() = default;

  arena_pool(u32 size) 
  : pool_size_(size) {
    pool_size_ = size;
    pool_ = mem_allocv<T>(size);
    memset(pool_, 0, sizeof(T) * size);
    first_free_ = nullptr;
    reached_ = 0;
  }

  // This will also zero initialize the data that we are allocating
  T *alloc() 
  {
    if (first_free_) 
    {
      T *new_free_node = first_free_;
      memset(new_free_node, 0, sizeof(T));

      first_free_ = next(first_free_);
      return new_free_node;
    }
    else 
    {
      assert(reached_ < pool_size_);
      memset(&pool_[reached_], 0, sizeof(T));
      return &pool_[reached_++];
    }
  }

  void free(T *node) 
  {
    next(node) = first_free_;
    first_free_ = node;
  }

  void clear() 
  {
    reached_ = 0;
    first_free_ = nullptr;
  }
  
  // Access / Iteration
  u32 get_node_idx(T *node) { return node - pool_; }
  T *get_node(u32 idx) { return &pool_[idx]; }
  T *&next(T *node) { return *(T **)node; }
  u32 size() { return reached_; };

  T *data() { return pool_; };

private:
  T *pool_;
  T *first_free_;

  // Which node have we reached
  u32 reached_;
  // How many nodes are there
  u32 pool_size_;
};

