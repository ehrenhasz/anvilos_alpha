 

 

#include <config.h>

#include <sys/types.h>
#include "intprops.h"

 

int
main (void)
{
   
  return TYPE_MAXIMUM (time_t) >> 31 == 0;
}
