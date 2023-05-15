#include "bump_alloc.h"

static void *bump_start_;
static void *bump_current_;
static uint32_t bump_max_size_;

void init_bump_allocator(u32 max_size) 
{
  bump_max_size_ = max_size;
  bump_start_ = bump_current_ = malloc(max_size);
}

void *bump_alloc(u32 size) 
{
  void *p = bump_current_;
  assert(bump_current_ < (uint8_t *)bump_start_ + bump_max_size_);
  bump_current_ = (void *)((uint8_t *)(bump_current_) + size);
  return p;
}

void bump_clear() 
{
  bump_current_ = bump_start_;
}
