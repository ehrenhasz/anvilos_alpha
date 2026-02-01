 
#include <signal.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#if HAVE_MSVC_INVALID_PARAMETER_HANDLER
# include "msvc-inval.h"
#endif

 

 
#undef signal

 
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
#ifdef SIGABRT_COMPAT
# define SIGABRT_COMPAT_MASK (1U << SIGABRT_COMPAT)
#else
# define SIGABRT_COMPAT_MASK 0
#endif

typedef void (*handler_t) (int);

#if HAVE_MSVC_INVALID_PARAMETER_HANDLER
static handler_t
signal_nothrow (int sig, handler_t handler)
{
  handler_t result;

  TRY_MSVC_INVAL
    {
      result = signal (sig, handler);
    }
  CATCH_MSVC_INVAL
    {
      result = SIG_ERR;
      errno = EINVAL;
    }
  DONE_MSVC_INVAL;

  return result;
}
# define signal signal_nothrow
#endif

 

#if GNULIB_defined_SIGPIPE
static handler_t SIGPIPE_handler = SIG_DFL;
#endif

#if GNULIB_defined_SIGPIPE
static handler_t
ext_signal (int sig, handler_t handler)
{
  switch (sig)
    {
    case SIGPIPE:
      {
        handler_t old_handler = SIGPIPE_handler;
        SIGPIPE_handler = handler;
        return old_handler;
      }
    default:  
      return signal (sig, handler);
    }
}
# undef signal
# define signal ext_signal
#endif

int
sigismember (const sigset_t *set, int sig)
{
  if (sig >= 0 && sig < NSIG)
    {
      #ifdef SIGABRT_COMPAT
      if (sig == SIGABRT_COMPAT)
        sig = SIGABRT;
      #endif

      return (*set >> sig) & 1;
    }
  else
    return 0;
}

int
sigemptyset (sigset_t *set)
{
  *set = 0;
  return 0;
}

int
sigaddset (sigset_t *set, int sig)
{
  if (sig >= 0 && sig < NSIG)
    {
      #ifdef SIGABRT_COMPAT
      if (sig == SIGABRT_COMPAT)
        sig = SIGABRT;
      #endif

      *set |= 1U << sig;
      return 0;
    }
  else
    {
      errno = EINVAL;
      return -1;
    }
}

int
sigdelset (sigset_t *set, int sig)
{
  if (sig >= 0 && sig < NSIG)
    {
      #ifdef SIGABRT_COMPAT
      if (sig == SIGABRT_COMPAT)
        sig = SIGABRT;
      #endif

      *set &= ~(1U << sig);
      return 0;
    }
  else
    {
      errno = EINVAL;
      return -1;
    }
}


int
sigfillset (sigset_t *set)
{
  *set = ((2U << (NSIG - 1)) - 1) & ~ SIGABRT_COMPAT_MASK;
  return 0;
}

 
static volatile sigset_t blocked_set  ;

 
static volatile sig_atomic_t pending_array[NSIG]  ;

 
static void
blocked_handler (int sig)
{
   
  signal (sig, blocked_handler);
  if (sig >= 0 && sig < NSIG)
    pending_array[sig] = 1;
}

int
sigpending (sigset_t *set)
{
  sigset_t pending = 0;
  int sig;

  for (sig = 0; sig < NSIG; sig++)
    if (pending_array[sig])
      pending |= 1U << sig;
  *set = pending;
  return 0;
}

 
static volatile handler_t old_handlers[NSIG];

int
sigprocmask (int operation, const sigset_t *set, sigset_t *old_set)
{
  if (old_set != NULL)
    *old_set = blocked_set;

  if (set != NULL)
    {
      sigset_t new_blocked_set;
      sigset_t to_unblock;
      sigset_t to_block;

      switch (operation)
        {
        case SIG_BLOCK:
          new_blocked_set = blocked_set | *set;
          break;
        case SIG_SETMASK:
          new_blocked_set = *set;
          break;
        case SIG_UNBLOCK:
          new_blocked_set = blocked_set & ~*set;
          break;
        default:
          errno = EINVAL;
          return -1;
        }
      to_unblock = blocked_set & ~new_blocked_set;
      to_block = new_blocked_set & ~blocked_set;

      if (to_block != 0)
        {
          int sig;

          for (sig = 0; sig < NSIG; sig++)
            if ((to_block >> sig) & 1)
              {
                pending_array[sig] = 0;
                if ((old_handlers[sig] = signal (sig, blocked_handler)) != SIG_ERR)
                  blocked_set |= 1U << sig;
              }
        }

      if (to_unblock != 0)
        {
          sig_atomic_t received[NSIG];
          int sig;

          for (sig = 0; sig < NSIG; sig++)
            if ((to_unblock >> sig) & 1)
              {
                if (signal (sig, old_handlers[sig]) != blocked_handler)
                   
                  abort ();
                received[sig] = pending_array[sig];
                blocked_set &= ~(1U << sig);
                pending_array[sig] = 0;
              }
            else
              received[sig] = 0;

          for (sig = 0; sig < NSIG; sig++)
            if (received[sig])
              raise (sig);
        }
    }
  return 0;
}

 
handler_t
rpl_signal (int sig, handler_t handler)
{
   
  if (sig >= 0 && sig < NSIG && sig != SIGKILL && sig != SIGSTOP
      && handler != SIG_ERR)
    {
      #ifdef SIGABRT_COMPAT
      if (sig == SIGABRT_COMPAT)
        sig = SIGABRT;
      #endif

      if (blocked_set & (1U << sig))
        {
           
          handler_t result = old_handlers[sig];
          old_handlers[sig] = handler;
          return result;
        }
      else
        return signal (sig, handler);
    }
  else
    {
      errno = EINVAL;
      return SIG_ERR;
    }
}

#if GNULIB_defined_SIGPIPE
 
int
_gl_raise_SIGPIPE (void)
{
  if (blocked_set & (1U << SIGPIPE))
    pending_array[SIGPIPE] = 1;
  else
    {
      handler_t handler = SIGPIPE_handler;
      if (handler == SIG_DFL)
        exit (128 + SIGPIPE);
      else if (handler != SIG_IGN)
        (*handler) (SIGPIPE);
    }
  return 0;
}
#endif
