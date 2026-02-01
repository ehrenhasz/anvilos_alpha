 
#include <math.h>

#include <string.h>
#include "isnand-nolibm.h"
#include "float+.h"

#ifdef gl_signbitd_OPTIMIZED_MACRO
# undef gl_signbitd
#endif

int
gl_signbitd (double arg)
{
#if defined DBL_SIGNBIT_WORD && defined DBL_SIGNBIT_BIT
   
# define NWORDS \
    ((sizeof (double) + sizeof (unsigned int) - 1) / sizeof (unsigned int))
  union { double value; unsigned int word[NWORDS]; } m;
  m.value = arg;
  return (m.word[DBL_SIGNBIT_WORD] >> DBL_SIGNBIT_BIT) & 1;
#elif HAVE_COPYSIGN_IN_LIBC
  return copysign (1.0, arg) < 0;
#else
   
  if (isnand (arg))
    return 0;
  if (arg < 0.0)
    return 1;
  else if (arg == 0.0)
    {
       
      static double plus_zero = 0.0;
      double arg_mem = arg;
      return (memcmp (&plus_zero, &arg_mem, SIZEOF_DBL) != 0);
    }
  else
    return 0;
#endif
}
