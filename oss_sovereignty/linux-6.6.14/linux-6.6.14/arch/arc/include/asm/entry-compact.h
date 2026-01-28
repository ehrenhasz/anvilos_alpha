#ifndef __ASM_ARC_ENTRY_COMPACT_H
#define __ASM_ARC_ENTRY_COMPACT_H
#include <asm/asm-offsets.h>
#include <asm/irqflags-compact.h>
#include <asm/thread_info.h>	 
.macro SWITCH_TO_KERNEL_STK
	bbit1   r9, STATUS_U_BIT, 88f
#ifdef CONFIG_ARC_COMPACT_IRQ_LEVELS
	brlo sp, VMALLOC_START, 88f
#endif
	b.d	66f
	st	sp, [sp, PT_sp - SZ_PT_REGS]
88:  
	GET_CURR_TASK_ON_CPU   r9
	GET_TSK_STACK_BASE  r9, r9
	st	sp, [r9, PT_sp - SZ_PT_REGS]
	mov	sp, r9
66:
.endm
.macro FAKE_RET_FROM_EXCPN
	lr	r9, [status32]
	bclr	r9, r9, STATUS_AE_BIT
	or	r9, r9, (STATUS_E1_MASK|STATUS_E2_MASK)
	sr	r9, [erstatus]
	mov	r9, 55f
	sr	r9, [eret]
	rtie
55:
.endm
.macro PROLOG_FREEUP_REG	reg, mem
	st  \reg, [\mem]
.endm
.macro PROLOG_RESTORE_REG	reg, mem
	ld  \reg, [\mem]
.endm
.macro EXCEPTION_PROLOGUE_KEEP_AE
	PROLOG_FREEUP_REG r9, @ex_saved_reg1
	lr  r9, [erstatus]
	SWITCH_TO_KERNEL_STK
	st.a	r0, [sp, -8]     
	sub	sp, sp, 4	 
	PROLOG_RESTORE_REG  r9, @ex_saved_reg1
	SAVE_R0_TO_R12
	PUSH	gp
	PUSH	fp
	PUSH	blink
	PUSHAX	eret
	PUSHAX	erstatus
	PUSH	lp_count
	PUSHAX	lp_end
	PUSHAX	lp_start
	PUSHAX	erbta
	lr	r10, [ecr]
	st      r10, [sp, PT_event]
#ifdef CONFIG_ARC_CURR_IN_REG
	GET_CURR_TASK_ON_CPU   gp
#endif
	; OUTPUT: r10 has ECR expected by EV_Trap
.endm
.macro EXCEPTION_PROLOGUE
	EXCEPTION_PROLOGUE_KEEP_AE	; return ECR in r10
	lr  r0, [efa]
	mov r1, sp
	FAKE_RET_FROM_EXCPN		; clobbers r9
.endm
.macro EXCEPTION_EPILOGUE
	POPAX	erbta
	POPAX	lp_start
	POPAX	lp_end
	POP	r9
	mov	lp_count, r9	;LD to lp_count is not allowed
	POPAX	erstatus
	POPAX	eret
	POP	blink
	POP	fp
	POP	gp
	RESTORE_R12_TO_R0
	ld  sp, [sp]  
.endm
#define event_IRQ1		0x0031abcd
#define event_IRQ2		0x0032abcd
.macro INTERRUPT_PROLOGUE  LVL
	PROLOG_FREEUP_REG r9, @int\LVL\()_saved_reg
	lr  r9, [status32_l\LVL\()]
	SWITCH_TO_KERNEL_STK
	PUSH	0x003\LVL\()abcd     
	sub	sp, sp, 8	     
	PROLOG_RESTORE_REG  r9, @int\LVL\()_saved_reg
	SAVE_R0_TO_R12
	PUSH	gp
	PUSH	fp
	PUSH	blink
	PUSH	ilink\LVL\()
	PUSHAX	status32_l\LVL\()
	PUSH	lp_count
	PUSHAX	lp_end
	PUSHAX	lp_start
	PUSHAX	bta_l\LVL\()
#ifdef CONFIG_ARC_CURR_IN_REG
	GET_CURR_TASK_ON_CPU   gp
#endif
.endm
.macro INTERRUPT_EPILOGUE  LVL
	POPAX	bta_l\LVL\()
	POPAX	lp_start
	POPAX	lp_end
	POP	r9
	mov	lp_count, r9	;LD to lp_count is not allowed
	POPAX	status32_l\LVL\()
	POP	ilink\LVL\()
	POP	blink
	POP	fp
	POP	gp
	RESTORE_R12_TO_R0
	ld  sp, [sp]  
.endm
.macro GET_CURR_THR_INFO_FROM_SP  reg
	bic \reg, sp, (THREAD_SIZE - 1)
.endm
.macro  GET_CPU_ID  reg
	lr  \reg, [identity]
	lsr \reg, \reg, 8
	bmsk \reg, \reg, 7
.endm
#endif   
