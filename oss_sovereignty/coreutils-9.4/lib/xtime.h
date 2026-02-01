 

#ifndef XTIME_H_
#define XTIME_H_ 1

 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

_GL_INLINE_HEADER_BEGIN
#ifndef XTIME_INLINE
# define XTIME_INLINE _GL_INLINE
#endif

 
typedef long long int xtime_t;
#define XTIME_PRECISION 1000000000

#ifdef  __cplusplus
extern "C" {
#endif

 
XTIME_INLINE xtime_t
xtime_make (xtime_t s, long int ns)
{
  return XTIME_PRECISION * s + ns;
}

 

 
XTIME_INLINE xtime_t
xtime_nonnegative_sec (xtime_t t)
{
  return t / XTIME_PRECISION;
}

 
XTIME_INLINE xtime_t
xtime_sec (xtime_t t)
{
  return (t + (t < 0)) / XTIME_PRECISION - (t < 0);
}

 
XTIME_INLINE long int
xtime_nonnegative_nsec (xtime_t t)
{
  return t % XTIME_PRECISION;
}

 
XTIME_INLINE long int
xtime_nsec (xtime_t t)
{
  long int ns = t % XTIME_PRECISION;
  if (ns < 0)
    ns += XTIME_PRECISION;
  return ns;
}

#ifdef  __cplusplus
}
#endif

_GL_INLINE_HEADER_END

#endif
