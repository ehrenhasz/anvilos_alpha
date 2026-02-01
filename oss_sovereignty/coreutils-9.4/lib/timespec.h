 
#if !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <time.h>

_GL_INLINE_HEADER_BEGIN
#ifndef _GL_TIMESPEC_INLINE
# define _GL_TIMESPEC_INLINE _GL_INLINE
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "arg-nonnull.h"

 

enum { TIMESPEC_HZ = 1000000000 };
enum { LOG10_TIMESPEC_HZ = 9 };

 

enum { TIMESPEC_RESOLUTION = TIMESPEC_HZ };
enum { LOG10_TIMESPEC_RESOLUTION = LOG10_TIMESPEC_HZ };

 

_GL_TIMESPEC_INLINE struct timespec
make_timespec (time_t s, long int ns)
{
  return (struct timespec) { .tv_sec = s, .tv_nsec = ns };
}

 

_GL_TIMESPEC_INLINE int _GL_ATTRIBUTE_PURE
timespec_cmp (struct timespec a, struct timespec b)
{
  return 2 * _GL_CMP (a.tv_sec, b.tv_sec) + _GL_CMP (a.tv_nsec, b.tv_nsec);
}

 
_GL_TIMESPEC_INLINE int _GL_ATTRIBUTE_PURE
timespec_sign (struct timespec a)
{
  return _GL_CMP (a.tv_sec, 0) + (!a.tv_sec & !!a.tv_nsec);
}

struct timespec timespec_add (struct timespec, struct timespec)
  _GL_ATTRIBUTE_CONST;
struct timespec timespec_sub (struct timespec, struct timespec)
  _GL_ATTRIBUTE_CONST;
struct timespec dtotimespec (double)
  _GL_ATTRIBUTE_CONST;

 
_GL_TIMESPEC_INLINE double
timespectod (struct timespec a)
{
  return a.tv_sec + a.tv_nsec / 1e9;
}

long int gettime_res (void);
struct timespec current_timespec (void);
void gettime (struct timespec *) _GL_ARG_NONNULL ((1));
int settime (struct timespec const *) _GL_ARG_NONNULL ((1));

#ifdef __cplusplus
}
#endif

_GL_INLINE_HEADER_END

#endif
