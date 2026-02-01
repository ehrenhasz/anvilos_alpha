 

#include "includes.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "openbsd-compat/bsd-signal.h"

#if !defined(HAVE_STRSIGNAL)
char *strsignal(int sig)
{
	static char buf[16];

	(void)snprintf(buf, sizeof(buf), "%d", sig);
	return buf;
}
#endif

