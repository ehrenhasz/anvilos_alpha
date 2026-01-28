#ifndef _UAPI__ASM_ARC_PTRACE_H
#define _UAPI__ASM_ARC_PTRACE_H
#define PTRACE_GET_THREAD_AREA	25
#ifndef __ASSEMBLY__
struct user_regs_struct {
	unsigned long pad;
	struct {
		unsigned long bta, lp_start, lp_end, lp_count;
		unsigned long status32, ret, blink, fp, gp;
		unsigned long r12, r11, r10, r9, r8, r7, r6, r5, r4, r3, r2, r1, r0;
		unsigned long sp;
	} scratch;
	unsigned long pad2;
	struct {
		unsigned long r25, r24, r23, r22, r21, r20;
		unsigned long r19, r18, r17, r16, r15, r14, r13;
	} callee;
	unsigned long efa;	 
	unsigned long stop_pc;	 
};
struct user_regs_arcv2 {
	unsigned long r30, r58, r59;
};
#endif  
#endif  
