 
#include <math.h>

#include <string.h>
#include "isnanl-nolibm.h"
#include "float+.h"

#ifdef gl_signbitl_OPTIMIZED_MACRO
# undef gl_signbitl
#endif

int
gl_signbitl (long double arg)
{
#if defined LDBL_SIGNBIT_WORD && defined LDBL_SIGNBIT_BIT
   
# define NWORDS \
    ((sizeof (long double) + sizeof (unsigned int) - 1) / sizeof (unsigned int))
  union { long double value; unsigned int word[NWORDS]; } m;
  m.value = arg;
  return (m.word[LDBL_SIGNBIT_WORD] >> LDBL_SIGNBIT_BIT) & 1;
#elif HAVE_COPYSIGNL_IN_LIBC
  return copysignl (1.0L, arg) < 0;
#else
   
  if (isnanl (arg))
    return 0;
  if (arg < 0.0L)
    return 1;
  else if (arg == 0.0L)
    {
       
      static long double plus_zero = 0.0L;
      long double arg_mem = arg;
      return (memcmp (&plus_zero, &arg_mem, SIZEOF_LDBL) != 0);
    }
  else
    return 0;
#endif
}
