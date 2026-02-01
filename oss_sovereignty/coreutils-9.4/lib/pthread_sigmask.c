 
#include <signal.h>

#include <errno.h>
#include <stddef.h>

#if PTHREAD_SIGMASK_INEFFECTIVE
# include <string.h>
#endif

#if PTHREAD_SIGMASK_UNBLOCK_BUG
# include <unistd.h>
#endif

int
pthread_sigmask (int how, const sigset_t *new_mask, sigset_t *old_mask)
#undef pthread_sigmask
{
#if HAVE_PTHREAD_SIGMASK
  int ret;

# if PTHREAD_SIGMASK_INEFFECTIVE
  sigset_t omask, omask_copy;
  sigset_t *old_mask_ptr = &omask;
  sigemptyset (&omask);
   
  sigaddset (&omask, SIGILL);
  memcpy (&omask_copy, &omask, sizeof omask);
# else
  sigset_t *old_mask_ptr = old_mask;
# endif

  ret = pthread_sigmask (how, new_mask, old_mask_ptr);

# if PTHREAD_SIGMASK_INEFFECTIVE
  if (ret == 0)
    {
       
      if (memcmp (&omask_copy, &omask, sizeof omask) == 0
          && pthread_sigmask (1729, &omask_copy, NULL) == 0)
        {
           
          return (sigprocmask (how, new_mask, old_mask) < 0 ? errno : 0);
        }

      if (old_mask)
        memcpy (old_mask, &omask, sizeof omask);
    }
# endif
# if PTHREAD_SIGMASK_FAILS_WITH_ERRNO
  if (ret == -1)
    return errno;
# endif
# if PTHREAD_SIGMASK_UNBLOCK_BUG
  if (ret == 0
      && new_mask != NULL
      && (how == SIG_UNBLOCK || how == SIG_SETMASK))
    {
       
      usleep (1);
    }
# endif
  return ret;
#else
  int ret = sigprocmask (how, new_mask, old_mask);
  return (ret < 0 ? errno : 0);
#endif
}
