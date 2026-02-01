 

#ifndef COUNT_LEADING_ZEROS_H
#define COUNT_LEADING_ZEROS_H 1

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <limits.h>
#include <stdlib.h>

_GL_INLINE_HEADER_BEGIN
#ifndef COUNT_LEADING_ZEROS_INLINE
# define COUNT_LEADING_ZEROS_INLINE _GL_INLINE
#endif

#ifdef __cplusplus
extern "C" {
#endif

 
#if __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4) \
    || (__clang_major__ >= 4)
# define COUNT_LEADING_ZEROS(BUILTIN, MSC_BUILTIN, TYPE)                \
  return x ? BUILTIN (x) : CHAR_BIT * sizeof x;
#elif _MSC_VER
# pragma intrinsic (_BitScanReverse)
# if defined _M_X64
#  pragma intrinsic (_BitScanReverse64)
# endif
# define COUNT_LEADING_ZEROS(BUILTIN, MSC_BUILTIN, TYPE)                \
    do                                                                  \
      {                                                                 \
        unsigned long result;                                           \
        if (MSC_BUILTIN (&result, x))                                   \
          return CHAR_BIT * sizeof x - 1 - result;                      \
        return CHAR_BIT * sizeof x;                                     \
      }                                                                 \
    while (0)
#else
# define COUNT_LEADING_ZEROS(BUILTIN, MSC_BUILTIN, TYPE)                \
    do                                                                  \
      {                                                                 \
        int count;                                                      \
        unsigned int leading_32;                                        \
        if (! x)                                                        \
          return CHAR_BIT * sizeof x;                                   \
        for (count = 0;                                                 \
             (leading_32 = ((x >> (sizeof (TYPE) * CHAR_BIT - 32))      \
                            & 0xffffffffU),                             \
              count < CHAR_BIT * sizeof x - 32 && !leading_32);         \
             count += 32)                                               \
          x = x << 31 << 1;                                             \
        return count + count_leading_zeros_32 (leading_32);             \
      }                                                                 \
    while (0)

 
COUNT_LEADING_ZEROS_INLINE int
count_leading_zeros_32 (unsigned int x)
{
   
COUNT_LEADING_ZEROS_INLINE int
count_leading_zeros (unsigned int x)
{
  COUNT_LEADING_ZEROS (__builtin_clz, _BitScanReverse, unsigned int);
}

 
COUNT_LEADING_ZEROS_INLINE int
count_leading_zeros_l (unsigned long int x)
{
  COUNT_LEADING_ZEROS (__builtin_clzl, _BitScanReverse, unsigned long int);
}

 
COUNT_LEADING_ZEROS_INLINE int
count_leading_zeros_ll (unsigned long long int x)
{
#if (defined _MSC_VER && !defined __clang__) && !defined _M_X64
   
  unsigned long result;
  if (_BitScanReverse (&result, (unsigned long) (x >> 32)))
    return CHAR_BIT * sizeof x - 1 - 32 - result;
  if (_BitScanReverse (&result, (unsigned long) x))
    return CHAR_BIT * sizeof x - 1 - result;
  return CHAR_BIT * sizeof x;
#else
  COUNT_LEADING_ZEROS (__builtin_clzll, _BitScanReverse64,
                       unsigned long long int);
#endif
}

#ifdef __cplusplus
}
#endif

_GL_INLINE_HEADER_END

#endif  
