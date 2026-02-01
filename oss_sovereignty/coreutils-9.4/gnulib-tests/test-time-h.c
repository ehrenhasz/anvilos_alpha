 

#include <config.h>

#include <time.h>

 
struct timespec t1;
#if 0
 
pid_t t2;
#endif

 
static_assert (sizeof NULL == sizeof (void *));

 
int t3 = TIME_UTC;
static_assert (TIME_UTC > 0);

int
main (void)
{
  return 0;
}
