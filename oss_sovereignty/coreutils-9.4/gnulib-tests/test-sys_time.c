 

#include <config.h>

#include <sys/time.h>

 
struct timeval a;

 
typedef int verify_tv_sec_type[sizeof (time_t) <= sizeof (a.tv_sec) ? 1 : -1];

int
main (void)
{
  return 0;
}
