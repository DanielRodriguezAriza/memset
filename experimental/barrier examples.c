#include <string.h>
#include <stdio.h>

static inline void do_nothing(void *p){}
typedef void (*barrier_t)(void*);
volatile barrier_t barrier = &do_nothing;

static inline void memset_safe(void *p, int c, size_t n)
{
    memset(p,c,n);
    barrier(p);
}

void dummy(void*);

int main()
{
    char arr[1024];
    printf("%s\n", arr);
    // memset(arr, 0, sizeof arr);
    memset_safe(arr, 0, sizeof arr);
    // __asm__ __volatile__("":::"memory");
    // __asm__ __volatile__(""::"r"(arr):"memory");
    // char v = *(volatile char*)(arr);
    // dummy(arr);
    return 0;
}

// Compile on GCC, Clang and MSVC on godbolt.
// https://godbolt.org/z/WPr9bcGb9
