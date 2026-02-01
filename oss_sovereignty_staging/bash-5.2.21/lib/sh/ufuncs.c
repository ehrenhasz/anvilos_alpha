 

 

#include "config.h"

#include "bashtypes.h"

#include "posixtime.h"

#if defined (HAVE_UNISTD_H)
#include <unistd.h>
#endif

#include <errno.h>
#if !defined (errno)
extern int errno;
#endif  

#if defined (HAVE_SELECT)
#  include "posixselect.h"
#  include "quit.h"
#  include "trap.h"
#  include "stat-time.h"
#endif

 

#if defined (HAVE_SETITIMER)
unsigned int
falarm(secs, usecs)
     unsigned int secs, usecs;
{
  struct itimerval it, oit;

  it.it_interval.tv_sec = 0;
  it.it_interval.tv_usec = 0;

  it.it_value.tv_sec = secs;
  it.it_value.tv_usec = usecs;

  if (setitimer(ITIMER_REAL, &it, &oit) < 0)
    return (-1);		 

   
  if (oit.it_value.tv_usec)
    oit.it_value.tv_sec++;
  return (oit.it_value.tv_sec);
}
#else
int
falarm (secs, usecs)
     unsigned int secs, usecs;
{
  if (secs == 0 && usecs == 0)
    return (alarm (0));

  if (secs == 0 || usecs >= 500000)
    {
      secs++;
      usecs = 0;
    }
  return (alarm (secs));
}
#endif  

 

#if defined (HAVE_TIMEVAL) && (defined (HAVE_SELECT) || defined (HAVE_PSELECT))
int
fsleep(sec, usec)
     unsigned int sec, usec;
{
  int e, r;
  sigset_t blocked_sigs, prevmask;
#if defined (HAVE_PSELECT)
  struct timespec ts;
#else
  struct timeval tv;
#endif

  sigemptyset (&blocked_sigs);
#  if defined (SIGCHLD)
  sigaddset (&blocked_sigs, SIGCHLD);
#  endif

#if defined (HAVE_PSELECT)
  ts.tv_sec = sec;
  ts.tv_nsec = usec * 1000;
#else
  sigemptyset (&prevmask);
  tv.tv_sec = sec;
  tv.tv_usec = usec;
#endif  

  do
    {
#if defined (HAVE_PSELECT)
      r = pselect(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &ts, &blocked_sigs);
#else
      sigprocmask (SIG_SETMASK, &blocked_sigs, &prevmask);
      r = select(0, (fd_set *)0, (fd_set *)0, (fd_set *)0, &tv);
      sigprocmask (SIG_SETMASK, &prevmask, NULL);
#endif
      e = errno;
      if (r < 0 && errno == EINTR)
	return -1;		 
      errno = e;
    }
  while (r < 0 && errno == EINTR);

  return r;
}
#else  
int
fsleep(sec, usec)
     long sec, usec;
{
  if (usec >= 500000)	 
   sec++;
  return (sleep(sec));
}
#endif  
