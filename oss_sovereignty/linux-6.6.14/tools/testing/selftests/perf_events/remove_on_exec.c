
 

#define _GNU_SOURCE

 
#include <sys/types.h>
#include <asm/siginfo.h>
#define __have_siginfo_t 1
#define __have_sigval_t 1
#define __have_sigevent_t 1
#define __siginfo_t_defined
#define __sigval_t_defined
#define __sigevent_t_defined
#define _BITS_SIGINFO_CONSTS_H 1
#define _BITS_SIGEVENT_CONSTS_H 1

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <linux/perf_event.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "../kselftest_harness.h"

static volatile int signal_count;

static struct perf_event_attr make_event_attr(void)
{
	struct perf_event_attr attr = {
		.type		= PERF_TYPE_HARDWARE,
		.size		= sizeof(attr),
		.config		= PERF_COUNT_HW_INSTRUCTIONS,
		.sample_period	= 1000,
		.exclude_kernel = 1,
		.exclude_hv	= 1,
		.disabled	= 1,
		.inherit	= 1,
		 
		.remove_on_exec = 1,
		.sigtrap	= 1,
	};
	return attr;
}

static void sigtrap_handler(int signum, siginfo_t *info, void *ucontext)
{
	if (info->si_code != TRAP_PERF) {
		fprintf(stderr, "%s: unexpected si_code %d\n", __func__, info->si_code);
		return;
	}

	signal_count++;
}

FIXTURE(remove_on_exec)
{
	struct sigaction oldact;
	int fd;
};

FIXTURE_SETUP(remove_on_exec)
{
	struct perf_event_attr attr = make_event_attr();
	struct sigaction action = {};

	signal_count = 0;

	 
	action.sa_flags = SA_SIGINFO | SA_NODEFER;
	action.sa_sigaction = sigtrap_handler;
	sigemptyset(&action.sa_mask);
	ASSERT_EQ(sigaction(SIGTRAP, &action, &self->oldact), 0);

	 
	self->fd = syscall(__NR_perf_event_open, &attr, 0, -1, -1, PERF_FLAG_FD_CLOEXEC);
	ASSERT_NE(self->fd, -1);
}

FIXTURE_TEARDOWN(remove_on_exec)
{
	close(self->fd);
	sigaction(SIGTRAP, &self->oldact, NULL);
}

 
TEST_F(remove_on_exec, fork_only)
{
	int status;
	pid_t pid = fork();

	if (pid == 0) {
		ASSERT_EQ(signal_count, 0);
		ASSERT_EQ(ioctl(self->fd, PERF_EVENT_IOC_ENABLE, 0), 0);
		while (!signal_count);
		_exit(42);
	}

	while (!signal_count);  
	EXPECT_EQ(waitpid(pid, &status, 0), pid);
	EXPECT_EQ(WEXITSTATUS(status), 42);
}

 
TEST_F(remove_on_exec, fork_exec_then_enable)
{
	pid_t pid_exec, pid_only_fork;
	int pipefd[2];
	int tmp;

	 
	pid_only_fork = fork();
	if (pid_only_fork == 0) {
		 
		while (!signal_count);
		_exit(42);
	}

	ASSERT_NE(pipe(pipefd), -1);
	pid_exec = fork();
	if (pid_exec == 0) {
		ASSERT_NE(dup2(pipefd[1], STDOUT_FILENO), -1);
		close(pipefd[0]);
		execl("/proc/self/exe", "exec_child", NULL);
		_exit((perror("exec failed"), 1));
	}
	close(pipefd[1]);

	ASSERT_EQ(waitpid(pid_exec, &tmp, WNOHANG), 0);  
	 
	EXPECT_EQ(read(pipefd[0], &tmp, sizeof(int)), sizeof(int));
	EXPECT_EQ(tmp, 42);
	close(pipefd[0]);
	 
	EXPECT_EQ(ioctl(self->fd, PERF_EVENT_IOC_ENABLE, 0), 0);
	 
	usleep(100000);  
	EXPECT_EQ(waitpid(pid_exec, &tmp, WNOHANG), 0);  
	EXPECT_EQ(kill(pid_exec, SIGKILL), 0);

	 
	tmp = signal_count;
	while (signal_count == tmp);  
	 
	EXPECT_EQ(waitpid(pid_only_fork, &tmp, 0), pid_only_fork);
	EXPECT_EQ(WEXITSTATUS(tmp), 42);
}

 
TEST_F(remove_on_exec, enable_then_fork_exec)
{
	pid_t pid_exec;
	int tmp;

	EXPECT_EQ(ioctl(self->fd, PERF_EVENT_IOC_ENABLE, 0), 0);

	pid_exec = fork();
	if (pid_exec == 0) {
		execl("/proc/self/exe", "exec_child", NULL);
		_exit((perror("exec failed"), 1));
	}

	 
	usleep(100000);  
	EXPECT_EQ(waitpid(pid_exec, &tmp, WNOHANG), 0);  
	EXPECT_EQ(kill(pid_exec, SIGKILL), 0);

	 
	tmp = signal_count;
	while (signal_count == tmp);  
}

TEST_F(remove_on_exec, exec_stress)
{
	pid_t pids[30];
	int i, tmp;

	for (i = 0; i < sizeof(pids) / sizeof(pids[0]); i++) {
		pids[i] = fork();
		if (pids[i] == 0) {
			execl("/proc/self/exe", "exec_child", NULL);
			_exit((perror("exec failed"), 1));
		}

		 
		if (i > 10)
			EXPECT_EQ(ioctl(self->fd, PERF_EVENT_IOC_ENABLE, 0), 0);
	}

	usleep(100000);  

	for (i = 0; i < sizeof(pids) / sizeof(pids[0]); i++) {
		 
		EXPECT_EQ(waitpid(pids[i], &tmp, WNOHANG), 0);
		EXPECT_EQ(kill(pids[i], SIGKILL), 0);
	}

	 
	tmp = signal_count;
	while (signal_count == tmp);
}

 
static void exec_child(void)
{
	struct sigaction action = {};
	const int val = 42;

	 
	action.sa_flags = SA_SIGINFO | SA_NODEFER;
	action.sa_sigaction = sigtrap_handler;
	sigemptyset(&action.sa_mask);
	if (sigaction(SIGTRAP, &action, NULL))
		_exit((perror("sigaction failed"), 1));

	 
	if (write(STDOUT_FILENO, &val, sizeof(int)) == -1)
		_exit((perror("write failed"), 1));

	 
	while (!signal_count);
}

#define main test_main
TEST_HARNESS_MAIN
#undef main
int main(int argc, char *argv[])
{
	if (!strcmp(argv[0], "exec_child")) {
		exec_child();
		return 1;
	}

	return test_main(argc, argv);
}
