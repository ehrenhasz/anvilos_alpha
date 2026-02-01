#include <linux/signal.h>

#define SIGUNKNOWN 0
#define MAXMAPPED_SIG 35
#define MAXMAPPED_SIGNAME (MAXMAPPED_SIG + 1)
#define SIGRT_BASE 128

 
static const int sig_map[MAXMAPPED_SIG] = {
	[0] = MAXMAPPED_SIG,	 
	[SIGHUP] = 1,
	[SIGINT] = 2,
	[SIGQUIT] = 3,
	[SIGILL] = 4,
	[SIGTRAP] = 5,		 
	[SIGABRT] = 6,		 
	[SIGBUS] = 7,		 
	[SIGFPE] = 8,
	[SIGKILL] = 9,
	[SIGUSR1] = 10,		 
	[SIGSEGV] = 11,
	[SIGUSR2] = 12,		 
	[SIGPIPE] = 13,
	[SIGALRM] = 14,
	[SIGTERM] = 15,
#ifdef SIGSTKFLT
	[SIGSTKFLT] = 16,	 
#endif
	[SIGCHLD] = 17,		 
	[SIGCONT] = 18,		 
	[SIGSTOP] = 19,		 
	[SIGTSTP] = 20,		 
	[SIGTTIN] = 21,		 
	[SIGTTOU] = 22,		 
	[SIGURG] = 23,		 
	[SIGXCPU] = 24,		 
	[SIGXFSZ] = 25,		 
	[SIGVTALRM] = 26,	 
	[SIGPROF] = 27,		 
	[SIGWINCH] = 28,	 
	[SIGIO] = 29,		 
	[SIGPWR] = 30,		 
#ifdef SIGSYS
	[SIGSYS] = 31,		 
#endif
#ifdef SIGEMT
	[SIGEMT] = 32,		 
#endif
#if defined(SIGLOST) && SIGPWR != SIGLOST		 
	[SIGLOST] = 33,		 
#endif
#if defined(SIGUNUSED) && \
    defined(SIGLOST) && defined(SIGSYS) && SIGLOST != SIGSYS
	[SIGUNUSED] = 34,	 
#endif
};

 
static const char *const sig_names[MAXMAPPED_SIGNAME] = {
	"unknown",
	"hup",
	"int",
	"quit",
	"ill",
	"trap",
	"abrt",
	"bus",
	"fpe",
	"kill",
	"usr1",
	"segv",
	"usr2",
	"pipe",
	"alrm",
	"term",
	"stkflt",
	"chld",
	"cont",
	"stop",
	"stp",
	"ttin",
	"ttou",
	"urg",
	"xcpu",
	"xfsz",
	"vtalrm",
	"prof",
	"winch",
	"io",
	"pwr",
	"sys",
	"emt",
	"lost",
	"unused",

	"exists",	 
};

