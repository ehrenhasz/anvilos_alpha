
 

#define _GNU_SOURCE

#include <sys/syscall.h>
#include <asm/mman.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <x86intrin.h>
#include <asm/prctl.h>
#include <sys/prctl.h>
#include <stdint.h>
#include <signal.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <linux/userfaultfd.h>
#include <setjmp.h>
#include <sys/ptrace.h>
#include <sys/signal.h>
#include <linux/elf.h>

 
#ifndef __NR_map_shadow_stack
#define __NR_map_shadow_stack	453

#define SHADOW_STACK_SET_TOKEN	(1ULL << 0)

#define ARCH_SHSTK_ENABLE	0x5001
#define ARCH_SHSTK_DISABLE	0x5002
#define ARCH_SHSTK_LOCK		0x5003
#define ARCH_SHSTK_UNLOCK	0x5004
#define ARCH_SHSTK_STATUS	0x5005

#define ARCH_SHSTK_SHSTK	(1ULL <<  0)
#define ARCH_SHSTK_WRSS		(1ULL <<  1)

#define NT_X86_SHSTK	0x204
#endif

#define SS_SIZE 0x200000
#define PAGE_SIZE 0x1000

#if (__GNUC__ < 8) || (__GNUC__ == 8 && __GNUC_MINOR__ < 5)
int main(int argc, char *argv[])
{
	printf("[SKIP]\tCompiler does not support CET.\n");
	return 0;
}
#else
void write_shstk(unsigned long *addr, unsigned long val)
{
	asm volatile("wrssq %[val], (%[addr])\n"
		     : "=m" (addr)
		     : [addr] "r" (addr), [val] "r" (val));
}

static inline unsigned long __attribute__((always_inline)) get_ssp(void)
{
	unsigned long ret = 0;

	asm volatile("xor %0, %0; rdsspq %0" : "=r" (ret));
	return ret;
}

 
#define ARCH_PRCTL(arg1, arg2)					\
({								\
	long _ret;						\
	register long _num  asm("eax") = __NR_arch_prctl;	\
	register long _arg1 asm("rdi") = (long)(arg1);		\
	register long _arg2 asm("rsi") = (long)(arg2);		\
								\
	asm volatile (						\
		"syscall\n"					\
		: "=a"(_ret)					\
		: "r"(_arg1), "r"(_arg2),			\
		  "0"(_num)					\
		: "rcx", "r11", "memory", "cc"			\
	);							\
	_ret;							\
})

void *create_shstk(void *addr)
{
	return (void *)syscall(__NR_map_shadow_stack, addr, SS_SIZE, SHADOW_STACK_SET_TOKEN);
}

void *create_normal_mem(void *addr)
{
	return mmap(addr, SS_SIZE, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
}

void free_shstk(void *shstk)
{
	munmap(shstk, SS_SIZE);
}

int reset_shstk(void *shstk)
{
	return madvise(shstk, SS_SIZE, MADV_DONTNEED);
}

void try_shstk(unsigned long new_ssp)
{
	unsigned long ssp;

	printf("[INFO]\tnew_ssp = %lx, *new_ssp = %lx\n",
	       new_ssp, *((unsigned long *)new_ssp));

	ssp = get_ssp();
	printf("[INFO]\tchanging ssp from %lx to %lx\n", ssp, new_ssp);

	asm volatile("rstorssp (%0)\n":: "r" (new_ssp));
	asm volatile("saveprevssp");
	printf("[INFO]\tssp is now %lx\n", get_ssp());

	 
	ssp -= 8;
	asm volatile("rstorssp (%0)\n":: "r" (ssp));
	asm volatile("saveprevssp");
}

int test_shstk_pivot(void)
{
	void *shstk = create_shstk(0);

	if (shstk == MAP_FAILED) {
		printf("[FAIL]\tError creating shadow stack: %d\n", errno);
		return 1;
	}
	try_shstk((unsigned long)shstk + SS_SIZE - 8);
	free_shstk(shstk);

	printf("[OK]\tShadow stack pivot\n");
	return 0;
}

int test_shstk_faults(void)
{
	unsigned long *shstk = create_shstk(0);

	 
	if (*shstk != 0)
		goto err;

	 
	write_shstk(shstk, 1);
	if (*shstk != 1)
		goto err;

	 
	if (reset_shstk((void *)shstk))
		goto err;

	write_shstk(shstk, 1);
	if (*shstk != 1)
		goto err;

	printf("[OK]\tShadow stack faults\n");
	return 0;

err:
	return 1;
}

unsigned long saved_ssp;
unsigned long saved_ssp_val;
volatile bool segv_triggered;

void __attribute__((noinline)) violate_ss(void)
{
	saved_ssp = get_ssp();
	saved_ssp_val = *(unsigned long *)saved_ssp;

	 
	printf("[INFO]\tCorrupting shadow stack\n");
	write_shstk((void *)saved_ssp, 0);
}

void segv_handler(int signum, siginfo_t *si, void *uc)
{
	printf("[INFO]\tGenerated shadow stack violation successfully\n");

	segv_triggered = true;

	 
	write_shstk((void *)saved_ssp, saved_ssp_val);
}

int test_shstk_violation(void)
{
	struct sigaction sa = {};

	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGSEGV, &sa, NULL))
		return 1;

	segv_triggered = false;

	 
	asm volatile("" : : : "memory");

	violate_ss();

	signal(SIGSEGV, SIG_DFL);

	printf("[OK]\tShadow stack violation test\n");

	return !segv_triggered;
}

 
#define MAGIC_VAL 0x12345678
bool is_shstk_access;
void *shstk_ptr;
int fd;

void reset_test_shstk(void *addr)
{
	if (shstk_ptr)
		free_shstk(shstk_ptr);
	shstk_ptr = create_shstk(addr);
}

void test_access_fix_handler(int signum, siginfo_t *si, void *uc)
{
	printf("[INFO]\tViolation from %s\n", is_shstk_access ? "shstk access" : "normal write");

	segv_triggered = true;

	 
	if (is_shstk_access) {
		reset_test_shstk(shstk_ptr);
		return;
	}

	free_shstk(shstk_ptr);
	create_normal_mem(shstk_ptr);
}

bool test_shstk_access(void *ptr)
{
	is_shstk_access = true;
	segv_triggered = false;
	write_shstk(ptr, MAGIC_VAL);

	asm volatile("" : : : "memory");

	return segv_triggered;
}

bool test_write_access(void *ptr)
{
	is_shstk_access = false;
	segv_triggered = false;
	*(unsigned long *)ptr = MAGIC_VAL;

	asm volatile("" : : : "memory");

	return segv_triggered;
}

bool gup_write(void *ptr)
{
	unsigned long val;

	lseek(fd, (unsigned long)ptr, SEEK_SET);
	if (write(fd, &val, sizeof(val)) < 0)
		return 1;

	return 0;
}

bool gup_read(void *ptr)
{
	unsigned long val;

	lseek(fd, (unsigned long)ptr, SEEK_SET);
	if (read(fd, &val, sizeof(val)) < 0)
		return 1;

	return 0;
}

int test_gup(void)
{
	struct sigaction sa = {};
	int status;
	pid_t pid;

	sa.sa_sigaction = test_access_fix_handler;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGSEGV, &sa, NULL))
		return 1;

	segv_triggered = false;

	fd = open("/proc/self/mem", O_RDWR);
	if (fd == -1)
		return 1;

	reset_test_shstk(0);
	if (gup_read(shstk_ptr))
		return 1;
	if (test_shstk_access(shstk_ptr))
		return 1;
	printf("[INFO]\tGup read -> shstk access success\n");

	reset_test_shstk(0);
	if (gup_write(shstk_ptr))
		return 1;
	if (test_shstk_access(shstk_ptr))
		return 1;
	printf("[INFO]\tGup write -> shstk access success\n");

	reset_test_shstk(0);
	if (gup_read(shstk_ptr))
		return 1;
	if (!test_write_access(shstk_ptr))
		return 1;
	printf("[INFO]\tGup read -> write access success\n");

	reset_test_shstk(0);
	if (gup_write(shstk_ptr))
		return 1;
	if (!test_write_access(shstk_ptr))
		return 1;
	printf("[INFO]\tGup write -> write access success\n");

	close(fd);

	 
	reset_test_shstk(0);
	pid = fork();
	if (!pid) {
		fd = open("/proc/self/mem", O_RDWR);
		if (fd == -1)
			exit(1);

		if (gup_write(shstk_ptr)) {
			close(fd);
			exit(1);
		}
		close(fd);
		exit(0);
	}
	waitpid(pid, &status, 0);
	if (WEXITSTATUS(status)) {
		printf("[FAIL]\tWrite in child failed\n");
		return 1;
	}
	if (*(unsigned long *)shstk_ptr == MAGIC_VAL) {
		printf("[FAIL]\tWrite in child wrote through to shared memory\n");
		return 1;
	}

	printf("[INFO]\tCow gup write -> write access success\n");

	free_shstk(shstk_ptr);

	signal(SIGSEGV, SIG_DFL);

	printf("[OK]\tShadow gup test\n");

	return 0;
}

int test_mprotect(void)
{
	struct sigaction sa = {};

	sa.sa_sigaction = test_access_fix_handler;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGSEGV, &sa, NULL))
		return 1;

	segv_triggered = false;

	 
	reset_test_shstk(0);
	if (mprotect(shstk_ptr, SS_SIZE, PROT_READ) < 0) {
		printf("[FAIL]\tmprotect(PROT_READ) failed\n");
		return 1;
	}

	 
	if (!test_shstk_access(shstk_ptr)) {
		printf("[FAIL]\tShadow stack access to read-only memory succeeded\n");
		return 1;
	}

	 
	if (mprotect(shstk_ptr, SS_SIZE, PROT_READ) < 0) {
		printf("[FAIL]\tmprotect(PROT_READ) failed\n");
		return 1;
	}

	 
	if (mprotect(shstk_ptr, SS_SIZE, PROT_WRITE | PROT_READ) < 0) {
		printf("[FAIL]\tmprotect(PROT_WRITE) failed\n");
		return 1;
	}

	 
	if (test_shstk_access(shstk_ptr)) {
		printf("[FAIL]\tShadow stack access to mprotect() writable memory failed\n");
		return 1;
	}

	free_shstk(shstk_ptr);

	signal(SIGSEGV, SIG_DFL);

	printf("[OK]\tmprotect() test\n");

	return 0;
}

char zero[4096];

static void *uffd_thread(void *arg)
{
	struct uffdio_copy req;
	int uffd = *(int *)arg;
	struct uffd_msg msg;
	int ret;

	while (1) {
		ret = read(uffd, &msg, sizeof(msg));
		if (ret > 0)
			break;
		else if (errno == EAGAIN)
			continue;
		return (void *)1;
	}

	req.dst = msg.arg.pagefault.address;
	req.src = (__u64)zero;
	req.len = 4096;
	req.mode = 0;

	if (ioctl(uffd, UFFDIO_COPY, &req))
		return (void *)1;

	return (void *)0;
}

int test_userfaultfd(void)
{
	struct uffdio_register uffdio_register;
	struct uffdio_api uffdio_api;
	struct sigaction sa = {};
	pthread_t thread;
	void *res;
	int uffd;

	sa.sa_sigaction = test_access_fix_handler;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGSEGV, &sa, NULL))
		return 1;

	uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
	if (uffd < 0) {
		printf("[SKIP]\tUserfaultfd unavailable.\n");
		return 0;
	}

	reset_test_shstk(0);

	uffdio_api.api = UFFD_API;
	uffdio_api.features = 0;
	if (ioctl(uffd, UFFDIO_API, &uffdio_api))
		goto err;

	uffdio_register.range.start = (__u64)shstk_ptr;
	uffdio_register.range.len = 4096;
	uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
	if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register))
		goto err;

	if (pthread_create(&thread, NULL, &uffd_thread, &uffd))
		goto err;

	reset_shstk(shstk_ptr);
	test_shstk_access(shstk_ptr);

	if (pthread_join(thread, &res))
		goto err;

	if (test_shstk_access(shstk_ptr))
		goto err;

	free_shstk(shstk_ptr);

	signal(SIGSEGV, SIG_DFL);

	if (!res)
		printf("[OK]\tUserfaultfd test\n");
	return !!res;
err:
	free_shstk(shstk_ptr);
	close(uffd);
	signal(SIGSEGV, SIG_DFL);
	return 1;
}

 
struct node {
	struct node *next;
	void *mapping;
};

 
int test_guard_gap(void)
{
	void *free_area, *shstk, *test_map = (void *)0xFFFFFFFFFFFFFFFF;
	struct node *head = NULL, *cur;

	free_area = mmap(0, SS_SIZE * 3, PROT_READ | PROT_WRITE,
			 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	munmap(free_area, SS_SIZE * 3);

	shstk = create_shstk(free_area + SS_SIZE);
	if (shstk == MAP_FAILED)
		return 1;

	while (test_map > shstk) {
		test_map = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (test_map == MAP_FAILED)
			return 1;
		cur = malloc(sizeof(*cur));
		cur->mapping = test_map;

		cur->next = head;
		head = cur;
	}

	while (head) {
		cur = head;
		head = cur->next;
		munmap(cur->mapping, PAGE_SIZE);
		free(cur);
	}

	free_shstk(shstk);

	if (shstk - test_map - PAGE_SIZE != PAGE_SIZE)
		return 1;

	printf("[OK]\tGuard gap test\n");

	return 0;
}

 
#define __NR_compat_sigaction 67

 
int sigaction32(int signum, const struct sigaction *restrict act,
		struct sigaction *restrict oldact)
{
	register long syscall_reg asm("eax") = __NR_compat_sigaction;
	register long signum_reg asm("ebx") = signum;
	register long act_reg asm("ecx") = (long)act;
	register long oldact_reg asm("edx") = (long)oldact;
	int ret = 0;

	asm volatile ("int $0x80;"
		      : "=a"(ret), "=m"(oldact)
		      : "r"(syscall_reg), "r"(signum_reg), "r"(act_reg),
			"r"(oldact_reg)
		      : "r8", "r9", "r10", "r11"
		     );

	return ret;
}

sigjmp_buf jmp_buffer;

void segv_gp_handler(int signum, siginfo_t *si, void *uc)
{
	segv_triggered = true;

	 
	ARCH_PRCTL(ARCH_SHSTK_DISABLE, ARCH_SHSTK_SHSTK);
	siglongjmp(jmp_buffer, -1);
}

 
int test_32bit(void)
{
	struct sigaction sa = {};
	struct sigaction *sa32;

	 
	sa32 = mmap(0, 4096, PROT_READ | PROT_WRITE,
		    MAP_32BIT | MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	sa32->sa_flags = SA_SIGINFO;

	sa.sa_sigaction = segv_gp_handler;
	sa.sa_flags = SA_SIGINFO;
	if (sigaction(SIGSEGV, &sa, NULL))
		return 1;


	segv_triggered = false;

	 
	asm volatile("" : : : "memory");

	 
	sa32->sa_handler = (void *)sa32;
	if (sigaction32(SIGUSR1, sa32, NULL))
		return 1;

	if (!sigsetjmp(jmp_buffer, 1))
		raise(SIGUSR1);

	if (segv_triggered)
		printf("[OK]\t32 bit test\n");

	return !segv_triggered;
}

void segv_handler_ptrace(int signum, siginfo_t *si, void *uc)
{
	 
	exit(0);
}

int test_ptrace(void)
{
	unsigned long saved_ssp, ssp = 0;
	struct sigaction sa= {};
	struct iovec iov;
	int status;
	int pid;

	iov.iov_base = &ssp;
	iov.iov_len = sizeof(ssp);

	pid = fork();
	if (!pid) {
		ssp = get_ssp();

		sa.sa_sigaction = segv_handler_ptrace;
		sa.sa_flags = SA_SIGINFO;
		if (sigaction(SIGSEGV, &sa, NULL))
			return 1;

		ptrace(PTRACE_TRACEME, NULL, NULL, NULL);
		 
		raise(SIGTRAP);

		exit(1);
	}

	while (waitpid(pid, &status, 0) != -1 && WSTOPSIG(status) != SIGTRAP);

	if (ptrace(PTRACE_GETREGSET, pid, NT_X86_SHSTK, &iov)) {
		printf("[INFO]\tFailed to PTRACE_GETREGS\n");
		goto out_kill;
	}

	if (!ssp) {
		printf("[INFO]\tPtrace child SSP was 0\n");
		goto out_kill;
	}

	saved_ssp = ssp;

	iov.iov_len = 0;
	if (!ptrace(PTRACE_SETREGSET, pid, NT_X86_SHSTK, &iov)) {
		printf("[INFO]\tToo small size accepted via PTRACE_SETREGS\n");
		goto out_kill;
	}

	iov.iov_len = sizeof(ssp) + 1;
	if (!ptrace(PTRACE_SETREGSET, pid, NT_X86_SHSTK, &iov)) {
		printf("[INFO]\tToo large size accepted via PTRACE_SETREGS\n");
		goto out_kill;
	}

	ssp += 1;
	if (!ptrace(PTRACE_SETREGSET, pid, NT_X86_SHSTK, &iov)) {
		printf("[INFO]\tUnaligned SSP written via PTRACE_SETREGS\n");
		goto out_kill;
	}

	ssp = 0xFFFFFFFFFFFF0000;
	if (!ptrace(PTRACE_SETREGSET, pid, NT_X86_SHSTK, &iov)) {
		printf("[INFO]\tKernel range SSP written via PTRACE_SETREGS\n");
		goto out_kill;
	}

	 
	ssp = saved_ssp + 8;
	iov.iov_len = sizeof(ssp);
	if (ptrace(PTRACE_SETREGSET, pid, NT_X86_SHSTK, &iov)) {
		printf("[INFO]\tFailed to PTRACE_SETREGS\n");
		goto out_kill;
	}

	if (ptrace(PTRACE_DETACH, pid, NULL, NULL)) {
		printf("[INFO]\tFailed to PTRACE_DETACH\n");
		goto out_kill;
	}

	waitpid(pid, &status, 0);
	if (WEXITSTATUS(status))
		return 1;

	printf("[OK]\tPtrace test\n");
	return 0;

out_kill:
	kill(pid, SIGKILL);
	return 1;
}

int main(int argc, char *argv[])
{
	int ret = 0;

	if (ARCH_PRCTL(ARCH_SHSTK_ENABLE, ARCH_SHSTK_SHSTK)) {
		printf("[SKIP]\tCould not enable Shadow stack\n");
		return 1;
	}

	if (ARCH_PRCTL(ARCH_SHSTK_DISABLE, ARCH_SHSTK_SHSTK)) {
		ret = 1;
		printf("[FAIL]\tDisabling shadow stack failed\n");
	}

	if (ARCH_PRCTL(ARCH_SHSTK_ENABLE, ARCH_SHSTK_SHSTK)) {
		printf("[SKIP]\tCould not re-enable Shadow stack\n");
		return 1;
	}

	if (ARCH_PRCTL(ARCH_SHSTK_ENABLE, ARCH_SHSTK_WRSS)) {
		printf("[SKIP]\tCould not enable WRSS\n");
		ret = 1;
		goto out;
	}

	 
	if (!get_ssp()) {
		printf("[FAIL]\tShadow stack disabled\n");
		return 1;
	}

	if (test_shstk_pivot()) {
		ret = 1;
		printf("[FAIL]\tShadow stack pivot\n");
		goto out;
	}

	if (test_shstk_faults()) {
		ret = 1;
		printf("[FAIL]\tShadow stack fault test\n");
		goto out;
	}

	if (test_shstk_violation()) {
		ret = 1;
		printf("[FAIL]\tShadow stack violation test\n");
		goto out;
	}

	if (test_gup()) {
		ret = 1;
		printf("[FAIL]\tShadow shadow stack gup\n");
		goto out;
	}

	if (test_mprotect()) {
		ret = 1;
		printf("[FAIL]\tShadow shadow mprotect test\n");
		goto out;
	}

	if (test_userfaultfd()) {
		ret = 1;
		printf("[FAIL]\tUserfaultfd test\n");
		goto out;
	}

	if (test_guard_gap()) {
		ret = 1;
		printf("[FAIL]\tGuard gap test\n");
		goto out;
	}

	if (test_ptrace()) {
		ret = 1;
		printf("[FAIL]\tptrace test\n");
	}

	if (test_32bit()) {
		ret = 1;
		printf("[FAIL]\t32 bit test\n");
		goto out;
	}

	return ret;

out:
	 
	if (ARCH_PRCTL(ARCH_SHSTK_DISABLE, ARCH_SHSTK_SHSTK)) {
		ret = 1;
		printf("[FAIL]\tDisabling shadow stack failed\n");
	}

	return ret;
}
#endif
