
 

#define _GNU_SOURCE
#include <sched.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <tracefs.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#include "utils.h"
#include "timerlat_u.h"

 
static int timerlat_u_main(int cpu, struct timerlat_u_params *params)
{
	struct sched_param sp = { .sched_priority = 95 };
	char buffer[1024];
	int timerlat_fd;
	cpu_set_t set;
	int retval;

	 
	CPU_ZERO(&set);
	CPU_SET(cpu, &set);

	retval = sched_setaffinity(gettid(), sizeof(set), &set);
	if (retval == -1) {
		debug_msg("Error setting user thread affinity %d, is the CPU online?\n", cpu);
		exit(1);
	}

	if (!params->sched_param) {
		retval = sched_setscheduler(0, SCHED_FIFO, &sp);
		if (retval < 0) {
			err_msg("Error setting timerlat u default priority: %s\n", strerror(errno));
			exit(1);
		}
	} else {
		retval = __set_sched_attr(getpid(), params->sched_param);
		if (retval) {
			 
			exit(0);
		}
	}

	if (params->cgroup_name) {
		retval = set_pid_cgroup(gettid(), params->cgroup_name);
		if (!retval) {
			err_msg("Error setting timerlat u cgroup pid\n");
			pthread_exit(&retval);
		}
	}

	 
	snprintf(buffer, sizeof(buffer), "osnoise/per_cpu/cpu%d/timerlat_fd", cpu);

	timerlat_fd = tracefs_instance_file_open(NULL, buffer, O_RDONLY);
	if (timerlat_fd < 0) {
		err_msg("Error opening %s:%s\n", buffer, strerror(errno));
		exit(1);
	}

	debug_msg("User-space timerlat pid %d on cpu %d\n", gettid(), cpu);

	 
	while (true) {
		retval = read(timerlat_fd, buffer, 1024);
		if (retval < 0)
			break;
	}

	close(timerlat_fd);

	debug_msg("Leaving timerlat pid %d on cpu %d\n", gettid(), cpu);
	exit(0);
}

 
static int timerlat_u_send_kill(pid_t *procs, int nr_cpus)
{
	int killed = 0;
	int i, retval;

	for (i = 0; i < nr_cpus; i++) {
		if (!procs[i])
			continue;
		retval = kill(procs[i], SIGKILL);
		if (!retval)
			killed++;
		else
			err_msg("Error killing child process %d\n", procs[i]);
	}

	return killed;
}

 
void *timerlat_u_dispatcher(void *data)
{
	int nr_cpus = sysconf(_SC_NPROCESSORS_CONF);
	struct timerlat_u_params *params = data;
	char proc_name[128];
	int procs_count = 0;
	int retval = 1;
	pid_t *procs;
	int wstatus;
	pid_t pid;
	int i;

	debug_msg("Dispatching timerlat u procs\n");

	procs = calloc(nr_cpus, sizeof(pid_t));
	if (!procs)
		pthread_exit(&retval);

	for (i = 0; i < nr_cpus; i++) {
		if (params->set && !CPU_ISSET(i, params->set))
			continue;

		pid = fork();

		 
		if (!pid) {

			 
			snprintf(proc_name, sizeof(proc_name), "timerlatu/%d", i);
			pthread_setname_np(pthread_self(), proc_name);
			prctl(PR_SET_NAME, (unsigned long)proc_name, 0, 0, 0);

			timerlat_u_main(i, params);
			 
			pthread_exit(&retval);
		}

		 
		if (pid == -1) {
			timerlat_u_send_kill(procs, nr_cpus);
			debug_msg("Failed to create child processes");
			pthread_exit(&retval);
		}

		procs_count++;
		procs[i] = pid;
	}

	while (params->should_run) {
		 
		pid = waitpid(-1, &wstatus, WNOHANG);
		if (pid != 0) {
			for (i = 0; i < nr_cpus; i++) {
				if (procs[i] == pid) {
					procs[i] = 0;
					procs_count--;
				}
			}

			if (!procs_count)
				break;
		}

		sleep(1);
	}

	timerlat_u_send_kill(procs, nr_cpus);

	while (procs_count) {
		pid = waitpid(-1, &wstatus, 0);
		if (pid == -1) {
			err_msg("Failed to monitor child processes");
			pthread_exit(&retval);
		}
		for (i = 0; i < nr_cpus; i++) {
			if (procs[i] == pid) {
				procs[i] = 0;
				procs_count--;
			}
		}
	}

	params->stopped_running = 1;

	free(procs);
	retval = 0;
	pthread_exit(&retval);

}
