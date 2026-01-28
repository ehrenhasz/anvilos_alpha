

#ifndef ZSTD_COMPILER_H
#define ZSTD_COMPILER_H




#if !defined(ZSTD_NO_INLINE)
#if (defined(__GNUC__) && !defined(__STRICT_ANSI__)) || defined(__cplusplus) || defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L   
#  define INLINE_KEYWORD inline
#else
#  define INLINE_KEYWORD
#endif

#if defined(__GNUC__) || defined(__ICCARM__)
#  define FORCE_INLINE_ATTR __attribute__((always_inline))
#elif defined(_MSC_VER)
#  define FORCE_INLINE_ATTR __forceinline
#else
#  define FORCE_INLINE_ATTR
#endif

#else

#define INLINE_KEYWORD
#define FORCE_INLINE_ATTR

#endif


#define FORCE_INLINE_TEMPLATE static INLINE_KEYWORD FORCE_INLINE_ATTR

#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ >= 4 && __GNUC_MINOR__ >= 8 && __GNUC__ < 5
#  define HINT_INLINE static INLINE_KEYWORD
#else
#  define HINT_INLINE static INLINE_KEYWORD FORCE_INLINE_ATTR
#endif


#if defined(__GNUC__)
#  define UNUSED_ATTR __attribute__((unused))
#else
#  define UNUSED_ATTR
#endif


#ifdef _MSC_VER
#  define FORCE_NOINLINE static __declspec(noinline)
#else
#  if defined(__GNUC__) || defined(__ICCARM__)
#    define FORCE_NOINLINE static __attribute__((__noinline__))
#  else
#    define FORCE_NOINLINE static
#  endif
#endif


#ifndef __has_attribute
  #define __has_attribute(x) 0  
#endif
#if defined(__GNUC__) || defined(__ICCARM__)
#  define TARGET_ATTRIBUTE(target) __attribute__((__target__(target)))
#else
#  define TARGET_ATTRIBUTE(target)
#endif


#ifndef DYNAMIC_BMI2
  #if ((defined(__clang__) && __has_attribute(__target__)) \
      || (defined(__GNUC__) \
          && (__GNUC__ >= 5 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 8)))) \
      && (defined(__x86_64__) || defined(_M_X86)) \
      && !defined(__BMI2__)
  #  define DYNAMIC_BMI2 1
  #else
  #  define DYNAMIC_BMI2 0
  #endif
#endif


#if defined(NO_PREFETCH)
#  define PREFETCH_L1(ptr)  (void)(ptr)  
#  define PREFETCH_L2(ptr)  (void)(ptr)  
#else
#  if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_I86))  
#    include <mmintrin.h>   
#    define PREFETCH_L1(ptr)  _mm_prefetch((const char*)(ptr), _MM_HINT_T0)
#    define PREFETCH_L2(ptr)  _mm_prefetch((const char*)(ptr), _MM_HINT_T1)
#    elif defined(__aarch64__)
#     define PREFETCH_L1(ptr)  __asm__ __volatile__("prfm pldl1keep, %0" ::"Q"(*(ptr)))
#     define PREFETCH_L2(ptr)  __asm__ __volatile__("prfm pldl2keep, %0" ::"Q"(*(ptr)))
#  elif defined(__GNUC__) && ( (__GNUC__ >= 4) || ( (__GNUC__ == 3) && (__GNUC_MINOR__ >= 1) ) )
#    define PREFETCH_L1(ptr)  __builtin_prefetch((ptr), 0 , 3 )
#    define PREFETCH_L2(ptr)  __builtin_prefetch((ptr), 0 , 2 )
#  else
#    define PREFETCH_L1(ptr) (void)(ptr)  
#    define PREFETCH_L2(ptr) (void)(ptr)  
#  endif
#endif  

#define CACHELINE_SIZE 64

#define PREFETCH_AREA(p, s)  {            \
    const char* const _ptr = (const char*)(p);  \
    size_t const _size = (size_t)(s);     \
    size_t _pos;                          \
    for (_pos=0; _pos<_size; _pos+=CACHELINE_SIZE) {  \
        PREFETCH_L2(_ptr + _pos);         \
    }                                     \
}


#if !defined(__INTEL_COMPILER) && !defined(__clang__) && defined(__GNUC__)
#  if (__GNUC__ == 4 && __GNUC_MINOR__ > 3) || (__GNUC__ >= 5)
#    define DONT_VECTORIZE __attribute__((optimize("no-tree-vectorize")))
#  else
#    define DONT_VECTORIZE _Pragma("GCC optimize(\"no-tree-vectorize\")")
#  endif
#else
#  define DONT_VECTORIZE
#endif


#if defined(__GNUC__)
#define LIKELY(x) (__builtin_expect((x), 1))
#define UNLIKELY(x) (__builtin_expect((x), 0))
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif


#ifdef _MSC_VER    
#  include <intrin.h>                    
#  pragma warning(disable : 4100)        
#  pragma warning(disable : 4127)        
#  pragma warning(disable : 4204)        
#  pragma warning(disable : 4214)        
#  pragma warning(disable : 4324)        
#endif

#endif 
