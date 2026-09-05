// COMPILER EXPLORER URL: https://godbolt.org/z/16jjbaKKe
// Fully working memset_explicit() custom implementation. Does not even need separate implementation file compiled and linked as a separate translation unit to fake the compiler optimization barrier. Which is good, because that can be very easily accidentally defeated just by using LTO, but this current implementation cannot be defeated so easily AFAIK.
#include <string.h>
#include <stddef.h>
#include <stdio.h>

#ifdef _WIN32
// No longer required because of the _ReadWriteBarrier() deprecation, so this implementation actually became even lighter weight even on Windows by virtue of being able to skip both including intrin.h and windows.h or any other additional system headers or intrinsics headers or whatever.
// #include <intrin.h>
#endif

void *memset_explicit(void *p, int c, size_t n)
{
#if defined(__clang__)
    void *x = memset(p, c, n);
    unsigned char *ptr = (unsigned char *)p;
    __asm__ __volatile__(""::"m"(*ptr):"memory"); // The compiler fence / barrier for clang uses 'm' instead of 'r' because going through the registers actually allows clang to optimize it away... clang is just too smart for it's own damn good! (not to be confused with a real hardware fence, this is a no-op, obviously, because it's just to signal to the compiler that it cannot assume certain properties about the memory access, preventing certain optimizations just as we want!)
    return x;
#elif defined(__GNUC__)
    void *x = memset(p, c, n);
    __asm__ __volatile__(""::"r"(p):"memory"); // The compiler barrier for GCC, could also just use ("":::"memory") and it'd work almost the same on most cases in GCC, but this is better imo.
    return x;
#elif defined(_WIN32)
    // Still faster than SecureZeroMemory(), LOL! And to top it all off, it does not even require me to include a ton of bloat from WIN32 headers, so even a bigger win imo, specially since this allows architrary byte value setting rather than just zeroing.
    if(n <= 0) return p; // The char access could cause a crash if we try to access null or a non valid piece of memory. Null is not my problem, don't pass null to this function, just like you cannot pass null to memset... but the size IS my problem.
    void *x = memset(p, c, n);
    (void)*(volatile char*)p; // Stupid trick that I accidentally found. Did not expect it to work, but it does on MSVC and GCC at the cost of an useless byte read. Clang is the only compiler smart enough to actually only memset a single byte as a workaround, still eliminating the memset call, LMAO. I looked for this online, and I could only find Google Benchmark also using a similar trick for MSVC and unknown compilers.
    // _ReadWriteBarrier(); // Deprecated, sadly, it compiles, emits a warning, but it no longer actually performs a compiler memory barrier operation during compilation, so the generated assembly ignores this and still performs DSE.
    return x;
#else
    // The best and prefered choice imo for unknown compilers.
    typedef void*(*memset_t)(void*,int,size_t);
    volatile static memset_t memset_ptr = &memset;
    return memset_ptr(p,c,n); // No compiler specific barriers, just use a fully standards compliant volatile function pointer call, which is a bit slower than the regular function call, but it cannot be optimized away, so yippie!
#endif
} // Fuck me sideways, this works on all major compilers now with the least possible overhead, afaik at least :)


int main()
{
    int arr1[1024];
    int arr2[1024];
    memset_explicit(arr1, 0, sizeof arr1); // curiously enough, the memory barrier is so powerful in GCC, that if we turn this into a regular memset, this call remains. On the other compilers, it is optimized away.
    memset_explicit(arr2, 0, sizeof arr2); // Meanwhile, if we were to do it the other way around and turn this other call into a raw memset, only the upper call remains on all compilers, which means that on GCC, the compiler memory barriers are strong enough to affect all pointers on calls that exist above the current call WHEN INLINED.
    return 0;
}
