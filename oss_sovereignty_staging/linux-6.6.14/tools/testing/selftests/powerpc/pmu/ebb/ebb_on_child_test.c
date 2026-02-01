
 

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ebb.h"


 

static int victim_child(union pipe read_pipe, union pipe write_pipe)
{
	int i;

	FAIL_IF(wait_for_parent(read_pipe));
	FAIL_IF(notify_parent(write_pipe));

	 

	FAIL_IF(wait_for_parent(read_pipe));
	FAIL_IF(notify_parent(write_pipe));

	 
	write_pmc1();

	 
	for (i = 0; i < 1000000; i++) ;

	return 0;
}

int ebb_on_child(void)
{
	union pipe read_pipe, write_pipe;
	struct event event;
	pid_t pid;

	SKIP_IF(!ebb_is_supported());

	FAIL_IF(pipe(read_pipe.fds) == -1);
	FAIL_IF(pipe(write_pipe.fds) == -1);

	pid = fork();
	if (pid == 0) {
		 
		exit(victim_child(write_pipe, read_pipe));
	}

	FAIL_IF(sync_with_child(read_pipe, write_pipe));

	 

	event_init_named(&event, 0x1001e, "cycles");
	event_leader_ebb_init(&event);

	event.attr.exclude_kernel = 1;
	event.attr.exclude_hv = 1;
	event.attr.exclude_idle = 1;

	FAIL_IF(event_open_with_pid(&event, pid));
	FAIL_IF(ebb_event_enable(&event));

	FAIL_IF(sync_with_child(read_pipe, write_pipe));

	 
	FAIL_IF(wait_for_child(pid));

	event_close(&event);

	return 0;
}

int main(void)
{
	return test_harness(ebb_on_child, "ebb_on_child");
}
