#ifndef _UAPI_ASM_PTRACE_H
#define _UAPI_ASM_PTRACE_H
#include <linux/types.h>
#ifndef __KERNEL__
#include <stdint.h>
#endif
#define GPR_BASE	0
#define GPR_NUM		32
#define GPR_END		(GPR_BASE + GPR_NUM - 1)
#define ARG0		(GPR_END + 1)
#define PC		(GPR_END + 2)
#define BADVADDR	(GPR_END + 3)
#define NUM_FPU_REGS	32
struct user_pt_regs {
	unsigned long regs[32];
	unsigned long orig_a0;
	unsigned long csr_era;
	unsigned long csr_badv;
	unsigned long reserved[10];
} __attribute__((aligned(8)));
struct user_fp_state {
	uint64_t fpr[32];
	uint64_t fcc;
	uint32_t fcsr;
};
struct user_lsx_state {
	uint64_t vregs[32*2];
};
struct user_lasx_state {
	uint64_t vregs[32*4];
};
struct user_lbt_state {
	uint64_t scr[4];
	uint32_t eflags;
	uint32_t ftop;
};
struct user_watch_state {
	uint64_t dbg_info;
	struct {
		uint64_t    addr;
		uint64_t    mask;
		uint32_t    ctrl;
		uint32_t    pad;
	} dbg_regs[8];
};
#define PTRACE_SYSEMU			0x1f
#define PTRACE_SYSEMU_SINGLESTEP	0x20
#endif  
