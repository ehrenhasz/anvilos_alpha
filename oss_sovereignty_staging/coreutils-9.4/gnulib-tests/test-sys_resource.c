 
#ifdef TODO  
rlim_t t1;
struct rlimit t2;
#endif
struct rusage t3;
#ifdef TODO
struct timeval t4;
id_t t5;
time_t t10;
suseconds_t t11;
fd_set t12;
#endif

 
#ifdef TODO  
int prios[] =
  {
    PRIO_PROCESS,
    PRIO_PGRP,
    PRIO_USER
  };
int rlims[] =
  {
    RLIM_INFINITY,
    RLIM_SAVED_MAX,
    RLIM_SAVED_CUR
  };
#endif
int rusages[] =
  {
    RUSAGE_SELF,
    RUSAGE_CHILDREN
  };
#ifdef TODO
int rlimits[] =
  {
    RLIMIT_CORE,
    RLIMIT_CPU,
    RLIMIT_DATA,
    RLIMIT_FSIZE,
    RLIMIT_NOFILE,
    RLIMIT_STACK,
    RLIMIT_AS
  };
#endif

int
main (void)
{
  return 0;
}
