
 

#define _GNU_SOURCE
#include <sys/prctl.h>
#include <sys/sysinfo.h>
#include <sys/syscall.h>
#include <signal.h>

#include <asm/unistd.h>
#include "../kselftest_harness.h"

#ifndef PR_SET_SYSCALL_USER_DISPATCH
# define PR_SET_SYSCALL_USER_DISPATCH	59
# define PR_SYS_DISPATCH_OFF	0
# define PR_SYS_DISPATCH_ON	1
# define SYSCALL_DISPATCH_FILTER_ALLOW	0
# define SYSCALL_DISPATCH_FILTER_BLOCK	1
#endif

#ifndef SYS_USER_DISPATCH
# define SYS_USER_DISPATCH	2
#endif

#ifdef __NR_syscalls
# define MAGIC_SYSCALL_1 (__NR_syscalls + 1)  
#else
# define MAGIC_SYSCALL_1 (0xff00)   
#endif

#define SYSCALL_DISPATCH_ON(x) ((x) = SYSCALL_DISPATCH_FILTER_BLOCK)
#define SYSCALL_DISPATCH_OFF(x) ((x) = SYSCALL_DISPATCH_FILTER_ALLOW)

 

TEST_SIGNAL(dispatch_trigger_sigsys, SIGSYS)
{
	char sel = SYSCALL_DISPATCH_FILTER_ALLOW;
	struct sysinfo info;
	int ret;

	ret = sysinfo(&info);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_ON, 0, 0, &sel);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support CONFIG_SYSCALL_USER_DISPATCH");
	}

	SYSCALL_DISPATCH_ON(sel);

	sysinfo(&info);

	EXPECT_FALSE(true) {
		TH_LOG("Unreachable!");
	}
}

TEST(bad_prctl_param)
{
	char sel = SYSCALL_DISPATCH_FILTER_ALLOW;
	int op;

	 
	op = -1;
	prctl(PR_SET_SYSCALL_USER_DISPATCH, op, 0, 0, &sel);
	ASSERT_EQ(EINVAL, errno);

	 
	op = PR_SYS_DISPATCH_OFF;

	 
	prctl(PR_SET_SYSCALL_USER_DISPATCH, op, 0x1, 0x0, 0);
	EXPECT_EQ(EINVAL, errno);

	 
	prctl(PR_SET_SYSCALL_USER_DISPATCH, op, 0x0, 0xff, 0);
	EXPECT_EQ(EINVAL, errno);

	 
	prctl(PR_SET_SYSCALL_USER_DISPATCH, op, 0x0, 0x0, &sel);
	EXPECT_EQ(EINVAL, errno);

	 
	errno = 0;
	prctl(PR_SET_SYSCALL_USER_DISPATCH, op, 0x0, 0x0, 0x0);
	EXPECT_EQ(0, errno);

	 
	op = PR_SYS_DISPATCH_ON;

	 
	prctl(PR_SET_SYSCALL_USER_DISPATCH, op, 0x1, 0x0, &sel);
	EXPECT_EQ(EINVAL, errno);
	prctl(PR_SET_SYSCALL_USER_DISPATCH, op, -1L, 0x0, &sel);
	EXPECT_EQ(EINVAL, errno);

	 
	prctl(PR_SET_SYSCALL_USER_DISPATCH, op, 0x0, 0x1, (void *) -1);
	ASSERT_EQ(EFAULT, errno);

	 
	prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_ON, 1, -1L, &sel);
	ASSERT_EQ(EINVAL, errno) {
		TH_LOG("Should reject bad syscall range");
	}

	 
	prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_ON, -1L, 0x1, &sel);
	ASSERT_EQ(EINVAL, errno) {
		TH_LOG("Should reject bad syscall range");
	}
}

 
char glob_sel;
int nr_syscalls_emulated;
int si_code;
int si_errno;

static void handle_sigsys(int sig, siginfo_t *info, void *ucontext)
{
	si_code = info->si_code;
	si_errno = info->si_errno;

	if (info->si_syscall == MAGIC_SYSCALL_1)
		nr_syscalls_emulated++;

	 
	SYSCALL_DISPATCH_OFF(glob_sel);
}

TEST(dispatch_and_return)
{
	long ret;
	struct sigaction act;
	sigset_t mask;

	glob_sel = 0;
	nr_syscalls_emulated = 0;
	si_code = 0;
	si_errno = 0;

	memset(&act, 0, sizeof(act));
	sigemptyset(&mask);

	act.sa_sigaction = handle_sigsys;
	act.sa_flags = SA_SIGINFO;
	act.sa_mask = mask;

	ret = sigaction(SIGSYS, &act, NULL);
	ASSERT_EQ(0, ret);

	 
	SYSCALL_DISPATCH_OFF(glob_sel);

	ret = prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_ON, 0, 0, &glob_sel);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support CONFIG_SYSCALL_USER_DISPATCH");
	}

	 
	SYSCALL_DISPATCH_OFF(glob_sel);
	ret = syscall(MAGIC_SYSCALL_1);
	EXPECT_EQ(-1, ret) {
		TH_LOG("Dispatch triggered unexpectedly");
	}

	 
	nr_syscalls_emulated = 0;
	SYSCALL_DISPATCH_ON(glob_sel);

	ret = syscall(MAGIC_SYSCALL_1);
	EXPECT_EQ(MAGIC_SYSCALL_1, ret) {
		TH_LOG("Failed to intercept syscall");
	}
	EXPECT_EQ(1, nr_syscalls_emulated) {
		TH_LOG("Failed to emulate syscall");
	}
	ASSERT_EQ(SYS_USER_DISPATCH, si_code) {
		TH_LOG("Bad si_code in SIGSYS");
	}
	ASSERT_EQ(0, si_errno) {
		TH_LOG("Bad si_errno in SIGSYS");
	}
}

TEST_SIGNAL(bad_selector, SIGSYS)
{
	long ret;
	struct sigaction act;
	sigset_t mask;
	struct sysinfo info;

	glob_sel = SYSCALL_DISPATCH_FILTER_ALLOW;
	nr_syscalls_emulated = 0;
	si_code = 0;
	si_errno = 0;

	memset(&act, 0, sizeof(act));
	sigemptyset(&mask);

	act.sa_sigaction = handle_sigsys;
	act.sa_flags = SA_SIGINFO;
	act.sa_mask = mask;

	ret = sigaction(SIGSYS, &act, NULL);
	ASSERT_EQ(0, ret);

	 
	SYSCALL_DISPATCH_OFF(glob_sel);

	ret = prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_ON, 0, 0, &glob_sel);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support CONFIG_SYSCALL_USER_DISPATCH");
	}

	glob_sel = -1;

	sysinfo(&info);

	 

	EXPECT_FALSE(true) {
		TH_LOG("Unreachable!");
	}
}

TEST(disable_dispatch)
{
	int ret;
	struct sysinfo info;
	char sel = 0;

	ret = prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_ON, 0, 0, &sel);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support CONFIG_SYSCALL_USER_DISPATCH");
	}

	 
	SYSCALL_DISPATCH_OFF(glob_sel);

	ret = prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_OFF, 0, 0, 0);
	EXPECT_EQ(0, ret) {
		TH_LOG("Failed to unset syscall user dispatch");
	}

	 
	SYSCALL_DISPATCH_ON(glob_sel);

	ret = syscall(__NR_sysinfo, &info);
	EXPECT_EQ(0, ret) {
		TH_LOG("Dispatch triggered unexpectedly");
	}
}

TEST(direct_dispatch_range)
{
	int ret = 0;
	struct sysinfo info;
	char sel = SYSCALL_DISPATCH_FILTER_ALLOW;

	 
	ret = prctl(PR_SET_SYSCALL_USER_DISPATCH, PR_SYS_DISPATCH_ON, 0, -1L, &sel);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support CONFIG_SYSCALL_USER_DISPATCH");
	}

	SYSCALL_DISPATCH_ON(sel);

	ret = sysinfo(&info);
	ASSERT_EQ(0, ret) {
		TH_LOG("Dispatch triggered unexpectedly");
	}
}

TEST_HARNESS_MAIN
