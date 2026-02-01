
 
#define _GNU_SOURCE
#include <sched.h>
#include <test_progs.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include "fexit_sleep.lskel.h"

static int do_sleep(void *skel)
{
	struct fexit_sleep_lskel *fexit_skel = skel;
	struct timespec ts1 = { .tv_nsec = 1 };
	struct timespec ts2 = { .tv_sec = 10 };

	fexit_skel->bss->pid = getpid();
	(void)syscall(__NR_nanosleep, &ts1, NULL);
	(void)syscall(__NR_nanosleep, &ts2, NULL);
	return 0;
}

#define STACK_SIZE (1024 * 1024)
static char child_stack[STACK_SIZE];

void test_fexit_sleep(void)
{
	struct fexit_sleep_lskel *fexit_skel = NULL;
	int wstatus, duration = 0;
	pid_t cpid;
	int err, fexit_cnt;

	fexit_skel = fexit_sleep_lskel__open_and_load();
	if (CHECK(!fexit_skel, "fexit_skel_load", "fexit skeleton failed\n"))
		goto cleanup;

	err = fexit_sleep_lskel__attach(fexit_skel);
	if (CHECK(err, "fexit_attach", "fexit attach failed: %d\n", err))
		goto cleanup;

	cpid = clone(do_sleep, child_stack + STACK_SIZE, CLONE_FILES | SIGCHLD, fexit_skel);
	if (CHECK(cpid == -1, "clone", "%s\n", strerror(errno)))
		goto cleanup;

	 
	while (READ_ONCE(fexit_skel->bss->fentry_cnt) != 2);
	fexit_cnt = READ_ONCE(fexit_skel->bss->fexit_cnt);
	if (CHECK(fexit_cnt != 1, "fexit_cnt", "%d", fexit_cnt))
		goto cleanup;

	 
	close(fexit_skel->progs.nanosleep_fentry.prog_fd);
	close(fexit_skel->progs.nanosleep_fexit.prog_fd);
	fexit_sleep_lskel__detach(fexit_skel);

	 
	kill(cpid, 9);

	if (CHECK(waitpid(cpid, &wstatus, 0) == -1, "waitpid", "%s\n", strerror(errno)))
		goto cleanup;
	if (CHECK(WEXITSTATUS(wstatus) != 0, "exitstatus", "failed"))
		goto cleanup;

	 
	fexit_cnt = READ_ONCE(fexit_skel->bss->fexit_cnt);
	if (CHECK(fexit_cnt != 1, "fexit_cnt", "%d", fexit_cnt))
		goto cleanup;

cleanup:
	fexit_sleep_lskel__destroy(fexit_skel);
}
