#ifndef _NEZHA_BPOOL_H_
#define _NEZHA_BPOOL_H_

#include "int.h"
#include <stdlib.h>

enum nz_block_pool_flag_bits
{
  /* When growing memory, use same base address. Could be needed
   * if the items in the block poolator have pointers to other
   * elements in the poolated memory. By default, randomize
   * repoolations. If not using this, make sure to use the offsets
   * API.*/
  NZ_BLOCK_POOL_STATIC_REALLOC = (1<<0),

  /* When growing memory, double the capacity. By default, just
   * add a single page. */
  NZ_BLOCK_POOL_DOUBLE_REALLOC = (1<<1),
};

/* Opaque block pool struct. Allocates only fixed size blocks of size
 * BLOCK_SIZE (provided in block_pool_init). */
struct nz_block_pool;

/* Alignment must be power of 2. */
struct nz_block_pool *nz_block_pool_init(size_t start_size, size_t block_size, 
                                         size_t alignment, int flags);
void nz_block_pool_destroy(struct nz_block_pool *pool);
void *nz_block_pool_alloc(struct nz_block_pool *pool);
void nz_block_pool_free(struct nz_block_pool *pool, byte *ptr);

#endif
