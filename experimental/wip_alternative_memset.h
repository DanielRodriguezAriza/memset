#ifndef DRA_MEMSET_H
#define DRA_MEMSET_H

#ifndef DRA_MEMSET_LINKAGE
#define DRA_MEMSET_LINKAGE static inline
#endif

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)
#define DRA_MEMSET_PLATFORM_WIN32
#elif defined(__GNUC__) || defined(__clang__)
#define DRA_MEMSET_PLATFORM_GNU
#endif

#define DRA_MEMSET_METHOD_FREESTANDING      0
#define DRA_MEMSET_METHOD_CSTDLIB           1

#define DRA_MEMSET_VOLATILE_METHOD_DATA_PTR 0
#define DRA_MEMSET_VOLATILE_METHOD_FUNC_PTR 1
#define DRA_MEMSET_VOLATILE_METHOD_COMPILER 2
#define DRA_MEMSET_VOLATILE_METHOD_CSTDLIB  3

#ifndef DRA_MEMSET_METHOD
#define DRA_MEMSET_METHOD DRA_MEMSET_METHOD_CSTDLIB
#endif

#ifndef DRA_MEMSET_VOLATILE_METHOD
#if defined(DRA_MEMSET_PLATFORM_GNU)
#define DRA_MEMSET_VOLATILE_METHOD DRA_MEMSET_VOLATILE_METHOD_COMPILER
#else
#define DRA_MEMSET_VOLATILE_METHOD DRA_MEMSET_VOLATILE_METHOD_FUNC_PTR
#endif

#include <stddef.h>
#if (DRA_MEMSET_METHOD == DRA_MEMSET_METHOD_CSTDLIB) || (DRA_MEMSET_VOLATILE_METHOD == DRA_MEMSET_VOLATILE_METHOD_CSTDLIB)
	#include <string.h>
#endif
#if (DRA_MEMSET_VOLATILE_METHOD == DRA_MEMSET_VOLATILE_METHOD_COMPILER)
	#if defined(DRA_MEMSET_PLATFORM_WIN32)
		#define WIN32_LEAN_AND_MEAN
		#include <windows.h>
	#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

DRA_MEMSET_LINKAGE void MemorySet(void*,int,size_t);
DRA_MEMSET_LINKAGE void MemorySetVolatile(void*,int,size_t);
DRA_MEMSET_LINKAGE void MemoryZero(void*,size_t);
DRA_MEMSET_LINKAGE void MemoryZeroVolatile(void*,size_t);

DRA_MEMSET_LINKAGE void MemorySet(void *p, int c, size_t n)
{
#if DRA_MEMSET_METHOD == DRA_MEMSET_METHOD_FREESTANDING
	unsigned char *ptr = (unsigned char *)(p);
	while(n--) *ptr++ = c;
#elif DRA_MEMSET_METHOD == DRA_MEMSET_METHOD_CSTDLIB
	memset(p, c, n);
#else
	#error "Unknown DRA_MEMSET_METHOD"
#endif
}

DRA_MEMSET_LINKAGE void MemorySetVolatile(void *p, int c, size_t n)
{
#if DRA_MEMSET_VOLATILE_METHOD == DRA_MEMSET_VOLATILE_METHOD_DATA_PTR
	volatile unsigned char *ptr = (volatile unsigned char *)p;
	while(n--) *ptr++ = c;
#elif DRA_MEMSET_VOLATILE_METHOD == DRA_MEMSET_VOLATILE_METHOD_CSTDLIB
	memset_explicit(p, c, n);
#elif DRA_MEMSET_VOLATILE_METHOD == DRA_MEMSET_VOLATILE_METHOD_FUNC_PTR
	dra_memset_function_pointer(p, c, n);
#elif DRA_MEMSET_VOLATILE_METHOD == DRA_MEMSET_VOLATILE_METHOD_COMPILER
	#if defined(DRA_MEMSET_PLATFORM_GNU)
		memset(p, c, n);
		__asm__ __volatile__("" ::"r"(p): "memory");
	#elif defined(DRA_MEMSET_PLATFORM_WIN32)
		SecureZeroMemory (p,  // SHIT! Here we have the problem that this is not a memzero function! we're on the memory set volatile func, which is meant to be generic, and there is no equivalent in windows as far as I know, so for this case, we should always use a pointer, and THEN we should allow using SecureZeroMemory for the MemoryZeroVolatile call! But tbh, at this point, that's just not worth it imo! better off using other barrier methods and that's it. I'm keeping this public for now just in case the rest of the changes to the header are useful to someone else, but this is more of a drawing board than actual code to use in the real world tbh. The simpler approach in the main file is better. Either that, or the compiler memory barriers approach from the secure_call.h file.
	#else
		#error "Unknown platform for DRA_MEMSET_VOLATILE_METHOD_COMPILER"
	#endif
#else
	#error "Unknown DRA_MEMSET_VOLATILE_METHOD"
#endif
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
