#ifndef DRA_MEMSET_H
#define DRA_MEMSET_H

#ifndef DRA_MEMSET_LINKAGE
#define DRA_MEMSET_LINKAGE static inline
#endif

#include <stddef.h>
#ifndef DRA_MEMSET_FREESTANDING
#include <string.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

DRA_MEMSET_LINKAGE void MemorySet(void*,int,size_t);
DRA_MEMSET_LINKAGE void MemorySetVolatile(void*,int,size_t);
DRA_MEMSET_LINKAGE void MemoryZero(void*,size_t);
DRA_MEMSET_LINKAGE void MemoryZeroVolatile(void*,size_t);

#ifdef DRA_MEMSET_FREESTANDING

DRA_MEMSET_LINKAGE void MemorySet(void *p, int c, size_t n)
{
	unsigned char *ptr = (unsigned char *)(p);
	while(n--) *ptr++ = c;
}

DRA_MEMSET_LINKAGE void MemorySetVolatile(void *p, int c, size_t n)
{
	volatile unsigned char *ptr = (volatile unsigned char *)p;
	while(n--) *ptr++ = c;
}

#else

typedef void *(*dra_memset_t)(void*,int,size_t);
volatile static dra_memset_t dra_memset_function_pointer = memset;

DRA_MEMSET_LINKAGE void MemorySet(void *p, int c, size_t n)
{
	memset(p, c, n);
}

static inline void MemorySetVolatile(void *p, int c, size_t n)
{
	dra_memset_function_pointer(p, c, n);
}

#endif

DRA_MEMSET_LINKAGE void MemoryZero(void *p, size_t n) { MemorySet(p, 0, n); }
DRA_MEMSET_LINKAGE void MemoryZeroVolatile(void *p, size_t n) { MemorySetVolatile(p, 0, n); }

#ifdef __cplusplus
}
#endif

#endif
