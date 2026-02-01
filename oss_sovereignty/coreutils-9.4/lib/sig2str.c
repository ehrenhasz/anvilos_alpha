 

#include <config.h>

#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "sig2str.h"

#ifndef SIGRTMIN
# define SIGRTMIN 0
# undef SIGRTMAX
#endif
#ifndef SIGRTMAX
# define SIGRTMAX (SIGRTMIN - 1)
#endif

#define NUMNAME(name) { SIG##name, #name }

 
static struct numname { int num; char const name[8]; } numname_table[] =
  {
     
#ifdef SIGHUP
    NUMNAME (HUP),
#endif
#ifdef SIGINT
    NUMNAME (INT),
#endif
#ifdef SIGQUIT
    NUMNAME (QUIT),
#endif
#ifdef SIGILL
    NUMNAME (ILL),
#endif
#ifdef SIGTRAP
    NUMNAME (TRAP),
#endif
#ifdef SIGABRT
    NUMNAME (ABRT),
#endif
#ifdef SIGFPE
    NUMNAME (FPE),
#endif
#ifdef SIGKILL
    NUMNAME (KILL),
#endif
#ifdef SIGSEGV
    NUMNAME (SEGV),
#endif
     
#ifdef SIGBUS
    NUMNAME (BUS),
#endif
#ifdef SIGPIPE
    NUMNAME (PIPE),
#endif
#ifdef SIGALRM
    NUMNAME (ALRM),
#endif
#ifdef SIGTERM
    NUMNAME (TERM),
#endif
#ifdef SIGUSR1
    NUMNAME (USR1),
#endif
#ifdef SIGUSR2
    NUMNAME (USR2),
#endif
#ifdef SIGCHLD
    NUMNAME (CHLD),
#endif
#ifdef SIGURG
    NUMNAME (URG),
#endif
#ifdef SIGSTOP
    NUMNAME (STOP),
#endif
#ifdef SIGTSTP
    NUMNAME (TSTP),
#endif
#ifdef SIGCONT
    NUMNAME (CONT),
#endif
#ifdef SIGTTIN
    NUMNAME (TTIN),
#endif
#ifdef SIGTTOU
    NUMNAME (TTOU),
#endif

     
#ifdef SIGSYS
    NUMNAME (SYS),
#endif
#ifdef SIGPOLL
    NUMNAME (POLL),
#endif
#ifdef SIGVTALRM
    NUMNAME (VTALRM),
#endif
#ifdef SIGPROF
    NUMNAME (PROF),
#endif
#ifdef SIGXCPU
    NUMNAME (XCPU),
#endif
#ifdef SIGXFSZ
    NUMNAME (XFSZ),
#endif

     
#ifdef SIGIOT
    NUMNAME (IOT),       
#endif
#ifdef SIGEMT
    NUMNAME (EMT),
#endif

     
#ifdef SIGPHONE
    NUMNAME (PHONE),
#endif
#ifdef SIGWIND
    NUMNAME (WIND),
#endif

     
#ifdef SIGCLD
    NUMNAME (CLD),
#endif
#ifdef SIGPWR
    NUMNAME (PWR),
#endif

     
#ifdef SIGCANCEL
    NUMNAME (CANCEL),
#endif
#ifdef SIGLWP
    NUMNAME (LWP),
#endif
#ifdef SIGWAITING
    NUMNAME (WAITING),
#endif
#ifdef SIGFREEZE
    NUMNAME (FREEZE),
#endif
#ifdef SIGTHAW
    NUMNAME (THAW),
#endif
#ifdef SIGLOST
    NUMNAME (LOST),
#endif
#ifdef SIGWINCH
    NUMNAME (WINCH),
#endif

     
#ifdef SIGINFO
    NUMNAME (INFO),
#endif
#ifdef SIGIO
    NUMNAME (IO),
#endif
#ifdef SIGSTKFLT
    NUMNAME (STKFLT),
#endif

     
#ifdef SIGCPUFAIL
    NUMNAME (CPUFAIL),
#endif

     
#ifdef SIGDANGER
    NUMNAME (DANGER),
#endif
#ifdef SIGGRANT
    NUMNAME (GRANT),
#endif
#ifdef SIGMIGRATE
    NUMNAME (MIGRATE),
#endif
#ifdef SIGMSG
    NUMNAME (MSG),
#endif
#ifdef SIGPRE
    NUMNAME (PRE),
#endif
#ifdef SIGRETRACT
    NUMNAME (RETRACT),
#endif
#ifdef SIGSAK
    NUMNAME (SAK),
#endif
#ifdef SIGSOUND
    NUMNAME (SOUND),
#endif

     
#ifdef SIGALRM1
    NUMNAME (ALRM1),     
#endif
#ifdef SIGKAP
    NUMNAME (KAP),       
#endif
#ifdef SIGVIRT
    NUMNAME (VIRT),      
#endif
#ifdef SIGWINDOW
    NUMNAME (WINDOW),    
#endif

     
#ifdef SIGTHR
    NUMNAME (THR),
#endif

     
#ifdef SIGKILLTHR
    NUMNAME (KILLTHR),
#endif

     
#ifdef SIGDIL
    NUMNAME (DIL),
#endif

     
#ifdef SIGBREAK
    NUMNAME (BREAK),
#endif

     
    { 0, "EXIT" }
  };

#define NUMNAME_ENTRIES (sizeof numname_table / sizeof numname_table[0])

 
#define ISDIGIT(c) ((unsigned int) (c) - '0' <= 9)

 

static int
str2signum (char const *signame)
{
  if (ISDIGIT (*signame))
    {
      char *endp;
      long int n = strtol (signame, &endp, 10);
      if (! *endp && n <= SIGNUM_BOUND)
        return n;
    }
  else
    {
      unsigned int i;
      for (i = 0; i < NUMNAME_ENTRIES; i++)
        if (strcmp (numname_table[i].name, signame) == 0)
          return numname_table[i].num;

      {
        char *endp;
        int rtmin = SIGRTMIN;
        int rtmax = SIGRTMAX;

        if (0 < rtmin && strncmp (signame, "RTMIN", 5) == 0)
          {
            long int n = strtol (signame + 5, &endp, 10);
            if (! *endp && 0 <= n && n <= rtmax - rtmin)
              return rtmin + n;
          }
        else if (0 < rtmax && strncmp (signame, "RTMAX", 5) == 0)
          {
            long int n = strtol (signame + 5, &endp, 10);
            if (! *endp && rtmin - rtmax <= n && n <= 0)
              return rtmax + n;
          }
      }
    }

  return -1;
}

 

int
str2sig (char const *signame, int *signum)
{
  *signum = str2signum (signame);
  return *signum < 0 ? -1 : 0;
}

 

int
sig2str (int signum, char *signame)
{
  unsigned int i;
  for (i = 0; i < NUMNAME_ENTRIES; i++)
    if (numname_table[i].num == signum)
      {
        strcpy (signame, numname_table[i].name);
        return 0;
      }

  {
    int rtmin = SIGRTMIN;
    int rtmax = SIGRTMAX;
    int base, delta;

    if (! (rtmin <= signum && signum <= rtmax))
      return -1;

    if (signum <= rtmin + (rtmax - rtmin) / 2)
      {
        strcpy (signame, "RTMIN");
        base = rtmin;
      }
    else
      {
        strcpy (signame, "RTMAX");
        base = rtmax;
      }

    delta = signum - base;
    if (delta != 0)
      sprintf (signame + 5, "%+d", delta);
    return 0;
  }
}
