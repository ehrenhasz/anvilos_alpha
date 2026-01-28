#ifndef _ASM_NIOS2_PTRACE_H
#define _ASM_NIOS2_PTRACE_H
#include <uapi/asm/ptrace.h>
#ifndef __ASSEMBLY__
struct pt_regs {
	unsigned long  r8;	 
	unsigned long  r9;
	unsigned long  r10;
	unsigned long  r11;
	unsigned long  r12;
	unsigned long  r13;
	unsigned long  r14;
	unsigned long  r15;
	unsigned long  r1;	 
	unsigned long  r2;	 
	unsigned long  r3;	 
	unsigned long  r4;	 
	unsigned long  r5;
	unsigned long  r6;
	unsigned long  r7;
	unsigned long  orig_r2;	 
	unsigned long  ra;	 
	unsigned long  fp;	 
	unsigned long  sp;	 
	unsigned long  gp;	 
	unsigned long  estatus;
	unsigned long  ea;	 
	unsigned long  orig_r7;
};
struct switch_stack {
	unsigned long  r16;	 
	unsigned long  r17;
	unsigned long  r18;
	unsigned long  r19;
	unsigned long  r20;
	unsigned long  r21;
	unsigned long  r22;
	unsigned long  r23;
	unsigned long  fp;
	unsigned long  gp;
	unsigned long  ra;
};
#define user_mode(regs)	(((regs)->estatus & ESTATUS_EU))
#define instruction_pointer(regs)	((regs)->ra)
#define profile_pc(regs)		instruction_pointer(regs)
#define user_stack_pointer(regs)	((regs)->sp)
extern void show_regs(struct pt_regs *);
#define current_pt_regs() \
	((struct pt_regs *)((unsigned long)current_thread_info() + THREAD_SIZE)\
		- 1)
#define force_successful_syscall_return() (current_pt_regs()->orig_r2 = -1)
int do_syscall_trace_enter(void);
void do_syscall_trace_exit(void);
#endif  
#endif  
