#ifndef DRA_SECURE_CALL_H
#define DRA_SECURE_CALL_H

#define DRA_SECURE_CALL_CAT_INTERNAL(x, y) x##y
#define DRA_SECURE_CALL_CAT(x, y) DRA_SECURE_CALL_CAT_INTERNAL(x, y)

#define DRA_SECURE_CALL_POINTER_NAME(line) DRA_SECURE_CALL_CAT(dra_secure_call_func_pointer_, line)

#ifdef __cplusplus
	#define DRA_SECURE_CALL_TYPEOF(x) decltype(x)
#elif defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 202311L)
	#define DRA_SECURE_CALL_TYPEOF(x) typeof(x)
#else
	#define DRA_SECURE_CALL_TYPEOF(x) __typeof__(x)
#endif

#define DRA_SECURE_CALL_INTERNAL(line, func_ptr_t, func_ptr, ...) do { \
	volatile func_ptr_t DRA_SECURE_CALL_POINTER_NAME(line) = func_ptr; \
    DRA_SECURE_CALL_POINTER_NAME(line)(__VA_ARGS__); \
} while(0)
#define DRA_SECURE_CALL_WITH_ASSIGNMENT_INTERNAL(line, out_var, func_ptr_t, func_ptr, ...) do { \
    volatile func_ptr_t DRA_SECURE_CALL_POINTER_NAME(line) = func_ptr; \
    *(out_var) = DRA_SECURE_CALL_POINTER_NAME(line)(__VA_ARGS__); \
} while(0)

#define DRA_SECURE_CALL(f, ...) DRA_SECURE_CALL_INTERNAL(__LINE__, DRA_SECURE_CALL_TYPEOF(&f), (&f), __VA_ARGS__)
#define DRA_SECURE_CALL_TYPED(t, f, ...) DRA_SECURE_CALL_INTERNAL(__LINE__, t, (&f), __VA_ARGS__)
#define DRA_SECURE_CALL_WITH_ASSIGNMENT(v, f, ...) DRA_SECURE_CALL_WITH_ASSIGNMENT_INTERNAL(__LINE__, (v), DRA_SECURE_CALL_TYPEOF(&f), (&f), __VA_ARGS__)
#define DRA_SECURE_CALL_WITH_ASSIGNMENT_TYPED(v, t, f, ...) DRA_SECURE_CALL_WITH_ASSIGNMENT_INTERNAL(__LINE__, (v), t, (&f), __VA_ARGS__)

#if defined(__GNUC__) || defined(__clang__)
    #define DRA_SAFE_CALL_BARRIER(p) do { __asm__ __volatile__("" ::"r"(p): "memory"); } while(0)
#else
    void safe_call_barrier_internal(void*) {}
    typedef void (*safe_call_barrier_t)(void*);
    volatile static safe_call_barrier_t safe_call_barrier_global = safe_call_barrier_internal;
    #define DRA_SAFE_CALL_BARRIER(p) safe_call_barrier_global(p)
#endif

#ifndef DRA_SECURE_CALL_DO_NOT_ADD_PUBLIC_NAMES
	#define secure_call(f, ...) DRA_SECURE_CALL(f, __VA_ARGS__)
	#define secure_call_typed(t, f, ...) DRA_SECURE_CALL_TYPED(t, f, __VA_ARGS__)
    #define secure_call_with_assignment(v, f, ...) DRA_SECURE_CALL_WITH_ASSIGNMENT(v, f, __VA_ARGS__)
    #define secure_call_with_assignment_typed(v, t, f, ...) DRA_SECURE_CALL_WITH_ASSIGNMENT_TYPED(v, t, f, __VA_ARGS__)
    #define safe_call_barrier(p) DRA_SAFE_CALL_BARRIER(p)
#endif

#endif
