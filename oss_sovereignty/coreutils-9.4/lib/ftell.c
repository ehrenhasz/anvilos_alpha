 
#include <stdio.h>

#include <errno.h>
#include <limits.h>

long
ftell (FILE *fp)
{
   
  off_t offset = ftello (fp);
  if (LONG_MIN <= offset && offset <= LONG_MAX)
    return   offset;
  else
    {
      errno = EOVERFLOW;
      return -1;
    }
}
