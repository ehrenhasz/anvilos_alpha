 
#include <signal.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

 

 
#if defined SIGCHLD || defined HAVE_SIGALTSTACK || defined HAVE_SIGINTERRUPT
# error "Revisit the assumptions made in the sigaction module"
#endif

 
#ifndef SIGKILL
# define SIGKILL (-1)
#endif
#ifndef SIGSTOP
# define SIGSTOP (-1)
#endif

 
#if defined _WIN32 && ! defined __CYGWIN__
# undef SIGABRT_COMPAT
# define SIGABRT_COMPAT 6
#endif

 
typedef void (*handler_t) (int signal);

 
static struct sigaction volatile action_array[NSIG]  ;

 
static void
sigaction_handler (int sig)
{
  handler_t handler;
  sigset_t mask;
  sigset_t oldmask;
  int saved_errno = errno;
  if (sig < 0 || NSIG <= sig || !action_array[sig].sa_handler)
    {
       
      if (sig == SIGABRT)
        signal (SIGABRT, SIG_DFL);
      abort ();
    }

   
  handler = action_array[sig].sa_handler;
  if ((action_array[sig].sa_flags & SA_RESETHAND) == 0)
    signal (sig, sigaction_handler);
  else
    action_array[sig].sa_handler = NULL;

   
  mask = action_array[sig].sa_mask;
  if ((action_array[sig].sa_flags & SA_NODEFER) == 0)
    sigaddset (&mask, sig);
  sigprocmask (SIG_BLOCK, &mask, &oldmask);

   
  errno = saved_errno;
  handler (sig);
  saved_errno = errno;
  sigprocmask (SIG_SETMASK, &oldmask, NULL);
  errno = saved_errno;
}

 
int
sigaction (int sig, const struct sigaction *restrict act,
           struct sigaction *restrict oact)
{
  sigset_t mask;
  sigset_t oldmask;
  int saved_errno;

  if (sig < 0 || NSIG <= sig || sig == SIGKILL || sig == SIGSTOP
      || (act && act->sa_handler == SIG_ERR))
    {
      errno = EINVAL;
      return -1;
    }

#ifdef SIGABRT_COMPAT
  if (sig == SIGABRT_COMPAT)
    sig = SIGABRT;
#endif

   
  if (!act && !oact)
    return 0;
  sigfillset (&mask);
  sigprocmask (SIG_BLOCK, &mask, &oldmask);
  if (oact)
    {
      if (action_array[sig].sa_handler)
        *oact = action_array[sig];
      else
        {
           
          oact->sa_handler = signal (sig, SIG_DFL);
          if (oact->sa_handler == SIG_ERR)
            goto failure;
          signal (sig, oact->sa_handler);
          oact->sa_flags = SA_RESETHAND | SA_NODEFER;
          sigemptyset (&oact->sa_mask);
        }
    }

  if (act)
    {
       
      if (act->sa_handler == SIG_DFL || act->sa_handler == SIG_IGN)
        {
          if (signal (sig, act->sa_handler) == SIG_ERR)
            goto failure;
          action_array[sig].sa_handler = NULL;
        }
      else
        {
          if (signal (sig, sigaction_handler) == SIG_ERR)
            goto failure;
          action_array[sig] = *act;
        }
    }
  sigprocmask (SIG_SETMASK, &oldmask, NULL);
  return 0;

 failure:
  saved_errno = errno;
  sigprocmask (SIG_SETMASK, &oldmask, NULL);
  errno = saved_errno;
  return -1;
}
