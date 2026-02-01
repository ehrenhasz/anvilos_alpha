 

 

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include "intprops.h"

 

int
main (void)
{
  int result = 0;

   
  if (TYPE_MAXIMUM (off_t) >> 31 >> 31 == 0)
    result |= 1;

   
  {
    struct stat st;
    if (sizeof st.st_size != sizeof (off_t))
      result |= 2;
  }

  return result;
}
