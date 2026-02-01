 

 
#if (__GNUC__ == 4 && 3 <= __GNUC_MINOR__) || 4 < __GNUC__
# pragma GCC diagnostic ignored "-Wtype-limits"
#elif defined __clang__
# pragma clang diagnostic ignored "-Wtautological-compare"
#endif

#include <config.h>

#include "inttostr.h"

 

_GL_ATTRIBUTE_NODISCARD char *
anytostr (inttype i, char *buf)
{
  char *p = buf + INT_STRLEN_BOUND (inttype);
  *p = 0;

  if (i < 0)
    {
      do
        *--p = '0' - i % 10;
      while ((i /= 10) != 0);

      *--p = '-';
    }
  else
    {
      do
        *--p = '0' + i % 10;
      while ((i /= 10) != 0);
    }

  return p;
}
