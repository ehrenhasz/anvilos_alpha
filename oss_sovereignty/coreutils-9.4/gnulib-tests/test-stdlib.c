 

#include <config.h>

#include <stdlib.h>

 
static int exitcode = EXIT_SUCCESS;
#if EXIT_SUCCESS
"oops"
#endif

 
#if EXIT_FAILURE != 1
"oops"
#endif

 
static_assert (sizeof NULL == sizeof (void *));

#if GNULIB_TEST_SYSTEM_POSIX
# include "test-sys_wait.h"
#else
# define test_sys_wait_macros() 0
#endif

int
main (void)
{
   
   
#ifndef __ANDROID__
  if (MB_CUR_MAX != 1)
    return 1;
#endif

  if (test_sys_wait_macros ())
    return 2;

  return exitcode;
}
