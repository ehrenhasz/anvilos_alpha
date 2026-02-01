

 

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <unistd.h>
#include <sys/mman.h>

#include "pkeys.h"


#define PPC_INST_NOP	0x60000000
#define PPC_INST_TRAP	0x7fe00008
#define PPC_INST_BLR	0x4e800020

static volatile sig_atomic_t fault_code;
static volatile sig_atomic_t remaining_faults;
static volatile unsigned int *fault_addr;
static unsigned long pgsize, numinsns;
static unsigned int *insns;
static bool pkeys_supported;

static bool is_fault_expected(int fault_code)
{
	if (fault_code == SEGV_ACCERR)
		return true;

	 
	if (fault_code == SEGV_PKUERR && pkeys_supported)
		return true;

	return false;
}

static void trap_handler(int signum, siginfo_t *sinfo, void *ctx)
{
	 
	if (sinfo->si_addr != (void *)fault_addr)
		sigsafe_err("got a fault for an unexpected address\n");

	_exit(1);
}

static void segv_handler(int signum, siginfo_t *sinfo, void *ctx)
{
	fault_code = sinfo->si_code;

	 
	if (sinfo->si_addr != (void *)fault_addr) {
		sigsafe_err("got a fault for an unexpected address\n");
		_exit(1);
	}

	 
	if (!remaining_faults) {
		sigsafe_err("got too many faults for the same address\n");
		_exit(1);
	}


	 
	if (is_fault_expected(fault_code)) {
		if (mprotect(insns, pgsize, PROT_READ | PROT_WRITE | PROT_EXEC)) {
			sigsafe_err("failed to set access permissions\n");
			_exit(1);
		}
	} else {
		sigsafe_err("got a fault with an unexpected code\n");
		_exit(1);
	}

	remaining_faults--;
}

static int check_exec_fault(int rights)
{
	 
	fault_code = -1;
	remaining_faults = 0;
	if (!(rights & PROT_EXEC))
		remaining_faults = 1;

	FAIL_IF(mprotect(insns, pgsize, rights) != 0);
	asm volatile("mtctr	%0; bctrl" : : "r"(insns));

	FAIL_IF(remaining_faults != 0);
	if (!(rights & PROT_EXEC))
		FAIL_IF(!is_fault_expected(fault_code));

	return 0;
}

static int test(void)
{
	struct sigaction segv_act, trap_act;
	int i;

	 
	SKIP_IF(!have_hwcap2(PPC_FEATURE2_ARCH_3_00));

	 
	pkeys_supported = pkeys_unsupported() == 0;

	 
	segv_act.sa_handler = 0;
	segv_act.sa_sigaction = segv_handler;
	FAIL_IF(sigprocmask(SIG_SETMASK, 0, &segv_act.sa_mask) != 0);
	segv_act.sa_flags = SA_SIGINFO;
	segv_act.sa_restorer = 0;
	FAIL_IF(sigaction(SIGSEGV, &segv_act, NULL) != 0);

	 
	trap_act.sa_handler = 0;
	trap_act.sa_sigaction = trap_handler;
	FAIL_IF(sigprocmask(SIG_SETMASK, 0, &trap_act.sa_mask) != 0);
	trap_act.sa_flags = SA_SIGINFO;
	trap_act.sa_restorer = 0;
	FAIL_IF(sigaction(SIGTRAP, &trap_act, NULL) != 0);

	 
	pgsize = getpagesize();
	numinsns = pgsize / sizeof(unsigned int);
	insns = (unsigned int *)mmap(NULL, pgsize, PROT_READ | PROT_WRITE,
				      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	FAIL_IF(insns == MAP_FAILED);

	 
	for (i = 1; i < numinsns - 1; i++)
		insns[i] = PPC_INST_NOP;

	 
	insns[0] = PPC_INST_TRAP;

	 
	insns[numinsns - 1] = PPC_INST_BLR;

	 
	fault_addr = insns;

	 
	fault_code = -1;
	remaining_faults = 1;
	printf("Testing read on --x, should fault...");
	FAIL_IF(mprotect(insns, pgsize, PROT_EXEC) != 0);
	i = *fault_addr;
	FAIL_IF(remaining_faults != 0 || !is_fault_expected(fault_code));
	printf("ok!\n");

	 
	fault_code = -1;
	remaining_faults = 1;
	printf("Testing write on --x, should fault...");
	FAIL_IF(mprotect(insns, pgsize, PROT_EXEC) != 0);
	*fault_addr = PPC_INST_NOP;
	FAIL_IF(remaining_faults != 0 || !is_fault_expected(fault_code));
	printf("ok!\n");

	printf("Testing exec on ---, should fault...");
	FAIL_IF(check_exec_fault(PROT_NONE));
	printf("ok!\n");

	printf("Testing exec on r--, should fault...");
	FAIL_IF(check_exec_fault(PROT_READ));
	printf("ok!\n");

	printf("Testing exec on -w-, should fault...");
	FAIL_IF(check_exec_fault(PROT_WRITE));
	printf("ok!\n");

	printf("Testing exec on rw-, should fault...");
	FAIL_IF(check_exec_fault(PROT_READ | PROT_WRITE));
	printf("ok!\n");

	printf("Testing exec on --x, should succeed...");
	FAIL_IF(check_exec_fault(PROT_EXEC));
	printf("ok!\n");

	printf("Testing exec on r-x, should succeed...");
	FAIL_IF(check_exec_fault(PROT_READ | PROT_EXEC));
	printf("ok!\n");

	printf("Testing exec on -wx, should succeed...");
	FAIL_IF(check_exec_fault(PROT_WRITE | PROT_EXEC));
	printf("ok!\n");

	printf("Testing exec on rwx, should succeed...");
	FAIL_IF(check_exec_fault(PROT_READ | PROT_WRITE | PROT_EXEC));
	printf("ok!\n");

	 
	FAIL_IF(munmap((void *)insns, pgsize));

	return 0;
}

int main(void)
{
	return test_harness(test, "exec_prot");
}
