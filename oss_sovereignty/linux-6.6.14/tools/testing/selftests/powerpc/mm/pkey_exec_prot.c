

 

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <unistd.h>

#include "pkeys.h"

#define PPC_INST_NOP	0x60000000
#define PPC_INST_TRAP	0x7fe00008
#define PPC_INST_BLR	0x4e800020

static volatile sig_atomic_t fault_pkey, fault_code, fault_type;
static volatile sig_atomic_t remaining_faults;
static volatile unsigned int *fault_addr;
static unsigned long pgsize, numinsns;
static unsigned int *insns;

static void trap_handler(int signum, siginfo_t *sinfo, void *ctx)
{
	 
	if (sinfo->si_addr != (void *) fault_addr)
		sigsafe_err("got a fault for an unexpected address\n");

	_exit(1);
}

static void segv_handler(int signum, siginfo_t *sinfo, void *ctx)
{
	int signal_pkey;

	signal_pkey = siginfo_pkey(sinfo);
	fault_code = sinfo->si_code;

	 
	if (sinfo->si_addr != (void *) fault_addr) {
		sigsafe_err("got a fault for an unexpected address\n");
		_exit(1);
	}

	 
	if (!remaining_faults) {
		sigsafe_err("got too many faults for the same address\n");
		_exit(1);
	}


	 
	switch (fault_code) {
	case SEGV_ACCERR:
		if (mprotect(insns, pgsize, PROT_READ | PROT_WRITE)) {
			sigsafe_err("failed to set access permissions\n");
			_exit(1);
		}
		break;
	case SEGV_PKUERR:
		if (signal_pkey != fault_pkey) {
			sigsafe_err("got a fault for an unexpected pkey\n");
			_exit(1);
		}

		switch (fault_type) {
		case PKEY_DISABLE_ACCESS:
			pkey_set_rights(fault_pkey, 0);
			break;
		case PKEY_DISABLE_EXECUTE:
			 
			if (mprotect(insns, pgsize, PROT_EXEC)) {
				sigsafe_err("failed to set execute permissions\n");
				_exit(1);
			}
			break;
		default:
			sigsafe_err("got a fault with an unexpected type\n");
			_exit(1);
		}
		break;
	default:
		sigsafe_err("got a fault with an unexpected code\n");
		_exit(1);
	}

	remaining_faults--;
}

static int test(void)
{
	struct sigaction segv_act, trap_act;
	unsigned long rights;
	int pkey, ret, i;

	ret = pkeys_unsupported();
	if (ret)
		return ret;

	 
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
	insns = (unsigned int *) mmap(NULL, pgsize, PROT_READ | PROT_WRITE,
				      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	FAIL_IF(insns == MAP_FAILED);

	 
	for (i = 1; i < numinsns - 1; i++)
		insns[i] = PPC_INST_NOP;

	 
	insns[0] = PPC_INST_TRAP;

	 
	insns[numinsns - 1] = PPC_INST_BLR;

	 
	rights = PKEY_DISABLE_EXECUTE;
	pkey = sys_pkey_alloc(0, rights);
	FAIL_IF(pkey < 0);

	 
	fault_addr = insns;

	 
	fault_type = -1;
	fault_pkey = -1;

	 
	remaining_faults = 0;
	FAIL_IF(sys_pkey_mprotect(insns, pgsize, PROT_EXEC, pkey) != 0);
	printf("read from %p, pkey permissions are %s\n", fault_addr,
	       pkey_rights(rights));
	i = *fault_addr;
	FAIL_IF(remaining_faults != 0);

	 
	remaining_faults = 1;
	FAIL_IF(sys_pkey_mprotect(insns, pgsize, PROT_EXEC, pkey) != 0);
	printf("write to %p, pkey permissions are %s\n", fault_addr,
	       pkey_rights(rights));
	*fault_addr = PPC_INST_TRAP;
	FAIL_IF(remaining_faults != 0 || fault_code != SEGV_ACCERR);

	 
	rights |= PKEY_DISABLE_ACCESS;
	fault_type = PKEY_DISABLE_ACCESS;
	fault_pkey = pkey;

	 
	remaining_faults = 1;
	FAIL_IF(sys_pkey_mprotect(insns, pgsize, PROT_EXEC, pkey) != 0);
	pkey_set_rights(pkey, rights);
	printf("read from %p, pkey permissions are %s\n", fault_addr,
	       pkey_rights(rights));
	i = *fault_addr;
	FAIL_IF(remaining_faults != 0 || fault_code != SEGV_PKUERR);

	 
	remaining_faults = 2;
	FAIL_IF(sys_pkey_mprotect(insns, pgsize, PROT_EXEC, pkey) != 0);
	pkey_set_rights(pkey, rights);
	printf("write to %p, pkey permissions are %s\n", fault_addr,
	       pkey_rights(rights));
	*fault_addr = PPC_INST_NOP;
	FAIL_IF(remaining_faults != 0 || fault_code != SEGV_ACCERR);

	 
	sys_pkey_free(pkey);

	rights = 0;
	do {
		 
		pkey = sys_pkey_alloc(0, rights);
		FAIL_IF(pkey < 0);

		 
		fault_pkey = pkey;
		fault_type = -1;
		remaining_faults = 0;
		if (rights & PKEY_DISABLE_EXECUTE) {
			fault_type = PKEY_DISABLE_EXECUTE;
			remaining_faults = 1;
		}

		FAIL_IF(sys_pkey_mprotect(insns, pgsize, PROT_EXEC, pkey) != 0);
		printf("execute at %p, pkey permissions are %s\n", fault_addr,
		       pkey_rights(rights));
		asm volatile("mtctr	%0; bctrl" : : "r"(insns));
		FAIL_IF(remaining_faults != 0);
		if (rights & PKEY_DISABLE_EXECUTE)
			FAIL_IF(fault_code != SEGV_PKUERR);

		 
		sys_pkey_free(pkey);

		 
		rights = next_pkey_rights(rights);
	} while (rights);

	 
	munmap((void *) insns, pgsize);

	return 0;
}

int main(void)
{
	return test_harness(test, "pkey_exec_prot");
}
