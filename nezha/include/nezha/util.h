#ifndef _NZ_UTIL_H_
#define _NZ_UTIL_H_

#if !defined(__APPLE__)
#include <malloc.h>
#endif

#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Causes a panic message to be printed and application exits. */
void nz_panic_and_exit(const char *msg);

/* Returns how many bits are set to 1 in a uint32. */
uint32_t nz_pop_count(uint32_t bits);

/* Zeros out a buffer in memory. */
void nz_zero_memory(void *ptr, uint32_t size);

/* Sets a breakpoint. */
void nz_set_breakpoint(void);

/* Dynamic stack allocation. */
#define NZ_STACK_ALLOC(type, n) (type *)alloca(sizeof(type) * (n))

#define NZ_MIN(a, b) (((a)<(b))?(a):(b))
#define NZ_MAX(a, b) (((a)>(b))?(a):(b))
#define NZ_CLAMP(value, a, b) NZ_MIN(NZ_MAX(value, a), b)

#endif
