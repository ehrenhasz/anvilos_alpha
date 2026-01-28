#ifndef __HEAD_BOOKE_H__
#define __HEAD_BOOKE_H__
#include <asm/ptrace.h>	 
#include <asm/kvm_asm.h>
#include <asm/kvm_booke_hv_asm.h>
#include <asm/thread_info.h>	 
#ifdef __ASSEMBLY__
#define SET_IVOR(vector_number, vector_label)		\
		li	r26,vector_label@l; 		\
		mtspr	SPRN_IVOR##vector_number,r26;	\
		sync
#if (THREAD_SHIFT < 15)
#define ALLOC_STACK_FRAME(reg, val)			\
	addi reg,reg,val
#else
#define ALLOC_STACK_FRAME(reg, val)			\
	addis	reg,reg,val@ha;				\
	addi	reg,reg,val@l
#endif
#define THREAD_NORMSAVE(offset)	(THREAD_NORMSAVES + (offset * 4))
#ifdef CONFIG_PPC_E500
#define BOOKE_CLEAR_BTB(reg)									\
START_BTB_FLUSH_SECTION								\
	BTB_FLUSH(reg)									\
END_BTB_FLUSH_SECTION
#else
#define BOOKE_CLEAR_BTB(reg)
#endif
#define NORMAL_EXCEPTION_PROLOG(trapno, intno)						     \
	mtspr	SPRN_SPRG_WSCRATCH0, r10;	 	     \
	mfspr	r10, SPRN_SPRG_THREAD;					     \
	stw	r11, THREAD_NORMSAVE(0)(r10);				     \
	stw	r13, THREAD_NORMSAVE(2)(r10);				     \
	mfcr	r13;			 \
	mfspr	r11, SPRN_SRR1;		                                     \
	DO_KVM	BOOKE_INTERRUPT_##intno SPRN_SRR1;			     \
	andi.	r11, r11, MSR_PR;	 \
	LOAD_REG_IMMEDIATE(r11, MSR_KERNEL);				\
	mtmsr	r11;							\
	mr	r11, r1;						     \
	beq	1f;							     \
	BOOKE_CLEAR_BTB(r11)						\
	        \
	lwz	r11, TASK_STACK - THREAD(r10);				     \
	ALLOC_STACK_FRAME(r11, THREAD_SIZE);				     \
1 :	subi	r11, r11, INT_FRAME_SIZE;       \
	stw	r13, _CCR(r11);		 	     \
	stw	r12,GPR12(r11);						     \
	stw	r9,GPR9(r11);						     \
	mfspr	r13, SPRN_SPRG_RSCRATCH0;				     \
	stw	r13, GPR10(r11);					     \
	lwz	r12, THREAD_NORMSAVE(0)(r10);				     \
	stw	r12,GPR11(r11);						     \
	lwz	r13, THREAD_NORMSAVE(2)(r10);  		     \
	mflr	r10;							     \
	stw	r10,_LINK(r11);						     \
	mfspr	r12,SPRN_SRR0;						     \
	stw	r1, GPR1(r11);						     \
	mfspr	r9,SPRN_SRR1;						     \
	stw	r1, 0(r11);						     \
	mr	r1, r11;						     \
	rlwinm	r9,r9,0,14,12;		 \
	COMMON_EXCEPTION_PROLOG_END trapno
.macro COMMON_EXCEPTION_PROLOG_END trapno
	stw	r0,GPR0(r1)
	lis	r10, STACK_FRAME_REGS_MARKER@ha	 
	addi	r10, r10, STACK_FRAME_REGS_MARKER@l
	stw	r10, STACK_INT_FRAME_MARKER(r1)
	li	r10, \trapno
	stw	r10,_TRAP(r1)
	SAVE_GPRS(3, 8, r1)
	SAVE_NVGPRS(r1)
	stw	r2,GPR2(r1)
	stw	r12,_NIP(r1)
	stw	r9,_MSR(r1)
	mfctr	r10
	mfspr	r2,SPRN_SPRG_THREAD
	stw	r10,_CTR(r1)
	tovirt(r2, r2)
	mfspr	r10,SPRN_XER
	addi	r2, r2, -THREAD
	stw	r10,_XER(r1)
	addi	r3,r1,STACK_INT_FRAME_REGS
.endm
.macro prepare_transfer_to_handler
#ifdef CONFIG_PPC_E500
	andi.	r12,r9,MSR_PR
	bne	777f
	bl	prepare_transfer_to_handler
777:
#endif
.endm
.macro SYSCALL_ENTRY trapno intno srr1
	mfspr	r10, SPRN_SPRG_THREAD
#ifdef CONFIG_KVM_BOOKE_HV
BEGIN_FTR_SECTION
	mtspr	SPRN_SPRG_WSCRATCH0, r10
	stw	r11, THREAD_NORMSAVE(0)(r10)
	stw	r13, THREAD_NORMSAVE(2)(r10)
	mfcr	r13			 
	mfspr	r11, SPRN_SRR1
	mtocrf	0x80, r11	 
	bf	3, 1975f
	b	kvmppc_handler_\intno\()_\srr1
1975:
	mr	r12, r13
	lwz	r13, THREAD_NORMSAVE(2)(r10)
FTR_SECTION_ELSE
	mfcr	r12
ALT_FTR_SECTION_END_IFSET(CPU_FTR_EMB_HV)
#else
	mfcr	r12
#endif
	mfspr	r9, SPRN_SRR1
	BOOKE_CLEAR_BTB(r11)
	mr	r11, r1
	lwz	r1, TASK_STACK - THREAD(r10)
	rlwinm	r12,r12,0,4,2	 
	ALLOC_STACK_FRAME(r1, THREAD_SIZE - INT_FRAME_SIZE)
	stw	r12, _CCR(r1)
	mfspr	r12,SPRN_SRR0
	stw	r12,_NIP(r1)
	b	transfer_to_syscall	 
.endm
#define MC_STACK_BASE		mcheckirq_ctx
#define CRIT_STACK_BASE		critirq_ctx
#define DBG_STACK_BASE		dbgirq_ctx
#ifdef CONFIG_SMP
#define BOOKE_LOAD_EXC_LEVEL_STACK(level)		\
	mfspr	r8,SPRN_PIR;				\
	slwi	r8,r8,2;				\
	addis	r8,r8,level##_STACK_BASE@ha;		\
	lwz	r8,level##_STACK_BASE@l(r8);		\
	addi	r8,r8,THREAD_SIZE - INT_FRAME_SIZE;
#else
#define BOOKE_LOAD_EXC_LEVEL_STACK(level)		\
	lis	r8,level##_STACK_BASE@ha;		\
	lwz	r8,level##_STACK_BASE@l(r8);		\
	addi	r8,r8,THREAD_SIZE - INT_FRAME_SIZE;
#endif
#define EXC_LEVEL_EXCEPTION_PROLOG(exc_level, trapno, intno, exc_level_srr0, exc_level_srr1) \
	mtspr	SPRN_SPRG_WSCRATCH_##exc_level,r8;			     \
	BOOKE_LOAD_EXC_LEVEL_STACK(exc_level);  \
	stw	r9,GPR9(r8);		 \
	mfcr	r9;			 \
	stw	r10,GPR10(r8);						     \
	stw	r11,GPR11(r8);						     \
	stw	r9,_CCR(r8);		 \
	mfspr	r11,exc_level_srr1;	 \
	DO_KVM	BOOKE_INTERRUPT_##intno exc_level_srr1;		             \
	BOOKE_CLEAR_BTB(r10)						\
	andi.	r11,r11,MSR_PR;						     \
	LOAD_REG_IMMEDIATE(r11, MSR_KERNEL & ~(MSR_ME|MSR_DE|MSR_CE));	\
	mtmsr	r11;							\
	mfspr	r11,SPRN_SPRG_THREAD;	 \
	lwz	r11, TASK_STACK - THREAD(r11);  \
	addi	r11,r11,THREAD_SIZE - INT_FRAME_SIZE;	 \
	beq	1f;							     \
	 					     \
	stw	r9,_CCR(r11);		 \
	lwz	r10,GPR10(r8);		 \
	lwz	r9,GPR9(r8);						     \
	stw	r10,GPR10(r11);						     \
	lwz	r10,GPR11(r8);						     \
	stw	r9,GPR9(r11);						     \
	stw	r10,GPR11(r11);						     \
	b	2f;							     \
	 					     \
1:	mr	r11, r8;							     \
2:	mfspr	r8,SPRN_SPRG_RSCRATCH_##exc_level;			     \
	stw	r12,GPR12(r11);		 \
	mflr	r10;							     \
	stw	r10,_LINK(r11);						     \
	mfspr	r12,SPRN_DEAR;		 \
	stw	r12,_DEAR(r11);		 \
	mfspr	r9,SPRN_ESR;		 \
	stw	r9,_ESR(r11);		 \
	mfspr	r12,exc_level_srr0;					     \
	stw	r1,GPR1(r11);						     \
	mfspr	r9,exc_level_srr1;					     \
	stw	r1,0(r11);						     \
	mr	r1,r11;							     \
	rlwinm	r9,r9,0,14,12;		 \
	COMMON_EXCEPTION_PROLOG_END trapno
#define SAVE_xSRR(xSRR)			\
	mfspr	r0,SPRN_##xSRR##0;	\
	stw	r0,_##xSRR##0(r1);	\
	mfspr	r0,SPRN_##xSRR##1;	\
	stw	r0,_##xSRR##1(r1)
.macro SAVE_MMU_REGS
#ifdef CONFIG_PPC_E500
	mfspr	r0,SPRN_MAS0
	stw	r0,MAS0(r1)
	mfspr	r0,SPRN_MAS1
	stw	r0,MAS1(r1)
	mfspr	r0,SPRN_MAS2
	stw	r0,MAS2(r1)
	mfspr	r0,SPRN_MAS3
	stw	r0,MAS3(r1)
	mfspr	r0,SPRN_MAS6
	stw	r0,MAS6(r1)
#ifdef CONFIG_PHYS_64BIT
	mfspr	r0,SPRN_MAS7
	stw	r0,MAS7(r1)
#endif  
#endif  
#ifdef CONFIG_44x
	mfspr	r0,SPRN_MMUCR
	stw	r0,MMUCR(r1)
#endif
.endm
#define CRITICAL_EXCEPTION_PROLOG(trapno, intno) \
		EXC_LEVEL_EXCEPTION_PROLOG(CRIT, trapno+2, intno, SPRN_CSRR0, SPRN_CSRR1)
#define DEBUG_EXCEPTION_PROLOG(trapno) \
		EXC_LEVEL_EXCEPTION_PROLOG(DBG, trapno+8, DEBUG, SPRN_DSRR0, SPRN_DSRR1)
#define MCHECK_EXCEPTION_PROLOG(trapno) \
		EXC_LEVEL_EXCEPTION_PROLOG(MC, trapno+4, MACHINE_CHECK, \
			SPRN_MCSRR0, SPRN_MCSRR1)
#define GUEST_DOORBELL_EXCEPTION \
	START_EXCEPTION(GuestDoorbell);					     \
	mtspr	SPRN_SPRG_WSCRATCH0, r10;	 	     \
	mfspr	r10, SPRN_SPRG_THREAD;					     \
	stw	r11, THREAD_NORMSAVE(0)(r10);				     \
	mfspr	r11, SPRN_SRR1;		                                     \
	stw	r13, THREAD_NORMSAVE(2)(r10);				     \
	mfcr	r13;			 \
	DO_KVM	BOOKE_INTERRUPT_GUEST_DBELL SPRN_GSRR1;			     \
	trap
#define	START_EXCEPTION(label)						     \
        .align 5;              						     \
label:
#define EXCEPTION(n, intno, label, hdlr)			\
	START_EXCEPTION(label);					\
	NORMAL_EXCEPTION_PROLOG(n, intno);			\
	prepare_transfer_to_handler;				\
	bl	hdlr;						\
	b	interrupt_return
#define CRITICAL_EXCEPTION(n, intno, label, hdlr)			\
	START_EXCEPTION(label);						\
	CRITICAL_EXCEPTION_PROLOG(n, intno);				\
	SAVE_MMU_REGS;							\
	SAVE_xSRR(SRR);							\
	prepare_transfer_to_handler;					\
	bl	hdlr;							\
	b	ret_from_crit_exc
#define MCHECK_EXCEPTION(n, label, hdlr)			\
	START_EXCEPTION(label);					\
	MCHECK_EXCEPTION_PROLOG(n);				\
	mfspr	r5,SPRN_ESR;					\
	stw	r5,_ESR(r11);					\
	SAVE_xSRR(DSRR);					\
	SAVE_xSRR(CSRR);					\
	SAVE_MMU_REGS;						\
	SAVE_xSRR(SRR);						\
	prepare_transfer_to_handler;				\
	bl	hdlr;						\
	b	ret_from_mcheck_exc
#define DEBUG_DEBUG_EXCEPTION						      \
	START_EXCEPTION(DebugDebug);					      \
	DEBUG_EXCEPTION_PROLOG(2000);						      \
									      \
	 								      \
	mfspr	r10,SPRN_DBSR;		   \
	andis.	r10,r10,(DBSR_IC|DBSR_BT)@h;				      \
	beq+	2f;							      \
									      \
	lis	r10,interrupt_base@h;	    \
	ori	r10,r10,interrupt_base@l;				      \
	cmplw	r12,r10;						      \
	blt+	2f;			     \
									      \
	lis	r10,interrupt_end@h;					      \
	ori	r10,r10,interrupt_end@l;				      \
	cmplw	r12,r10;						      \
	bgt+	2f;			     \
									      \
	      \
1:	rlwinm	r9,r9,0,~MSR_DE;	      \
	lis	r10,(DBSR_IC|DBSR_BT)@h;	       \
	mtspr	SPRN_DBSR,r10;						      \
	 					      \
	lwz	r10,_CCR(r11);						      \
	lwz	r0,GPR0(r11);						      \
	lwz	r1,GPR1(r11);						      \
	mtcrf	0x80,r10;						      \
	mtspr	SPRN_DSRR0,r12;						      \
	mtspr	SPRN_DSRR1,r9;						      \
	lwz	r9,GPR9(r11);						      \
	lwz	r12,GPR12(r11);						      \
	mtspr	SPRN_SPRG_WSCRATCH_DBG,r8;				      \
	BOOKE_LOAD_EXC_LEVEL_STACK(DBG);   \
	lwz	r10,GPR10(r8);						      \
	lwz	r11,GPR11(r8);						      \
	mfspr	r8,SPRN_SPRG_RSCRATCH_DBG;				      \
									      \
	PPC_RFDI;							      \
	b	.;							      \
									      \
	 		      \
2:	mfspr	r4,SPRN_DBSR;						      \
	stw	r4,_ESR(r11);		 \
	SAVE_xSRR(CSRR);						      \
	SAVE_MMU_REGS;							      \
	SAVE_xSRR(SRR);							      \
	prepare_transfer_to_handler;				      \
	bl	DebugException;						      \
	b	ret_from_debug_exc
#define DEBUG_CRIT_EXCEPTION						      \
	START_EXCEPTION(DebugCrit);					      \
	CRITICAL_EXCEPTION_PROLOG(2000,DEBUG);				      \
									      \
	 								      \
	mfspr	r10,SPRN_DBSR;		   \
	andis.	r10,r10,(DBSR_IC|DBSR_BT)@h;				      \
	beq+	2f;							      \
									      \
	lis	r10,interrupt_base@h;	    \
	ori	r10,r10,interrupt_base@l;				      \
	cmplw	r12,r10;						      \
	blt+	2f;			     \
									      \
	lis	r10,interrupt_end@h;					      \
	ori	r10,r10,interrupt_end@l;				      \
	cmplw	r12,r10;						      \
	bgt+	2f;			     \
									      \
	      \
1:	rlwinm	r9,r9,0,~MSR_DE;	      \
	lis	r10,(DBSR_IC|DBSR_BT)@h;	       \
	mtspr	SPRN_DBSR,r10;						      \
	 					      \
	lwz	r10,_CCR(r11);						      \
	lwz	r0,GPR0(r11);						      \
	lwz	r1,GPR1(r11);						      \
	mtcrf	0x80,r10;						      \
	mtspr	SPRN_CSRR0,r12;						      \
	mtspr	SPRN_CSRR1,r9;						      \
	lwz	r9,GPR9(r11);						      \
	lwz	r12,GPR12(r11);						      \
	mtspr	SPRN_SPRG_WSCRATCH_CRIT,r8;				      \
	BOOKE_LOAD_EXC_LEVEL_STACK(CRIT);    \
	lwz	r10,GPR10(r8);						      \
	lwz	r11,GPR11(r8);						      \
	mfspr	r8,SPRN_SPRG_RSCRATCH_CRIT;				      \
									      \
	rfci;								      \
	b	.;							      \
									      \
	 	      \
2:	mfspr	r4,SPRN_DBSR;						      \
	stw	r4,_ESR(r11);		 \
	SAVE_MMU_REGS;							      \
	SAVE_xSRR(SRR);							      \
	prepare_transfer_to_handler;					      \
	bl	DebugException;						      \
	b	ret_from_crit_exc
#define DATA_STORAGE_EXCEPTION						      \
	START_EXCEPTION(DataStorage)					      \
	NORMAL_EXCEPTION_PROLOG(0x300, DATA_STORAGE);		      \
	mfspr	r5,SPRN_ESR;		 	      \
	stw	r5,_ESR(r11);						      \
	mfspr	r4,SPRN_DEAR;		 		      \
	stw	r4, _DEAR(r11);						      \
	prepare_transfer_to_handler;					      \
	bl	do_page_fault;						      \
	b	interrupt_return
#define INSTRUCTION_STORAGE_EXCEPTION					      \
	START_EXCEPTION(InstructionStorage)				      \
	NORMAL_EXCEPTION_PROLOG(0x400, INST_STORAGE);			      \
	li	r5,0;			     \
	stw	r5,_ESR(r11);						      \
	stw	r12, _DEAR(r11);	     \
	prepare_transfer_to_handler;					      \
	bl	do_page_fault;						      \
	b	interrupt_return
#define ALIGNMENT_EXCEPTION						      \
	START_EXCEPTION(Alignment)					      \
	NORMAL_EXCEPTION_PROLOG(0x600, ALIGNMENT);		      \
	mfspr   r4,SPRN_DEAR;            	      \
	stw     r4,_DEAR(r11);						      \
	prepare_transfer_to_handler;					      \
	bl	alignment_exception;					      \
	REST_NVGPRS(r1);						      \
	b	interrupt_return
#define PROGRAM_EXCEPTION						      \
	START_EXCEPTION(Program)					      \
	NORMAL_EXCEPTION_PROLOG(0x700, PROGRAM);		      \
	mfspr	r4,SPRN_ESR;		 	      \
	stw	r4,_ESR(r11);						      \
	prepare_transfer_to_handler;					      \
	bl	program_check_exception;				      \
	REST_NVGPRS(r1);						      \
	b	interrupt_return
#define DECREMENTER_EXCEPTION						      \
	START_EXCEPTION(Decrementer)					      \
	NORMAL_EXCEPTION_PROLOG(0x900, DECREMENTER);		      \
	lis     r0,TSR_DIS@h;                \
	mtspr   SPRN_TSR,r0;		 	      \
	prepare_transfer_to_handler;					      \
	bl	timer_interrupt;					      \
	b	interrupt_return
#define FP_UNAVAILABLE_EXCEPTION					      \
	START_EXCEPTION(FloatingPointUnavailable)			      \
	NORMAL_EXCEPTION_PROLOG(0x800, FP_UNAVAIL);		      \
	beq	1f;							      \
	bl	load_up_fpu;		    \
	b	fast_exception_return;					      \
1:	prepare_transfer_to_handler;					      \
	bl	kernel_fp_unavailable_exception;			      \
	b	interrupt_return
#endif  
#endif  
