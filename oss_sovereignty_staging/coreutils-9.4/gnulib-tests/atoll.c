 
#include <stdlib.h>

#if _LIBC
# undef atoll
#endif


 
long long int
atoll (const char *nptr)
{
  return strtoll (nptr, (char **) NULL, 10);
}
