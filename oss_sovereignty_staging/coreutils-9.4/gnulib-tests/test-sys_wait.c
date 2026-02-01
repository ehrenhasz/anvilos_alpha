 

#include <config.h>

#include <sys/wait.h>

 
static pid_t a;

#include "test-sys_wait.h"

int
main (void)
{
  if (test_sys_wait_macros ())
    return 1;

#if 0
  switch (WCONTINUED)
    {
   
    case WCONTINUED:
    case WEXITED:
    case WNOWAIT:
    case WSTOPPED:
      break;
    }
#endif

  return a ? 1 : 0;
}
