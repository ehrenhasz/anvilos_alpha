 
 

#include "includes.h"

#ifdef SANDBOX_PLEDGE

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>

#include "log.h"
#include "ssh-sandbox.h"
#include "xmalloc.h"

struct ssh_sandbox {
	pid_t child_pid;
};

struct ssh_sandbox *
ssh_sandbox_init(struct monitor *m)
{
	struct ssh_sandbox *box;

	debug3_f("preparing pledge sandbox");
	box = xcalloc(1, sizeof(*box));
	box->child_pid = 0;

	return box;
}

void
ssh_sandbox_child(struct ssh_sandbox *box)
{
	if (pledge("stdio", NULL) == -1)
		fatal_f("pledge()");
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
