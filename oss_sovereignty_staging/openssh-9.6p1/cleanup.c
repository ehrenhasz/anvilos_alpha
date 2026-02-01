 
 

#include "includes.h"

#include <sys/types.h>

#include <unistd.h>
#include <stdarg.h>

#include "log.h"

 
void
cleanup_exit(int i)
{
	_exit(i);
}
