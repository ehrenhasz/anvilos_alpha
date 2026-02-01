
 
#define _GNU_SOURCE
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <asm-generic/unistd.h>
#include "vm_util.h"
#include "../kselftest.h"

#define MB(x) (x << 20)
#define MAX_SIZE_MB 1024

static int alloc_noexit(unsigned long nr_pages, int pipefd)
{
	int ppid = getppid();
	int timeout = 10;  
	unsigned long i;
	char *buf;

	buf = (char *)mmap(NULL, nr_pages * psize(), PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_ANON, 0, 0);
	if (buf == MAP_FAILED) {
		perror("mmap failed, halting the test");
		return KSFT_FAIL;
	}

	for (i = 0; i < nr_pages; i++)
		*((unsigned long *)(buf + (i * psize()))) = i;

	 
	if (write(pipefd, "", 1) < 0) {
		perror("write");
		return KSFT_FAIL;
	}

	 
	while (getppid() == ppid && timeout > 0) {
		sleep(1);
		timeout--;
	}

	munmap(buf, nr_pages * psize());

	return (timeout > 0) ? KSFT_PASS : KSFT_FAIL;
}

 
static void run_negative_tests(int pidfd)
{
	int res;
	 
	if (!syscall(__NR_process_mrelease, pidfd, (unsigned int)-1) ||
			errno != EINVAL) {
		res = (errno == ENOSYS ? KSFT_SKIP : KSFT_FAIL);
		perror("process_mrelease with wrong flags");
		exit(res);
	}
	 
	if (!syscall(__NR_process_mrelease, pidfd, 0) || errno != EINVAL) {
		res = (errno == ENOSYS ? KSFT_SKIP : KSFT_FAIL);
		perror("process_mrelease on a live process");
		exit(res);
	}
}

static int child_main(int pipefd[], size_t size)
{
	int res;

	 
	close(pipefd[0]);
	res = alloc_noexit(MB(size) / psize(), pipefd[1]);
	close(pipefd[1]);
	return res;
}

int main(void)
{
	int pipefd[2], pidfd;
	bool success, retry;
	size_t size;
	pid_t pid;
	char byte;
	int res;

	 
	if (!syscall(__NR_process_mrelease, -1, 0) || errno != EBADF) {
		res = (errno == ENOSYS ? KSFT_SKIP : KSFT_FAIL);
		perror("process_mrelease with wrong pidfd");
		exit(res);
	}

	 
	size = 1;
retry:
	 
	if (pipe(pipefd)) {
		perror("pipe");
		exit(KSFT_FAIL);
	}
	pid = fork();
	if (pid < 0) {
		perror("fork");
		close(pipefd[0]);
		close(pipefd[1]);
		exit(KSFT_FAIL);
	}

	if (pid == 0) {
		 
		res = child_main(pipefd, size);
		exit(res);
	}

	 
	close(pipefd[1]);
	 
	res = read(pipefd[0], &byte, 1);
	close(pipefd[0]);
	if (res < 0) {
		perror("read");
		if (!kill(pid, SIGKILL))
			waitpid(pid, NULL, 0);
		exit(KSFT_FAIL);
	}

	pidfd = syscall(__NR_pidfd_open, pid, 0);
	if (pidfd < 0) {
		perror("pidfd_open");
		if (!kill(pid, SIGKILL))
			waitpid(pid, NULL, 0);
		exit(KSFT_FAIL);
	}

	 
	run_negative_tests(pidfd);

	if (kill(pid, SIGKILL)) {
		res = (errno == ENOSYS ? KSFT_SKIP : KSFT_FAIL);
		perror("kill");
		exit(res);
	}

	success = (syscall(__NR_process_mrelease, pidfd, 0) == 0);
	if (!success) {
		 
		if (errno == ESRCH) {
			retry = (size <= MAX_SIZE_MB);
		} else {
			res = (errno == ENOSYS ? KSFT_SKIP : KSFT_FAIL);
			perror("process_mrelease");
			waitpid(pid, NULL, 0);
			exit(res);
		}
	}

	 
	if (waitpid(pid, NULL, 0) < 0) {
		perror("waitpid");
		exit(KSFT_FAIL);
	}
	close(pidfd);

	if (!success) {
		if (retry) {
			size *= 2;
			goto retry;
		}
		printf("All process_mrelease attempts failed!\n");
		exit(KSFT_FAIL);
	}

	printf("Success reaping a child with %zuMB of memory allocations\n",
	       size);
	return KSFT_PASS;
}
