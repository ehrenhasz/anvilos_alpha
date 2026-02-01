
 

#define _GNU_SOURCE

#include <features.h>
#include <stdio.h>

#include "helpers.h"

#if defined(__GLIBC__) && __GLIBC__ == 2 && __GLIBC_MINOR__ < 16

int main()
{
	 
	printf("[SKIP]\tGLIBC before 2.16 cannot compile this test\n");
	return 0;
}

#else

#include <sys/time.h>
#include <stdlib.h>
#include <syscall.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/ucontext.h>
#include <err.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <link.h>
#include <sys/auxv.h>
#include <dlfcn.h>
#include <unwind.h>

static void sethandler(int sig, void (*handler)(int, siginfo_t *, void *),
		       int flags)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = handler;
	sa.sa_flags = SA_SIGINFO | flags;
	sigemptyset(&sa.sa_mask);
	if (sigaction(sig, &sa, 0))
		err(1, "sigaction");
}

static volatile sig_atomic_t nerrs;
static unsigned long sysinfo;
static bool got_sysinfo = false;
static unsigned long return_address;

struct unwind_state {
	unsigned long ip;	 
	int depth;		 
};

_Unwind_Reason_Code trace_fn(struct _Unwind_Context * ctx, void *opaque)
{
	struct unwind_state *state = opaque;
	unsigned long ip = _Unwind_GetIP(ctx);

	if (state->depth == -1) {
		if (ip == state->ip)
			state->depth = 0;
		else
			return _URC_NO_REASON;	 
	}
	printf("\t  0x%lx\n", ip);

	if (ip == return_address) {
		 
		unsigned long eax = _Unwind_GetGR(ctx, 0);
		unsigned long ecx = _Unwind_GetGR(ctx, 1);
		unsigned long edx = _Unwind_GetGR(ctx, 2);
		unsigned long ebx = _Unwind_GetGR(ctx, 3);
		unsigned long ebp = _Unwind_GetGR(ctx, 5);
		unsigned long esi = _Unwind_GetGR(ctx, 6);
		unsigned long edi = _Unwind_GetGR(ctx, 7);
		bool ok = (eax == SYS_getpid || eax == getpid()) &&
			ebx == 1 && ecx == 2 && edx == 3 &&
			esi == 4 && edi == 5 && ebp == 6;

		if (!ok)
			nerrs++;
		printf("[%s]\t  NR = %ld, args = %ld, %ld, %ld, %ld, %ld, %ld\n",
		       (ok ? "OK" : "FAIL"),
		       eax, ebx, ecx, edx, esi, edi, ebp);

		return _URC_NORMAL_STOP;
	} else {
		state->depth++;
		return _URC_NO_REASON;
	}
}

static void sigtrap(int sig, siginfo_t *info, void *ctx_void)
{
	ucontext_t *ctx = (ucontext_t *)ctx_void;
	struct unwind_state state;
	unsigned long ip = ctx->uc_mcontext.gregs[REG_EIP];

	if (!got_sysinfo && ip == sysinfo) {
		got_sysinfo = true;

		 
		return_address = *(unsigned long *)(unsigned long)ctx->uc_mcontext.gregs[REG_ESP];

		printf("\tIn vsyscall at 0x%lx, returning to 0x%lx\n",
		       ip, return_address);
	}

	if (!got_sysinfo)
		return;		 

	if (ip == return_address) {
		ctx->uc_mcontext.gregs[REG_EFL] &= ~X86_EFLAGS_TF;
		printf("\tVsyscall is done\n");
		return;
	}

	printf("\tSIGTRAP at 0x%lx\n", ip);

	state.ip = ip;
	state.depth = -1;
	_Unwind_Backtrace(trace_fn, &state);
}

int main()
{
	sysinfo = getauxval(AT_SYSINFO);
	printf("\tAT_SYSINFO is 0x%lx\n", sysinfo);

	Dl_info info;
	if (!dladdr((void *)sysinfo, &info)) {
		printf("[WARN]\tdladdr failed on AT_SYSINFO\n");
	} else {
		printf("[OK]\tAT_SYSINFO maps to %s, loaded at 0x%p\n",
		       info.dli_fname, info.dli_fbase);
	}

	sethandler(SIGTRAP, sigtrap, 0);

	syscall(SYS_getpid);   
	printf("[RUN]\tSet TF and check a fast syscall\n");
	set_eflags(get_eflags() | X86_EFLAGS_TF);
	syscall(SYS_getpid, 1, 2, 3, 4, 5, 6);
	if (!got_sysinfo) {
		set_eflags(get_eflags() & ~X86_EFLAGS_TF);

		 
		printf("[WARN]\tsyscall(2) didn't enter AT_SYSINFO\n");
	}

	if (get_eflags() & X86_EFLAGS_TF) {
		printf("[FAIL]\tTF is still set\n");
		nerrs++;
	}

	if (nerrs) {
		printf("[FAIL]\tThere were errors\n");
		return 1;
	} else {
		printf("[OK]\tAll is well\n");
		return 0;
	}
}

#endif	 
