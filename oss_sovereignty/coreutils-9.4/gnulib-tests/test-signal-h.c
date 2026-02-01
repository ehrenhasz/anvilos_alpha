 

#include <config.h>

#include <signal.h>

 
struct
{
  size_t a;
  uid_t b;
  volatile sig_atomic_t c;
  sigset_t d;
  pid_t e;
#if 0
   
  pthread_t f;
  struct timespec g;
#endif
} s;

 
int nsig = NSIG;

int
main (void)
{
  switch (0)
    {
       
    case 0:
    case SIGABRT:
    case SIGFPE:
    case SIGILL:
    case SIGINT:
    case SIGSEGV:
    case SIGTERM:
       
#if GNULIB_SIGPIPE || defined SIGPIPE
    case SIGPIPE:
#endif
       
#ifdef SIGALRM
    case SIGALRM:
#endif
       
#if defined SIGBUS && SIGBUS != SIGSEGV
    case SIGBUS:
#endif
#ifdef SIGCHLD
    case SIGCHLD:
#endif
#ifdef SIGCONT
    case SIGCONT:
#endif
#ifdef SIGHUP
    case SIGHUP:
#endif
#ifdef SIGKILL
    case SIGKILL:
#endif
#ifdef SIGQUIT
    case SIGQUIT:
#endif
#ifdef SIGSTOP
    case SIGSTOP:
#endif
#ifdef SIGTSTP
    case SIGTSTP:
#endif
#ifdef SIGTTIN
    case SIGTTIN:
#endif
#ifdef SIGTTOU
    case SIGTTOU:
#endif
#ifdef SIGUSR1
    case SIGUSR1:
#endif
#ifdef SIGUSR2
    case SIGUSR2:
#endif
#ifdef SIGSYS
    case SIGSYS:
#endif
#ifdef SIGTRAP
    case SIGTRAP:
#endif
#ifdef SIGURG
    case SIGURG:
#endif
#ifdef SIGVTALRM
    case SIGVTALRM:
#endif
#ifdef SIGXCPU
    case SIGXCPU:
#endif
#ifdef SIGXFSZ
    case SIGXFSZ:
#endif
       
#if 0
# ifdef SIGRTMIN
    case SIGRTMIN:
# endif
# ifdef SIGRTMAX
    case SIGRTMAX:
# endif
#endif
      ;
    }
  return s.a + s.b + s.c + s.e;
}
