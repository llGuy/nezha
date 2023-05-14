#ifndef _NEZHA_HEAP_VECTOR_H_
#define _NEZHA_HEAP_VECTOR_H_

#include <nezha/int.h>

struct nz_heap_vector;

struct nz_heap_vector *nz_heap_vector_init(size_t start_size, 
                                           size_t block_size);
void nz_heap_vector_destroy(struct nz_heap_vector *);
void *nz_heap_vector_push(struct nz_heap_vector *);
/* Just guarantee that there is this much space in the vector. */
void nz_heap_vector_guarantee(struct nz_heap_vector *, int);
void *nz_heap_vector_get(struct nz_heap_vector *, int);
size_t nz_heap_vector_size(struct nz_heap_vector *);
void nz_heap_vector_clear(struct nz_heap_vector *);

#endif
