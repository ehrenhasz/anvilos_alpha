 

#include <config.h>

#include <signal.h>

#include "signature.h"
SIGNATURE_CHECK (sigaction, int, (int, struct sigaction const *,
                                  struct sigaction *));

#include <stddef.h>

#include "macros.h"

#ifndef SA_NOCLDSTOP
# define SA_NOCLDSTOP 0
#endif
#ifndef SA_ONSTACK
# define SA_ONSTACK 0
#endif
#ifndef SA_RESETHAND
# define SA_RESETHAND 0
#endif
#ifndef SA_RESTART
# define SA_RESTART 0
#endif
#ifndef SA_SIGINFO
# define SA_SIGINFO 0
#endif
#ifndef SA_NOCLDWAIT
# define SA_NOCLDWAIT 0
#endif

 
#define MASK_SA_FLAGS (SA_NOCLDSTOP | SA_ONSTACK | SA_RESETHAND | SA_RESTART \
                       | SA_SIGINFO | SA_NOCLDWAIT | SA_NODEFER)

 

static void
handler (int sig)
{
  static int entry_count;
  struct sigaction sa;
  ASSERT (sig == SIGABRT);
  ASSERT (sigaction (SIGABRT, NULL, &sa) == 0);
  ASSERT ((sa.sa_flags & SA_SIGINFO) == 0);
  switch (entry_count++)
    {
    case 0:
      ASSERT ((sa.sa_flags & SA_RESETHAND) == 0);
      ASSERT (sa.sa_handler == handler);
      break;
    case 1:
       
#if !(defined __GLIBC__ || defined __UCLIBC__)
      ASSERT (sa.sa_handler == SIG_DFL);
#endif
      break;
    default:
      ASSERT (0);
    }
}

int
main (void)
{
  struct sigaction sa;
  struct sigaction old_sa;
  sa.sa_handler = handler;

  sa.sa_flags = 0;
  ASSERT (sigemptyset (&sa.sa_mask) == 0);
  ASSERT (sigaction (SIGABRT, &sa, NULL) == 0);
  ASSERT (raise (SIGABRT) == 0);

  sa.sa_flags = SA_RESETHAND | SA_NODEFER;
  ASSERT (sigaction (SIGABRT, &sa, &old_sa) == 0);
  ASSERT ((old_sa.sa_flags & MASK_SA_FLAGS) == 0);
  ASSERT (old_sa.sa_handler == handler);
  ASSERT (raise (SIGABRT) == 0);

  sa.sa_handler = SIG_DFL;
  ASSERT (sigaction (SIGABRT, &sa, &old_sa) == 0);
  ASSERT ((old_sa.sa_flags & SA_SIGINFO) == 0);
#if !(defined __GLIBC__ || defined __UCLIBC__)  
  ASSERT (old_sa.sa_handler == SIG_DFL);
#endif

  sa.sa_handler = SIG_IGN;
  ASSERT (sigaction (SIGABRT, &sa, NULL) == 0);
  ASSERT (raise (SIGABRT) == 0);
  ASSERT (sigaction (SIGABRT, NULL, &old_sa) == 0);
  ASSERT (old_sa.sa_handler == SIG_IGN);
  ASSERT (raise (SIGABRT) == 0);

  return 0;
}
