


#ifndef _NOLIBC_SIGNAL_H
#define _NOLIBC_SIGNAL_H

#include "std.h"
#include "arch.h"
#include "types.h"
#include "sys.h"


__attribute__((weak,unused,section(".text.nolibc_raise")))
int raise(int signal)
{
	return sys_kill(sys_getpid(), signal);
}


#include "nolibc.h"

#endif 
