#ifndef _ASM_S390_FPU_API_H
#define _ASM_S390_FPU_API_H
#include <linux/preempt.h>
#include <asm/asm-extable.h>
void save_fpu_regs(void);
void load_fpu_regs(void);
void __load_fpu_regs(void);
static inline int test_fp_ctl(u32 fpc)
{
	u32 orig_fpc;
	int rc;
	asm volatile(
		"	efpc    %1\n"
		"	sfpc	%2\n"
		"0:	sfpc	%1\n"
		"	la	%0,0\n"
		"1:\n"
		EX_TABLE(0b,1b)
		: "=d" (rc), "=&d" (orig_fpc)
		: "d" (fpc), "0" (-EINVAL));
	return rc;
}
#define KERNEL_FPC		1
#define KERNEL_VXR_V0V7		2
#define KERNEL_VXR_V8V15	4
#define KERNEL_VXR_V16V23	8
#define KERNEL_VXR_V24V31	16
#define KERNEL_VXR_LOW		(KERNEL_VXR_V0V7|KERNEL_VXR_V8V15)
#define KERNEL_VXR_MID		(KERNEL_VXR_V8V15|KERNEL_VXR_V16V23)
#define KERNEL_VXR_HIGH		(KERNEL_VXR_V16V23|KERNEL_VXR_V24V31)
#define KERNEL_VXR		(KERNEL_VXR_LOW|KERNEL_VXR_HIGH)
#define KERNEL_FPR		(KERNEL_FPC|KERNEL_VXR_LOW)
struct kernel_fpu;
void __kernel_fpu_begin(struct kernel_fpu *state, u32 flags);
void __kernel_fpu_end(struct kernel_fpu *state, u32 flags);
static inline void kernel_fpu_begin(struct kernel_fpu *state, u32 flags)
{
	preempt_disable();
	state->mask = S390_lowcore.fpu_flags;
	if (!test_cpu_flag(CIF_FPU))
		save_fpu_regs();
	else if (state->mask & flags)
		__kernel_fpu_begin(state, flags);
	S390_lowcore.fpu_flags |= flags;
}
static inline void kernel_fpu_end(struct kernel_fpu *state, u32 flags)
{
	S390_lowcore.fpu_flags = state->mask;
	if (state->mask & flags)
		__kernel_fpu_end(state, flags);
	preempt_enable();
}
#endif  
