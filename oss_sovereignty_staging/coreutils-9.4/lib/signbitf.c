 
#include <math.h>

#include <string.h>
#include "isnanf-nolibm.h"
#include "float+.h"

#ifdef gl_signbitf_OPTIMIZED_MACRO
# undef gl_signbitf
#endif

int
gl_signbitf (float arg)
{
#if defined FLT_SIGNBIT_WORD && defined FLT_SIGNBIT_BIT
   
# define NWORDS \
    ((sizeof (float) + sizeof (unsigned int) - 1) / sizeof (unsigned int))
  union { float value; unsigned int word[NWORDS]; } m;
  m.value = arg;
  return (m.word[FLT_SIGNBIT_WORD] >> FLT_SIGNBIT_BIT) & 1;
#elif HAVE_COPYSIGNF_IN_LIBC
  return copysignf (1.0f, arg) < 0;
#else
   
  if (isnanf (arg))
    return 0;
  if (arg < 0.0f)
    return 1;
  else if (arg == 0.0f)
    {
       
      static float plus_zero = 0.0f;
      float arg_mem = arg;
      return (memcmp (&plus_zero, &arg_mem, SIZEOF_FLT) != 0);
    }
  else
    return 0;
#endif
}
