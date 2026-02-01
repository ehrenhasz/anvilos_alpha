 

 

 

#ifndef	_BASH_SYSTIMES_H
#define _BASH_SYSTIMES_H	1

#if defined (HAVE_SYS_TIMES_H)
#  include <sys/times.h>
#else  

#include <stdc.h>

 
struct tms
  {
    clock_t tms_utime;		 
    clock_t tms_stime;		 

    clock_t tms_cutime;		 
    clock_t tms_cstime;		 
  };

 
extern clock_t times PARAMS((struct tms *buffer));

#endif  
#endif  
