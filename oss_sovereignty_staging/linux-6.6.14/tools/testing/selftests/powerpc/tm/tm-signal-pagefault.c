
 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/userfaultfd.h>
#include <poll.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>

#include "tm.h"


#define UF_MEM_SIZE 655360	 

 
static char *uf_mem;
static size_t uf_mem_offset = 0;

 
static char backing_mem[UF_MEM_SIZE];

static size_t pagesize;

 
void *get_uf_mem(size_t size, void *backing_data)
{
	void *ret;

	if (uf_mem_offset + size > UF_MEM_SIZE) {
		fprintf(stderr, "Requesting more uf_mem than expected!\n");
		exit(EXIT_FAILURE);
	}

	ret = &uf_mem[uf_mem_offset];

	 
	if (backing_data != NULL)
		memcpy(&backing_mem[uf_mem_offset], backing_data, size);

	 
	uf_mem_offset += size;
	 
	uf_mem_offset = (uf_mem_offset + pagesize - 1) & ~(pagesize - 1);

	return ret;
}

void *fault_handler_thread(void *arg)
{
	struct uffd_msg msg;	 
	long uffd;		 
	struct uffdio_copy uffdio_copy;
	struct pollfd pollfd;
	ssize_t nread, offset;

	uffd = (long) arg;

	for (;;) {
		pollfd.fd = uffd;
		pollfd.events = POLLIN;
		if (poll(&pollfd, 1, -1) == -1) {
			perror("poll() failed");
			exit(EXIT_FAILURE);
		}

		nread = read(uffd, &msg, sizeof(msg));
		if (nread == 0) {
			fprintf(stderr, "read(): EOF on userfaultfd\n");
			exit(EXIT_FAILURE);
		}

		if (nread == -1) {
			perror("read() failed");
			exit(EXIT_FAILURE);
		}

		 
		if (msg.event != UFFD_EVENT_PAGEFAULT) {
			fprintf(stderr, "Unexpected event on userfaultfd\n");
			exit(EXIT_FAILURE);
		}

		 
		uffdio_copy.dst = msg.arg.pagefault.address & ~(pagesize-1);

		offset = (char *) uffdio_copy.dst - uf_mem;
		uffdio_copy.src = (unsigned long) &backing_mem[offset];

		uffdio_copy.len = pagesize;
		uffdio_copy.mode = 0;
		uffdio_copy.copy = 0;
		if (ioctl(uffd, UFFDIO_COPY, &uffdio_copy) == -1) {
			perror("ioctl-UFFDIO_COPY failed");
			exit(EXIT_FAILURE);
		}
	}
}

void setup_uf_mem(void)
{
	long uffd;		 
	pthread_t thr;
	struct uffdio_api uffdio_api;
	struct uffdio_register uffdio_register;
	int ret;

	pagesize = sysconf(_SC_PAGE_SIZE);

	 
	uffd = syscall(__NR_userfaultfd, O_CLOEXEC | O_NONBLOCK);
	if (uffd == -1) {
		perror("userfaultfd() failed");
		exit(EXIT_FAILURE);
	}
	uffdio_api.api = UFFD_API;
	uffdio_api.features = 0;
	if (ioctl(uffd, UFFDIO_API, &uffdio_api) == -1) {
		perror("ioctl-UFFDIO_API failed");
		exit(EXIT_FAILURE);
	}

	 
	uf_mem = mmap(NULL, UF_MEM_SIZE, PROT_READ | PROT_WRITE,
		      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (uf_mem == MAP_FAILED) {
		perror("mmap() failed");
		exit(EXIT_FAILURE);
	}

	 
	uffdio_register.range.start = (unsigned long) uf_mem;
	uffdio_register.range.len = UF_MEM_SIZE;
	uffdio_register.mode = UFFDIO_REGISTER_MODE_MISSING;
	if (ioctl(uffd, UFFDIO_REGISTER, &uffdio_register) == -1) {
		perror("ioctl-UFFDIO_REGISTER");
		exit(EXIT_FAILURE);
	}

	 
	ret = pthread_create(&thr, NULL, fault_handler_thread, (void *) uffd);
	if (ret != 0) {
		fprintf(stderr, "pthread_create(): Error. Returned %d\n", ret);
		exit(EXIT_FAILURE);
	}
}

 
void signal_handler(int signo, siginfo_t *si, void *uc)
{
	ucontext_t *ucp = uc;

	 
	ucp->uc_link->uc_mcontext.regs->nip += 4;

	ucp->uc_mcontext.v_regs =
		get_uf_mem(sizeof(elf_vrreg_t), ucp->uc_mcontext.v_regs);

	ucp->uc_link->uc_mcontext.v_regs =
		get_uf_mem(sizeof(elf_vrreg_t), ucp->uc_link->uc_mcontext.v_regs);

	ucp->uc_link = get_uf_mem(sizeof(ucontext_t), ucp->uc_link);
}

bool have_userfaultfd(void)
{
	long rc;

	errno = 0;
	rc = syscall(__NR_userfaultfd, -1);

	return rc == 0 || errno != ENOSYS;
}

int tm_signal_pagefault(void)
{
	struct sigaction sa;
	stack_t ss;

	SKIP_IF(!have_htm());
	SKIP_IF(htm_is_synthetic());
	SKIP_IF(!have_userfaultfd());

	setup_uf_mem();

	 
	ss.ss_sp = get_uf_mem(SIGSTKSZ, NULL);
	ss.ss_size = SIGSTKSZ;
	ss.ss_flags = 0;
	if (sigaltstack(&ss, NULL) == -1) {
		perror("sigaltstack() failed");
		exit(EXIT_FAILURE);
	}

	sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
	sa.sa_sigaction = signal_handler;
	if (sigaction(SIGTRAP, &sa, NULL) == -1) {
		perror("sigaction() failed");
		exit(EXIT_FAILURE);
	}

	 
	asm __volatile__(
			"tbegin.;"
			"beq    1f;"
			"trap;"
			"1: ;"
			: : : "memory");

	 
	asm __volatile__(
			"tbegin.;"
			"beq    1f;"
			"tsuspend.;"
			"trap;"
			"tresume.;"
			"1: ;"
			: : : "memory");

	return EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	 
	test_harness_set_timeout(2);
	return test_harness(tm_signal_pagefault, "tm_signal_pagefault");
}
