 
 

#include "includes.h"

#include <sys/types.h>

#include <stdarg.h>

#include "log.h"

 

void
sshfatal(const char *file, const char *func, int line, int showfunc,
    LogLevel level, const char *suffix, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	sshlogv(file, func, line, showfunc, level, suffix, fmt, args);
	va_end(args);
	cleanup_exit(255);
}
