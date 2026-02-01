
 

#define _GNU_SOURCE
#include <sys/types.h>

 
#if !__GLIBC_PREREQ(2, 26)
# include <asm/siginfo.h>
# define __have_siginfo_t 1
# define __have_sigval_t 1
# define __have_sigevent_t 1
#endif

#include <errno.h>
#include <linux/filter.h>
#include <sys/prctl.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <linux/prctl.h>
#include <linux/ptrace.h>
#include <linux/seccomp.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <linux/elf.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <sys/times.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/kcmp.h>
#include <sys/resource.h>
#include <sys/capability.h>

#include <unistd.h>
#include <sys/syscall.h>
#include <poll.h>

#include "../kselftest_harness.h"
#include "../clone3/clone3_selftests.h"

 
#ifndef SKIP
#define SKIP(s, ...)	XFAIL(s, ##__VA_ARGS__)
#endif

#define MIN(X, Y) ((X) < (Y) ? (X) : (Y))

#ifndef PR_SET_PTRACER
# define PR_SET_PTRACER 0x59616d61
#endif

#ifndef PR_SET_NO_NEW_PRIVS
#define PR_SET_NO_NEW_PRIVS 38
#define PR_GET_NO_NEW_PRIVS 39
#endif

#ifndef PR_SECCOMP_EXT
#define PR_SECCOMP_EXT 43
#endif

#ifndef SECCOMP_EXT_ACT
#define SECCOMP_EXT_ACT 1
#endif

#ifndef SECCOMP_EXT_ACT_TSYNC
#define SECCOMP_EXT_ACT_TSYNC 1
#endif

#ifndef SECCOMP_MODE_STRICT
#define SECCOMP_MODE_STRICT 1
#endif

#ifndef SECCOMP_MODE_FILTER
#define SECCOMP_MODE_FILTER 2
#endif

#ifndef SECCOMP_RET_ALLOW
struct seccomp_data {
	int nr;
	__u32 arch;
	__u64 instruction_pointer;
	__u64 args[6];
};
#endif

#ifndef SECCOMP_RET_KILL_PROCESS
#define SECCOMP_RET_KILL_PROCESS 0x80000000U  
#define SECCOMP_RET_KILL_THREAD	 0x00000000U  
#endif
#ifndef SECCOMP_RET_KILL
#define SECCOMP_RET_KILL	 SECCOMP_RET_KILL_THREAD
#define SECCOMP_RET_TRAP	 0x00030000U  
#define SECCOMP_RET_ERRNO	 0x00050000U  
#define SECCOMP_RET_TRACE	 0x7ff00000U  
#define SECCOMP_RET_ALLOW	 0x7fff0000U  
#endif
#ifndef SECCOMP_RET_LOG
#define SECCOMP_RET_LOG		 0x7ffc0000U  
#endif

#ifndef __NR_seccomp
# if defined(__i386__)
#  define __NR_seccomp 354
# elif defined(__x86_64__)
#  define __NR_seccomp 317
# elif defined(__arm__)
#  define __NR_seccomp 383
# elif defined(__aarch64__)
#  define __NR_seccomp 277
# elif defined(__riscv)
#  define __NR_seccomp 277
# elif defined(__csky__)
#  define __NR_seccomp 277
# elif defined(__loongarch__)
#  define __NR_seccomp 277
# elif defined(__hppa__)
#  define __NR_seccomp 338
# elif defined(__powerpc__)
#  define __NR_seccomp 358
# elif defined(__s390__)
#  define __NR_seccomp 348
# elif defined(__xtensa__)
#  define __NR_seccomp 337
# elif defined(__sh__)
#  define __NR_seccomp 372
# elif defined(__mc68000__)
#  define __NR_seccomp 380
# else
#  warning "seccomp syscall number unknown for this architecture"
#  define __NR_seccomp 0xffff
# endif
#endif

#ifndef SECCOMP_SET_MODE_STRICT
#define SECCOMP_SET_MODE_STRICT 0
#endif

#ifndef SECCOMP_SET_MODE_FILTER
#define SECCOMP_SET_MODE_FILTER 1
#endif

#ifndef SECCOMP_GET_ACTION_AVAIL
#define SECCOMP_GET_ACTION_AVAIL 2
#endif

#ifndef SECCOMP_GET_NOTIF_SIZES
#define SECCOMP_GET_NOTIF_SIZES 3
#endif

#ifndef SECCOMP_FILTER_FLAG_TSYNC
#define SECCOMP_FILTER_FLAG_TSYNC (1UL << 0)
#endif

#ifndef SECCOMP_FILTER_FLAG_LOG
#define SECCOMP_FILTER_FLAG_LOG (1UL << 1)
#endif

#ifndef SECCOMP_FILTER_FLAG_SPEC_ALLOW
#define SECCOMP_FILTER_FLAG_SPEC_ALLOW (1UL << 2)
#endif

#ifndef PTRACE_SECCOMP_GET_METADATA
#define PTRACE_SECCOMP_GET_METADATA	0x420d

struct seccomp_metadata {
	__u64 filter_off;        
	__u64 flags;              
};
#endif

#ifndef SECCOMP_FILTER_FLAG_NEW_LISTENER
#define SECCOMP_FILTER_FLAG_NEW_LISTENER	(1UL << 3)
#endif

#ifndef SECCOMP_RET_USER_NOTIF
#define SECCOMP_RET_USER_NOTIF 0x7fc00000U

#define SECCOMP_IOC_MAGIC		'!'
#define SECCOMP_IO(nr)			_IO(SECCOMP_IOC_MAGIC, nr)
#define SECCOMP_IOR(nr, type)		_IOR(SECCOMP_IOC_MAGIC, nr, type)
#define SECCOMP_IOW(nr, type)		_IOW(SECCOMP_IOC_MAGIC, nr, type)
#define SECCOMP_IOWR(nr, type)		_IOWR(SECCOMP_IOC_MAGIC, nr, type)

 
#define SECCOMP_IOCTL_NOTIF_RECV	SECCOMP_IOWR(0, struct seccomp_notif)
#define SECCOMP_IOCTL_NOTIF_SEND	SECCOMP_IOWR(1,	\
						struct seccomp_notif_resp)
#define SECCOMP_IOCTL_NOTIF_ID_VALID	SECCOMP_IOW(2, __u64)

struct seccomp_notif {
	__u64 id;
	__u32 pid;
	__u32 flags;
	struct seccomp_data data;
};

struct seccomp_notif_resp {
	__u64 id;
	__s64 val;
	__s32 error;
	__u32 flags;
};

struct seccomp_notif_sizes {
	__u16 seccomp_notif;
	__u16 seccomp_notif_resp;
	__u16 seccomp_data;
};
#endif

#ifndef SECCOMP_IOCTL_NOTIF_ADDFD
 
#define SECCOMP_IOCTL_NOTIF_ADDFD	SECCOMP_IOW(3,	\
						struct seccomp_notif_addfd)

 
#define SECCOMP_ADDFD_FLAG_SETFD	(1UL << 0)  

struct seccomp_notif_addfd {
	__u64 id;
	__u32 flags;
	__u32 srcfd;
	__u32 newfd;
	__u32 newfd_flags;
};
#endif

#ifndef SECCOMP_ADDFD_FLAG_SEND
#define SECCOMP_ADDFD_FLAG_SEND	(1UL << 1)  
#endif

struct seccomp_notif_addfd_small {
	__u64 id;
	char weird[4];
};
#define SECCOMP_IOCTL_NOTIF_ADDFD_SMALL	\
	SECCOMP_IOW(3, struct seccomp_notif_addfd_small)

struct seccomp_notif_addfd_big {
	union {
		struct seccomp_notif_addfd addfd;
		char buf[sizeof(struct seccomp_notif_addfd) + 8];
	};
};
#define SECCOMP_IOCTL_NOTIF_ADDFD_BIG	\
	SECCOMP_IOWR(3, struct seccomp_notif_addfd_big)

#ifndef PTRACE_EVENTMSG_SYSCALL_ENTRY
#define PTRACE_EVENTMSG_SYSCALL_ENTRY	1
#define PTRACE_EVENTMSG_SYSCALL_EXIT	2
#endif

#ifndef SECCOMP_USER_NOTIF_FLAG_CONTINUE
#define SECCOMP_USER_NOTIF_FLAG_CONTINUE 0x00000001
#endif

#ifndef SECCOMP_FILTER_FLAG_TSYNC_ESRCH
#define SECCOMP_FILTER_FLAG_TSYNC_ESRCH (1UL << 4)
#endif

#ifndef SECCOMP_FILTER_FLAG_WAIT_KILLABLE_RECV
#define SECCOMP_FILTER_FLAG_WAIT_KILLABLE_RECV (1UL << 5)
#endif

#ifndef seccomp
int seccomp(unsigned int op, unsigned int flags, void *args)
{
	errno = 0;
	return syscall(__NR_seccomp, op, flags, args);
}
#endif

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define syscall_arg(_n) (offsetof(struct seccomp_data, args[_n]))
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define syscall_arg(_n) (offsetof(struct seccomp_data, args[_n]) + sizeof(__u32))
#else
#error "wut? Unknown __BYTE_ORDER__?!"
#endif

#define SIBLING_EXIT_UNKILLED	0xbadbeef
#define SIBLING_EXIT_FAILURE	0xbadface
#define SIBLING_EXIT_NEWPRIVS	0xbadfeed

static int __filecmp(pid_t pid1, pid_t pid2, int fd1, int fd2)
{
#ifdef __NR_kcmp
	errno = 0;
	return syscall(__NR_kcmp, pid1, pid2, KCMP_FILE, fd1, fd2);
#else
	errno = ENOSYS;
	return -1;
#endif
}

 
#define filecmp(pid1, pid2, fd1, fd2)	({		\
	int _ret;					\
							\
	_ret = __filecmp(pid1, pid2, fd1, fd2);		\
	if (_ret != 0) {				\
		if (_ret < 0 && errno == ENOSYS) {	\
			TH_LOG("kcmp() syscall missing (test is less accurate)");\
			_ret = 0;			\
		}					\
	}						\
	_ret; })

TEST(kcmp)
{
	int ret;

	ret = __filecmp(getpid(), getpid(), 1, 1);
	EXPECT_EQ(ret, 0);
	if (ret != 0 && errno == ENOSYS)
		SKIP(return, "Kernel does not support kcmp() (missing CONFIG_KCMP?)");
}

TEST(mode_strict_support)
{
	long ret;

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT, NULL, NULL, NULL);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support CONFIG_SECCOMP");
	}
	syscall(__NR_exit, 0);
}

TEST_SIGNAL(mode_strict_cannot_call_prctl, SIGKILL)
{
	long ret;

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT, NULL, NULL, NULL);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support CONFIG_SECCOMP");
	}
	syscall(__NR_prctl, PR_SET_SECCOMP, SECCOMP_MODE_FILTER,
		NULL, NULL, NULL);
	EXPECT_FALSE(true) {
		TH_LOG("Unreachable!");
	}
}

 
TEST(no_new_privs_support)
{
	long ret;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	EXPECT_EQ(0, ret) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}
}

 
TEST(mode_filter_support)
{
	long ret;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, NULL, 0, 0);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, NULL, NULL, NULL);
	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EFAULT, errno) {
		TH_LOG("Kernel does not support CONFIG_SECCOMP_FILTER!");
	}
}

TEST(mode_filter_without_nnp)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;
	cap_t cap = cap_get_proc();
	cap_flag_value_t is_cap_sys_admin = 0;

	ret = prctl(PR_GET_NO_NEW_PRIVS, 0, NULL, 0, 0);
	ASSERT_LE(0, ret) {
		TH_LOG("Expected 0 or unsupported for NO_NEW_PRIVS");
	}
	errno = 0;
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog, 0, 0);
	 
	cap_get_flag(cap, CAP_SYS_ADMIN, CAP_EFFECTIVE, &is_cap_sys_admin);
	if (!is_cap_sys_admin) {
		EXPECT_EQ(-1, ret);
		EXPECT_EQ(EACCES, errno);
	} else {
		EXPECT_EQ(0, ret);
	}
}

#define MAX_INSNS_PER_PATH 32768

TEST(filter_size_limits)
{
	int i;
	int count = BPF_MAXINSNS + 1;
	struct sock_filter allow[] = {
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_filter *filter;
	struct sock_fprog prog = { };
	long ret;

	filter = calloc(count, sizeof(*filter));
	ASSERT_NE(NULL, filter);

	for (i = 0; i < count; i++)
		filter[i] = allow[0];

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	prog.filter = filter;
	prog.len = count;

	 
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog, 0, 0);
	ASSERT_NE(0, ret) {
		TH_LOG("Installing %d insn filter was allowed", prog.len);
	}

	 
	prog.len -= 1;
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog, 0, 0);
	ASSERT_EQ(0, ret) {
		TH_LOG("Installing %d insn filter wasn't allowed", prog.len);
	}
}

TEST(filter_chain_limits)
{
	int i;
	int count = BPF_MAXINSNS;
	struct sock_filter allow[] = {
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_filter *filter;
	struct sock_fprog prog = { };
	long ret;

	filter = calloc(count, sizeof(*filter));
	ASSERT_NE(NULL, filter);

	for (i = 0; i < count; i++)
		filter[i] = allow[0];

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	prog.filter = filter;
	prog.len = 1;

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog, 0, 0);
	ASSERT_EQ(0, ret);

	prog.len = count;

	 
	for (i = 0; i < MAX_INSNS_PER_PATH; i++) {
		ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog, 0, 0);
		if (ret != 0)
			break;
	}
	ASSERT_NE(0, ret) {
		TH_LOG("Allowed %d %d-insn filters (total with penalties:%d)",
		       i, count, i * (count + 4));
	}
}

TEST(mode_filter_cannot_move_to_strict)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT, NULL, 0, 0);
	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EINVAL, errno);
}


TEST(mode_filter_get_seccomp)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_GET_SECCOMP, 0, 0, 0, 0);
	EXPECT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_GET_SECCOMP, 0, 0, 0, 0);
	EXPECT_EQ(2, ret);
}


TEST(ALLOW_all)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
	ASSERT_EQ(0, ret);
}

TEST(empty_prog)
{
	struct sock_filter filter[] = {
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EINVAL, errno);
}

TEST(log_all)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_LOG),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;
	pid_t parent = getppid();

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
	ASSERT_EQ(0, ret);

	 
	EXPECT_EQ(parent, syscall(__NR_getppid));
}

TEST_SIGNAL(unknown_ret_is_kill_inside, SIGSYS)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_RET|BPF_K, 0x10000000U),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
	ASSERT_EQ(0, ret);
	EXPECT_EQ(0, syscall(__NR_getpid)) {
		TH_LOG("getpid() shouldn't ever return");
	}
}

 
TEST_SIGNAL(unknown_ret_is_kill_above_allow, SIGSYS)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_RET|BPF_K, 0x90000000U),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
	ASSERT_EQ(0, ret);
	EXPECT_EQ(0, syscall(__NR_getpid)) {
		TH_LOG("getpid() shouldn't ever return");
	}
}

TEST_SIGNAL(KILL_all, SIGSYS)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_KILL),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
	ASSERT_EQ(0, ret);
}

TEST_SIGNAL(KILL_one, SIGSYS)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_getpid, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_KILL),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;
	pid_t parent = getppid();

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
	ASSERT_EQ(0, ret);

	EXPECT_EQ(parent, syscall(__NR_getppid));
	 
	EXPECT_EQ(0, syscall(__NR_getpid));
}

TEST_SIGNAL(KILL_one_arg_one, SIGSYS)
{
	void *fatal_address;
	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_times, 1, 0),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
		 
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS, syscall_arg(0)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K,
			(unsigned long)&fatal_address, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_KILL),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;
	pid_t parent = getppid();
	struct tms timebuf;
	clock_t clock = times(&timebuf);

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
	ASSERT_EQ(0, ret);

	EXPECT_EQ(parent, syscall(__NR_getppid));
	EXPECT_LE(clock, syscall(__NR_times, &timebuf));
	 
	EXPECT_EQ(0, syscall(__NR_times, &fatal_address));
}

TEST_SIGNAL(KILL_one_arg_six, SIGSYS)
{
#ifndef __NR_mmap2
	int sysno = __NR_mmap;
#else
	int sysno = __NR_mmap2;
#endif
	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, sysno, 1, 0),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
		 
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS, syscall_arg(5)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, 0x0C0FFEE, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_KILL),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;
	pid_t parent = getppid();
	int fd;
	void *map1, *map2;
	int page_size = sysconf(_SC_PAGESIZE);

	ASSERT_LT(0, page_size);

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
	ASSERT_EQ(0, ret);

	fd = open("/dev/zero", O_RDONLY);
	ASSERT_NE(-1, fd);

	EXPECT_EQ(parent, syscall(__NR_getppid));
	map1 = (void *)syscall(sysno,
		NULL, page_size, PROT_READ, MAP_PRIVATE, fd, page_size);
	EXPECT_NE(MAP_FAILED, map1);
	 
	map2 = (void *)syscall(sysno,
		 NULL, page_size, PROT_READ, MAP_PRIVATE, fd, 0x0C0FFEE);
	EXPECT_EQ(MAP_FAILED, map2);

	 
	munmap(map1, page_size);
	munmap(map2, page_size);
	close(fd);
}

 
void *kill_thread(void *data)
{
	bool die = (bool)data;

	if (die) {
		prctl(PR_GET_SECCOMP, 0, 0, 0, 0);
		return (void *)SIBLING_EXIT_FAILURE;
	}

	return (void *)SIBLING_EXIT_UNKILLED;
}

enum kill_t {
	KILL_THREAD,
	KILL_PROCESS,
	RET_UNKNOWN
};

 
void kill_thread_or_group(struct __test_metadata *_metadata,
			  enum kill_t kill_how)
{
	pthread_t thread;
	void *status;
	 
	struct sock_filter filter_thread[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_prctl, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_KILL_THREAD),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog_thread = {
		.len = (unsigned short)ARRAY_SIZE(filter_thread),
		.filter = filter_thread,
	};
	int kill = kill_how == KILL_PROCESS ? SECCOMP_RET_KILL_PROCESS : 0xAAAAAAAA;
	struct sock_filter filter_process[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_prctl, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, kill),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog_process = {
		.len = (unsigned short)ARRAY_SIZE(filter_process),
		.filter = filter_process,
	};

	ASSERT_EQ(0, prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	ASSERT_EQ(0, seccomp(SECCOMP_SET_MODE_FILTER, 0,
			     kill_how == KILL_THREAD ? &prog_thread
						     : &prog_process));

	 
	if (kill_how == KILL_PROCESS)
		ASSERT_EQ(0, seccomp(SECCOMP_SET_MODE_FILTER, 0, &prog_thread));

	 
	ASSERT_EQ(0, pthread_create(&thread, NULL, kill_thread, (void *)false));
	ASSERT_EQ(0, pthread_join(thread, &status));
	ASSERT_EQ(SIBLING_EXIT_UNKILLED, (unsigned long)status);

	 
	ASSERT_EQ(0, pthread_create(&thread, NULL, kill_thread, (void *)true));
	ASSERT_EQ(0, pthread_join(thread, &status));
	ASSERT_NE(SIBLING_EXIT_FAILURE, (unsigned long)status);

	 
	exit(42);
}

TEST(KILL_thread)
{
	int status;
	pid_t child_pid;

	child_pid = fork();
	ASSERT_LE(0, child_pid);
	if (child_pid == 0) {
		kill_thread_or_group(_metadata, KILL_THREAD);
		_exit(38);
	}

	ASSERT_EQ(child_pid, waitpid(child_pid, &status, 0));

	 
	ASSERT_TRUE(WIFEXITED(status));
	ASSERT_EQ(42, WEXITSTATUS(status));
}

TEST(KILL_process)
{
	int status;
	pid_t child_pid;

	child_pid = fork();
	ASSERT_LE(0, child_pid);
	if (child_pid == 0) {
		kill_thread_or_group(_metadata, KILL_PROCESS);
		_exit(38);
	}

	ASSERT_EQ(child_pid, waitpid(child_pid, &status, 0));

	 
	ASSERT_TRUE(WIFSIGNALED(status));
	ASSERT_EQ(SIGSYS, WTERMSIG(status));
}

TEST(KILL_unknown)
{
	int status;
	pid_t child_pid;

	child_pid = fork();
	ASSERT_LE(0, child_pid);
	if (child_pid == 0) {
		kill_thread_or_group(_metadata, RET_UNKNOWN);
		_exit(38);
	}

	ASSERT_EQ(child_pid, waitpid(child_pid, &status, 0));

	 
	EXPECT_TRUE(WIFSIGNALED(status)) {
		TH_LOG("Unknown SECCOMP_RET is only killing the thread?");
	}
	ASSERT_EQ(SIGSYS, WTERMSIG(status));
}

 
TEST(arg_out_of_range)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS, syscall_arg(6)),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog);
	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EINVAL, errno);
}

#define ERRNO_FILTER(name, errno)					\
	struct sock_filter _read_filter_##name[] = {			\
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,				\
			offsetof(struct seccomp_data, nr)),		\
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_read, 0, 1),	\
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ERRNO | errno),	\
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),		\
	};								\
	struct sock_fprog prog_##name = {				\
		.len = (unsigned short)ARRAY_SIZE(_read_filter_##name),	\
		.filter = _read_filter_##name,				\
	}

 
TEST(ERRNO_valid)
{
	ERRNO_FILTER(valid, E2BIG);
	long ret;
	pid_t parent = getppid();

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog_valid);
	ASSERT_EQ(0, ret);

	EXPECT_EQ(parent, syscall(__NR_getppid));
	EXPECT_EQ(-1, read(-1, NULL, 0));
	EXPECT_EQ(E2BIG, errno);
}

 
TEST(ERRNO_zero)
{
	ERRNO_FILTER(zero, 0);
	long ret;
	pid_t parent = getppid();

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog_zero);
	ASSERT_EQ(0, ret);

	EXPECT_EQ(parent, syscall(__NR_getppid));
	 
	EXPECT_EQ(0, read(-1, NULL, 0));
}

 
TEST(ERRNO_capped)
{
	ERRNO_FILTER(capped, 4096);
	long ret;
	pid_t parent = getppid();

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog_capped);
	ASSERT_EQ(0, ret);

	EXPECT_EQ(parent, syscall(__NR_getppid));
	EXPECT_EQ(-1, read(-1, NULL, 0));
	EXPECT_EQ(4095, errno);
}

 
TEST(ERRNO_order)
{
	ERRNO_FILTER(first,  11);
	ERRNO_FILTER(second, 13);
	ERRNO_FILTER(third,  12);
	long ret;
	pid_t parent = getppid();

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog_first);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog_second);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog_third);
	ASSERT_EQ(0, ret);

	EXPECT_EQ(parent, syscall(__NR_getppid));
	EXPECT_EQ(-1, read(-1, NULL, 0));
	EXPECT_EQ(12, errno);
}

FIXTURE(TRAP) {
	struct sock_fprog prog;
};

FIXTURE_SETUP(TRAP)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_getpid, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_TRAP),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};

	memset(&self->prog, 0, sizeof(self->prog));
	self->prog.filter = malloc(sizeof(filter));
	ASSERT_NE(NULL, self->prog.filter);
	memcpy(self->prog.filter, filter, sizeof(filter));
	self->prog.len = (unsigned short)ARRAY_SIZE(filter);
}

FIXTURE_TEARDOWN(TRAP)
{
	if (self->prog.filter)
		free(self->prog.filter);
}

TEST_F_SIGNAL(TRAP, dfl, SIGSYS)
{
	long ret;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->prog);
	ASSERT_EQ(0, ret);
	syscall(__NR_getpid);
}

 
TEST_F_SIGNAL(TRAP, ign, SIGSYS)
{
	long ret;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	signal(SIGSYS, SIG_IGN);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->prog);
	ASSERT_EQ(0, ret);
	syscall(__NR_getpid);
}

static siginfo_t TRAP_info;
static volatile int TRAP_nr;
static void TRAP_action(int nr, siginfo_t *info, void *void_context)
{
	memcpy(&TRAP_info, info, sizeof(TRAP_info));
	TRAP_nr = nr;
}

TEST_F(TRAP, handler)
{
	int ret, test;
	struct sigaction act;
	sigset_t mask;

	memset(&act, 0, sizeof(act));
	sigemptyset(&mask);
	sigaddset(&mask, SIGSYS);

	act.sa_sigaction = &TRAP_action;
	act.sa_flags = SA_SIGINFO;
	ret = sigaction(SIGSYS, &act, NULL);
	ASSERT_EQ(0, ret) {
		TH_LOG("sigaction failed");
	}
	ret = sigprocmask(SIG_UNBLOCK, &mask, NULL);
	ASSERT_EQ(0, ret) {
		TH_LOG("sigprocmask failed");
	}

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->prog);
	ASSERT_EQ(0, ret);
	TRAP_nr = 0;
	memset(&TRAP_info, 0, sizeof(TRAP_info));
	 
	ret = syscall(__NR_getpid);
	 
	test = TRAP_nr;
	EXPECT_EQ(SIGSYS, test);
	struct local_sigsys {
		void *_call_addr;	 
		int _syscall;		 
		unsigned int _arch;	 
	} *sigsys = (struct local_sigsys *)
#ifdef si_syscall
		&(TRAP_info.si_call_addr);
#else
		&TRAP_info.si_pid;
#endif
	EXPECT_EQ(__NR_getpid, sigsys->_syscall);
	 
	EXPECT_NE(0, sigsys->_arch);
	EXPECT_NE(0, (unsigned long)sigsys->_call_addr);
}

FIXTURE(precedence) {
	struct sock_fprog allow;
	struct sock_fprog log;
	struct sock_fprog trace;
	struct sock_fprog error;
	struct sock_fprog trap;
	struct sock_fprog kill;
};

FIXTURE_SETUP(precedence)
{
	struct sock_filter allow_insns[] = {
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_filter log_insns[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_getpid, 1, 0),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_LOG),
	};
	struct sock_filter trace_insns[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_getpid, 1, 0),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_TRACE),
	};
	struct sock_filter error_insns[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_getpid, 1, 0),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ERRNO),
	};
	struct sock_filter trap_insns[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_getpid, 1, 0),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_TRAP),
	};
	struct sock_filter kill_insns[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_getpid, 1, 0),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_KILL),
	};

	memset(self, 0, sizeof(*self));
#define FILTER_ALLOC(_x) \
	self->_x.filter = malloc(sizeof(_x##_insns)); \
	ASSERT_NE(NULL, self->_x.filter); \
	memcpy(self->_x.filter, &_x##_insns, sizeof(_x##_insns)); \
	self->_x.len = (unsigned short)ARRAY_SIZE(_x##_insns)
	FILTER_ALLOC(allow);
	FILTER_ALLOC(log);
	FILTER_ALLOC(trace);
	FILTER_ALLOC(error);
	FILTER_ALLOC(trap);
	FILTER_ALLOC(kill);
}

FIXTURE_TEARDOWN(precedence)
{
#define FILTER_FREE(_x) if (self->_x.filter) free(self->_x.filter)
	FILTER_FREE(allow);
	FILTER_FREE(log);
	FILTER_FREE(trace);
	FILTER_FREE(error);
	FILTER_FREE(trap);
	FILTER_FREE(kill);
}

TEST_F(precedence, allow_ok)
{
	pid_t parent, res = 0;
	long ret;

	parent = getppid();
	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->allow);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->log);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->trace);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->error);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->trap);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->kill);
	ASSERT_EQ(0, ret);
	 
	res = syscall(__NR_getppid);
	EXPECT_EQ(parent, res);
}

TEST_F_SIGNAL(precedence, kill_is_highest, SIGSYS)
{
	pid_t parent, res = 0;
	long ret;

	parent = getppid();
	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->allow);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->log);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->trace);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->error);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->trap);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->kill);
	ASSERT_EQ(0, ret);
	 
	res = syscall(__NR_getppid);
	EXPECT_EQ(parent, res);
	 
	res = syscall(__NR_getpid);
	EXPECT_EQ(0, res);
}

TEST_F_SIGNAL(precedence, kill_is_highest_in_any_order, SIGSYS)
{
	pid_t parent;
	long ret;

	parent = getppid();
	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->allow);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->kill);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->error);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->log);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->trace);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->trap);
	ASSERT_EQ(0, ret);
	 
	EXPECT_EQ(parent, syscall(__NR_getppid));
	 
	EXPECT_EQ(0, syscall(__NR_getpid));
}

TEST_F_SIGNAL(precedence, trap_is_second, SIGSYS)
{
	pid_t parent;
	long ret;

	parent = getppid();
	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->allow);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->log);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->trace);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->error);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->trap);
	ASSERT_EQ(0, ret);
	 
	EXPECT_EQ(parent, syscall(__NR_getppid));
	 
	EXPECT_EQ(0, syscall(__NR_getpid));
}

TEST_F_SIGNAL(precedence, trap_is_second_in_any_order, SIGSYS)
{
	pid_t parent;
	long ret;

	parent = getppid();
	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->allow);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->trap);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->log);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->trace);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->error);
	ASSERT_EQ(0, ret);
	 
	EXPECT_EQ(parent, syscall(__NR_getppid));
	 
	EXPECT_EQ(0, syscall(__NR_getpid));
}

TEST_F(precedence, errno_is_third)
{
	pid_t parent;
	long ret;

	parent = getppid();
	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->allow);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->log);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->trace);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->error);
	ASSERT_EQ(0, ret);
	 
	EXPECT_EQ(parent, syscall(__NR_getppid));
	EXPECT_EQ(0, syscall(__NR_getpid));
}

TEST_F(precedence, errno_is_third_in_any_order)
{
	pid_t parent;
	long ret;

	parent = getppid();
	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->log);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->error);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->trace);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->allow);
	ASSERT_EQ(0, ret);
	 
	EXPECT_EQ(parent, syscall(__NR_getppid));
	EXPECT_EQ(0, syscall(__NR_getpid));
}

TEST_F(precedence, trace_is_fourth)
{
	pid_t parent;
	long ret;

	parent = getppid();
	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->allow);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->log);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->trace);
	ASSERT_EQ(0, ret);
	 
	EXPECT_EQ(parent, syscall(__NR_getppid));
	 
	EXPECT_EQ(-1, syscall(__NR_getpid));
}

TEST_F(precedence, trace_is_fourth_in_any_order)
{
	pid_t parent;
	long ret;

	parent = getppid();
	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->trace);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->allow);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->log);
	ASSERT_EQ(0, ret);
	 
	EXPECT_EQ(parent, syscall(__NR_getppid));
	 
	EXPECT_EQ(-1, syscall(__NR_getpid));
}

TEST_F(precedence, log_is_fifth)
{
	pid_t mypid, parent;
	long ret;

	mypid = getpid();
	parent = getppid();
	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->allow);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->log);
	ASSERT_EQ(0, ret);
	 
	EXPECT_EQ(parent, syscall(__NR_getppid));
	 
	EXPECT_EQ(mypid, syscall(__NR_getpid));
}

TEST_F(precedence, log_is_fifth_in_any_order)
{
	pid_t mypid, parent;
	long ret;

	mypid = getpid();
	parent = getppid();
	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->log);
	ASSERT_EQ(0, ret);
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->allow);
	ASSERT_EQ(0, ret);
	 
	EXPECT_EQ(parent, syscall(__NR_getppid));
	 
	EXPECT_EQ(mypid, syscall(__NR_getpid));
}

#ifndef PTRACE_O_TRACESECCOMP
#define PTRACE_O_TRACESECCOMP	0x00000080
#endif

 
#if PTRACE_EVENT_SECCOMP != 7
#undef PTRACE_EVENT_SECCOMP
#endif

#ifndef PTRACE_EVENT_SECCOMP
#define PTRACE_EVENT_SECCOMP 7
#endif

#define PTRACE_EVENT_MASK(status) ((status) >> 16)
bool tracer_running;
void tracer_stop(int sig)
{
	tracer_running = false;
}

typedef void tracer_func_t(struct __test_metadata *_metadata,
			   pid_t tracee, int status, void *args);

void start_tracer(struct __test_metadata *_metadata, int fd, pid_t tracee,
	    tracer_func_t tracer_func, void *args, bool ptrace_syscall)
{
	int ret = -1;
	struct sigaction action = {
		.sa_handler = tracer_stop,
	};

	 
	tracer_running = true;
	ASSERT_EQ(0, sigaction(SIGUSR1, &action, NULL));

	errno = 0;
	while (ret == -1 && errno != EINVAL)
		ret = ptrace(PTRACE_ATTACH, tracee, NULL, 0);
	ASSERT_EQ(0, ret) {
		kill(tracee, SIGKILL);
	}
	 
	wait(NULL);

	ret = ptrace(PTRACE_SETOPTIONS, tracee, NULL, ptrace_syscall ?
						      PTRACE_O_TRACESYSGOOD :
						      PTRACE_O_TRACESECCOMP);
	ASSERT_EQ(0, ret) {
		TH_LOG("Failed to set PTRACE_O_TRACESECCOMP");
		kill(tracee, SIGKILL);
	}
	ret = ptrace(ptrace_syscall ? PTRACE_SYSCALL : PTRACE_CONT,
		     tracee, NULL, 0);
	ASSERT_EQ(0, ret);

	 
	ASSERT_EQ(1, write(fd, "A", 1));
	ASSERT_EQ(0, close(fd));

	 
	while (tracer_running) {
		int status;

		if (wait(&status) != tracee)
			continue;

		if (WIFSIGNALED(status)) {
			 
			return;
		}
		if (WIFEXITED(status)) {
			 
			return;
		}

		 
		ASSERT_EQ(WIFCONTINUED(status), false);
		ASSERT_EQ(WIFSTOPPED(status), true);
		ASSERT_EQ(WSTOPSIG(status) & SIGTRAP, SIGTRAP) {
			TH_LOG("Unexpected WSTOPSIG: %d", WSTOPSIG(status));
		}

		tracer_func(_metadata, tracee, status, args);

		ret = ptrace(ptrace_syscall ? PTRACE_SYSCALL : PTRACE_CONT,
			     tracee, NULL, 0);
		ASSERT_EQ(0, ret);
	}
	 
	syscall(__NR_exit, _metadata->passed ? EXIT_SUCCESS : EXIT_FAILURE);
}

 
void cont_handler(int num)
{ }
pid_t setup_trace_fixture(struct __test_metadata *_metadata,
			  tracer_func_t func, void *args, bool ptrace_syscall)
{
	char sync;
	int pipefd[2];
	pid_t tracer_pid;
	pid_t tracee = getpid();

	 
	ASSERT_EQ(0, pipe(pipefd));

	 
	tracer_pid = fork();
	ASSERT_LE(0, tracer_pid);
	signal(SIGALRM, cont_handler);
	if (tracer_pid == 0) {
		close(pipefd[0]);
		start_tracer(_metadata, pipefd[1], tracee, func, args,
			     ptrace_syscall);
		syscall(__NR_exit, 0);
	}
	close(pipefd[1]);
	prctl(PR_SET_PTRACER, tracer_pid, 0, 0, 0);
	read(pipefd[0], &sync, 1);
	close(pipefd[0]);

	return tracer_pid;
}

void teardown_trace_fixture(struct __test_metadata *_metadata,
			    pid_t tracer)
{
	if (tracer) {
		int status;
		 
		ASSERT_EQ(0, kill(tracer, SIGUSR1));
		ASSERT_EQ(tracer, waitpid(tracer, &status, 0));
		if (WEXITSTATUS(status))
			_metadata->passed = 0;
	}
}

 
struct tracer_args_poke_t {
	unsigned long poke_addr;
};

void tracer_poke(struct __test_metadata *_metadata, pid_t tracee, int status,
		 void *args)
{
	int ret;
	unsigned long msg;
	struct tracer_args_poke_t *info = (struct tracer_args_poke_t *)args;

	ret = ptrace(PTRACE_GETEVENTMSG, tracee, NULL, &msg);
	EXPECT_EQ(0, ret);
	 
	ASSERT_EQ(0x1001, msg) {
		kill(tracee, SIGKILL);
	}
	 
	ret = ptrace(PTRACE_POKEDATA, tracee, info->poke_addr, 0x1001);
	EXPECT_EQ(0, ret);
}

FIXTURE(TRACE_poke) {
	struct sock_fprog prog;
	pid_t tracer;
	long poked;
	struct tracer_args_poke_t tracer_args;
};

FIXTURE_SETUP(TRACE_poke)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_read, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_TRACE | 0x1001),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};

	self->poked = 0;
	memset(&self->prog, 0, sizeof(self->prog));
	self->prog.filter = malloc(sizeof(filter));
	ASSERT_NE(NULL, self->prog.filter);
	memcpy(self->prog.filter, filter, sizeof(filter));
	self->prog.len = (unsigned short)ARRAY_SIZE(filter);

	 
	self->tracer_args.poke_addr = (unsigned long)&self->poked;

	 
	self->tracer = setup_trace_fixture(_metadata, tracer_poke,
					   &self->tracer_args, false);
}

FIXTURE_TEARDOWN(TRACE_poke)
{
	teardown_trace_fixture(_metadata, self->tracer);
	if (self->prog.filter)
		free(self->prog.filter);
}

TEST_F(TRACE_poke, read_has_side_effects)
{
	ssize_t ret;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->prog, 0, 0);
	ASSERT_EQ(0, ret);

	EXPECT_EQ(0, self->poked);
	ret = read(-1, NULL, 0);
	EXPECT_EQ(-1, ret);
	EXPECT_EQ(0x1001, self->poked);
}

TEST_F(TRACE_poke, getpid_runs_normally)
{
	long ret;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &self->prog, 0, 0);
	ASSERT_EQ(0, ret);

	EXPECT_EQ(0, self->poked);
	EXPECT_NE(0, syscall(__NR_getpid));
	EXPECT_EQ(0, self->poked);
}

#if defined(__x86_64__)
# define ARCH_REGS		struct user_regs_struct
# define SYSCALL_NUM(_regs)	(_regs).orig_rax
# define SYSCALL_RET(_regs)	(_regs).rax
#elif defined(__i386__)
# define ARCH_REGS		struct user_regs_struct
# define SYSCALL_NUM(_regs)	(_regs).orig_eax
# define SYSCALL_RET(_regs)	(_regs).eax
#elif defined(__arm__)
# define ARCH_REGS		struct pt_regs
# define SYSCALL_NUM(_regs)	(_regs).ARM_r7
# ifndef PTRACE_SET_SYSCALL
#  define PTRACE_SET_SYSCALL   23
# endif
# define SYSCALL_NUM_SET(_regs, _nr)	\
		EXPECT_EQ(0, ptrace(PTRACE_SET_SYSCALL, tracee, NULL, _nr))
# define SYSCALL_RET(_regs)	(_regs).ARM_r0
#elif defined(__aarch64__)
# define ARCH_REGS		struct user_pt_regs
# define SYSCALL_NUM(_regs)	(_regs).regs[8]
# ifndef NT_ARM_SYSTEM_CALL
#  define NT_ARM_SYSTEM_CALL 0x404
# endif
# define SYSCALL_NUM_SET(_regs, _nr)				\
	do {							\
		struct iovec __v;				\
		typeof(_nr) __nr = (_nr);			\
		__v.iov_base = &__nr;				\
		__v.iov_len = sizeof(__nr);			\
		EXPECT_EQ(0, ptrace(PTRACE_SETREGSET, tracee,	\
				    NT_ARM_SYSTEM_CALL, &__v));	\
	} while (0)
# define SYSCALL_RET(_regs)	(_regs).regs[0]
#elif defined(__loongarch__)
# define ARCH_REGS		struct user_pt_regs
# define SYSCALL_NUM(_regs)	(_regs).regs[11]
# define SYSCALL_RET(_regs)	(_regs).regs[4]
#elif defined(__riscv) && __riscv_xlen == 64
# define ARCH_REGS		struct user_regs_struct
# define SYSCALL_NUM(_regs)	(_regs).a7
# define SYSCALL_RET(_regs)	(_regs).a0
#elif defined(__csky__)
# define ARCH_REGS		struct pt_regs
#  if defined(__CSKYABIV2__)
#   define SYSCALL_NUM(_regs)	(_regs).regs[3]
#  else
#   define SYSCALL_NUM(_regs)	(_regs).regs[9]
#  endif
# define SYSCALL_RET(_regs)	(_regs).a0
#elif defined(__hppa__)
# define ARCH_REGS		struct user_regs_struct
# define SYSCALL_NUM(_regs)	(_regs).gr[20]
# define SYSCALL_RET(_regs)	(_regs).gr[28]
#elif defined(__powerpc__)
# define ARCH_REGS		struct pt_regs
# define SYSCALL_NUM(_regs)	(_regs).gpr[0]
# define SYSCALL_RET(_regs)	(_regs).gpr[3]
# define SYSCALL_RET_SET(_regs, _val)				\
	do {							\
		typeof(_val) _result = (_val);			\
		if ((_regs.trap & 0xfff0) == 0x3000) {		\
			 					\
			SYSCALL_RET(_regs) = _result;		\
		} else {					\
			 					\
			if (_result < 0) {			\
				SYSCALL_RET(_regs) = -_result;	\
				(_regs).ccr |= 0x10000000;	\
			} else {				\
				SYSCALL_RET(_regs) = _result;	\
				(_regs).ccr &= ~0x10000000;	\
			}					\
		}						\
	} while (0)
# define SYSCALL_RET_SET_ON_PTRACE_EXIT
#elif defined(__s390__)
# define ARCH_REGS		s390_regs
# define SYSCALL_NUM(_regs)	(_regs).gprs[2]
# define SYSCALL_RET_SET(_regs, _val)			\
		TH_LOG("Can't modify syscall return on this architecture")
#elif defined(__mips__)
# include <asm/unistd_nr_n32.h>
# include <asm/unistd_nr_n64.h>
# include <asm/unistd_nr_o32.h>
# define ARCH_REGS		struct pt_regs
# define SYSCALL_NUM(_regs)				\
	({						\
		typeof((_regs).regs[2]) _nr;		\
		if ((_regs).regs[2] == __NR_O32_Linux)	\
			_nr = (_regs).regs[4];		\
		else					\
			_nr = (_regs).regs[2];		\
		_nr;					\
	})
# define SYSCALL_NUM_SET(_regs, _nr)			\
	do {						\
		if ((_regs).regs[2] == __NR_O32_Linux)	\
			(_regs).regs[4] = _nr;		\
		else					\
			(_regs).regs[2] = _nr;		\
	} while (0)
# define SYSCALL_RET_SET(_regs, _val)			\
		TH_LOG("Can't modify syscall return on this architecture")
#elif defined(__xtensa__)
# define ARCH_REGS		struct user_pt_regs
# define SYSCALL_NUM(_regs)	(_regs).syscall
 
#define SYSCALL_RET(_regs)	(_regs).a[(_regs).windowbase * 4 + 2]
#elif defined(__sh__)
# define ARCH_REGS		struct pt_regs
# define SYSCALL_NUM(_regs)	(_regs).regs[3]
# define SYSCALL_RET(_regs)	(_regs).regs[0]
#elif defined(__mc68000__)
# define ARCH_REGS		struct user_regs_struct
# define SYSCALL_NUM(_regs)	(_regs).orig_d0
# define SYSCALL_RET(_regs)	(_regs).d0
#else
# error "Do not know how to find your architecture's registers and syscalls"
#endif

 
#ifndef SYSCALL_NUM_SET
# define SYSCALL_NUM_SET(_regs, _nr)		\
	do {					\
		SYSCALL_NUM(_regs) = (_nr);	\
	} while (0)
#endif
 
#if !defined(SYSCALL_RET) && !defined(SYSCALL_RET_SET)
# error "One of SYSCALL_RET or SYSCALL_RET_SET is needed for this arch"
#endif
#ifndef SYSCALL_RET_SET
# define SYSCALL_RET_SET(_regs, _val)		\
	do {					\
		SYSCALL_RET(_regs) = (_val);	\
	} while (0)
#endif

 
#ifndef SYSCALL_RET
# define EXPECT_SYSCALL_RETURN(val, action)	EXPECT_EQ(-1, action)
#else
# define EXPECT_SYSCALL_RETURN(val, action)		\
	do {						\
		errno = 0;				\
		if (val < 0) {				\
			EXPECT_EQ(-1, action);		\
			EXPECT_EQ(-(val), errno);	\
		} else {				\
			EXPECT_EQ(val, action);		\
		}					\
	} while (0)
#endif

 
const bool ptrace_entry_set_syscall_nr = true;
const bool ptrace_entry_set_syscall_ret =
#ifndef SYSCALL_RET_SET_ON_PTRACE_EXIT
	true;
#else
	false;
#endif

 
#if defined(__x86_64__) || defined(__i386__) || defined(__mips__) || defined(__mc68000__)
# define ARCH_GETREGS(_regs)	ptrace(PTRACE_GETREGS, tracee, 0, &(_regs))
# define ARCH_SETREGS(_regs)	ptrace(PTRACE_SETREGS, tracee, 0, &(_regs))
#else
# define ARCH_GETREGS(_regs)	({					\
		struct iovec __v;					\
		__v.iov_base = &(_regs);				\
		__v.iov_len = sizeof(_regs);				\
		ptrace(PTRACE_GETREGSET, tracee, NT_PRSTATUS, &__v);	\
	})
# define ARCH_SETREGS(_regs)	({					\
		struct iovec __v;					\
		__v.iov_base = &(_regs);				\
		__v.iov_len = sizeof(_regs);				\
		ptrace(PTRACE_SETREGSET, tracee, NT_PRSTATUS, &__v);	\
	})
#endif

 
int get_syscall(struct __test_metadata *_metadata, pid_t tracee)
{
	ARCH_REGS regs;

	EXPECT_EQ(0, ARCH_GETREGS(regs)) {
		return -1;
	}

	return SYSCALL_NUM(regs);
}

 
void __change_syscall(struct __test_metadata *_metadata,
		    pid_t tracee, long *syscall, long *ret)
{
	ARCH_REGS orig, regs;

	 
	if (!syscall && !ret)
		return;

	EXPECT_EQ(0, ARCH_GETREGS(regs)) {
		return;
	}
	orig = regs;

	if (syscall)
		SYSCALL_NUM_SET(regs, *syscall);

	if (ret)
		SYSCALL_RET_SET(regs, *ret);

	 
	if (memcmp(&orig, &regs, sizeof(orig)) != 0)
		EXPECT_EQ(0, ARCH_SETREGS(regs));
}

 
void change_syscall_nr(struct __test_metadata *_metadata,
		       pid_t tracee, long syscall)
{
	__change_syscall(_metadata, tracee, &syscall, NULL);
}

 
void change_syscall_ret(struct __test_metadata *_metadata,
			pid_t tracee, long ret)
{
	long syscall = -1;

	__change_syscall(_metadata, tracee, &syscall, &ret);
}

void tracer_seccomp(struct __test_metadata *_metadata, pid_t tracee,
		    int status, void *args)
{
	int ret;
	unsigned long msg;

	EXPECT_EQ(PTRACE_EVENT_MASK(status), PTRACE_EVENT_SECCOMP) {
		TH_LOG("Unexpected ptrace event: %d", PTRACE_EVENT_MASK(status));
		return;
	}

	 
	ret = ptrace(PTRACE_GETEVENTMSG, tracee, NULL, &msg);
	EXPECT_EQ(0, ret);

	 
	switch (msg) {
	case 0x1002:
		 
		EXPECT_EQ(__NR_getpid, get_syscall(_metadata, tracee));
		change_syscall_nr(_metadata, tracee, __NR_getppid);
		break;
	case 0x1003:
		 
		EXPECT_EQ(__NR_gettid, get_syscall(_metadata, tracee));
		change_syscall_ret(_metadata, tracee, 45000);
		break;
	case 0x1004:
		 
		EXPECT_EQ(__NR_openat, get_syscall(_metadata, tracee));
		change_syscall_ret(_metadata, tracee, -ESRCH);
		break;
	case 0x1005:
		 
		EXPECT_EQ(__NR_getppid, get_syscall(_metadata, tracee));
		break;
	default:
		EXPECT_EQ(0, msg) {
			TH_LOG("Unknown PTRACE_GETEVENTMSG: 0x%lx", msg);
			kill(tracee, SIGKILL);
		}
	}

}

FIXTURE(TRACE_syscall) {
	struct sock_fprog prog;
	pid_t tracer, mytid, mypid, parent;
	long syscall_nr;
};

void tracer_ptrace(struct __test_metadata *_metadata, pid_t tracee,
		   int status, void *args)
{
	int ret;
	unsigned long msg;
	static bool entry;
	long syscall_nr_val, syscall_ret_val;
	long *syscall_nr = NULL, *syscall_ret = NULL;
	FIXTURE_DATA(TRACE_syscall) *self = args;

	EXPECT_EQ(WSTOPSIG(status) & 0x80, 0x80) {
		TH_LOG("Unexpected WSTOPSIG: %d", WSTOPSIG(status));
		return;
	}

	 
	entry = !entry;

	 
	ret = ptrace(PTRACE_GETEVENTMSG, tracee, NULL, &msg);
	EXPECT_EQ(0, ret);
	EXPECT_EQ(entry ? PTRACE_EVENTMSG_SYSCALL_ENTRY
			: PTRACE_EVENTMSG_SYSCALL_EXIT, msg);

	 
	if (entry)
		self->syscall_nr = get_syscall(_metadata, tracee);

	 
	if (entry == ptrace_entry_set_syscall_nr)
		syscall_nr = &syscall_nr_val;
	if (entry == ptrace_entry_set_syscall_ret)
		syscall_ret = &syscall_ret_val;

	 
	switch (self->syscall_nr) {
	case __NR_getpid:
		syscall_nr_val = __NR_getppid;
		 
		syscall_ret = NULL;
		break;
	case __NR_gettid:
		syscall_nr_val = -1;
		syscall_ret_val = 45000;
		break;
	case __NR_openat:
		syscall_nr_val = -1;
		syscall_ret_val = -ESRCH;
		break;
	default:
		 
		return;
	}

	__change_syscall(_metadata, tracee, syscall_nr, syscall_ret);
}

FIXTURE_VARIANT(TRACE_syscall) {
	 
	bool use_ptrace;
};

FIXTURE_VARIANT_ADD(TRACE_syscall, ptrace) {
	.use_ptrace = true,
};

FIXTURE_VARIANT_ADD(TRACE_syscall, seccomp) {
	.use_ptrace = false,
};

FIXTURE_SETUP(TRACE_syscall)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_getpid, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_TRACE | 0x1002),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_gettid, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_TRACE | 0x1003),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_openat, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_TRACE | 0x1004),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_getppid, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_TRACE | 0x1005),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;

	 
	self->mytid = syscall(__NR_gettid);
	ASSERT_GT(self->mytid, 0);
	ASSERT_NE(self->mytid, 1) {
		TH_LOG("Running this test as init is not supported. :)");
	}

	self->mypid = getpid();
	ASSERT_GT(self->mypid, 0);
	ASSERT_EQ(self->mytid, self->mypid);

	self->parent = getppid();
	ASSERT_GT(self->parent, 0);
	ASSERT_NE(self->parent, self->mypid);

	 
	self->tracer = setup_trace_fixture(_metadata,
					   variant->use_ptrace ? tracer_ptrace
							       : tracer_seccomp,
					   self, variant->use_ptrace);

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	 
	if (variant->use_ptrace)
		return;

	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog, 0, 0);
	ASSERT_EQ(0, ret);
}

FIXTURE_TEARDOWN(TRACE_syscall)
{
	teardown_trace_fixture(_metadata, self->tracer);
}

TEST(negative_ENOSYS)
{
#if defined(__arm__)
	SKIP(return, "arm32 does not support calling syscall -1");
#endif
	 
	errno = 0;
	EXPECT_EQ(-1, syscall(-1));
	EXPECT_EQ(errno, ENOSYS);
	 
	errno = 0;
	EXPECT_EQ(-1, syscall(-101));
	EXPECT_EQ(errno, ENOSYS);
}

TEST_F(TRACE_syscall, negative_ENOSYS)
{
	negative_ENOSYS(_metadata);
}

TEST_F(TRACE_syscall, syscall_allowed)
{
	 
	EXPECT_EQ(self->parent, syscall(__NR_getppid));
	EXPECT_NE(self->mypid, syscall(__NR_getppid));
}

TEST_F(TRACE_syscall, syscall_redirected)
{
	 
	EXPECT_EQ(self->parent, syscall(__NR_getpid));
	EXPECT_NE(self->mypid, syscall(__NR_getpid));
}

TEST_F(TRACE_syscall, syscall_errno)
{
	 
	EXPECT_SYSCALL_RETURN(-ESRCH, syscall(__NR_openat));
}

TEST_F(TRACE_syscall, syscall_faked)
{
	 
	EXPECT_SYSCALL_RETURN(45000, syscall(__NR_gettid));
}

TEST_F_SIGNAL(TRACE_syscall, kill_immediate, SIGSYS)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_mknodat, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_KILL_THREAD),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;

	 
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog, 0, 0);
	ASSERT_EQ(0, ret);

	 
	EXPECT_EQ(-1, syscall(__NR_mknodat, -1, NULL, 0, 0));
}

TEST_F(TRACE_syscall, skip_after)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_getppid, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ERRNO | EPERM),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;

	 
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog, 0, 0);
	ASSERT_EQ(0, ret);

	 
	errno = 0;
	EXPECT_EQ(-1, syscall(__NR_getpid));
	EXPECT_EQ(EPERM, errno);
}

TEST_F_SIGNAL(TRACE_syscall, kill_after, SIGSYS)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_getppid, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_KILL),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;

	 
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog, 0, 0);
	ASSERT_EQ(0, ret);

	 
	EXPECT_NE(self->mypid, syscall(__NR_getpid));
}

TEST(seccomp_syscall)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	 
	ret = seccomp(-1, 0, &prog);
	ASSERT_NE(ENOSYS, errno) {
		TH_LOG("Kernel does not support seccomp syscall!");
	}
	EXPECT_EQ(EINVAL, errno) {
		TH_LOG("Did not reject crazy op value!");
	}

	 
	ret = seccomp(SECCOMP_SET_MODE_STRICT, -1, NULL);
	EXPECT_EQ(EINVAL, errno) {
		TH_LOG("Did not reject mode strict with flags!");
	}
	ret = seccomp(SECCOMP_SET_MODE_STRICT, 0, &prog);
	EXPECT_EQ(EINVAL, errno) {
		TH_LOG("Did not reject mode strict with uargs!");
	}

	 
	ret = seccomp(SECCOMP_SET_MODE_FILTER, -1, &prog);
	EXPECT_EQ(EINVAL, errno) {
		TH_LOG("Did not reject crazy filter flags!");
	}
	ret = seccomp(SECCOMP_SET_MODE_FILTER, 0, NULL);
	EXPECT_EQ(EFAULT, errno) {
		TH_LOG("Did not reject NULL filter!");
	}

	ret = seccomp(SECCOMP_SET_MODE_FILTER, 0, &prog);
	EXPECT_EQ(0, errno) {
		TH_LOG("Kernel does not support SECCOMP_SET_MODE_FILTER: %s",
			strerror(errno));
	}
}

TEST(seccomp_syscall_mode_lock)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, NULL, 0, 0);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	ret = seccomp(SECCOMP_SET_MODE_FILTER, 0, &prog);
	ASSERT_NE(ENOSYS, errno) {
		TH_LOG("Kernel does not support seccomp syscall!");
	}
	EXPECT_EQ(0, ret) {
		TH_LOG("Could not install filter!");
	}

	 
	ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT, 0, 0, 0);
	EXPECT_EQ(EINVAL, errno) {
		TH_LOG("Switched to mode strict!");
	}

	ret = seccomp(SECCOMP_SET_MODE_STRICT, 0, NULL);
	EXPECT_EQ(EINVAL, errno) {
		TH_LOG("Switched to mode strict!");
	}
}

 
TEST(detect_seccomp_filter_flags)
{
	unsigned int flags[] = { SECCOMP_FILTER_FLAG_TSYNC,
				 SECCOMP_FILTER_FLAG_LOG,
				 SECCOMP_FILTER_FLAG_SPEC_ALLOW,
				 SECCOMP_FILTER_FLAG_NEW_LISTENER,
				 SECCOMP_FILTER_FLAG_TSYNC_ESRCH };
	unsigned int exclusive[] = {
				SECCOMP_FILTER_FLAG_TSYNC,
				SECCOMP_FILTER_FLAG_NEW_LISTENER };
	unsigned int flag, all_flags, exclusive_mask;
	int i;
	long ret;

	 
	for (i = 0, all_flags = 0; i < ARRAY_SIZE(flags); i++) {
		int bits = 0;

		flag = flags[i];
		 
		while (flag) {
			if (flag & 0x1)
				bits ++;
			flag >>= 1;
		}
		ASSERT_EQ(1, bits);
		flag = flags[i];

		ret = seccomp(SECCOMP_SET_MODE_FILTER, flag, NULL);
		ASSERT_NE(ENOSYS, errno) {
			TH_LOG("Kernel does not support seccomp syscall!");
		}
		EXPECT_EQ(-1, ret);
		EXPECT_EQ(EFAULT, errno) {
			TH_LOG("Failed to detect that a known-good filter flag (0x%X) is supported!",
			       flag);
		}

		all_flags |= flag;
	}

	 
	exclusive_mask = 0;
	for (i = 0; i < ARRAY_SIZE(exclusive); i++)
		exclusive_mask |= exclusive[i];
	for (i = 0; i < ARRAY_SIZE(exclusive); i++) {
		flag = all_flags & ~exclusive_mask;
		flag |= exclusive[i];

		ret = seccomp(SECCOMP_SET_MODE_FILTER, flag, NULL);
		EXPECT_EQ(-1, ret);
		EXPECT_EQ(EFAULT, errno) {
			TH_LOG("Failed to detect that all known-good filter flags (0x%X) are supported!",
			       flag);
		}
	}

	 
	flag = -1;
	flag &= ~exclusive_mask;
	ret = seccomp(SECCOMP_SET_MODE_FILTER, flag, NULL);
	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EINVAL, errno) {
		TH_LOG("Failed to detect that an unknown filter flag (0x%X) is unsupported!",
		       flag);
	}

	 
	flag = flags[ARRAY_SIZE(flags) - 1] << 1;
	ret = seccomp(SECCOMP_SET_MODE_FILTER, flag, NULL);
	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EINVAL, errno) {
		TH_LOG("Failed to detect that an unknown filter flag (0x%X) is unsupported! Does a new flag need to be added to this test?",
		       flag);
	}
}

TEST(TSYNC_first)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
	long ret;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, NULL, 0, 0);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	ret = seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_TSYNC,
		      &prog);
	ASSERT_NE(ENOSYS, errno) {
		TH_LOG("Kernel does not support seccomp syscall!");
	}
	EXPECT_EQ(0, ret) {
		TH_LOG("Could not install initial filter with TSYNC!");
	}
}

#define TSYNC_SIBLINGS 2
struct tsync_sibling {
	pthread_t tid;
	pid_t system_tid;
	sem_t *started;
	pthread_cond_t *cond;
	pthread_mutex_t *mutex;
	int diverge;
	int num_waits;
	struct sock_fprog *prog;
	struct __test_metadata *metadata;
};

 
#define PTHREAD_JOIN(tid, status)					\
	do {								\
		int _rc = pthread_join(tid, status);			\
		if (_rc) {						\
			TH_LOG("pthread_join of tid %u failed: %d\n",	\
				(unsigned int)tid, _rc);		\
		} else {						\
			tid = 0;					\
		}							\
	} while (0)

FIXTURE(TSYNC) {
	struct sock_fprog root_prog, apply_prog;
	struct tsync_sibling sibling[TSYNC_SIBLINGS];
	sem_t started;
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	int sibling_count;
};

FIXTURE_SETUP(TSYNC)
{
	struct sock_filter root_filter[] = {
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_filter apply_filter[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_read, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_KILL),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};

	memset(&self->root_prog, 0, sizeof(self->root_prog));
	memset(&self->apply_prog, 0, sizeof(self->apply_prog));
	memset(&self->sibling, 0, sizeof(self->sibling));
	self->root_prog.filter = malloc(sizeof(root_filter));
	ASSERT_NE(NULL, self->root_prog.filter);
	memcpy(self->root_prog.filter, &root_filter, sizeof(root_filter));
	self->root_prog.len = (unsigned short)ARRAY_SIZE(root_filter);

	self->apply_prog.filter = malloc(sizeof(apply_filter));
	ASSERT_NE(NULL, self->apply_prog.filter);
	memcpy(self->apply_prog.filter, &apply_filter, sizeof(apply_filter));
	self->apply_prog.len = (unsigned short)ARRAY_SIZE(apply_filter);

	self->sibling_count = 0;
	pthread_mutex_init(&self->mutex, NULL);
	pthread_cond_init(&self->cond, NULL);
	sem_init(&self->started, 0, 0);
	self->sibling[0].tid = 0;
	self->sibling[0].cond = &self->cond;
	self->sibling[0].started = &self->started;
	self->sibling[0].mutex = &self->mutex;
	self->sibling[0].diverge = 0;
	self->sibling[0].num_waits = 1;
	self->sibling[0].prog = &self->root_prog;
	self->sibling[0].metadata = _metadata;
	self->sibling[1].tid = 0;
	self->sibling[1].cond = &self->cond;
	self->sibling[1].started = &self->started;
	self->sibling[1].mutex = &self->mutex;
	self->sibling[1].diverge = 0;
	self->sibling[1].prog = &self->root_prog;
	self->sibling[1].num_waits = 1;
	self->sibling[1].metadata = _metadata;
}

FIXTURE_TEARDOWN(TSYNC)
{
	int sib = 0;

	if (self->root_prog.filter)
		free(self->root_prog.filter);
	if (self->apply_prog.filter)
		free(self->apply_prog.filter);

	for ( ; sib < self->sibling_count; ++sib) {
		struct tsync_sibling *s = &self->sibling[sib];

		if (!s->tid)
			continue;
		 
		pthread_kill(s->tid, 9);
	}
	pthread_mutex_destroy(&self->mutex);
	pthread_cond_destroy(&self->cond);
	sem_destroy(&self->started);
}

void *tsync_sibling(void *data)
{
	long ret = 0;
	struct tsync_sibling *me = data;

	me->system_tid = syscall(__NR_gettid);

	pthread_mutex_lock(me->mutex);
	if (me->diverge) {
		 
		ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER,
				me->prog, 0, 0);
	}
	sem_post(me->started);
	 
	if (ret) {
		pthread_mutex_unlock(me->mutex);
		return (void *)SIBLING_EXIT_FAILURE;
	}
	do {
		pthread_cond_wait(me->cond, me->mutex);
		me->num_waits = me->num_waits - 1;
	} while (me->num_waits);
	pthread_mutex_unlock(me->mutex);

	ret = prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0);
	if (!ret)
		return (void *)SIBLING_EXIT_NEWPRIVS;
	read(-1, NULL, 0);
	return (void *)SIBLING_EXIT_UNKILLED;
}

void tsync_start_sibling(struct tsync_sibling *sibling)
{
	pthread_create(&sibling->tid, NULL, tsync_sibling, (void *)sibling);
}

TEST_F(TSYNC, siblings_fail_prctl)
{
	long ret;
	void *status;
	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_prctl, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ERRNO | EINVAL),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};

	ASSERT_EQ(0, prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	 
	ret = seccomp(SECCOMP_SET_MODE_FILTER, 0, &prog);
	ASSERT_NE(ENOSYS, errno) {
		TH_LOG("Kernel does not support seccomp syscall!");
	}
	ASSERT_EQ(0, ret) {
		TH_LOG("setting filter failed");
	}

	self->sibling[0].diverge = 1;
	tsync_start_sibling(&self->sibling[0]);
	tsync_start_sibling(&self->sibling[1]);

	while (self->sibling_count < TSYNC_SIBLINGS) {
		sem_wait(&self->started);
		self->sibling_count++;
	}

	 
	pthread_mutex_lock(&self->mutex);
	ASSERT_EQ(0, pthread_cond_broadcast(&self->cond)) {
		TH_LOG("cond broadcast non-zero");
	}
	pthread_mutex_unlock(&self->mutex);

	 
	PTHREAD_JOIN(self->sibling[0].tid, &status);
	EXPECT_EQ(SIBLING_EXIT_FAILURE, (long)status);
	PTHREAD_JOIN(self->sibling[1].tid, &status);
	EXPECT_EQ(SIBLING_EXIT_UNKILLED, (long)status);
}

TEST_F(TSYNC, two_siblings_with_ancestor)
{
	long ret;
	void *status;

	ASSERT_EQ(0, prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	ret = seccomp(SECCOMP_SET_MODE_FILTER, 0, &self->root_prog);
	ASSERT_NE(ENOSYS, errno) {
		TH_LOG("Kernel does not support seccomp syscall!");
	}
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support SECCOMP_SET_MODE_FILTER!");
	}
	tsync_start_sibling(&self->sibling[0]);
	tsync_start_sibling(&self->sibling[1]);

	while (self->sibling_count < TSYNC_SIBLINGS) {
		sem_wait(&self->started);
		self->sibling_count++;
	}

	ret = seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_TSYNC,
		      &self->apply_prog);
	ASSERT_EQ(0, ret) {
		TH_LOG("Could install filter on all threads!");
	}
	 
	pthread_mutex_lock(&self->mutex);
	ASSERT_EQ(0, pthread_cond_broadcast(&self->cond)) {
		TH_LOG("cond broadcast non-zero");
	}
	pthread_mutex_unlock(&self->mutex);
	 
	PTHREAD_JOIN(self->sibling[0].tid, &status);
	EXPECT_EQ(0x0, (long)status);
	PTHREAD_JOIN(self->sibling[1].tid, &status);
	EXPECT_EQ(0x0, (long)status);
}

TEST_F(TSYNC, two_sibling_want_nnp)
{
	void *status;

	 
	tsync_start_sibling(&self->sibling[0]);
	tsync_start_sibling(&self->sibling[1]);
	while (self->sibling_count < TSYNC_SIBLINGS) {
		sem_wait(&self->started);
		self->sibling_count++;
	}

	 
	pthread_mutex_lock(&self->mutex);
	ASSERT_EQ(0, pthread_cond_broadcast(&self->cond)) {
		TH_LOG("cond broadcast non-zero");
	}
	pthread_mutex_unlock(&self->mutex);

	 
	PTHREAD_JOIN(self->sibling[0].tid, &status);
	EXPECT_EQ(SIBLING_EXIT_NEWPRIVS, (long)status);
	PTHREAD_JOIN(self->sibling[1].tid, &status);
	EXPECT_EQ(SIBLING_EXIT_NEWPRIVS, (long)status);
}

TEST_F(TSYNC, two_siblings_with_no_filter)
{
	long ret;
	void *status;

	 
	tsync_start_sibling(&self->sibling[0]);
	tsync_start_sibling(&self->sibling[1]);
	while (self->sibling_count < TSYNC_SIBLINGS) {
		sem_wait(&self->started);
		self->sibling_count++;
	}

	ASSERT_EQ(0, prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	ret = seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_TSYNC,
		      &self->apply_prog);
	ASSERT_NE(ENOSYS, errno) {
		TH_LOG("Kernel does not support seccomp syscall!");
	}
	ASSERT_EQ(0, ret) {
		TH_LOG("Could install filter on all threads!");
	}

	 
	pthread_mutex_lock(&self->mutex);
	ASSERT_EQ(0, pthread_cond_broadcast(&self->cond)) {
		TH_LOG("cond broadcast non-zero");
	}
	pthread_mutex_unlock(&self->mutex);

	 
	PTHREAD_JOIN(self->sibling[0].tid, &status);
	EXPECT_EQ(0x0, (long)status);
	PTHREAD_JOIN(self->sibling[1].tid, &status);
	EXPECT_EQ(0x0, (long)status);
}

TEST_F(TSYNC, two_siblings_with_one_divergence)
{
	long ret;
	void *status;

	ASSERT_EQ(0, prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	ret = seccomp(SECCOMP_SET_MODE_FILTER, 0, &self->root_prog);
	ASSERT_NE(ENOSYS, errno) {
		TH_LOG("Kernel does not support seccomp syscall!");
	}
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support SECCOMP_SET_MODE_FILTER!");
	}
	self->sibling[0].diverge = 1;
	tsync_start_sibling(&self->sibling[0]);
	tsync_start_sibling(&self->sibling[1]);

	while (self->sibling_count < TSYNC_SIBLINGS) {
		sem_wait(&self->started);
		self->sibling_count++;
	}

	ret = seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_TSYNC,
		      &self->apply_prog);
	ASSERT_EQ(self->sibling[0].system_tid, ret) {
		TH_LOG("Did not fail on diverged sibling.");
	}

	 
	pthread_mutex_lock(&self->mutex);
	ASSERT_EQ(0, pthread_cond_broadcast(&self->cond)) {
		TH_LOG("cond broadcast non-zero");
	}
	pthread_mutex_unlock(&self->mutex);

	 
	PTHREAD_JOIN(self->sibling[0].tid, &status);
	EXPECT_EQ(SIBLING_EXIT_UNKILLED, (long)status);
	PTHREAD_JOIN(self->sibling[1].tid, &status);
	EXPECT_EQ(SIBLING_EXIT_UNKILLED, (long)status);
}

TEST_F(TSYNC, two_siblings_with_one_divergence_no_tid_in_err)
{
	long ret, flags;
	void *status;

	ASSERT_EQ(0, prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	ret = seccomp(SECCOMP_SET_MODE_FILTER, 0, &self->root_prog);
	ASSERT_NE(ENOSYS, errno) {
		TH_LOG("Kernel does not support seccomp syscall!");
	}
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support SECCOMP_SET_MODE_FILTER!");
	}
	self->sibling[0].diverge = 1;
	tsync_start_sibling(&self->sibling[0]);
	tsync_start_sibling(&self->sibling[1]);

	while (self->sibling_count < TSYNC_SIBLINGS) {
		sem_wait(&self->started);
		self->sibling_count++;
	}

	flags = SECCOMP_FILTER_FLAG_TSYNC | \
		SECCOMP_FILTER_FLAG_TSYNC_ESRCH;
	ret = seccomp(SECCOMP_SET_MODE_FILTER, flags, &self->apply_prog);
	ASSERT_EQ(ESRCH, errno) {
		TH_LOG("Did not return ESRCH for diverged sibling.");
	}
	ASSERT_EQ(-1, ret) {
		TH_LOG("Did not fail on diverged sibling.");
	}

	 
	pthread_mutex_lock(&self->mutex);
	ASSERT_EQ(0, pthread_cond_broadcast(&self->cond)) {
		TH_LOG("cond broadcast non-zero");
	}
	pthread_mutex_unlock(&self->mutex);

	 
	PTHREAD_JOIN(self->sibling[0].tid, &status);
	EXPECT_EQ(SIBLING_EXIT_UNKILLED, (long)status);
	PTHREAD_JOIN(self->sibling[1].tid, &status);
	EXPECT_EQ(SIBLING_EXIT_UNKILLED, (long)status);
}

TEST_F(TSYNC, two_siblings_not_under_filter)
{
	long ret, sib;
	void *status;
	struct timespec delay = { .tv_nsec = 100000000 };

	ASSERT_EQ(0, prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	 
	self->sibling[0].diverge = 1;
	tsync_start_sibling(&self->sibling[0]);
	tsync_start_sibling(&self->sibling[1]);

	while (self->sibling_count < TSYNC_SIBLINGS) {
		sem_wait(&self->started);
		self->sibling_count++;
	}

	ret = seccomp(SECCOMP_SET_MODE_FILTER, 0, &self->root_prog);
	ASSERT_NE(ENOSYS, errno) {
		TH_LOG("Kernel does not support seccomp syscall!");
	}
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support SECCOMP_SET_MODE_FILTER!");
	}

	ret = seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_TSYNC,
		      &self->apply_prog);
	ASSERT_EQ(ret, self->sibling[0].system_tid) {
		TH_LOG("Did not fail on diverged sibling.");
	}
	sib = 1;
	if (ret == self->sibling[0].system_tid)
		sib = 0;

	pthread_mutex_lock(&self->mutex);

	 
	self->sibling[!sib].num_waits += 1;

	 
	ASSERT_EQ(0, pthread_cond_broadcast(&self->cond)) {
		TH_LOG("cond broadcast non-zero");
	}
	pthread_mutex_unlock(&self->mutex);
	PTHREAD_JOIN(self->sibling[sib].tid, &status);
	EXPECT_EQ(SIBLING_EXIT_UNKILLED, (long)status);
	 
	while (!kill(self->sibling[sib].system_tid, 0))
		nanosleep(&delay, NULL);
	 
	sib = !sib;

	ret = seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_TSYNC,
		      &self->apply_prog);
	ASSERT_EQ(0, ret) {
		TH_LOG("Expected the remaining sibling to sync");
	};

	pthread_mutex_lock(&self->mutex);

	 
	if (self->sibling[sib].num_waits > 1)
		self->sibling[sib].num_waits = 1;
	ASSERT_EQ(0, pthread_cond_broadcast(&self->cond)) {
		TH_LOG("cond broadcast non-zero");
	}
	pthread_mutex_unlock(&self->mutex);
	PTHREAD_JOIN(self->sibling[sib].tid, &status);
	EXPECT_EQ(0, (long)status);
	 
	while (!kill(self->sibling[sib].system_tid, 0))
		nanosleep(&delay, NULL);

	ret = seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_TSYNC,
		      &self->apply_prog);
	ASSERT_EQ(0, ret);   
}

 
TEST(syscall_restart)
{
	long ret;
	unsigned long msg;
	pid_t child_pid;
	int pipefd[2];
	int status;
	siginfo_t info = { };
	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			 offsetof(struct seccomp_data, nr)),

#ifdef __NR_sigreturn
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_sigreturn, 7, 0),
#endif
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_read, 6, 0),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_exit, 5, 0),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_rt_sigreturn, 4, 0),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_nanosleep, 5, 0),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_clock_nanosleep, 4, 0),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_restart_syscall, 4, 0),

		 
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_write, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_KILL),
		 
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_TRACE|0x100),
		 
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_TRACE|0x200),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};
#if defined(__arm__)
	struct utsname utsbuf;
#endif

	ASSERT_EQ(0, pipe(pipefd));

	child_pid = fork();
	ASSERT_LE(0, child_pid);
	if (child_pid == 0) {
		 
		char buf = ' ';
		struct timespec timeout = { };

		 
		EXPECT_EQ(0, ptrace(PTRACE_TRACEME));
		EXPECT_EQ(0, raise(SIGSTOP));

		EXPECT_EQ(0, close(pipefd[1]));

		EXPECT_EQ(0, prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
			TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
		}

		ret = prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog, 0, 0);
		EXPECT_EQ(0, ret) {
			TH_LOG("Failed to install filter!");
		}

		EXPECT_EQ(1, read(pipefd[0], &buf, 1)) {
			TH_LOG("Failed to read() sync from parent");
		}
		EXPECT_EQ('.', buf) {
			TH_LOG("Failed to get sync data from read()");
		}

		 
		timeout.tv_sec = 1;
		errno = 0;
		EXPECT_EQ(0, nanosleep(&timeout, NULL)) {
			TH_LOG("Call to nanosleep() failed (errno %d: %s)",
				errno, strerror(errno));
		}

		 
		EXPECT_EQ(1, read(pipefd[0], &buf, 1)) {
			TH_LOG("Failed final read() from parent");
		}
		EXPECT_EQ('!', buf) {
			TH_LOG("Failed to get final data from read()");
		}

		 
		syscall(__NR_exit, _metadata->passed ? EXIT_SUCCESS
						     : EXIT_FAILURE);
	}
	EXPECT_EQ(0, close(pipefd[0]));

	 
	ASSERT_EQ(child_pid, waitpid(child_pid, &status, 0));
	ASSERT_EQ(true, WIFSTOPPED(status));
	ASSERT_EQ(0, ptrace(PTRACE_SETOPTIONS, child_pid, NULL,
			    PTRACE_O_TRACESECCOMP));
	ASSERT_EQ(0, ptrace(PTRACE_CONT, child_pid, NULL, 0));
	ASSERT_EQ(1, write(pipefd[1], ".", 1));

	 
	ASSERT_EQ(child_pid, waitpid(child_pid, &status, 0));
	ASSERT_EQ(true, WIFSTOPPED(status));
	ASSERT_EQ(SIGTRAP, WSTOPSIG(status));
	ASSERT_EQ(PTRACE_EVENT_SECCOMP, (status >> 16));
	ASSERT_EQ(0, ptrace(PTRACE_GETEVENTMSG, child_pid, NULL, &msg));
	ASSERT_EQ(0x100, msg);
	ret = get_syscall(_metadata, child_pid);
	EXPECT_TRUE(ret == __NR_nanosleep || ret == __NR_clock_nanosleep);

	 
	ASSERT_EQ(0, ptrace(PTRACE_GETSIGINFO, child_pid, NULL, &info));
	ASSERT_EQ(SIGTRAP, info.si_signo);
	ASSERT_EQ(SIGTRAP | (PTRACE_EVENT_SECCOMP << 8), info.si_code);
	EXPECT_EQ(0, info.si_errno);
	EXPECT_EQ(getuid(), info.si_uid);
	 
	EXPECT_EQ(child_pid, info.si_pid);

	 
	ASSERT_EQ(0, kill(child_pid, SIGSTOP));
	ASSERT_EQ(0, ptrace(PTRACE_CONT, child_pid, NULL, 0));
	ASSERT_EQ(child_pid, waitpid(child_pid, &status, 0));
	ASSERT_EQ(true, WIFSTOPPED(status));
	ASSERT_EQ(SIGSTOP, WSTOPSIG(status));
	ASSERT_EQ(0, ptrace(PTRACE_GETSIGINFO, child_pid, NULL, &info));
	 
	EXPECT_EQ(SIGSTOP, info.si_signo);

	 
	ASSERT_EQ(0, kill(child_pid, SIGCONT));
	ASSERT_EQ(0, ptrace(PTRACE_CONT, child_pid, NULL, 0));
	ASSERT_EQ(child_pid, waitpid(child_pid, &status, 0));
	ASSERT_EQ(true, WIFSTOPPED(status));
	ASSERT_EQ(SIGCONT, WSTOPSIG(status));
	ASSERT_EQ(0, ptrace(PTRACE_CONT, child_pid, NULL, 0));

	 
	ASSERT_EQ(child_pid, waitpid(child_pid, &status, 0));
	ASSERT_EQ(true, WIFSTOPPED(status));
	ASSERT_EQ(SIGTRAP, WSTOPSIG(status));
	ASSERT_EQ(PTRACE_EVENT_SECCOMP, (status >> 16));
	ASSERT_EQ(0, ptrace(PTRACE_GETEVENTMSG, child_pid, NULL, &msg));

	ASSERT_EQ(0x200, msg);
	ret = get_syscall(_metadata, child_pid);
#if defined(__arm__)
	 
	ASSERT_EQ(0, uname(&utsbuf));
	if (strncmp(utsbuf.machine, "arm", 3) == 0) {
		EXPECT_EQ(__NR_nanosleep, ret);
	} else
#endif
	{
		EXPECT_EQ(__NR_restart_syscall, ret);
	}

	 
	ASSERT_EQ(0, ptrace(PTRACE_CONT, child_pid, NULL, 0));
	ASSERT_EQ(1, write(pipefd[1], "!", 1));
	EXPECT_EQ(0, close(pipefd[1]));

	ASSERT_EQ(child_pid, waitpid(child_pid, &status, 0));
	if (WIFSIGNALED(status) || WEXITSTATUS(status))
		_metadata->passed = 0;
}

TEST_SIGNAL(filter_flag_log, SIGSYS)
{
	struct sock_filter allow_filter[] = {
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_filter kill_filter[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, __NR_getpid, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_KILL),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog allow_prog = {
		.len = (unsigned short)ARRAY_SIZE(allow_filter),
		.filter = allow_filter,
	};
	struct sock_fprog kill_prog = {
		.len = (unsigned short)ARRAY_SIZE(kill_filter),
		.filter = kill_filter,
	};
	long ret;
	pid_t parent = getppid();

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret);

	 
	ret = seccomp(SECCOMP_SET_MODE_STRICT, SECCOMP_FILTER_FLAG_LOG,
		      &allow_prog);
	ASSERT_NE(ENOSYS, errno) {
		TH_LOG("Kernel does not support seccomp syscall!");
	}
	EXPECT_NE(0, ret) {
		TH_LOG("Kernel accepted FILTER_FLAG_LOG flag in strict mode!");
	}
	EXPECT_EQ(EINVAL, errno) {
		TH_LOG("Kernel returned unexpected errno for FILTER_FLAG_LOG flag in strict mode!");
	}

	 
	ret = seccomp(SECCOMP_SET_MODE_FILTER, 0, &allow_prog);
	EXPECT_EQ(0, ret);

	 
	ret = seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_LOG,
		      &allow_prog);
	ASSERT_NE(EINVAL, errno) {
		TH_LOG("Kernel does not support the FILTER_FLAG_LOG flag!");
	}
	EXPECT_EQ(0, ret);

	 
	ret = seccomp(SECCOMP_SET_MODE_FILTER, SECCOMP_FILTER_FLAG_LOG,
		      &kill_prog);
	EXPECT_EQ(0, ret);

	EXPECT_EQ(parent, syscall(__NR_getppid));
	 
	EXPECT_EQ(0, syscall(__NR_getpid));
}

TEST(get_action_avail)
{
	__u32 actions[] = { SECCOMP_RET_KILL_THREAD, SECCOMP_RET_TRAP,
			    SECCOMP_RET_ERRNO, SECCOMP_RET_TRACE,
			    SECCOMP_RET_LOG,   SECCOMP_RET_ALLOW };
	__u32 unknown_action = 0x10000000U;
	int i;
	long ret;

	ret = seccomp(SECCOMP_GET_ACTION_AVAIL, 0, &actions[0]);
	ASSERT_NE(ENOSYS, errno) {
		TH_LOG("Kernel does not support seccomp syscall!");
	}
	ASSERT_NE(EINVAL, errno) {
		TH_LOG("Kernel does not support SECCOMP_GET_ACTION_AVAIL operation!");
	}
	EXPECT_EQ(ret, 0);

	for (i = 0; i < ARRAY_SIZE(actions); i++) {
		ret = seccomp(SECCOMP_GET_ACTION_AVAIL, 0, &actions[i]);
		EXPECT_EQ(ret, 0) {
			TH_LOG("Expected action (0x%X) not available!",
			       actions[i]);
		}
	}

	 
	ret = seccomp(SECCOMP_GET_ACTION_AVAIL, 0, &unknown_action);
	EXPECT_EQ(ret, -1);
	EXPECT_EQ(errno, EOPNOTSUPP);
}

TEST(get_metadata)
{
	pid_t pid;
	int pipefd[2];
	char buf;
	struct seccomp_metadata md;
	long ret;

	 
	if (geteuid()) {
		SKIP(return, "get_metadata requires real root");
		return;
	}

	ASSERT_EQ(0, pipe(pipefd));

	pid = fork();
	ASSERT_GE(pid, 0);
	if (pid == 0) {
		struct sock_filter filter[] = {
			BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
		};
		struct sock_fprog prog = {
			.len = (unsigned short)ARRAY_SIZE(filter),
			.filter = filter,
		};

		 
		EXPECT_EQ(0, seccomp(SECCOMP_SET_MODE_FILTER,
				     SECCOMP_FILTER_FLAG_LOG, &prog));
		EXPECT_EQ(0, seccomp(SECCOMP_SET_MODE_FILTER, 0, &prog));

		EXPECT_EQ(0, close(pipefd[0]));
		ASSERT_EQ(1, write(pipefd[1], "1", 1));
		ASSERT_EQ(0, close(pipefd[1]));

		while (1)
			sleep(100);
	}

	ASSERT_EQ(0, close(pipefd[1]));
	ASSERT_EQ(1, read(pipefd[0], &buf, 1));

	ASSERT_EQ(0, ptrace(PTRACE_ATTACH, pid));
	ASSERT_EQ(pid, waitpid(pid, NULL, 0));

	 

	md.filter_off = 0;
	errno = 0;
	ret = ptrace(PTRACE_SECCOMP_GET_METADATA, pid, sizeof(md), &md);
	EXPECT_EQ(sizeof(md), ret) {
		if (errno == EINVAL)
			SKIP(goto skip, "Kernel does not support PTRACE_SECCOMP_GET_METADATA (missing CONFIG_CHECKPOINT_RESTORE?)");
	}

	EXPECT_EQ(md.flags, SECCOMP_FILTER_FLAG_LOG);
	EXPECT_EQ(md.filter_off, 0);

	md.filter_off = 1;
	ret = ptrace(PTRACE_SECCOMP_GET_METADATA, pid, sizeof(md), &md);
	EXPECT_EQ(sizeof(md), ret);
	EXPECT_EQ(md.flags, 0);
	EXPECT_EQ(md.filter_off, 1);

skip:
	ASSERT_EQ(0, kill(pid, SIGKILL));
}

static int user_notif_syscall(int nr, unsigned int flags)
{
	struct sock_filter filter[] = {
		BPF_STMT(BPF_LD|BPF_W|BPF_ABS,
			offsetof(struct seccomp_data, nr)),
		BPF_JUMP(BPF_JMP|BPF_JEQ|BPF_K, nr, 0, 1),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_USER_NOTIF),
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};

	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};

	return seccomp(SECCOMP_SET_MODE_FILTER, flags, &prog);
}

#define USER_NOTIF_MAGIC INT_MAX
TEST(user_notification_basic)
{
	pid_t pid;
	long ret;
	int status, listener;
	struct seccomp_notif req = {};
	struct seccomp_notif_resp resp = {};
	struct pollfd pollfd;

	struct sock_filter filter[] = {
		BPF_STMT(BPF_RET|BPF_K, SECCOMP_RET_ALLOW),
	};
	struct sock_fprog prog = {
		.len = (unsigned short)ARRAY_SIZE(filter),
		.filter = filter,
	};

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	pid = fork();
	ASSERT_GE(pid, 0);

	 
	if (pid == 0) {
		if (user_notif_syscall(__NR_getppid, 0) < 0)
			exit(1);
		ret = syscall(__NR_getppid);
		exit(ret >= 0 || errno != ENOSYS);
	}

	EXPECT_EQ(waitpid(pid, &status, 0), pid);
	EXPECT_EQ(true, WIFEXITED(status));
	EXPECT_EQ(0, WEXITSTATUS(status));

	 
	EXPECT_EQ(seccomp(SECCOMP_SET_MODE_FILTER, 0, &prog), 0);
	EXPECT_EQ(seccomp(SECCOMP_SET_MODE_FILTER, 0, &prog), 0);
	EXPECT_EQ(seccomp(SECCOMP_SET_MODE_FILTER, 0, &prog), 0);
	EXPECT_EQ(seccomp(SECCOMP_SET_MODE_FILTER, 0, &prog), 0);

	 
	listener = user_notif_syscall(__NR_getppid,
				      SECCOMP_FILTER_FLAG_NEW_LISTENER);
	ASSERT_GE(listener, 0);

	 
	EXPECT_EQ(user_notif_syscall(__NR_getppid,
				     SECCOMP_FILTER_FLAG_NEW_LISTENER),
		  -1);
	EXPECT_EQ(errno, EBUSY);

	pid = fork();
	ASSERT_GE(pid, 0);

	if (pid == 0) {
		ret = syscall(__NR_getppid);
		exit(ret != USER_NOTIF_MAGIC);
	}

	pollfd.fd = listener;
	pollfd.events = POLLIN | POLLOUT;

	EXPECT_GT(poll(&pollfd, 1, -1), 0);
	EXPECT_EQ(pollfd.revents, POLLIN);

	 
	memset(&req, 0, sizeof(req));
	req.pid = -1;
	errno = 0;
	ret = ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, &req);
	EXPECT_EQ(-1, ret);
	EXPECT_EQ(EINVAL, errno);

	if (ret) {
		req.pid = 0;
		EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, &req), 0);
	}

	pollfd.fd = listener;
	pollfd.events = POLLIN | POLLOUT;

	EXPECT_GT(poll(&pollfd, 1, -1), 0);
	EXPECT_EQ(pollfd.revents, POLLOUT);

	EXPECT_EQ(req.data.nr,  __NR_getppid);

	resp.id = req.id;
	resp.error = 0;
	resp.val = USER_NOTIF_MAGIC;

	 
	resp.flags = 1;
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_SEND, &resp), -1);
	EXPECT_EQ(errno, EINVAL);

	resp.flags = 0;
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_SEND, &resp), 0);

	EXPECT_EQ(waitpid(pid, &status, 0), pid);
	EXPECT_EQ(true, WIFEXITED(status));
	EXPECT_EQ(0, WEXITSTATUS(status));
}

TEST(user_notification_with_tsync)
{
	int ret;
	unsigned int flags;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	 
	flags = SECCOMP_FILTER_FLAG_NEW_LISTENER |
		SECCOMP_FILTER_FLAG_TSYNC;
	ASSERT_EQ(-1, user_notif_syscall(__NR_getppid, flags));
	ASSERT_EQ(EINVAL, errno);

	 
	flags |= SECCOMP_FILTER_FLAG_TSYNC_ESRCH;
	ret = user_notif_syscall(__NR_getppid, flags);
	close(ret);
	ASSERT_LE(0, ret);
}

TEST(user_notification_kill_in_middle)
{
	pid_t pid;
	long ret;
	int listener;
	struct seccomp_notif req = {};
	struct seccomp_notif_resp resp = {};

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	listener = user_notif_syscall(__NR_getppid,
				      SECCOMP_FILTER_FLAG_NEW_LISTENER);
	ASSERT_GE(listener, 0);

	 
	pid = fork();
	ASSERT_GE(pid, 0);

	if (pid == 0) {
		ret = syscall(__NR_getppid);
		exit(ret != USER_NOTIF_MAGIC);
	}

	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, &req), 0);
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_ID_VALID, &req.id), 0);

	EXPECT_EQ(kill(pid, SIGKILL), 0);
	EXPECT_EQ(waitpid(pid, NULL, 0), pid);

	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_ID_VALID, &req.id), -1);

	resp.id = req.id;
	ret = ioctl(listener, SECCOMP_IOCTL_NOTIF_SEND, &resp);
	EXPECT_EQ(ret, -1);
	EXPECT_EQ(errno, ENOENT);
}

static int handled = -1;

static void signal_handler(int signal)
{
	if (write(handled, "c", 1) != 1)
		perror("write from signal");
}

TEST(user_notification_signal)
{
	pid_t pid;
	long ret;
	int status, listener, sk_pair[2];
	struct seccomp_notif req = {};
	struct seccomp_notif_resp resp = {};
	char c;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	ASSERT_EQ(socketpair(PF_LOCAL, SOCK_SEQPACKET, 0, sk_pair), 0);

	listener = user_notif_syscall(__NR_gettid,
				      SECCOMP_FILTER_FLAG_NEW_LISTENER);
	ASSERT_GE(listener, 0);

	pid = fork();
	ASSERT_GE(pid, 0);

	if (pid == 0) {
		close(sk_pair[0]);
		handled = sk_pair[1];
		if (signal(SIGUSR1, signal_handler) == SIG_ERR) {
			perror("signal");
			exit(1);
		}
		 
		ret = syscall(__NR_gettid);
		exit(!(ret == -1 && errno == 512));
	}

	close(sk_pair[1]);

	memset(&req, 0, sizeof(req));
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, &req), 0);

	EXPECT_EQ(kill(pid, SIGUSR1), 0);

	 
	EXPECT_EQ(read(sk_pair[0], &c, 1), 1);

	resp.id = req.id;
	resp.error = -EPERM;
	resp.val = 0;

	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_SEND, &resp), -1);
	EXPECT_EQ(errno, ENOENT);

	memset(&req, 0, sizeof(req));
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, &req), 0);

	resp.id = req.id;
	resp.error = -512;  
	resp.val = 0;

	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_SEND, &resp), 0);

	EXPECT_EQ(waitpid(pid, &status, 0), pid);
	EXPECT_EQ(true, WIFEXITED(status));
	EXPECT_EQ(0, WEXITSTATUS(status));
}

TEST(user_notification_closed_listener)
{
	pid_t pid;
	long ret;
	int status, listener;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	listener = user_notif_syscall(__NR_getppid,
				      SECCOMP_FILTER_FLAG_NEW_LISTENER);
	ASSERT_GE(listener, 0);

	 
	pid = fork();
	ASSERT_GE(pid, 0);
	if (pid == 0) {
		close(listener);
		ret = syscall(__NR_getppid);
		exit(ret != -1 && errno != ENOSYS);
	}

	close(listener);

	EXPECT_EQ(waitpid(pid, &status, 0), pid);
	EXPECT_EQ(true, WIFEXITED(status));
	EXPECT_EQ(0, WEXITSTATUS(status));
}

 
TEST(user_notification_child_pid_ns)
{
	pid_t pid;
	int status, listener;
	struct seccomp_notif req = {};
	struct seccomp_notif_resp resp = {};

	ASSERT_EQ(unshare(CLONE_NEWUSER | CLONE_NEWPID), 0) {
		if (errno == EINVAL)
			SKIP(return, "kernel missing CLONE_NEWUSER support");
	};

	listener = user_notif_syscall(__NR_getppid,
				      SECCOMP_FILTER_FLAG_NEW_LISTENER);
	ASSERT_GE(listener, 0);

	pid = fork();
	ASSERT_GE(pid, 0);

	if (pid == 0)
		exit(syscall(__NR_getppid) != USER_NOTIF_MAGIC);

	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, &req), 0);
	EXPECT_EQ(req.pid, pid);

	resp.id = req.id;
	resp.error = 0;
	resp.val = USER_NOTIF_MAGIC;

	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_SEND, &resp), 0);

	EXPECT_EQ(waitpid(pid, &status, 0), pid);
	EXPECT_EQ(true, WIFEXITED(status));
	EXPECT_EQ(0, WEXITSTATUS(status));
	close(listener);
}

 
TEST(user_notification_sibling_pid_ns)
{
	pid_t pid, pid2;
	int status, listener;
	struct seccomp_notif req = {};
	struct seccomp_notif_resp resp = {};

	ASSERT_EQ(prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0), 0) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	listener = user_notif_syscall(__NR_getppid,
				      SECCOMP_FILTER_FLAG_NEW_LISTENER);
	ASSERT_GE(listener, 0);

	pid = fork();
	ASSERT_GE(pid, 0);

	if (pid == 0) {
		ASSERT_EQ(unshare(CLONE_NEWPID), 0);

		pid2 = fork();
		ASSERT_GE(pid2, 0);

		if (pid2 == 0)
			exit(syscall(__NR_getppid) != USER_NOTIF_MAGIC);

		EXPECT_EQ(waitpid(pid2, &status, 0), pid2);
		EXPECT_EQ(true, WIFEXITED(status));
		EXPECT_EQ(0, WEXITSTATUS(status));
		exit(WEXITSTATUS(status));
	}

	 
	ASSERT_EQ(unshare(CLONE_NEWPID), 0) {
		if (errno == EPERM)
			SKIP(return, "CLONE_NEWPID requires CAP_SYS_ADMIN");
	}
	ASSERT_EQ(errno, 0);

	pid2 = fork();
	ASSERT_GE(pid2, 0);

	if (pid2 == 0) {
		ASSERT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, &req), 0);
		 
		EXPECT_EQ(req.pid, 0);

		resp.id = req.id;
		resp.error = 0;
		resp.val = USER_NOTIF_MAGIC;

		ASSERT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_SEND, &resp), 0);
		exit(0);
	}

	close(listener);

	EXPECT_EQ(waitpid(pid, &status, 0), pid);
	EXPECT_EQ(true, WIFEXITED(status));
	EXPECT_EQ(0, WEXITSTATUS(status));

	EXPECT_EQ(waitpid(pid2, &status, 0), pid2);
	EXPECT_EQ(true, WIFEXITED(status));
	EXPECT_EQ(0, WEXITSTATUS(status));
}

TEST(user_notification_fault_recv)
{
	pid_t pid;
	int status, listener;
	struct seccomp_notif req = {};
	struct seccomp_notif_resp resp = {};

	ASSERT_EQ(unshare(CLONE_NEWUSER), 0) {
		if (errno == EINVAL)
			SKIP(return, "kernel missing CLONE_NEWUSER support");
	}

	listener = user_notif_syscall(__NR_getppid,
				      SECCOMP_FILTER_FLAG_NEW_LISTENER);
	ASSERT_GE(listener, 0);

	pid = fork();
	ASSERT_GE(pid, 0);

	if (pid == 0)
		exit(syscall(__NR_getppid) != USER_NOTIF_MAGIC);

	 
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, NULL), -1);
	EXPECT_EQ(errno, EFAULT);

	 
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, &req), 0);
	EXPECT_EQ(req.pid, pid);

	resp.id = req.id;
	resp.error = 0;
	resp.val = USER_NOTIF_MAGIC;

	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_SEND, &resp), 0);

	EXPECT_EQ(waitpid(pid, &status, 0), pid);
	EXPECT_EQ(true, WIFEXITED(status));
	EXPECT_EQ(0, WEXITSTATUS(status));
}

TEST(seccomp_get_notif_sizes)
{
	struct seccomp_notif_sizes sizes;

	ASSERT_EQ(seccomp(SECCOMP_GET_NOTIF_SIZES, 0, &sizes), 0);
	EXPECT_EQ(sizes.seccomp_notif, sizeof(struct seccomp_notif));
	EXPECT_EQ(sizes.seccomp_notif_resp, sizeof(struct seccomp_notif_resp));
}

TEST(user_notification_continue)
{
	pid_t pid;
	long ret;
	int status, listener;
	struct seccomp_notif req = {};
	struct seccomp_notif_resp resp = {};
	struct pollfd pollfd;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	listener = user_notif_syscall(__NR_dup, SECCOMP_FILTER_FLAG_NEW_LISTENER);
	ASSERT_GE(listener, 0);

	pid = fork();
	ASSERT_GE(pid, 0);

	if (pid == 0) {
		int dup_fd, pipe_fds[2];
		pid_t self;

		ASSERT_GE(pipe(pipe_fds), 0);

		dup_fd = dup(pipe_fds[0]);
		ASSERT_GE(dup_fd, 0);
		EXPECT_NE(pipe_fds[0], dup_fd);

		self = getpid();
		ASSERT_EQ(filecmp(self, self, pipe_fds[0], dup_fd), 0);
		exit(0);
	}

	pollfd.fd = listener;
	pollfd.events = POLLIN | POLLOUT;

	EXPECT_GT(poll(&pollfd, 1, -1), 0);
	EXPECT_EQ(pollfd.revents, POLLIN);

	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, &req), 0);

	pollfd.fd = listener;
	pollfd.events = POLLIN | POLLOUT;

	EXPECT_GT(poll(&pollfd, 1, -1), 0);
	EXPECT_EQ(pollfd.revents, POLLOUT);

	EXPECT_EQ(req.data.nr, __NR_dup);

	resp.id = req.id;
	resp.flags = SECCOMP_USER_NOTIF_FLAG_CONTINUE;

	 
	resp.error = 0;
	resp.val = USER_NOTIF_MAGIC;
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_SEND, &resp), -1);
	EXPECT_EQ(errno, EINVAL);

	resp.error = USER_NOTIF_MAGIC;
	resp.val = 0;
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_SEND, &resp), -1);
	EXPECT_EQ(errno, EINVAL);

	resp.error = 0;
	resp.val = 0;
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_SEND, &resp), 0) {
		if (errno == EINVAL)
			SKIP(goto skip, "Kernel does not support SECCOMP_USER_NOTIF_FLAG_CONTINUE");
	}

skip:
	EXPECT_EQ(waitpid(pid, &status, 0), pid);
	EXPECT_EQ(true, WIFEXITED(status));
	EXPECT_EQ(0, WEXITSTATUS(status)) {
		if (WEXITSTATUS(status) == 2) {
			SKIP(return, "Kernel does not support kcmp() syscall");
			return;
		}
	}
}

TEST(user_notification_filter_empty)
{
	pid_t pid;
	long ret;
	int status;
	struct pollfd pollfd;
	struct __clone_args args = {
		.flags = CLONE_FILES,
		.exit_signal = SIGCHLD,
	};

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	if (__NR_clone3 < 0)
		SKIP(return, "Test not built with clone3 support");

	pid = sys_clone3(&args, sizeof(args));
	ASSERT_GE(pid, 0);

	if (pid == 0) {
		int listener;

		listener = user_notif_syscall(__NR_mknodat, SECCOMP_FILTER_FLAG_NEW_LISTENER);
		if (listener < 0)
			_exit(EXIT_FAILURE);

		if (dup2(listener, 200) != 200)
			_exit(EXIT_FAILURE);

		close(listener);

		_exit(EXIT_SUCCESS);
	}

	EXPECT_EQ(waitpid(pid, &status, 0), pid);
	EXPECT_EQ(true, WIFEXITED(status));
	EXPECT_EQ(0, WEXITSTATUS(status));

	 
	pollfd.fd = 200;
	pollfd.events = POLLHUP;

	EXPECT_GT(poll(&pollfd, 1, 2000), 0);
	EXPECT_GT((pollfd.revents & POLLHUP) ?: 0, 0);
}

static void *do_thread(void *data)
{
	return NULL;
}

TEST(user_notification_filter_empty_threaded)
{
	pid_t pid;
	long ret;
	int status;
	struct pollfd pollfd;
	struct __clone_args args = {
		.flags = CLONE_FILES,
		.exit_signal = SIGCHLD,
	};

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	if (__NR_clone3 < 0)
		SKIP(return, "Test not built with clone3 support");

	pid = sys_clone3(&args, sizeof(args));
	ASSERT_GE(pid, 0);

	if (pid == 0) {
		pid_t pid1, pid2;
		int listener, status;
		pthread_t thread;

		listener = user_notif_syscall(__NR_dup, SECCOMP_FILTER_FLAG_NEW_LISTENER);
		if (listener < 0)
			_exit(EXIT_FAILURE);

		if (dup2(listener, 200) != 200)
			_exit(EXIT_FAILURE);

		close(listener);

		pid1 = fork();
		if (pid1 < 0)
			_exit(EXIT_FAILURE);

		if (pid1 == 0)
			_exit(EXIT_SUCCESS);

		pid2 = fork();
		if (pid2 < 0)
			_exit(EXIT_FAILURE);

		if (pid2 == 0)
			_exit(EXIT_SUCCESS);

		if (pthread_create(&thread, NULL, do_thread, NULL) ||
		    pthread_join(thread, NULL))
			_exit(EXIT_FAILURE);

		if (pthread_create(&thread, NULL, do_thread, NULL) ||
		    pthread_join(thread, NULL))
			_exit(EXIT_FAILURE);

		if (waitpid(pid1, &status, 0) != pid1 || !WIFEXITED(status) ||
		    WEXITSTATUS(status))
			_exit(EXIT_FAILURE);

		if (waitpid(pid2, &status, 0) != pid2 || !WIFEXITED(status) ||
		    WEXITSTATUS(status))
			_exit(EXIT_FAILURE);

		exit(EXIT_SUCCESS);
	}

	EXPECT_EQ(waitpid(pid, &status, 0), pid);
	EXPECT_EQ(true, WIFEXITED(status));
	EXPECT_EQ(0, WEXITSTATUS(status));

	 
	pollfd.fd = 200;
	pollfd.events = POLLHUP;

	EXPECT_GT(poll(&pollfd, 1, 2000), 0);
	EXPECT_GT((pollfd.revents & POLLHUP) ?: 0, 0);
}

TEST(user_notification_addfd)
{
	pid_t pid;
	long ret;
	int status, listener, memfd, fd, nextfd;
	struct seccomp_notif_addfd addfd = {};
	struct seccomp_notif_addfd_small small = {};
	struct seccomp_notif_addfd_big big = {};
	struct seccomp_notif req = {};
	struct seccomp_notif_resp resp = {};
	 
	struct timespec delay = { .tv_nsec = 100000000 };

	 
	memfd = memfd_create("test", 0);
	ASSERT_GE(memfd, 0);
	nextfd = memfd + 1;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	 
	 
	listener = user_notif_syscall(__NR_getppid,
				      SECCOMP_FILTER_FLAG_NEW_LISTENER);
	ASSERT_EQ(listener, nextfd++);

	pid = fork();
	ASSERT_GE(pid, 0);

	if (pid == 0) {
		 
		if (syscall(__NR_getppid) != USER_NOTIF_MAGIC)
			exit(1);

		 
		if (fcntl(syscall(__NR_getppid), F_GETFD) == -1)
			exit(1);

		exit(syscall(__NR_getppid) != USER_NOTIF_MAGIC);
	}

	ASSERT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, &req), 0);

	addfd.srcfd = memfd;
	addfd.newfd = 0;
	addfd.id = req.id;
	addfd.flags = 0x0;

	 
	addfd.newfd_flags = ~O_CLOEXEC;
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_ADDFD, &addfd), -1);
	EXPECT_EQ(errno, EINVAL);
	addfd.newfd_flags = O_CLOEXEC;

	 
	addfd.flags = 0xff;
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_ADDFD, &addfd), -1);
	EXPECT_EQ(errno, EINVAL);
	addfd.flags = 0;

	 
	addfd.newfd = 1;
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_ADDFD, &addfd), -1);
	EXPECT_EQ(errno, EINVAL);
	addfd.newfd = 0;

	 
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_ADDFD_SMALL, &small), -1);
	EXPECT_EQ(errno, EINVAL);

	 
	memset(&big, 0xAA, sizeof(big));
	big.addfd = addfd;
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_ADDFD_BIG, &big), -1);
	EXPECT_EQ(errno, E2BIG);


	 
	fd = ioctl(listener, SECCOMP_IOCTL_NOTIF_ADDFD, &addfd);
	EXPECT_EQ(fd, nextfd++);
	EXPECT_EQ(filecmp(getpid(), pid, memfd, fd), 0);

	 
	memset(&big, 0x0, sizeof(big));
	big.addfd = addfd;
	fd = ioctl(listener, SECCOMP_IOCTL_NOTIF_ADDFD_BIG, &big);
	EXPECT_EQ(fd, nextfd++);

	 
	addfd.newfd = 42;
	addfd.flags = SECCOMP_ADDFD_FLAG_SETFD;
	fd = ioctl(listener, SECCOMP_IOCTL_NOTIF_ADDFD, &addfd);
	EXPECT_EQ(fd, 42);
	EXPECT_EQ(filecmp(getpid(), pid, memfd, fd), 0);

	 
	resp.id = req.id;
	resp.error = 0;
	resp.val = USER_NOTIF_MAGIC;
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_SEND, &resp), 0);

	 
	addfd.id = req.id + 1;

	 
	while (ioctl(listener, SECCOMP_IOCTL_NOTIF_ADDFD, &addfd) != -1 &&
	       errno != -EINPROGRESS)
		nanosleep(&delay, NULL);

	memset(&req, 0, sizeof(req));
	ASSERT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, &req), 0);
	ASSERT_EQ(addfd.id, req.id);

	 
	addfd.newfd = 0;
	addfd.flags = SECCOMP_ADDFD_FLAG_SEND;
	fd = ioctl(listener, SECCOMP_IOCTL_NOTIF_ADDFD, &addfd);
	 
	EXPECT_EQ(fd, nextfd++);
	ASSERT_EQ(filecmp(getpid(), pid, memfd, fd), 0);

	 
	addfd.id = req.id + 1;

	 
	while (ioctl(listener, SECCOMP_IOCTL_NOTIF_ADDFD, &addfd) != -1 &&
	       errno != -EINPROGRESS)
		nanosleep(&delay, NULL);

	memset(&req, 0, sizeof(req));
	ASSERT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, &req), 0);
	ASSERT_EQ(addfd.id, req.id);

	resp.id = req.id;
	resp.error = 0;
	resp.val = USER_NOTIF_MAGIC;
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_SEND, &resp), 0);

	 
	EXPECT_EQ(waitpid(pid, &status, 0), pid);
	EXPECT_EQ(true, WIFEXITED(status));
	EXPECT_EQ(0, WEXITSTATUS(status));

	close(memfd);
}

TEST(user_notification_addfd_rlimit)
{
	pid_t pid;
	long ret;
	int status, listener, memfd;
	struct seccomp_notif_addfd addfd = {};
	struct seccomp_notif req = {};
	struct seccomp_notif_resp resp = {};
	const struct rlimit lim = {
		.rlim_cur	= 0,
		.rlim_max	= 0,
	};

	memfd = memfd_create("test", 0);
	ASSERT_GE(memfd, 0);

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	 
	listener = user_notif_syscall(__NR_getppid,
				      SECCOMP_FILTER_FLAG_NEW_LISTENER);
	ASSERT_GE(listener, 0);

	pid = fork();
	ASSERT_GE(pid, 0);

	if (pid == 0)
		exit(syscall(__NR_getppid) != USER_NOTIF_MAGIC);


	ASSERT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, &req), 0);

	ASSERT_EQ(prlimit(pid, RLIMIT_NOFILE, &lim, NULL), 0);

	addfd.srcfd = memfd;
	addfd.newfd_flags = O_CLOEXEC;
	addfd.newfd = 0;
	addfd.id = req.id;
	addfd.flags = 0;

	 
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_ADDFD, &addfd), -1);
	EXPECT_EQ(errno, EMFILE);

	addfd.flags = SECCOMP_ADDFD_FLAG_SEND;
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_ADDFD, &addfd), -1);
	EXPECT_EQ(errno, EMFILE);

	addfd.newfd = 100;
	addfd.flags = SECCOMP_ADDFD_FLAG_SETFD;
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_ADDFD, &addfd), -1);
	EXPECT_EQ(errno, EBADF);

	resp.id = req.id;
	resp.error = 0;
	resp.val = USER_NOTIF_MAGIC;

	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_SEND, &resp), 0);

	 
	EXPECT_EQ(waitpid(pid, &status, 0), pid);
	EXPECT_EQ(true, WIFEXITED(status));
	EXPECT_EQ(0, WEXITSTATUS(status));

	close(memfd);
}

#ifndef SECCOMP_USER_NOTIF_FD_SYNC_WAKE_UP
#define SECCOMP_USER_NOTIF_FD_SYNC_WAKE_UP (1UL << 0)
#define SECCOMP_IOCTL_NOTIF_SET_FLAGS  SECCOMP_IOW(4, __u64)
#endif

TEST(user_notification_sync)
{
	struct seccomp_notif req = {};
	struct seccomp_notif_resp resp = {};
	int status, listener;
	pid_t pid;
	long ret;

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	listener = user_notif_syscall(__NR_getppid,
				      SECCOMP_FILTER_FLAG_NEW_LISTENER);
	ASSERT_GE(listener, 0);

	 
	EXPECT_SYSCALL_RETURN(-EINVAL,
		ioctl(listener, SECCOMP_IOCTL_NOTIF_SET_FLAGS, 0xffffffff, 0));

	ASSERT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_SET_FLAGS,
			SECCOMP_USER_NOTIF_FD_SYNC_WAKE_UP, 0), 0);

	pid = fork();
	ASSERT_GE(pid, 0);
	if (pid == 0) {
		ret = syscall(__NR_getppid);
		ASSERT_EQ(ret, USER_NOTIF_MAGIC) {
			_exit(1);
		}
		_exit(0);
	}

	req.pid = 0;
	ASSERT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, &req), 0);

	ASSERT_EQ(req.data.nr,  __NR_getppid);

	resp.id = req.id;
	resp.error = 0;
	resp.val = USER_NOTIF_MAGIC;
	resp.flags = 0;
	ASSERT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_SEND, &resp), 0);

	ASSERT_EQ(waitpid(pid, &status, 0), pid);
	ASSERT_EQ(status, 0);
}


 
FIXTURE(O_SUSPEND_SECCOMP) {
	pid_t pid;
};

FIXTURE_SETUP(O_SUSPEND_SECCOMP)
{
	ERRNO_FILTER(block_read, E2BIG);
	cap_value_t cap_list[] = { CAP_SYS_ADMIN };
	cap_t caps;

	self->pid = 0;

	 
	caps = cap_get_proc();
	ASSERT_NE(NULL, caps);
	ASSERT_EQ(0, cap_set_flag(caps, CAP_EFFECTIVE, 1, cap_list, CAP_CLEAR));
	ASSERT_EQ(0, cap_set_proc(caps));
	cap_free(caps);

	ASSERT_EQ(0, prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0));
	ASSERT_EQ(0, prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog_block_read));

	self->pid = fork();
	ASSERT_GE(self->pid, 0);

	if (self->pid == 0) {
		while (1)
			pause();
		_exit(127);
	}
}

FIXTURE_TEARDOWN(O_SUSPEND_SECCOMP)
{
	if (self->pid)
		kill(self->pid, SIGKILL);
}

TEST_F(O_SUSPEND_SECCOMP, setoptions)
{
	int wstatus;

	ASSERT_EQ(0, ptrace(PTRACE_ATTACH, self->pid, NULL, 0));
	ASSERT_EQ(self->pid, wait(&wstatus));
	ASSERT_EQ(-1, ptrace(PTRACE_SETOPTIONS, self->pid, NULL, PTRACE_O_SUSPEND_SECCOMP));
	if (errno == EINVAL)
		SKIP(return, "Kernel does not support PTRACE_O_SUSPEND_SECCOMP (missing CONFIG_CHECKPOINT_RESTORE?)");
	ASSERT_EQ(EPERM, errno);
}

TEST_F(O_SUSPEND_SECCOMP, seize)
{
	int ret;

	ret = ptrace(PTRACE_SEIZE, self->pid, NULL, PTRACE_O_SUSPEND_SECCOMP);
	ASSERT_EQ(-1, ret);
	if (errno == EINVAL)
		SKIP(return, "Kernel does not support PTRACE_O_SUSPEND_SECCOMP (missing CONFIG_CHECKPOINT_RESTORE?)");
	ASSERT_EQ(EPERM, errno);
}

 
static ssize_t get_nth(struct __test_metadata *_metadata, const char *path,
		     const unsigned int position, char **entry)
{
	char *line = NULL;
	unsigned int i;
	ssize_t nread;
	size_t len = 0;
	FILE *f;

	f = fopen(path, "r");
	ASSERT_NE(f, NULL) {
		TH_LOG("Could not open %s: %s", path, strerror(errno));
	}

	for (i = 0; i < position; i++) {
		nread = getdelim(&line, &len, ' ', f);
		ASSERT_GE(nread, 0) {
			TH_LOG("Failed to read %d entry in file %s", i, path);
		}
	}
	fclose(f);

	ASSERT_GT(nread, 0) {
		TH_LOG("Entry in file %s had zero length", path);
	}

	*entry = line;
	return nread - 1;
}

 
static char get_proc_stat(struct __test_metadata *_metadata, pid_t pid)
{
	char proc_path[100] = {0};
	char status;
	char *line;

	snprintf(proc_path, sizeof(proc_path), "/proc/%d/stat", pid);
	ASSERT_EQ(get_nth(_metadata, proc_path, 3, &line), 1);

	status = *line;
	free(line);

	return status;
}

TEST(user_notification_fifo)
{
	struct seccomp_notif_resp resp = {};
	struct seccomp_notif req = {};
	int i, status, listener;
	pid_t pid, pids[3];
	__u64 baseid;
	long ret;
	 
	struct timespec delay = { .tv_nsec = 100000000 };

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret) {
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	 
	listener = user_notif_syscall(__NR_getppid,
				      SECCOMP_FILTER_FLAG_NEW_LISTENER);
	ASSERT_GE(listener, 0);

	pid = fork();
	ASSERT_GE(pid, 0);

	if (pid == 0) {
		ret = syscall(__NR_getppid);
		exit(ret != USER_NOTIF_MAGIC);
	}

	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, &req), 0);
	baseid = req.id + 1;

	resp.id = req.id;
	resp.error = 0;
	resp.val = USER_NOTIF_MAGIC;

	 
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_SEND, &resp), 0);

	EXPECT_EQ(waitpid(pid, &status, 0), pid);
	EXPECT_EQ(true, WIFEXITED(status));
	EXPECT_EQ(0, WEXITSTATUS(status));

	 
	for (i = 0; i < ARRAY_SIZE(pids); i++) {
		pid = fork();
		if (pid == 0) {
			ret = syscall(__NR_getppid);
			exit(ret != USER_NOTIF_MAGIC);
		}
		pids[i] = pid;
	}

	 
restart_wait:
	for (i = 0; i < ARRAY_SIZE(pids); i++) {
		if (get_proc_stat(_metadata, pids[i]) != 'S') {
			nanosleep(&delay, NULL);
			goto restart_wait;
		}
	}

	 
	for (i = 0; i < ARRAY_SIZE(pids); i++) {
		memset(&req, 0, sizeof(req));
		EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, &req), 0);
		EXPECT_EQ(req.id, baseid + i);
		resp.id = req.id;
		EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_SEND, &resp), 0);
	}

	 
	for (i = 0; i < ARRAY_SIZE(pids); i++) {
		EXPECT_EQ(waitpid(pids[i], &status, 0), pids[i]);
		EXPECT_EQ(true, WIFEXITED(status));
		EXPECT_EQ(0, WEXITSTATUS(status));
	}
}

 
static long get_proc_syscall(struct __test_metadata *_metadata, int pid)
{
	char proc_path[100] = {0};
	long ret = -1;
	ssize_t nread;
	char *line;

	snprintf(proc_path, sizeof(proc_path), "/proc/%d/syscall", pid);
	nread = get_nth(_metadata, proc_path, 1, &line);
	ASSERT_GT(nread, 0);

	if (!strncmp("running", line, MIN(7, nread)))
		ret = strtol(line, NULL, 16);

	free(line);
	return ret;
}

 
TEST(user_notification_wait_killable_pre_notification)
{
	struct sigaction new_action = {
		.sa_handler = signal_handler,
	};
	int listener, status, sk_pair[2];
	pid_t pid;
	long ret;
	char c;
	 
	struct timespec delay = { .tv_nsec = 100000000 };

	ASSERT_EQ(sigemptyset(&new_action.sa_mask), 0);

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret)
	{
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	ASSERT_EQ(socketpair(PF_LOCAL, SOCK_SEQPACKET, 0, sk_pair), 0);

	listener = user_notif_syscall(
		__NR_getppid, SECCOMP_FILTER_FLAG_NEW_LISTENER |
				      SECCOMP_FILTER_FLAG_WAIT_KILLABLE_RECV);
	ASSERT_GE(listener, 0);

	 
	pid = fork();
	ASSERT_GE(pid, 0);

	if (pid == 0) {
		close(sk_pair[0]);
		handled = sk_pair[1];

		 
		if (sigaction(SIGUSR1, &new_action, NULL)) {
			perror("sigaction");
			exit(1);
		}

		ret = syscall(__NR_getppid);
		 
		exit(ret != -1 || errno != EINTR);
	}

	 
	while (get_proc_syscall(_metadata, pid) != __NR_getppid &&
	       get_proc_stat(_metadata, pid) != 'S')
		nanosleep(&delay, NULL);

	 
	EXPECT_EQ(kill(pid, SIGUSR1), 0);

	 
	EXPECT_EQ(waitpid(pid, &status, 0), pid);
	EXPECT_EQ(true, WIFEXITED(status));
	EXPECT_EQ(0, WEXITSTATUS(status));

	EXPECT_EQ(read(sk_pair[0], &c, 1), 1);
}

 
TEST(user_notification_wait_killable)
{
	struct sigaction new_action = {
		.sa_handler = signal_handler,
	};
	struct seccomp_notif_resp resp = {};
	struct seccomp_notif req = {};
	int listener, status, sk_pair[2];
	pid_t pid;
	long ret;
	char c;
	 
	struct timespec delay = { .tv_nsec = 100000000 };

	ASSERT_EQ(sigemptyset(&new_action.sa_mask), 0);

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret)
	{
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	ASSERT_EQ(socketpair(PF_LOCAL, SOCK_SEQPACKET, 0, sk_pair), 0);

	listener = user_notif_syscall(
		__NR_getppid, SECCOMP_FILTER_FLAG_NEW_LISTENER |
				      SECCOMP_FILTER_FLAG_WAIT_KILLABLE_RECV);
	ASSERT_GE(listener, 0);

	pid = fork();
	ASSERT_GE(pid, 0);

	if (pid == 0) {
		close(sk_pair[0]);
		handled = sk_pair[1];

		 
		if (sigaction(SIGUSR1, &new_action, NULL)) {
			perror("sigaction");
			exit(1);
		}

		 
		ret = syscall(__NR_getppid);
		exit(ret != USER_NOTIF_MAGIC);
	}

	 
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, &req), 0);
	 
	EXPECT_EQ(kill(pid, SIGUSR1), 0);

	 
	while (get_proc_stat(_metadata, pid) != 'D')
		nanosleep(&delay, NULL);

	resp.id = req.id;
	resp.val = USER_NOTIF_MAGIC;
	 
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_SEND, &resp), 0);

	 
	EXPECT_EQ(read(sk_pair[0], &c, 1), 1);
	 
	EXPECT_EQ(waitpid(pid, &status, 0), pid);
	EXPECT_EQ(true, WIFEXITED(status));
	EXPECT_EQ(0, WEXITSTATUS(status));
}

 
TEST(user_notification_wait_killable_fatal)
{
	struct seccomp_notif req = {};
	int listener, status;
	pid_t pid;
	long ret;
	 
	struct timespec delay = { .tv_nsec = 100000000 };

	ret = prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
	ASSERT_EQ(0, ret)
	{
		TH_LOG("Kernel does not support PR_SET_NO_NEW_PRIVS!");
	}

	listener = user_notif_syscall(
		__NR_getppid, SECCOMP_FILTER_FLAG_NEW_LISTENER |
				      SECCOMP_FILTER_FLAG_WAIT_KILLABLE_RECV);
	ASSERT_GE(listener, 0);

	pid = fork();
	ASSERT_GE(pid, 0);

	if (pid == 0) {
		 
		syscall(__NR_getppid);
		exit(1);
	}

	while (get_proc_stat(_metadata, pid) != 'S')
		nanosleep(&delay, NULL);

	 
	EXPECT_EQ(ioctl(listener, SECCOMP_IOCTL_NOTIF_RECV, &req), 0);
	 
	EXPECT_EQ(kill(pid, SIGTERM), 0);

	 
	EXPECT_EQ(waitpid(pid, &status, 0), pid);
	EXPECT_EQ(true, WIFSIGNALED(status));
	EXPECT_EQ(SIGTERM, WTERMSIG(status));
}

 

TEST_HARNESS_MAIN
