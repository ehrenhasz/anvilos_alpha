 

#include <config.h>

#include "utimens.h"

#include <stdio.h>
#include <stdlib.h>

#include "ignore-value.h"
#include "macros.h"

#define BASE "test-utimens.t"

#include "test-futimens.h"
#include "test-lutimens.h"
#include "test-utimens.h"

 
static int
do_futimens (int fd, struct timespec const times[2])
{
  return fdutimens (fd, NULL, times);
}

 
static int
do_fdutimens (char const *name, struct timespec const times[2])
{
  int result;
  int fd = open (name, O_WRONLY);
  if (fd < 0)
    fd = open (name, O_RDONLY);
  errno = 0;
  result = fdutimens (fd, name, times);
  if (0 <= fd)
    {
      int saved_errno = errno;
      close (fd);
      errno = saved_errno;
    }
  return result;
}

int
main (void)
{
  int result1;  
  int result2;  
  int result3;  

   
  ignore_value (system ("rm -rf " BASE "*"));

  result1 = test_utimens (utimens, true);
  ASSERT (test_utimens (do_fdutimens, false) == result1);
   
  result2 = test_futimens (do_futimens, result1 == 0);
  result3 = test_lutimens (lutimens, (result1 + result2) == 0);
   
  ASSERT (result1 <= result3);
  return result1 | result2 | result3;
}
