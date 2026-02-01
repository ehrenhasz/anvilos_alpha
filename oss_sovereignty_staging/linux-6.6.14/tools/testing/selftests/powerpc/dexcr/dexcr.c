

#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "dexcr.h"
#include "reg.h"
#include "utils.h"

static jmp_buf generic_signal_jump_buf;

static void generic_signal_handler(int signum, siginfo_t *info, void *context)
{
	longjmp(generic_signal_jump_buf, 0);
}

bool dexcr_exists(void)
{
	struct sigaction old;
	volatile bool exists;

	old = push_signal_handler(SIGILL, generic_signal_handler);
	if (setjmp(generic_signal_jump_buf))
		goto out;

	 
	exists = false;
	mfspr(SPRN_DEXCR_RO);
	exists = true;

out:
	pop_signal_handler(SIGILL, old);
	return exists;
}

 
bool hashchk_triggers(void)
{
	struct sigaction old;
	volatile bool triggers;

	old = push_signal_handler(SIGILL, generic_signal_handler);
	if (setjmp(generic_signal_jump_buf))
		goto out;

	triggers = true;
	do_bad_hashchk();
	triggers = false;

out:
	pop_signal_handler(SIGILL, old);
	return triggers;
}

unsigned int get_dexcr(enum dexcr_source source)
{
	switch (source) {
	case DEXCR:
		return mfspr(SPRN_DEXCR_RO);
	case HDEXCR:
		return mfspr(SPRN_HDEXCR_RO);
	case EFFECTIVE:
		return mfspr(SPRN_DEXCR_RO) | mfspr(SPRN_HDEXCR_RO);
	default:
		FAIL_IF_EXIT_MSG(true, "bad enum dexcr_source");
	}
}

void await_child_success(pid_t pid)
{
	int wstatus;

	FAIL_IF_EXIT_MSG(pid == -1, "fork failed");
	FAIL_IF_EXIT_MSG(waitpid(pid, &wstatus, 0) == -1, "wait failed");
	FAIL_IF_EXIT_MSG(!WIFEXITED(wstatus), "child did not exit cleanly");
	FAIL_IF_EXIT_MSG(WEXITSTATUS(wstatus) != 0, "child exit error");
}

 
void hashst(unsigned long lr, void *sp)
{
	asm volatile ("addi 31, %0, 0;"		 
		      "addi 30, %1, 8;"		 
		      PPC_RAW_HASHST(31, -8, 30)	 
		      : : "r" (lr), "r" (sp) : "r31", "r30", "memory");
}

 
void hashchk(unsigned long lr, void *sp)
{
	asm volatile ("addi 31, %0, 0;"		 
		      "addi 30, %1, 8;"		 
		      PPC_RAW_HASHCHK(31, -8, 30)	 
		      : : "r" (lr), "r" (sp) : "r31", "r30", "memory");
}

void do_bad_hashchk(void)
{
	unsigned long hash = 0;

	hashst(0, &hash);
	hash += 1;
	hashchk(0, &hash);
}
