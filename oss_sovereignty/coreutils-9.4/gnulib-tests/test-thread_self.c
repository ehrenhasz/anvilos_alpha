 

#include <config.h>

#include "glthread/thread.h"

gl_thread_t main_thread;

int
main ()
{
   
   
#if !defined _AIX
  main_thread = gl_thread_self ();
#endif

  return 0;
}
