 

#include <config.h>

#include <sched.h>

 
static struct sched_param a;

 
int b[] = { SCHED_FIFO, SCHED_RR, SCHED_OTHER };

 
pid_t t1;

int f1;

int
main ()
{
   
  f1 = a.sched_priority;

  return 0;
}
