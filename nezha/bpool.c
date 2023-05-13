#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <nezha/def.h>
#include <nezha/bpool.h>

#define BPOOL_DEF_START_SIZE (1<<12) // 4 KB

struct free_node
{
  struct free_node *next;
};

struct nz_block_pool
{
  /* First free block in the pool. */
  struct free_node *first_free;

  /* The next pool in the chain of pools. */
  struct nz_block_pool *next_pool;

  /* Size of a block. */
  size_t block_size;

  /* Size of the pool. */
  size_t pool_size;

  /* Reached offset from the allocations. */
  size_t reached;
};

static struct nz_block_pool *block_pool_get_last(struct nz_block_pool *pool);

struct nz_block_pool *nz_block_pool_init(size_t start_size, size_t block_size)
{
  if (start_size == NZ_DEFVAL)
    start_size = BPOOL_DEF_START_SIZE;

  assert(start_size > sizeof(struct nz_block_pool) + block_size);

  /* Allocate the initial pool. */
  struct nz_block_pool *pool = (struct nz_block_pool *)malloc(start_size);

  pool->first_free = NULL;
  pool->next_pool = NULL;
  pool->block_size = block_size;
  pool->pool_size = start_size;

  /* Start allocating after the metadata of the pool. */
  pool->reached = sizeof(struct nz_block_pool);

  return pool;
}

void nz_block_pool_destroy(struct nz_block_pool *pool)
{
  while (pool)
  {
    void *ptr = pool->next_pool;
    free(pool);

    pool = (struct nz_block_pool *)ptr;
  }
}

void *nz_block_pool_alloc(struct nz_block_pool *pool)
{
  /* If there is a free node, we just use that node. */
  if (pool->first_free)
  {
    void *ptr = pool->first_free;
    pool->first_free = pool->first_free->next;

    return ptr;
  }

  pool = block_pool_get_last(pool);

  /* Otherwise, we increment the reached offset. First, make sure
   * we can actually increment reached.*/
  if (pool->reached + pool->block_size > pool->pool_size)
  {
    if (!pool->next_pool)
      pool->next_pool = nz_block_pool_init(pool->pool_size, pool->block_size);

    pool = pool->next_pool;
  }

  byte *start = (byte *)pool + pool->reached;
  pool->reached += pool->block_size;

  return (void *)start;
}

void nz_block_pool_free(struct nz_block_pool *pool, byte *ptr)
{
  /* For debugging purposes. */
  memset(ptr, 0xDD, pool->block_size);

  struct free_node *new_free_node = (struct free_node *)ptr;
  new_free_node->next = pool->first_free;
  pool->first_free = new_free_node;
}

static struct nz_block_pool *block_pool_get_last(struct nz_block_pool *pool)
{
  while (pool->next_pool)
    pool = pool->next_pool;

  return pool;
}

void nz_block_pool_reset(struct nz_block_pool *pool)
{
  pool->reached = sizeof(struct nz_block_pool);
  pool->first_free = NULL;
}
