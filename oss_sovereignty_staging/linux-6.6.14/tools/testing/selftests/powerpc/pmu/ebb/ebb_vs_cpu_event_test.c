
 

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ebb.h"


 

static int setup_cpu_event(struct event *event, int cpu)
{
	event_init_named(event, 0x400FA, "PM_RUN_INST_CMPL");

	event->attr.exclude_kernel = 1;
	event->attr.exclude_hv = 1;
	event->attr.exclude_idle = 1;

	SKIP_IF(require_paranoia_below(1));
	FAIL_IF(event_open_with_cpu(event, cpu));
	FAIL_IF(event_enable(event));

	return 0;
}

int ebb_vs_cpu_event(void)
{
	union pipe read_pipe, write_pipe;
	struct event event;
	int cpu, rc;
	pid_t pid;

	SKIP_IF(!ebb_is_supported());

	cpu = bind_to_cpu(BIND_CPU_ANY);
	FAIL_IF(cpu < 0);

	FAIL_IF(pipe(read_pipe.fds) == -1);
	FAIL_IF(pipe(write_pipe.fds) == -1);

	pid = fork();
	if (pid == 0) {
		 
		exit(ebb_child(write_pipe, read_pipe));
	}

	 
	FAIL_IF(sync_with_child(read_pipe, write_pipe));

	 
	rc = setup_cpu_event(&event, cpu);
	if (rc) {
		kill_child_and_wait(pid);
		return rc;
	}

	 
	FAIL_IF(sync_with_child(read_pipe, write_pipe));

	 
	FAIL_IF(wait_for_child(pid));
	FAIL_IF(event_disable(&event));
	FAIL_IF(event_read(&event));

	event_report(&event);

	 
	FAIL_IF(event.result.enabled >= event.result.running);

	return 0;
}

int main(void)
{
	return test_harness(ebb_vs_cpu_event, "ebb_vs_cpu_event");
}
