 
#if !_LIBC && !_GL_CONFIG_H_INCLUDED
 #error "Please include config.h first."
#endif

#include <limits.h>
#ifdef _LIBC
# include <sys/param.h>
# define _GL_ATTRIBUTE_CONST __attribute__ ((const))
#else
# include <unistd.h>
# include "minmax.h"
# define __sysconf sysconf
# if (!defined SYMLOOP_MAX \
      && ! (defined _SC_SYMLOOP_MAX && defined _POSIX_SYMLOOP_MAX))
#  define SYMLOOP_MAX 8
# endif
#endif

 

#ifndef MIN_ELOOP_THRESHOLD
# define MIN_ELOOP_THRESHOLD    40
#endif

 
static inline unsigned int _GL_ATTRIBUTE_CONST
__eloop_threshold (void)
{
#ifdef SYMLOOP_MAX
  const int symloop_max = SYMLOOP_MAX;
#else
   
  static long int sysconf_symloop_max;
  if (sysconf_symloop_max == 0)
    sysconf_symloop_max = __sysconf (_SC_SYMLOOP_MAX);
  const unsigned int symloop_max = (sysconf_symloop_max <= 0
                                    ? _POSIX_SYMLOOP_MAX
                                    : sysconf_symloop_max);
#endif

  return MAX (symloop_max, MIN_ELOOP_THRESHOLD);
}

#endif   
