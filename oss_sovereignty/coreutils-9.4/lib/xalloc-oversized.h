 
#define __xalloc_oversized(n, s) \
  ((s) != 0 \
   && ((size_t) (PTRDIFF_MAX < SIZE_MAX ? PTRDIFF_MAX : SIZE_MAX - 1) / (s) \
       < (n)))

 
#if 7 <= __GNUC__ && !defined __clang__ && PTRDIFF_MAX < SIZE_MAX
# define xalloc_oversized(n, s) \
   __builtin_mul_overflow_p (n, s, (ptrdiff_t) 1)
#elif (5 <= __GNUC__ && !defined __ICC && !__STRICT_ANSI__ \
       && PTRDIFF_MAX < SIZE_MAX)
# define xalloc_oversized(n, s) \
   (__builtin_constant_p (n) && __builtin_constant_p (s) \
    ? __xalloc_oversized (n, s) \
    : ({ ptrdiff_t __xalloc_count; \
         __builtin_mul_overflow (n, s, &__xalloc_count); }))

 
#else
# define xalloc_oversized(n, s) __xalloc_oversized (n, s)
#endif

#endif  
