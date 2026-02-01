 

#include "includes.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef HAVE_ERR
void
err(int r, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	fprintf(stderr, "%s: ", strerror(errno));
	vfprintf(stderr, fmt, args);
	fputc('\n', stderr);
	va_end(args);
	exit(r);
}
#endif

#ifndef HAVE_ERRX
void
errx(int r, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	fputc('\n', stderr);
	va_end(args);
	exit(r);
}
#endif

#ifndef HAVE_WARN
void
warn(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	fprintf(stderr, "%s: ", strerror(errno));
	vfprintf(stderr, fmt, args);
	fputc('\n', stderr);
	va_end(args);
}
#endif
