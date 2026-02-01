 

#include <config.h>

#include <sys/select.h>

#include <errno.h>
#include <signal.h>

 

#if !HAVE_PSELECT

int
pselect (int nfds, fd_set *restrict rfds,
         fd_set *restrict wfds, fd_set *restrict xfds,
         struct timespec const *restrict timeout,
         sigset_t const *restrict sigmask)
{
  int select_result;
  sigset_t origmask;
  struct timeval tv, *tvp;

  if (nfds < 0 || nfds > FD_SETSIZE)
    {
      errno = EINVAL;
      return -1;
    }

  if (timeout)
    {
      if (! (0 <= timeout->tv_nsec && timeout->tv_nsec < 1000000000))
        {
          errno = EINVAL;
          return -1;
        }

      tv = (struct timeval) {
        .tv_sec = timeout->tv_sec,
        .tv_usec = (timeout->tv_nsec + 999) / 1000
      };
      tvp = &tv;
    }
  else
    tvp = NULL;

   
  if (sigmask)
    pthread_sigmask (SIG_SETMASK, sigmask, &origmask);

  select_result = select (nfds, rfds, wfds, xfds, tvp);

  if (sigmask)
    {
      int select_errno = errno;
      pthread_sigmask (SIG_SETMASK, &origmask, NULL);
      errno = select_errno;
    }

  return select_result;
}

#else  
# include <unistd.h>
# undef pselect

int
rpl_pselect (int nfds, fd_set *restrict rfds,
             fd_set *restrict wfds, fd_set *restrict xfds,
             struct timespec const *restrict timeout,
             sigset_t const *restrict sigmask)
{
  int i;

   
  if (nfds < 0 || nfds > FD_SETSIZE)
    {
      errno = EINVAL;
      return -1;
    }
  for (i = 0; i < nfds; i++)
    {
      if (((rfds && FD_ISSET (i, rfds))
           || (wfds && FD_ISSET (i, wfds))
           || (xfds && FD_ISSET (i, xfds)))
          && dup2 (i, i) != i)
        return -1;
    }

  return pselect (nfds, rfds, wfds, xfds, timeout, sigmask);
}

#endif
