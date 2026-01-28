#ifndef __ASM_ARC_ENTRY_H
#define __ASM_ARC_ENTRY_H
#include <asm/unistd.h>		 
#include <asm/arcregs.h>
#include <asm/ptrace.h>
#include <asm/processor.h>	 
#include <asm/mmu.h>
#ifdef __ASSEMBLY__
#ifdef CONFIG_ISA_ARCOMPACT
#include <asm/entry-compact.h>	 
#else
#include <asm/entry-arcv2.h>
#endif
.macro PUSH reg
	st.a	\reg, [sp, -4]
.endm
.macro PUSHAX aux
	lr	r9, [\aux]
	PUSH	r9
.endm
.macro POP reg
	ld.ab	\reg, [sp, 4]
.endm
.macro POPAX aux
	POP	r9
	sr	r9, [\aux]
.endm
.macro  SAVE_R0_TO_R12
	PUSH	r0
	PUSH	r1
	PUSH	r2
	PUSH	r3
	PUSH	r4
	PUSH	r5
	PUSH	r6
	PUSH	r7
	PUSH	r8
	PUSH	r9
	PUSH	r10
	PUSH	r11
	PUSH	r12
.endm
.macro RESTORE_R12_TO_R0
	POP	r12
	POP	r11
	POP	r10
	POP	r9
	POP	r8
	POP	r7
	POP	r6
	POP	r5
	POP	r4
	POP	r3
	POP	r2
	POP	r1
	POP	r0
.endm
.macro SAVE_R13_TO_R25
	PUSH	r13
	PUSH	r14
	PUSH	r15
	PUSH	r16
	PUSH	r17
	PUSH	r18
	PUSH	r19
	PUSH	r20
	PUSH	r21
	PUSH	r22
	PUSH	r23
	PUSH	r24
	PUSH	r25
.endm
.macro RESTORE_R25_TO_R13
	POP	r25
	POP	r24
	POP	r23
	POP	r22
	POP	r21
	POP	r20
	POP	r19
	POP	r18
	POP	r17
	POP	r16
	POP	r15
	POP	r14
	POP	r13
.endm
.macro SAVE_CALLEE_SAVED_USER
	SAVE_R13_TO_R25
.endm
.macro RESTORE_CALLEE_SAVED_USER
	RESTORE_R25_TO_R13
.endm
.macro SAVE_CALLEE_SAVED_KERNEL
	SAVE_R13_TO_R25
.endm
.macro RESTORE_CALLEE_SAVED_KERNEL
	RESTORE_R25_TO_R13
.endm
.macro DISCARD_CALLEE_SAVED_USER
	add     sp, sp, SZ_CALLEE_REGS
.endm
.macro GET_TSK_STACK_BASE tsk, out
	ld  \out, [\tsk, TASK_THREAD_INFO]
	add2 \out, \out, (THREAD_SIZE)/4
.endm
.macro GET_CURR_THR_INFO_FLAGS  reg
	GET_CURR_THR_INFO_FROM_SP  \reg
	ld  \reg, [\reg, THREAD_INFO_FLAGS]
.endm
#ifdef CONFIG_SMP
.macro  GET_CURR_TASK_ON_CPU   reg
	GET_CPU_ID  \reg
	ld.as  \reg, [@_current_task, \reg]
.endm
.macro  SET_CURR_TASK_ON_CPU    tsk, tmp
	GET_CPU_ID  \tmp
	add2 \tmp, @_current_task, \tmp
	st   \tsk, [\tmp]
#ifdef CONFIG_ARC_CURR_IN_REG
	mov gp, \tsk
#endif
.endm
#else    
.macro  GET_CURR_TASK_ON_CPU    reg
	ld  \reg, [@_current_task]
.endm
.macro  SET_CURR_TASK_ON_CPU    tsk, tmp
	st  \tsk, [@_current_task]
#ifdef CONFIG_ARC_CURR_IN_REG
	mov gp, \tsk
#endif
.endm
#endif  
#ifdef CONFIG_ARC_CURR_IN_REG
.macro GET_CURR_TASK_FIELD_PTR  off,  reg
	add \reg, gp, \off
.endm
#else
.macro GET_CURR_TASK_FIELD_PTR  off,  reg
	GET_CURR_TASK_ON_CPU  \reg
	add \reg, \reg, \off
.endm
#endif	 
#else	 
extern void do_signal(struct pt_regs *);
extern void do_notify_resume(struct pt_regs *);
extern int do_privilege_fault(unsigned long, struct pt_regs *);
extern int do_extension_fault(unsigned long, struct pt_regs *);
extern int insterror_is_error(unsigned long, struct pt_regs *);
extern int do_memory_error(unsigned long, struct pt_regs *);
extern int trap_is_brkpt(unsigned long, struct pt_regs *);
extern int do_misaligned_error(unsigned long, struct pt_regs *);
extern int do_trap5_error(unsigned long, struct pt_regs *);
extern int do_misaligned_access(unsigned long, struct pt_regs *, struct callee_regs *);
extern void do_machine_check_fault(unsigned long, struct pt_regs *);
extern void do_non_swi_trap(unsigned long, struct pt_regs *);
extern void do_insterror_or_kprobe(unsigned long, struct pt_regs *);
extern void do_page_fault(unsigned long, struct pt_regs *);
#endif
#endif   
