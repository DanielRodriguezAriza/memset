#ifndef DRA_COMPILER_BARRIER_H
#define DRA_COMPILER_BARRIER_H

/* TODO: Clean up the code to make the customization interface a little bit less disgusting to my taste... */

/*
	General purpose compiler memory barrier implementation.
	Not to be confused with a real hardware fence.
	This is a no-op, because it's just to signal to the compiler that it cannot assume certain properties about the memory access to a certain memory location, preventing certain optimizations from taking place. Useful to avoid DSE and DCE.
*/

#ifdef DRA_COMPILER_BARRIER_FORCE_VFP

	#define DRA_COMPILER_BARRIER_GENERIC

#else

	#if defined(__clang__)
		#define DRA_COMPILER_BARRIER_CLANG
	#elif defined(__GNUC__)
		#define DRA_COMPILER_BARRIER_GNUC
	#elif (defined(_MSC_VER) && (defined(_WI32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)))
		#define DRA_COMPILER_BARRIER_MSVC
	#else
		#define DRA_COMPILER_BARRIER_GENERIC
	#endif

#endif

/*
	Usage of _ReadWriteBarrier() is no longer ideal because of its deprecation. Some users may prefer to not pollute their code with stuff like this.
	This implementation became even lighter weight on Windows by virtue of being able to skip both including intrin.h and windows.h or any other additional system headers or intrinsics headers.
	It can still be optionally included if the user considers it to be an useful addition in the event that future MSVC versions do not find it to be enough for a volatile dummy byte read to be performed to ensure a compiler memory barrier.
*/
#if defined(DRA_COMPILER_BARRIER_USE_WIN32_INTRINSICS) && defined(DRA_COMPILER_BARRIER_MSVC)
#include <intrin.h>
#endif


/*
	Memory barrier function.
	Prevents the compiler from optimicing away certain calls.
	Takes a pointer to the memory location that needs to be protected from optimizations.
	Useful to avoid DSE (Dead Store Elimination) and DCE (Dead Code Elimination).
*/
void dra_barrier(void *p);


#if defined(DRA_COMPILER_BARRIER_GENERIC)
void dra_barrier_dummy(void*){}
typedef void(*dra_barrier_func_t)(void*);
volatile static dra_barrier_func_t dra_barrier_dummy_ptr = &dra_barrier_dummy;
#endif

void dra_barrier(void *p)
{
#if defined(DRA_COMPILER_BARRIER_CLANG)
	unsigned char *ptr = (unsigned char *)p;
	__asm__ __volatile__(""::"m"(*ptr):"memory");
	/*
		CLANG IMPLEMENTATION:
		Compiler memory barrier implementation for clang.
		Uses an asm volatile statement with a compiler memory barrier, specifying potential volatile memory access to a specific memory location, provided by pointer.
		The compiler memory barrier for clang requires usage of 'm' instead of 'r' because going through the registers actually still allows clang to optimize the memory operation away, so we must ensure that the access requires a memory storage.
		This is because clang's optimizer is just too damn smart for it's own good!
	*/
#elif defined(DRA_COMPILER_BARRIER_GNUC)
	__asm__ __volatile__(""::"r"(p):"memory");
	/*
		GCC IMPLEMENTATION:
		Compiler memory barrier implementation for GCC.
		Simply tells the compiler that the specified memory location may be modified.
		Could also use one of the following alternatives:
			1) asm volatile ("":::"memory")
			2) asm volatile (""::"r"(p):"memory")
			3) asm volatile (""::"m"(p):"memory"), same as the clang implementation in this file
			4) asm volatile (""::"g"(p):"memory")
		All of these alternatives would work almost the same on most cases in GCC, but the current implementation is the best one from my current experience.
		Feel free to change it for whatever purposes you require.
	*/
#elif defined(DRA_COMPILER_BARRIER_MSVC)
	(void)*(volatile unsigned char*)p;
	#if defined(DRA_COMPILER_BARRIER_USE_WIN32_INTRINSICS)
	_ReadWriteBarrier();
	#endif
	/*
		MSVC IMPLEMENTATION:
		Uses a stupid trick where the compiler can be tricked into believing that the pointed to memory must be preserved because a read access was performed for a single byte, potentially signalling that side effects could take place after the call that we want to preserve, making it so that the compiler cannot optimize it away directly.
		It works on MSVC and GCC at the small cost of a single useless byte read from memory. Clang is the only compiler that I know of smart enough to actually only preserve the effect of the prior call over the single byte that we'd read as a workaround to still allow eliminating the call that would be optimized by DSE or DCE.
		Having looked online, I could only find Google Benchmark to be using a similar trick for MSVC and unknown compiler support for their DoNotOptimize() function implementation.
		The _ReadWriteBarrier() intrinsic is sadly deprecated. It still compiles, and under certain MSVC configurations, it emits a warning. It also has the issue that on modern MSVC versions, it no longer actually performs a compiler memory barrier operation during compilation, so the generated assembly ignores this and still performs DSE.
		Having both the byte read and the _ReadWriteBarrier() call should be enough to coerce most versions of MSVC into doing what we want...
	*/
#elif defined(DRA_COMPILER_BARRIER_GENERIC)
	dra_barrier_dummy_ptr(p);
	/*
		GENERIC IMPLEMENTATION:
		This is the prefered implementation for unknown compilers.
		It does not make use of any compiler specific barriers, so it works purely by using fully standards compliant C and C++ behaviour.
		It makes use of a fully standards compliant volatile function pointer call, which is a bit slower than the regular function call, but it cannot be optimized away, which ensures that the compiler cannot assume that no side effects take place over the entire memory pointed to by the input pointer.
		This option works on all existing compilers that I know of. The other implementations exist for the sake of avoiding this unnecessary overhead that comes from obtaining a compiler memory barrier by tricking the compiler through a volatile function pointer call.
	*/
#else
	#error "No proper implementation of DRA_COMPILER_BARRIER could be found for the current system. This should never happen. Report this error to https://github.com/DanielRodriguezAriza/memset"
#endif
}

#endif /* DRA_COMPILER_BARRIER_H */
