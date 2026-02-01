 

#include "includes.h"

#ifndef HAVE_WAITPID
#include <errno.h>
#include <sys/wait.h>
#include "bsd-waitpid.h"

pid_t
waitpid(int pid, int *stat_loc, int options)
{
	union wait statusp;
	pid_t wait_pid;

	if (pid <= 0) {
		if (pid != -1) {
			errno = EINVAL;
			return (-1);
		}
		 
		pid = 0;
	}
	wait_pid = wait4(pid, &statusp, options, NULL);
	if (stat_loc)
		*stat_loc = (int) statusp.w_status;

	return (wait_pid);
}

#endif  
