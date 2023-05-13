#include <stdlib.h>
#include <assert.h>
#include <nezha/def.h>
#include <nezha/util.h>
#include <nezha/heap_vector.h>

#define HEAP_VECTOR_DEF_START_SIZE (1<<12) // 4KB

struct nz_heap_vector
{
  size_t cap;
  size_t size;
  size_t block_size;
};

b8 *heap_vector_start(struct nz_heap_vector *v)
{
  return (b8 *)v + sizeof(struct nz_heap_vector);
}

struct nz_heap_vector *nz_heap_vector_init(size_t start_size, 
                                           size_t block_size)
{
  if (start_size == NZ_DEFVAL)
    start_size = HEAP_VECTOR_DEF_START_SIZE;

  struct nz_heap_vector *v = (struct nz_heap_vector *)
    malloc(sizeof(struct nz_heap_vector) + start_size);

  v->cap = start_size;
  v->size = 0;
  v->block_size = block_size;

  nz_zero_memory(heap_vector_start(v), v->block_size * v->cap);

  return v;
}

void nz_heap_vector_destroy(struct nz_heap_vector *v)
{
  free(v);
}

int nz_heap_vector_push(struct nz_heap_vector *v)
{
  if (v->size >= v->cap)
  {
    v = realloc(v, sizeof(struct nz_heap_vector) + (v->cap *= 2));
    nz_zero_memory(heap_vector_start(v) + v->size * v->block_size, 
                   v->block_size * v->cap);
  }

  return v->size++;
}

void nz_heap_vector_guarantee(struct nz_heap_vector *v, int i)
{
  size_t old_size = v->size;
  v->size = NZ_MAX(i+1, v->size);

  while (v->size >= v->cap)
    v->cap *= 2;

  v = realloc(v, sizeof(struct nz_heap_vector) + v->cap);
  nz_zero_memory(heap_vector_start(v) + old_size * v->block_size, 
                 v->block_size * v->cap);
}

void *nz_heap_vector_get(struct nz_heap_vector *v, int i)
{
  assert(v->size > i);

  b8 *start = (b8 *)v + sizeof(struct nz_heap_vector);
  return start + i * v->block_size;
}

size_t nz_heap_size(struct nz_heap_vector *v)
{
  return v->size;
}
