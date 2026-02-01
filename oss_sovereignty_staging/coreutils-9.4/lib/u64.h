 

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <stdint.h>

_GL_INLINE_HEADER_BEGIN
#ifndef _GL_U64_INLINE
# define _GL_U64_INLINE _GL_INLINE
#endif

 
#define u64rol(x, n) u64or (u64shl (x, n), u64shr (x, 64 - n))

#ifdef UINT64_MAX

 
typedef uint64_t u64;
# define u64hilo(hi, lo) ((u64) (((u64) (hi) << 32) + (lo)))
# define u64init(hi, lo) u64hilo (hi, lo)
# define u64lo(x) ((u64) (x))
# define u64size(x) u64lo (x)
# define u64lt(x, y) ((x) < (y))
# define u64and(x, y) ((x) & (y))
# define u64or(x, y) ((x) | (y))
# define u64xor(x, y) ((x) ^ (y))
# define u64plus(x, y) ((x) + (y))
# define u64shl(x, n) ((x) << (n))
# define u64shr(x, n) ((x) >> (n))

#else

 
# ifdef WORDS_BIGENDIAN
typedef struct { uint32_t hi, lo; } u64;
#  define u64init(hi, lo) { hi, lo }
# else
typedef struct { uint32_t lo, hi; } u64;
#  define u64init(hi, lo) { lo, hi }
# endif

 
_GL_U64_INLINE u64
u64hilo (uint32_t hi, uint32_t lo)
{
  u64 r;
  r.hi = hi;
  r.lo = lo;
  return r;
}

 
_GL_U64_INLINE u64
u64lo (uint32_t lo)
{
  u64 r;
  r.hi = 0;
  r.lo = lo;
  return r;
}

 
_GL_U64_INLINE u64
u64size (size_t size)
{
  u64 r;
  r.hi = size >> 31 >> 1;
  r.lo = size;
  return r;
}

 
_GL_U64_INLINE int
u64lt (u64 x, u64 y)
{
  return x.hi < y.hi || (x.hi == y.hi && x.lo < y.lo);
}

 
_GL_U64_INLINE u64
u64and (u64 x, u64 y)
{
  u64 r;
  r.hi = x.hi & y.hi;
  r.lo = x.lo & y.lo;
  return r;
}

 
_GL_U64_INLINE u64
u64or (u64 x, u64 y)
{
  u64 r;
  r.hi = x.hi | y.hi;
  r.lo = x.lo | y.lo;
  return r;
}

 
_GL_U64_INLINE u64
u64xor (u64 x, u64 y)
{
  u64 r;
  r.hi = x.hi ^ y.hi;
  r.lo = x.lo ^ y.lo;
  return r;
}

 
_GL_U64_INLINE u64
u64plus (u64 x, u64 y)
{
  u64 r;
  r.lo = x.lo + y.lo;
  r.hi = x.hi + y.hi + (r.lo < x.lo);
  return r;
}

 
_GL_U64_INLINE u64
u64shl (u64 x, int n)
{
  u64 r;
  if (n < 32)
    {
      r.hi = (x.hi << n) | (x.lo >> (32 - n));
      r.lo = x.lo << n;
    }
  else
    {
      r.hi = x.lo << (n - 32);
      r.lo = 0;
    }
  return r;
}

 
_GL_U64_INLINE u64
u64shr (u64 x, int n)
{
  u64 r;
  if (n < 32)
    {
      r.hi = x.hi >> n;
      r.lo = (x.hi << (32 - n)) | (x.lo >> n);
    }
  else
    {
      r.hi = 0;
      r.lo = x.hi >> (n - 32);
    }
  return r;
}

#endif

_GL_INLINE_HEADER_END
