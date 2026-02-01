 

 

#include "includes.h"

#ifndef HAVE_SETPROCTITLE

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef HAVE_SYS_PSTAT_H
#include <sys/pstat.h>
#endif
#include <string.h>

#include <vis.h>

#define SPT_NONE	0	 
#define SPT_PSTAT	1	 
#define SPT_REUSEARGV	2	 

#ifndef SPT_TYPE
# define SPT_TYPE	SPT_NONE
#endif

#ifndef SPT_PADCHAR
# define SPT_PADCHAR	'\0'
#endif

#if SPT_TYPE == SPT_REUSEARGV
static char *argv_start = NULL;
static size_t argv_env_len = 0;
#endif

#endif  

void
compat_init_setproctitle(int argc, char *argv[])
{
#if !defined(HAVE_SETPROCTITLE) && \
    defined(SPT_TYPE) && SPT_TYPE == SPT_REUSEARGV
	extern char **environ;
	char *lastargv = NULL;
	char **envp = environ;
	int i;

	 

	if (argc == 0 || argv[0] == NULL)
		return;

	 
	for (i = 0; envp[i] != NULL; i++)
		;
	if ((environ = calloc(i + 1, sizeof(*environ))) == NULL) {
		environ = envp;	 
		return;
	}

	 
	for (i = 0; i < argc; i++) {
		if (lastargv == NULL || lastargv + 1 == argv[i])
			lastargv = argv[i] + strlen(argv[i]);
	}
	for (i = 0; envp[i] != NULL; i++) {
		if (lastargv + 1 == envp[i])
			lastargv = envp[i] + strlen(envp[i]);
	}

	argv[1] = NULL;
	argv_start = argv[0];
	argv_env_len = lastargv - argv[0] - 1;

	 
	for (i = 0; envp[i] != NULL; i++)
		environ[i] = strdup(envp[i]);
	environ[i] = NULL;
#endif  
}

#ifndef HAVE_SETPROCTITLE
void
setproctitle(const char *fmt, ...)
{
#if SPT_TYPE != SPT_NONE
	va_list ap;
	char buf[1024], ptitle[1024];
	size_t len = 0;
	int r;
	extern char *__progname;
#if SPT_TYPE == SPT_PSTAT
	union pstun pst;
#endif

#if SPT_TYPE == SPT_REUSEARGV
	if (argv_env_len <= 0)
		return;
#endif

	strlcpy(buf, __progname, sizeof(buf));

	r = -1;
	va_start(ap, fmt);
	if (fmt != NULL) {
		len = strlcat(buf, ": ", sizeof(buf));
		if (len < sizeof(buf))
			r = vsnprintf(buf + len, sizeof(buf) - len , fmt, ap);
	}
	va_end(ap);
	if (r == -1 || (size_t)r >= sizeof(buf) - len)
		return;
	strnvis(ptitle, buf, sizeof(ptitle),
	    VIS_CSTYLE|VIS_NL|VIS_TAB|VIS_OCTAL);

#if SPT_TYPE == SPT_PSTAT
	pst.pst_command = ptitle;
	pstat(PSTAT_SETCMD, pst, strlen(ptitle), 0, 0);
#elif SPT_TYPE == SPT_REUSEARGV
 
	len = strlcpy(argv_start, ptitle, argv_env_len);
	for(; len < argv_env_len; len++)
		argv_start[len] = SPT_PADCHAR;
#endif

#endif  
}

#endif  
