
 

#define _GNU_SOURCE

#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/signal.h>
#include <sys/ucontext.h>
#include <asm/ldt.h>
#include <err.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/ptrace.h>
#include <sys/user.h>

 
typedef unsigned int u32;
typedef unsigned short u16;
#include "../../../../arch/x86/include/asm/desc_defs.h"

 
#ifdef __x86_64__
 
#define UC_SIGCONTEXT_SS       0x2
#define UC_STRICT_RESTORE_SS   0x4
#endif

 
#define LDT_OFFSET 6

 
static unsigned char stack16[65536] __attribute__((aligned(4096)));

 
asm (".pushsection .text\n\t"
     ".type int3, @function\n\t"
     ".align 4096\n\t"
     "int3:\n\t"
     "mov %ss,%ecx\n\t"
     "int3\n\t"
     ".size int3, . - int3\n\t"
     ".align 4096, 0xcc\n\t"
     ".popsection");
extern char int3[4096];

 
static unsigned short ldt_nonexistent_sel;
static unsigned short code16_sel, data16_sel, npcode32_sel, npdata32_sel;

static unsigned short gdt_data16_idx, gdt_npdata32_idx;

static unsigned short GDT3(int idx)
{
	return (idx << 3) | 3;
}

static unsigned short LDT3(int idx)
{
	return (idx << 3) | 7;
}

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

static void clearhandler(int sig)
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	if (sigaction(sig, &sa, 0))
		err(1, "sigaction");
}

static void add_ldt(const struct user_desc *desc, unsigned short *var,
		    const char *name)
{
	if (syscall(SYS_modify_ldt, 1, desc, sizeof(*desc)) == 0) {
		*var = LDT3(desc->entry_number);
	} else {
		printf("[NOTE]\tFailed to create %s segment\n", name);
		*var = 0;
	}
}

static void setup_ldt(void)
{
	if ((unsigned long)stack16 > (1ULL << 32) - sizeof(stack16))
		errx(1, "stack16 is too high\n");
	if ((unsigned long)int3 > (1ULL << 32) - sizeof(int3))
		errx(1, "int3 is too high\n");

	ldt_nonexistent_sel = LDT3(LDT_OFFSET + 2);

	const struct user_desc code16_desc = {
		.entry_number    = LDT_OFFSET + 0,
		.base_addr       = (unsigned long)int3,
		.limit           = 4095,
		.seg_32bit       = 0,
		.contents        = 2,  
		.read_exec_only  = 0,
		.limit_in_pages  = 0,
		.seg_not_present = 0,
		.useable         = 0
	};
	add_ldt(&code16_desc, &code16_sel, "code16");

	const struct user_desc data16_desc = {
		.entry_number    = LDT_OFFSET + 1,
		.base_addr       = (unsigned long)stack16,
		.limit           = 0xffff,
		.seg_32bit       = 0,
		.contents        = 0,  
		.read_exec_only  = 0,
		.limit_in_pages  = 0,
		.seg_not_present = 0,
		.useable         = 0
	};
	add_ldt(&data16_desc, &data16_sel, "data16");

	const struct user_desc npcode32_desc = {
		.entry_number    = LDT_OFFSET + 3,
		.base_addr       = (unsigned long)int3,
		.limit           = 4095,
		.seg_32bit       = 1,
		.contents        = 2,  
		.read_exec_only  = 0,
		.limit_in_pages  = 0,
		.seg_not_present = 1,
		.useable         = 0
	};
	add_ldt(&npcode32_desc, &npcode32_sel, "npcode32");

	const struct user_desc npdata32_desc = {
		.entry_number    = LDT_OFFSET + 4,
		.base_addr       = (unsigned long)stack16,
		.limit           = 0xffff,
		.seg_32bit       = 1,
		.contents        = 0,  
		.read_exec_only  = 0,
		.limit_in_pages  = 0,
		.seg_not_present = 1,
		.useable         = 0
	};
	add_ldt(&npdata32_desc, &npdata32_sel, "npdata32");

	struct user_desc gdt_data16_desc = {
		.entry_number    = -1,
		.base_addr       = (unsigned long)stack16,
		.limit           = 0xffff,
		.seg_32bit       = 0,
		.contents        = 0,  
		.read_exec_only  = 0,
		.limit_in_pages  = 0,
		.seg_not_present = 0,
		.useable         = 0
	};

	if (syscall(SYS_set_thread_area, &gdt_data16_desc) == 0) {
		 
		printf("[WARN]\tset_thread_area allocated data16 at index %d\n",
		       gdt_data16_desc.entry_number);
		gdt_data16_idx = gdt_data16_desc.entry_number;
	} else {
		printf("[OK]\tset_thread_area refused 16-bit data\n");
	}

	struct user_desc gdt_npdata32_desc = {
		.entry_number    = -1,
		.base_addr       = (unsigned long)stack16,
		.limit           = 0xffff,
		.seg_32bit       = 1,
		.contents        = 0,  
		.read_exec_only  = 0,
		.limit_in_pages  = 0,
		.seg_not_present = 1,
		.useable         = 0
	};

	if (syscall(SYS_set_thread_area, &gdt_npdata32_desc) == 0) {
		 
		printf("[WARN]\tset_thread_area allocated npdata32 at index %d\n",
		       gdt_npdata32_desc.entry_number);
		gdt_npdata32_idx = gdt_npdata32_desc.entry_number;
	} else {
		printf("[OK]\tset_thread_area refused 16-bit data\n");
	}
}

 
static gregset_t initial_regs, requested_regs, resulting_regs;

 
static volatile unsigned short sig_cs, sig_ss;
static volatile sig_atomic_t sig_trapped, sig_err, sig_trapno;
#ifdef __x86_64__
static volatile sig_atomic_t sig_corrupt_final_ss;
#endif

 
#ifdef __x86_64__
# define REG_IP REG_RIP
# define REG_SP REG_RSP
# define REG_CX REG_RCX

struct selectors {
	unsigned short cs, gs, fs, ss;
};

static unsigned short *ssptr(ucontext_t *ctx)
{
	struct selectors *sels = (void *)&ctx->uc_mcontext.gregs[REG_CSGSFS];
	return &sels->ss;
}

static unsigned short *csptr(ucontext_t *ctx)
{
	struct selectors *sels = (void *)&ctx->uc_mcontext.gregs[REG_CSGSFS];
	return &sels->cs;
}
#else
# define REG_IP REG_EIP
# define REG_SP REG_ESP
# define REG_CX REG_ECX

static greg_t *ssptr(ucontext_t *ctx)
{
	return &ctx->uc_mcontext.gregs[REG_SS];
}

static greg_t *csptr(ucontext_t *ctx)
{
	return &ctx->uc_mcontext.gregs[REG_CS];
}
#endif

 
int cs_bitness(unsigned short cs)
{
	uint32_t valid = 0, ar;
	asm ("lar %[cs], %[ar]\n\t"
	     "jnz 1f\n\t"
	     "mov $1, %[valid]\n\t"
	     "1:"
	     : [ar] "=r" (ar), [valid] "+rm" (valid)
	     : [cs] "r" (cs));

	if (!valid)
		return -1;

	bool db = (ar & (1 << 22));
	bool l = (ar & (1 << 21));

	if (!(ar & (1<<11)))
	    return -1;	 

	if (l && !db)
		return 64;
	else if (!l && db)
		return 32;
	else if (!l && !db)
		return 16;
	else
		return -1;	 
}

 
bool is_valid_ss(unsigned short cs)
{
	uint32_t valid = 0, ar;
	asm ("lar %[cs], %[ar]\n\t"
	     "jnz 1f\n\t"
	     "mov $1, %[valid]\n\t"
	     "1:"
	     : [ar] "=r" (ar), [valid] "+rm" (valid)
	     : [cs] "r" (cs));

	if (!valid)
		return false;

	if ((ar & AR_TYPE_MASK) != AR_TYPE_RWDATA &&
	    (ar & AR_TYPE_MASK) != AR_TYPE_RWDATA_EXPDOWN)
		return false;

	return (ar & AR_P);
}

 
static volatile sig_atomic_t nerrs;

static void validate_signal_ss(int sig, ucontext_t *ctx)
{
#ifdef __x86_64__
	bool was_64bit = (cs_bitness(*csptr(ctx)) == 64);

	if (!(ctx->uc_flags & UC_SIGCONTEXT_SS)) {
		printf("[FAIL]\tUC_SIGCONTEXT_SS was not set\n");
		nerrs++;

		 
		return;
	}

	 
	if (!!(ctx->uc_flags & UC_STRICT_RESTORE_SS) != was_64bit) {
		printf("[FAIL]\tUC_STRICT_RESTORE_SS was wrong in signal %d\n",
		       sig);
		nerrs++;
	}

	if (is_valid_ss(*ssptr(ctx))) {
		 
		unsigned short hw_ss;
		asm ("mov %%ss, %0" : "=rm" (hw_ss));
		if (hw_ss != *ssptr(ctx)) {
			printf("[FAIL]\tHW SS didn't match saved SS\n");
			nerrs++;
		}
	}
#endif
}

 
static void sigusr1(int sig, siginfo_t *info, void *ctx_void)
{
	ucontext_t *ctx = (ucontext_t*)ctx_void;

	validate_signal_ss(sig, ctx);

	memcpy(&initial_regs, &ctx->uc_mcontext.gregs, sizeof(gregset_t));

	*csptr(ctx) = sig_cs;
	*ssptr(ctx) = sig_ss;

	ctx->uc_mcontext.gregs[REG_IP] =
		sig_cs == code16_sel ? 0 : (unsigned long)&int3;
	ctx->uc_mcontext.gregs[REG_SP] = (unsigned long)0x8badf00d5aadc0deULL;
	ctx->uc_mcontext.gregs[REG_CX] = 0;

#ifdef __i386__
	 
	ctx->uc_mcontext.gregs[REG_DS] = 0;
	ctx->uc_mcontext.gregs[REG_ES] = 0;
#endif

	memcpy(&requested_regs, &ctx->uc_mcontext.gregs, sizeof(gregset_t));
	requested_regs[REG_CX] = *ssptr(ctx);	 

	return;
}

 
static void sigtrap(int sig, siginfo_t *info, void *ctx_void)
{
	ucontext_t *ctx = (ucontext_t*)ctx_void;

	validate_signal_ss(sig, ctx);

	sig_err = ctx->uc_mcontext.gregs[REG_ERR];
	sig_trapno = ctx->uc_mcontext.gregs[REG_TRAPNO];

	unsigned short ss;
	asm ("mov %%ss,%0" : "=r" (ss));

	greg_t asm_ss = ctx->uc_mcontext.gregs[REG_CX];
	if (asm_ss != sig_ss && sig == SIGTRAP) {
		 
		printf("[FAIL]\tSIGTRAP: ss = %hx, frame ss = %hx, ax = %llx\n",
		       ss, *ssptr(ctx), (unsigned long long)asm_ss);
		nerrs++;
	}

	memcpy(&resulting_regs, &ctx->uc_mcontext.gregs, sizeof(gregset_t));
	memcpy(&ctx->uc_mcontext.gregs, &initial_regs, sizeof(gregset_t));

#ifdef __x86_64__
	if (sig_corrupt_final_ss) {
		if (ctx->uc_flags & UC_STRICT_RESTORE_SS) {
			printf("[FAIL]\tUC_STRICT_RESTORE_SS was set inappropriately\n");
			nerrs++;
		} else {
			 
			printf("\tCorrupting SS on return to 64-bit mode\n");
			*ssptr(ctx) = 0;
		}
	}
#endif

	sig_trapped = sig;
}

#ifdef __x86_64__
 
static void sigusr2(int sig, siginfo_t *info, void *ctx_void)
{
	ucontext_t *ctx = (ucontext_t*)ctx_void;

	if (!(ctx->uc_flags & UC_STRICT_RESTORE_SS)) {
		printf("[FAIL]\traise(2) didn't set UC_STRICT_RESTORE_SS\n");
		nerrs++;
		return;   
	}

	ctx->uc_flags &= ~UC_STRICT_RESTORE_SS;
	*ssptr(ctx) = 0;

	 
}

static int test_nonstrict_ss(void)
{
	clearhandler(SIGUSR1);
	clearhandler(SIGTRAP);
	clearhandler(SIGSEGV);
	clearhandler(SIGILL);
	sethandler(SIGUSR2, sigusr2, 0);

	nerrs = 0;

	printf("[RUN]\tClear UC_STRICT_RESTORE_SS and corrupt SS\n");
	raise(SIGUSR2);
	if (!nerrs)
		printf("[OK]\tIt worked\n");

	return nerrs;
}
#endif

 
int find_cs(int bitness)
{
	unsigned short my_cs;

	asm ("mov %%cs,%0" :  "=r" (my_cs));

	if (cs_bitness(my_cs) == bitness)
		return my_cs;
	if (cs_bitness(my_cs + (2 << 3)) == bitness)
		return my_cs + (2 << 3);
	if (my_cs > (2<<3) && cs_bitness(my_cs - (2 << 3)) == bitness)
	    return my_cs - (2 << 3);
	if (cs_bitness(code16_sel) == bitness)
		return code16_sel;

	printf("[WARN]\tCould not find %d-bit CS\n", bitness);
	return -1;
}

static int test_valid_sigreturn(int cs_bits, bool use_16bit_ss, int force_ss)
{
	int cs = find_cs(cs_bits);
	if (cs == -1) {
		printf("[SKIP]\tCode segment unavailable for %d-bit CS, %d-bit SS\n",
		       cs_bits, use_16bit_ss ? 16 : 32);
		return 0;
	}

	if (force_ss != -1) {
		sig_ss = force_ss;
	} else {
		if (use_16bit_ss) {
			if (!data16_sel) {
				printf("[SKIP]\tData segment unavailable for %d-bit CS, 16-bit SS\n",
				       cs_bits);
				return 0;
			}
			sig_ss = data16_sel;
		} else {
			asm volatile ("mov %%ss,%0" : "=r" (sig_ss));
		}
	}

	sig_cs = cs;

	printf("[RUN]\tValid sigreturn: %d-bit CS (%hx), %d-bit SS (%hx%s)\n",
	       cs_bits, sig_cs, use_16bit_ss ? 16 : 32, sig_ss,
	       (sig_ss & 4) ? "" : ", GDT");

	raise(SIGUSR1);

	nerrs = 0;

	 
	for (int i = 0; i < NGREG; i++) {
		greg_t req = requested_regs[i], res = resulting_regs[i];

		if (i == REG_TRAPNO || i == REG_IP)
			continue;	 

		if (i == REG_SP) {
			 

			if (res == req)
				continue;

			if (cs_bits != 64 && ((res ^ req) & 0xFFFFFFFF) == 0) {
				printf("[NOTE]\tSP: %llx -> %llx\n",
				       (unsigned long long)req,
				       (unsigned long long)res);
				continue;
			}

			printf("[FAIL]\tSP mismatch: requested 0x%llx; got 0x%llx\n",
			       (unsigned long long)requested_regs[i],
			       (unsigned long long)resulting_regs[i]);
			nerrs++;
			continue;
		}

		bool ignore_reg = false;
#if __i386__
		if (i == REG_UESP)
			ignore_reg = true;
#else
		if (i == REG_CSGSFS) {
			struct selectors *req_sels =
				(void *)&requested_regs[REG_CSGSFS];
			struct selectors *res_sels =
				(void *)&resulting_regs[REG_CSGSFS];
			if (req_sels->cs != res_sels->cs) {
				printf("[FAIL]\tCS mismatch: requested 0x%hx; got 0x%hx\n",
				       req_sels->cs, res_sels->cs);
				nerrs++;
			}

			if (req_sels->ss != res_sels->ss) {
				printf("[FAIL]\tSS mismatch: requested 0x%hx; got 0x%hx\n",
				       req_sels->ss, res_sels->ss);
				nerrs++;
			}

			continue;
		}
#endif

		 
		if (i == REG_CX && req != res) {
			printf("[FAIL]\tCX (saved SP) mismatch: requested 0x%llx; got 0x%llx\n",
			       (unsigned long long)req,
			       (unsigned long long)res);
			nerrs++;
			continue;
		}

		if (req != res && !ignore_reg) {
			printf("[FAIL]\tReg %d mismatch: requested 0x%llx; got 0x%llx\n",
			       i, (unsigned long long)req,
			       (unsigned long long)res);
			nerrs++;
		}
	}

	if (nerrs == 0)
		printf("[OK]\tall registers okay\n");

	return nerrs;
}

static int test_bad_iret(int cs_bits, unsigned short ss, int force_cs)
{
	int cs = force_cs == -1 ? find_cs(cs_bits) : force_cs;
	if (cs == -1)
		return 0;

	sig_cs = cs;
	sig_ss = ss;

	printf("[RUN]\t%d-bit CS (%hx), bogus SS (%hx)\n",
	       cs_bits, sig_cs, sig_ss);

	sig_trapped = 0;
	raise(SIGUSR1);
	if (sig_trapped) {
		char errdesc[32] = "";
		if (sig_err) {
			const char *src = (sig_err & 1) ? " EXT" : "";
			const char *table;
			if ((sig_err & 0x6) == 0x0)
				table = "GDT";
			else if ((sig_err & 0x6) == 0x4)
				table = "LDT";
			else if ((sig_err & 0x6) == 0x2)
				table = "IDT";
			else
				table = "???";

			sprintf(errdesc, "%s%s index %d, ",
				table, src, sig_err >> 3);
		}

		char trapname[32];
		if (sig_trapno == 13)
			strcpy(trapname, "GP");
		else if (sig_trapno == 11)
			strcpy(trapname, "NP");
		else if (sig_trapno == 12)
			strcpy(trapname, "SS");
		else if (sig_trapno == 32)
			strcpy(trapname, "IRET");   
		else
			sprintf(trapname, "%d", sig_trapno);

		printf("[OK]\tGot #%s(0x%lx) (i.e. %s%s)\n",
		       trapname, (unsigned long)sig_err,
		       errdesc, strsignal(sig_trapped));
		return 0;
	} else {
		 
		printf("[FAIL]\tDid not get SIGSEGV\n");
		return 1;
	}
}

int main()
{
	int total_nerrs = 0;
	unsigned short my_cs, my_ss;

	asm volatile ("mov %%cs,%0" : "=r" (my_cs));
	asm volatile ("mov %%ss,%0" : "=r" (my_ss));
	setup_ldt();

	stack_t stack = {
		 
		.ss_sp = malloc(sizeof(char) * SIGSTKSZ),
		.ss_size = SIGSTKSZ,
	};
	if (sigaltstack(&stack, NULL) != 0)
		err(1, "sigaltstack");

	sethandler(SIGUSR1, sigusr1, 0);
	sethandler(SIGTRAP, sigtrap, SA_ONSTACK);

	 
	total_nerrs += test_valid_sigreturn(64, false, -1);
	total_nerrs += test_valid_sigreturn(32, false, -1);
	total_nerrs += test_valid_sigreturn(16, false, -1);

	 
	total_nerrs += test_valid_sigreturn(64, true, -1);
	total_nerrs += test_valid_sigreturn(32, true, -1);
	total_nerrs += test_valid_sigreturn(16, true, -1);

	if (gdt_data16_idx) {
		 
		total_nerrs += test_valid_sigreturn(64, true,
						    GDT3(gdt_data16_idx));
		total_nerrs += test_valid_sigreturn(32, true,
						    GDT3(gdt_data16_idx));
		total_nerrs += test_valid_sigreturn(16, true,
						    GDT3(gdt_data16_idx));
	}

#ifdef __x86_64__
	 
	sig_corrupt_final_ss = 1;
	total_nerrs += test_valid_sigreturn(32, false, -1);
	total_nerrs += test_valid_sigreturn(32, true, -1);
	sig_corrupt_final_ss = 0;
#endif

	 
	clearhandler(SIGTRAP);
	sethandler(SIGSEGV, sigtrap, SA_ONSTACK);
	sethandler(SIGBUS, sigtrap, SA_ONSTACK);
	sethandler(SIGILL, sigtrap, SA_ONSTACK);   

	 
	test_bad_iret(64, ldt_nonexistent_sel, -1);
	test_bad_iret(32, ldt_nonexistent_sel, -1);
	test_bad_iret(16, ldt_nonexistent_sel, -1);

	 
	test_bad_iret(64, my_cs, -1);
	test_bad_iret(32, my_cs, -1);
	test_bad_iret(16, my_cs, -1);

	 
	test_bad_iret(32, my_ss, npcode32_sel);

	 
	test_bad_iret(32, npdata32_sel, -1);

	 
	if (gdt_npdata32_idx)
		test_bad_iret(32, GDT3(gdt_npdata32_idx), -1);

#ifdef __x86_64__
	total_nerrs += test_nonstrict_ss();
#endif

	free(stack.ss_sp);
	return total_nerrs ? 1 : 0;
}
