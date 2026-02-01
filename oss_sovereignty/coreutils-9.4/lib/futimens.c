 

#include <config.h>

#include <sys/stat.h>

#include "utimens.h"

 
int
futimens (int fd, struct timespec const times[2])
{
   
  return fdutimens (fd, NULL, times);
}
