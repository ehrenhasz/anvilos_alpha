 

 

 

#if !defined (_SIG_H_)
#  define _SIG_H_

#include "stdc.h"

#include <signal.h>		 

#if !defined (SIGABRT) && defined (SIGIOT)
#  define SIGABRT SIGIOT
#endif

#define sighandler void
typedef void SigHandler PARAMS((int));

#define SIGRETURN(n)	return

 
#if !defined (HAVE_POSIX_SIGNALS)
#  define set_signal_handler(sig, handler) (SigHandler *)signal (sig, handler)
#else
extern SigHandler *set_signal_handler PARAMS((int, SigHandler *));	 
#endif  

#if !defined (SIGCHLD) && defined (SIGCLD)
#  define SIGCHLD SIGCLD
#endif

#if !defined (HAVE_POSIX_SIGNALS) && !defined (sigmask)
#  define sigmask(x) (1 << ((x)-1))
#endif  

#if !defined (HAVE_POSIX_SIGNALS)
#  if !defined (SIG_BLOCK)
#    define SIG_BLOCK 2
#    define SIG_SETMASK 3
#  endif  

 

 
#  define sigemptyset(set) (*(set) = 0)

 
#  define sigfillset(set) (*set) = sigmask (NSIG) - 1

 
#  define sigaddset(set, sig) *(set) |= sigmask (sig)

 
#  define sigdelset(set, sig) *(set) &= ~sigmask (sig)

 
#  define sigismember(set, sig) ((*(set) & sigmask (sig)) != 0)

 
#  define sigsuspend(set) sigpause (*(set))
#endif  

 

#define BLOCK_SIGNAL(sig, nvar, ovar) \
do { \
  sigemptyset (&nvar); \
  sigaddset (&nvar, sig); \
  sigemptyset (&ovar); \
  sigprocmask (SIG_BLOCK, &nvar, &ovar); \
} while (0)

#define UNBLOCK_SIGNAL(ovar) sigprocmask (SIG_SETMASK, &ovar, (sigset_t *) NULL)

#if defined (HAVE_POSIX_SIGNALS)
#  define BLOCK_CHILD(nvar, ovar) BLOCK_SIGNAL (SIGCHLD, nvar, ovar)
#  define UNBLOCK_CHILD(ovar) UNBLOCK_SIGNAL(ovar)
#else  
#  define BLOCK_CHILD(nvar, ovar) ovar = sigblock (sigmask (SIGCHLD))
#  define UNBLOCK_CHILD(ovar) sigsetmask (ovar)
#endif  

 
extern volatile sig_atomic_t sigwinch_received;
extern volatile sig_atomic_t sigterm_received;

extern int interrupt_immediately;	 
extern int terminate_immediately;

 
extern sighandler termsig_sighandler PARAMS((int));
extern void termsig_handler PARAMS((int));
extern sighandler sigint_sighandler PARAMS((int));
extern void initialize_signals PARAMS((int));
extern void initialize_terminating_signals PARAMS((void));
extern void reset_terminating_signals PARAMS((void));
extern void top_level_cleanup PARAMS((void));
extern void throw_to_top_level PARAMS((void));
extern void jump_to_top_level PARAMS((int)) __attribute__((__noreturn__));
extern void restore_sigmask PARAMS((void));

extern sighandler sigwinch_sighandler PARAMS((int));
extern void set_sigwinch_handler PARAMS((void));
extern void unset_sigwinch_handler PARAMS((void));

extern sighandler sigterm_sighandler PARAMS((int));

 
extern SigHandler *set_sigint_handler PARAMS((void));
extern SigHandler *trap_to_sighandler PARAMS((int));
extern sighandler trap_handler PARAMS((int));

extern int block_trapped_signals PARAMS((sigset_t *, sigset_t *));
extern int unblock_trapped_signals PARAMS((sigset_t *));
#endif  
