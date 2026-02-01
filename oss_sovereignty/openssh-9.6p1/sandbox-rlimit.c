 
 

#include "includes.h"

#ifdef SANDBOX_RLIMIT

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "ssh-sandbox.h"
#include "xmalloc.h"

 

struct ssh_sandbox {
	pid_t child_pid;
};

struct ssh_sandbox *
ssh_sandbox_init(struct monitor *monitor)
{
	struct ssh_sandbox *box;

	 
	debug3_f("preparing rlimit sandbox");
	box = xcalloc(1, sizeof(*box));
	box->child_pid = 0;

	return box;
}

void
ssh_sandbox_child(struct ssh_sandbox *box)
{
	struct rlimit rl_zero;

	rl_zero.rlim_cur = rl_zero.rlim_max = 0;

#ifndef SANDBOX_SKIP_RLIMIT_FSIZE
	if (setrlimit(RLIMIT_FSIZE, &rl_zero) == -1)
		fatal_f("setrlimit(RLIMIT_FSIZE, { 0, 0 }): %s",
			strerror(errno));
#endif
#ifndef SANDBOX_SKIP_RLIMIT_NOFILE
	if (setrlimit(RLIMIT_NOFILE, &rl_zero) == -1)
		fatal_f("setrlimit(RLIMIT_NOFILE, { 0, 0 }): %s",
			strerror(errno));
#endif
#ifdef HAVE_RLIMIT_NPROC
	if (setrlimit(RLIMIT_NPROC, &rl_zero) == -1)
		fatal_f("setrlimit(RLIMIT_NPROC, { 0, 0 }): %s",
			strerror(errno));
#endif
}

void
ssh_sandbox_parent_finish(struct ssh_sandbox *box)
{
	free(box);
	debug3_f("finished");
}

void
ssh_sandbox_parent_preauth(struct ssh_sandbox *box, pid_t child_pid)
{
	box->child_pid = child_pid;
}

#endif  
