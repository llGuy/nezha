#ifndef _NEZHA_BPOOL_H_
#define _NEZHA_BPOOL_H_

#include <nezha/int.h>

/* Opaque block pool struct. Allocates only fixed size blocks of size
 * BLOCK_SIZE (provided in block_pool_init). */
struct nz_block_pool;

/* TODO: Alignment must be power of 2. */
struct nz_block_pool *nz_block_pool_init(size_t start_size, size_t block_size);
void nz_block_pool_destroy(struct nz_block_pool *pool);
void *nz_block_pool_alloc(struct nz_block_pool *pool);
void nz_block_pool_free(struct nz_block_pool *pool, byte *ptr);
void nz_block_pool_reset(struct nz_block_pool *pool);

#endif
