

#ifndef _DEBUG_H
#define _DEBUG_H

#include <stdarg.h>
#include <syslog.h>

extern int libtirpc_debug_level;
extern int  log_stderr;

void    libtirpc_log_dbg(char *format, ...);
void 	libtirpc_set_debug(char *name, int level, int use_stderr);

#define LIBTIRPC_DEBUG(level, msg) \
	do { \
		if (level <= libtirpc_debug_level) \
			libtirpc_log_dbg msg; \
	} while (0)

static inline void 
vlibtirpc_log_dbg(int level, const char *fmt, va_list args)
{
	if (level <= libtirpc_debug_level) {
		if (log_stderr) {
			vfprintf(stderr, fmt, args);
			fprintf(stderr, "\n");
		} else
			vsyslog(LOG_NOTICE, fmt, args);
	}
}
#endif 
