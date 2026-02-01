 
 

#include "includes.h"

#ifdef SANDBOX_NULL

#include <sys/types.h>

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
	int junk;
};

struct ssh_sandbox *
ssh_sandbox_init(struct monitor *monitor)
{
	struct ssh_sandbox *box;

	 
	box = xcalloc(1, sizeof(*box));
	return box;
}

void
ssh_sandbox_child(struct ssh_sandbox *box)
{
	 
}

void
ssh_sandbox_parent_finish(struct ssh_sandbox *box)
{
	free(box);
}

void
ssh_sandbox_parent_preauth(struct ssh_sandbox *box, pid_t child_pid)
{
	 
}

#endif  
