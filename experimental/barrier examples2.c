#include <string.h>
#include <stddef.h>
typedef typeof(&memset) memset_ptr_t;
volatile static memset_ptr_t memset_ptr = &memset;
static inline void *memset_explicit_small(void *p, int c, size_t n) {
    volatile unsigned char *ptr = (volatile unsigned char *)p;
    while(n--) *ptr++ = c;
    return p;
}
static inline void *memset_explicit_ex(void *p, int c, size_t n) {
    if(n < 256)
        return memset_explicit_small(p, c, n);
    else
        return memset_ptr(p, c, n);
}
static inline void *memset_explicit(void *p, int c, size_t n) {
    void *x = memset(p, c, n);
    __asm__ __volatile__(""::"r"(p):"memory");
    return x;
}

int main()
{
    int arr[500];
    memset_explicit(arr, 0, sizeof arr);
    return 0;
}
