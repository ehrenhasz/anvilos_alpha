 

#include "includes.h"

#include <sys/types.h>
#ifdef HAVE_SYS_PROCCTL_H
#include <sys/procctl.h>
#endif
#if defined(HAVE_SYS_PRCTL_H)
#include <sys/prctl.h>	 
#endif
#ifdef HAVE_SYS_PTRACE_H
#include <sys/ptrace.h>
#endif
#ifdef HAVE_PRIV_H
#include <priv.h>  
#endif
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "log.h"

void
platform_disable_tracing(int strict)
{
#if defined(HAVE_PROCCTL) && defined(PROC_TRACE_CTL)
	 
	int disable_trace = PROC_TRACE_CTL_DISABLE;

	 
	if (procctl(P_PID, 0, PROC_TRACE_CTL, &disable_trace) == 0)
		return;
	if (procctl(P_PID, getpid(), PROC_TRACE_CTL, &disable_trace) == 0)
		return;
	if (strict)
		fatal("unable to make the process untraceable: %s",
		    strerror(errno));
#endif
#if defined(HAVE_PRCTL) && defined(PR_SET_DUMPABLE)
	 
	if (prctl(PR_SET_DUMPABLE, 0) != 0 && strict)
		fatal("unable to make the process undumpable: %s",
		    strerror(errno));
#endif
#if defined(HAVE_SETPFLAGS) && defined(__PROC_PROTECT)
	 
	if (setpflags(__PROC_PROTECT, 1) != 0 && strict)
		fatal("unable to make the process untraceable: %s",
		    strerror(errno));
#endif
#ifdef PT_DENY_ATTACH
	 
	if (ptrace(PT_DENY_ATTACH, 0, 0, 0) == -1 && strict)
		fatal("unable to set PT_DENY_ATTACH: %s", strerror(errno));
#endif
}
