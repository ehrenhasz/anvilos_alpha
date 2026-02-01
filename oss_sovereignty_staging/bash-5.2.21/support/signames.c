 

 

#include <config.h>

#include <stdio.h>

#include <sys/types.h>
#include <signal.h>

#if defined (HAVE_STDLIB_H)
#  include <stdlib.h>
#else
#  include "ansi_stdlib.h"
#endif  

#if !defined (NSIG)
#  define NSIG 64
#endif

 
#define LASTSIG NSIG+2

char *signal_names[2 * (LASTSIG)];

#define signal_names_size (sizeof(signal_names)/sizeof(signal_names[0]))

 
#if defined (SIGRTMAX) && defined (UNUSABLE_RT_SIGNALS)
#  undef SIGRTMAX
#  undef SIGRTMIN
#endif

#if defined (SIGRTMAX) || defined (SIGRTMIN)
#  define RTLEN 14
#  define RTLIM 256
#endif

#if defined (BUILDTOOL)
extern char *progname;
#endif

void
initialize_signames ()
{
  register int i;
#if defined (SIGRTMAX) || defined (SIGRTMIN)
  int rtmin, rtmax, rtcnt;
#endif

  for (i = 1; i < signal_names_size; i++)
    signal_names[i] = (char *)NULL;

   
  signal_names[0] = "EXIT";

   

   

   

#if defined (SIGRTMIN)
  rtmin = SIGRTMIN;
  signal_names[rtmin] = "SIGRTMIN";
#endif

#if defined (SIGRTMAX)
  rtmax = SIGRTMAX;
  signal_names[rtmax] = "SIGRTMAX";
#endif

#if defined (SIGRTMAX) && defined (SIGRTMIN)
  if (rtmax > rtmin)
    {
      rtcnt = (rtmax - rtmin - 1) / 2;
       
      if (rtcnt >= RTLIM/2)
	{
	  rtcnt = RTLIM/2-1;
#ifdef BUILDTOOL
	  fprintf(stderr, "%s: error: more than %d real time signals, fix `%s'\n",
		  progname, RTLIM, progname);
#endif
	}

      for (i = 1; i <= rtcnt; i++)
	{
	  signal_names[rtmin+i] = (char *)malloc(RTLEN);
	  if (signal_names[rtmin+i])
	    sprintf (signal_names[rtmin+i], "SIGRTMIN+%d", i);
	  signal_names[rtmax-i] = (char *)malloc(RTLEN);
	  if (signal_names[rtmax-i])
	    sprintf (signal_names[rtmax-i], "SIGRTMAX-%d", i);
	}

      if (rtcnt < RTLIM/2-1 && rtcnt != (rtmax-rtmin)/2)
	{
	   
	  signal_names[rtmin+rtcnt+1] = (char *)malloc(RTLEN);
	  if (signal_names[rtmin+rtcnt+1])
	    sprintf (signal_names[rtmin+rtcnt+1], "SIGRTMIN+%d", rtcnt+1);
	}
    }
#endif  

#if defined (SIGLOST)	 
  signal_names[SIGLOST] = "SIGLOST";
#endif

 
#if defined (SIGMSG)	 
  signal_names[SIGMSG] = "SIGMSG";
#endif

#if defined (SIGDANGER)	 
  signal_names[SIGDANGER] = "SIGDANGER";
#endif

#if defined (SIGMIGRATE)  
  signal_names[SIGMIGRATE] = "SIGMIGRATE";
#endif

#if defined (SIGPRE)	 
  signal_names[SIGPRE] = "SIGPRE";
#endif

#if defined (SIGPHONE)	 
  signal_names[SIGPHONE] = "SIGPHONE";
#endif

#if defined (SIGVIRT)	 
  signal_names[SIGVIRT] = "SIGVIRT";
#endif

#if defined (SIGTINT)	 
  signal_names[SIGTINT] = "SIGTINT";
#endif

#if defined (SIGALRM1)	 
  signal_names[SIGALRM1] = "SIGALRM1";
#endif

#if defined (SIGWAITING)	 
  signal_names[SIGWAITING] = "SIGWAITING";
#endif

#if defined (SIGGRANT)	 
  signal_names[SIGGRANT] = "SIGGRANT";
#endif

#if defined (SIGKAP)	 
  signal_names[SIGKAP] = "SIGKAP";
#endif

#if defined (SIGRETRACT)  
  signal_names[SIGRETRACT] = "SIGRETRACT";
#endif

#if defined (SIGSOUND)	 
  signal_names[SIGSOUND] = "SIGSOUND";
#endif

#if defined (SIGSAK)	 
  signal_names[SIGSAK] = "SIGSAK";
#endif

#if defined (SIGCPUFAIL)	 
  signal_names[SIGCPUFAIL] = "SIGCPUFAIL";
#endif

#if defined (SIGAIO)	 
  signal_names[SIGAIO] = "SIGAIO";
#endif

#if defined (SIGLAB)	 
  signal_names[SIGLAB] = "SIGLAB";
#endif

 
#if defined (SIGLWP)	 
  signal_names[SIGLWP] = "SIGLWP";
#endif

#if defined (SIGFREEZE)	 
  signal_names[SIGFREEZE] = "SIGFREEZE";
#endif

#if defined (SIGTHAW)	 
  signal_names[SIGTHAW] = "SIGTHAW";
#endif

#if defined (SIGCANCEL)	 
  signal_names[SIGCANCEL] = "SIGCANCEL";
#endif

#if defined (SIGXRES)	 
  signal_names[SIGXRES] = "SIGXRES";
#endif

#if defined (SIGJVM1)	 
  signal_names[SIGJVM1] = "SIGJVM1";
#endif

#if defined (SIGJVM2)	 
  signal_names[SIGJVM2] = "SIGJVM2";
#endif

#if defined (SIGDGTIMER1)
  signal_names[SIGDGTIMER1] = "SIGDGTIMER1";
#endif

#if defined (SIGDGTIMER2)
  signal_names[SIGDGTIMER2] = "SIGDGTIMER2";
#endif

#if defined (SIGDGTIMER3)
  signal_names[SIGDGTIMER3] = "SIGDGTIMER3";
#endif

#if defined (SIGDGTIMER4)
  signal_names[SIGDGTIMER4] = "SIGDGTIMER4";
#endif

#if defined (SIGDGNOTIFY)
  signal_names[SIGDGNOTIFY] = "SIGDGNOTIFY";
#endif

 
#if defined (SIGAPOLLO)
  signal_names[SIGAPOLLO] = "SIGAPOLLO";
#endif

 
#if defined (SIGDIL)	 
  signal_names[SIGDIL] = "SIGDIL";
#endif

 
#if defined (SIGCLD)	 
  signal_names[SIGCLD] = "SIGCLD";
#endif

#if defined (SIGPWR)	 
  signal_names[SIGPWR] = "SIGPWR";
#endif

#if defined (SIGPOLL)	 
  signal_names[SIGPOLL] = "SIGPOLL";
#endif

 
#if defined (SIGWINDOW)
  signal_names[SIGWINDOW] = "SIGWINDOW";
#endif

 
#if defined (SIGSTKFLT)
  signal_names[SIGSTKFLT] = "SIGSTKFLT";
#endif

 
#if defined (SIGTHR)	 
  signal_names[SIGTHR] = "SIGTHR";
#endif

 
#if defined (SIGHUP)	 
  signal_names[SIGHUP] = "SIGHUP";
#endif

#if defined (SIGINT)	 
  signal_names[SIGINT] = "SIGINT";
#endif

#if defined (SIGQUIT)	 
  signal_names[SIGQUIT] = "SIGQUIT";
#endif

#if defined (SIGILL)	 
  signal_names[SIGILL] = "SIGILL";
#endif

#if defined (SIGTRAP)	 
  signal_names[SIGTRAP] = "SIGTRAP";
#endif

#if defined (SIGIOT)	 
  signal_names[SIGIOT] = "SIGIOT";
#endif

#if defined (SIGABRT)	 
  signal_names[SIGABRT] = "SIGABRT";
#endif

#if defined (SIGEMT)	 
  signal_names[SIGEMT] = "SIGEMT";
#endif

#if defined (SIGFPE)	 
  signal_names[SIGFPE] = "SIGFPE";
#endif

#if defined (SIGKILL)	 
  signal_names[SIGKILL] = "SIGKILL";
#endif

#if defined (SIGBUS)	 
  signal_names[SIGBUS] = "SIGBUS";
#endif

#if defined (SIGSEGV)	 
  signal_names[SIGSEGV] = "SIGSEGV";
#endif

#if defined (SIGSYS)	 
  signal_names[SIGSYS] = "SIGSYS";
#endif

#if defined (SIGPIPE)	 
  signal_names[SIGPIPE] = "SIGPIPE";
#endif

#if defined (SIGALRM)	 
  signal_names[SIGALRM] = "SIGALRM";
#endif

#if defined (SIGTERM)	 
  signal_names[SIGTERM] = "SIGTERM";
#endif

#if defined (SIGURG)	 
  signal_names[SIGURG] = "SIGURG";
#endif

#if defined (SIGSTOP)	 
  signal_names[SIGSTOP] = "SIGSTOP";
#endif

#if defined (SIGTSTP)	 
  signal_names[SIGTSTP] = "SIGTSTP";
#endif

#if defined (SIGCONT)	 
  signal_names[SIGCONT] = "SIGCONT";
#endif

#if defined (SIGCHLD)	 
  signal_names[SIGCHLD] = "SIGCHLD";
#endif

#if defined (SIGTTIN)	 
  signal_names[SIGTTIN] = "SIGTTIN";
#endif

#if defined (SIGTTOU)	 
  signal_names[SIGTTOU] = "SIGTTOU";
#endif

#if defined (SIGIO)	 
  signal_names[SIGIO] = "SIGIO";
#endif

#if defined (SIGXCPU)	 
  signal_names[SIGXCPU] = "SIGXCPU";
#endif

#if defined (SIGXFSZ)	 
  signal_names[SIGXFSZ] = "SIGXFSZ";
#endif

#if defined (SIGVTALRM)	 
  signal_names[SIGVTALRM] = "SIGVTALRM";
#endif

#if defined (SIGPROF)	 
  signal_names[SIGPROF] = "SIGPROF";
#endif

#if defined (SIGWINCH)	 
  signal_names[SIGWINCH] = "SIGWINCH";
#endif

 
#if defined (SIGINFO) && !defined (_SEQUENT_)	 
  signal_names[SIGINFO] = "SIGINFO";
#endif

#if defined (SIGUSR1)	 
  signal_names[SIGUSR1] = "SIGUSR1";
#endif

#if defined (SIGUSR2)	 
  signal_names[SIGUSR2] = "SIGUSR2";
#endif

#if defined (SIGKILLTHR)	 
  signal_names[SIGKILLTHR] = "SIGKILLTHR";
#endif

  for (i = 0; i < NSIG; i++)
    if (signal_names[i] == (char *)NULL)
      {
	signal_names[i] = (char *)malloc (18);
	if (signal_names[i])
	  sprintf (signal_names[i], "SIGJUNK(%d)", i);
      }

  signal_names[NSIG] = "DEBUG";
  signal_names[NSIG+1] = "ERR";
  signal_names[NSIG+2] = "RETURN";
}
