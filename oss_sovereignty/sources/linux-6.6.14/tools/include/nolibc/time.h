


#ifndef _NOLIBC_TIME_H
#define _NOLIBC_TIME_H

#include "std.h"
#include "arch.h"
#include "types.h"
#include "sys.h"

static __attribute__((unused))
time_t time(time_t *tptr)
{
	struct timeval tv;

	
	sys_gettimeofday(&tv, NULL);

	if (tptr)
		*tptr = tv.tv_sec;
	return tv.tv_sec;
}


#include "nolibc.h"

#endif 
