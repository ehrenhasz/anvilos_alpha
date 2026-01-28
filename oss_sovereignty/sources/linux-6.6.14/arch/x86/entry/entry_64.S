

#include <linux/linkage.h>
#include <asm/segment.h>
#include <asm/cache.h>
#include <asm/errno.h>
#include <asm/asm-offsets.h>
#include <asm/msr.h>
#include <asm/unistd.h>
#include <asm/thread_info.h>
#include <asm/hw_irq.h>
#include <asm/page_types.h>
#include <asm/irqflags.h>
#include <asm/paravirt.h>
#include <asm/percpu.h>
#include <asm/asm.h>
#include <asm/smap.h>
#include <asm/pgtable_types.h>
#include <asm/export.h>
#include <asm/frame.h>
#include <asm/trapnr.h>
#include <asm/nospec-branch.h>
#include <asm/fsgsbase.h>
#include <linux/err.h>

#include "calling.h"

.code64
.section .entry.text, "ax"



SYM_CODE_START(entry_SYSCALL_64)
	UNWIND_HINT_ENTRY
	ENDBR

	swapgs
	
	movq	%rsp, PER_CPU_VAR(cpu_tss_rw + TSS_sp2)
	SWITCH_TO_KERNEL_CR3 scratch_reg=%rsp
	movq	PER_CPU_VAR(pcpu_hot + X86_top_of_stack), %rsp

SYM_INNER_LABEL(entry_SYSCALL_64_safe_stack, SYM_L_GLOBAL)
	ANNOTATE_NOENDBR

	
	pushq	$__USER_DS				
	pushq	PER_CPU_VAR(cpu_tss_rw + TSS_sp2)	
	pushq	%r11					
	pushq	$__USER_CS				
	pushq	%rcx					
SYM_INNER_LABEL(entry_SYSCALL_64_after_hwframe, SYM_L_GLOBAL)
	pushq	%rax					

	PUSH_AND_CLEAR_REGS rax=$-ENOSYS

	
	movq	%rsp, %rdi
	
	movslq	%eax, %rsi

	
	IBRS_ENTER
	UNTRAIN_RET

	call	do_syscall_64		

	

	ALTERNATIVE "", "jmp	swapgs_restore_regs_and_return_to_usermode", \
		X86_FEATURE_XENPV

	movq	RCX(%rsp), %rcx
	movq	RIP(%rsp), %r11

	cmpq	%rcx, %r11	
	jne	swapgs_restore_regs_and_return_to_usermode

	
#ifdef CONFIG_X86_5LEVEL
	ALTERNATIVE "shl $(64 - 48), %rcx; sar $(64 - 48), %rcx", \
		"shl $(64 - 57), %rcx; sar $(64 - 57), %rcx", X86_FEATURE_LA57
#else
	shl	$(64 - (__VIRTUAL_MASK_SHIFT+1)), %rcx
	sar	$(64 - (__VIRTUAL_MASK_SHIFT+1)), %rcx
#endif

	
	cmpq	%rcx, %r11
	jne	swapgs_restore_regs_and_return_to_usermode

	cmpq	$__USER_CS, CS(%rsp)		
	jne	swapgs_restore_regs_and_return_to_usermode

	movq	R11(%rsp), %r11
	cmpq	%r11, EFLAGS(%rsp)		
	jne	swapgs_restore_regs_and_return_to_usermode

	
	testq	$(X86_EFLAGS_RF|X86_EFLAGS_TF), %r11
	jnz	swapgs_restore_regs_and_return_to_usermode

	

	cmpq	$__USER_DS, SS(%rsp)		
	jne	swapgs_restore_regs_and_return_to_usermode

	
syscall_return_via_sysret:
	IBRS_EXIT
	POP_REGS pop_rdi=0

	
	movq	%rsp, %rdi
	movq	PER_CPU_VAR(cpu_tss_rw + TSS_sp0), %rsp
	UNWIND_HINT_END_OF_STACK

	pushq	RSP-RDI(%rdi)	
	pushq	(%rdi)		

	
	STACKLEAK_ERASE_NOCLOBBER

	SWITCH_TO_USER_CR3_STACK scratch_reg=%rdi

	popq	%rdi
	popq	%rsp
SYM_INNER_LABEL(entry_SYSRETQ_unsafe_stack, SYM_L_GLOBAL)
	ANNOTATE_NOENDBR
	swapgs
	sysretq
SYM_INNER_LABEL(entry_SYSRETQ_end, SYM_L_GLOBAL)
	ANNOTATE_NOENDBR
	int3
SYM_CODE_END(entry_SYSCALL_64)


.pushsection .text, "ax"
SYM_FUNC_START(__switch_to_asm)
	
	pushq	%rbp
	pushq	%rbx
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	
	movq	%rsp, TASK_threadsp(%rdi)
	movq	TASK_threadsp(%rsi), %rsp

#ifdef CONFIG_STACKPROTECTOR
	movq	TASK_stack_canary(%rsi), %rbx
	movq	%rbx, PER_CPU_VAR(fixed_percpu_data) + FIXED_stack_canary
#endif

	
	FILL_RETURN_BUFFER %r12, RSB_CLEAR_LOOPS, X86_FEATURE_RSB_CTXSW

	
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbx
	popq	%rbp

	jmp	__switch_to
SYM_FUNC_END(__switch_to_asm)
.popsection


.pushsection .text, "ax"
SYM_CODE_START(ret_from_fork_asm)
	
	UNWIND_HINT_END_OF_STACK
	ANNOTATE_NOENDBR 
	CALL_DEPTH_ACCOUNT

	movq	%rax, %rdi		
	movq	%rsp, %rsi		
	movq	%rbx, %rdx		
	movq	%r12, %rcx		
	call	ret_from_fork

	
	UNWIND_HINT_REGS
	jmp	swapgs_restore_regs_and_return_to_usermode
SYM_CODE_END(ret_from_fork_asm)
.popsection

.macro DEBUG_ENTRY_ASSERT_IRQS_OFF
#ifdef CONFIG_DEBUG_ENTRY
	pushq %rax
	SAVE_FLAGS
	testl $X86_EFLAGS_IF, %eax
	jz .Lokay_\@
	ud2
.Lokay_\@:
	popq %rax
#endif
.endm

SYM_CODE_START(xen_error_entry)
	ANNOTATE_NOENDBR
	UNWIND_HINT_FUNC
	PUSH_AND_CLEAR_REGS save_ret=1
	ENCODE_FRAME_POINTER 8
	UNTRAIN_RET_FROM_CALL
	RET
SYM_CODE_END(xen_error_entry)


.macro idtentry_body cfunc has_error_code:req

	
	ALTERNATIVE "call error_entry; movq %rax, %rsp", \
		    "call xen_error_entry", X86_FEATURE_XENPV

	ENCODE_FRAME_POINTER
	UNWIND_HINT_REGS

	movq	%rsp, %rdi			

	.if \has_error_code == 1
		movq	ORIG_RAX(%rsp), %rsi	
		movq	$-1, ORIG_RAX(%rsp)	
	.endif

	call	\cfunc

	
	REACHABLE

	jmp	error_return
.endm


.macro idtentry vector asmsym cfunc has_error_code:req
SYM_CODE_START(\asmsym)

	.if \vector == X86_TRAP_BP
		
		UNWIND_HINT_IRET_ENTRY offset=\has_error_code*8 signal=0
	.else
		UNWIND_HINT_IRET_ENTRY offset=\has_error_code*8
	.endif

	ENDBR
	ASM_CLAC
	cld

	.if \has_error_code == 0
		pushq	$-1			
	.endif

	.if \vector == X86_TRAP_BP
		
		testb	$3, CS-ORIG_RAX(%rsp)
		jnz	.Lfrom_usermode_no_gap_\@
		.rept	6
		pushq	5*8(%rsp)
		.endr
		UNWIND_HINT_IRET_REGS offset=8
.Lfrom_usermode_no_gap_\@:
	.endif

	idtentry_body \cfunc \has_error_code

_ASM_NOKPROBE(\asmsym)
SYM_CODE_END(\asmsym)
.endm


.macro idtentry_irq vector cfunc
	.p2align CONFIG_X86_L1_CACHE_SHIFT
	idtentry \vector asm_\cfunc \cfunc has_error_code=1
.endm


.macro idtentry_sysvec vector cfunc
	idtentry \vector asm_\cfunc \cfunc has_error_code=0
.endm


.macro idtentry_mce_db vector asmsym cfunc
SYM_CODE_START(\asmsym)
	UNWIND_HINT_IRET_ENTRY
	ENDBR
	ASM_CLAC
	cld

	pushq	$-1			

	
	testb	$3, CS-ORIG_RAX(%rsp)
	jnz	.Lfrom_usermode_switch_stack_\@

	
	call	paranoid_entry

	UNWIND_HINT_REGS

	movq	%rsp, %rdi		

	call	\cfunc

	jmp	paranoid_exit

	
.Lfrom_usermode_switch_stack_\@:
	idtentry_body noist_\cfunc, has_error_code=0

_ASM_NOKPROBE(\asmsym)
SYM_CODE_END(\asmsym)
.endm

#ifdef CONFIG_AMD_MEM_ENCRYPT

.macro idtentry_vc vector asmsym cfunc
SYM_CODE_START(\asmsym)
	UNWIND_HINT_IRET_ENTRY
	ENDBR
	ASM_CLAC
	cld

	
	testb	$3, CS-ORIG_RAX(%rsp)
	jnz	.Lfrom_usermode_switch_stack_\@

	
	call	paranoid_entry

	UNWIND_HINT_REGS

	
	movq	%rsp, %rdi		
	call	vc_switch_off_ist
	movq	%rax, %rsp		

	ENCODE_FRAME_POINTER
	UNWIND_HINT_REGS

	
	movq	ORIG_RAX(%rsp), %rsi	
	movq	$-1, ORIG_RAX(%rsp)	

	movq	%rsp, %rdi		

	call	kernel_\cfunc

	
	jmp	paranoid_exit

	
.Lfrom_usermode_switch_stack_\@:
	idtentry_body user_\cfunc, has_error_code=1

_ASM_NOKPROBE(\asmsym)
SYM_CODE_END(\asmsym)
.endm
#endif


.macro idtentry_df vector asmsym cfunc
SYM_CODE_START(\asmsym)
	UNWIND_HINT_IRET_ENTRY offset=8
	ENDBR
	ASM_CLAC
	cld

	
	call	paranoid_entry
	UNWIND_HINT_REGS

	movq	%rsp, %rdi		
	movq	ORIG_RAX(%rsp), %rsi	
	movq	$-1, ORIG_RAX(%rsp)	
	call	\cfunc

	
	REACHABLE

	jmp	paranoid_exit

_ASM_NOKPROBE(\asmsym)
SYM_CODE_END(\asmsym)
.endm


	__ALIGN
	.globl __irqentry_text_start
__irqentry_text_start:

#include <asm/idtentry.h>

	__ALIGN
	.globl __irqentry_text_end
__irqentry_text_end:
	ANNOTATE_NOENDBR

SYM_CODE_START_LOCAL(common_interrupt_return)
SYM_INNER_LABEL(swapgs_restore_regs_and_return_to_usermode, SYM_L_GLOBAL)
	IBRS_EXIT
#ifdef CONFIG_DEBUG_ENTRY
	
	testb	$3, CS(%rsp)
	jnz	1f
	ud2
1:
#endif
#ifdef CONFIG_XEN_PV
	ALTERNATIVE "", "jmp xenpv_restore_regs_and_return_to_usermode", X86_FEATURE_XENPV
#endif

	POP_REGS pop_rdi=0

	
	movq	%rsp, %rdi
	movq	PER_CPU_VAR(cpu_tss_rw + TSS_sp0), %rsp
	UNWIND_HINT_END_OF_STACK

	
	pushq	6*8(%rdi)	
	pushq	5*8(%rdi)	
	pushq	4*8(%rdi)	
	pushq	3*8(%rdi)	
	pushq	2*8(%rdi)	

	
	pushq	(%rdi)

	
	STACKLEAK_ERASE_NOCLOBBER

	SWITCH_TO_USER_CR3_STACK scratch_reg=%rdi

	
	popq	%rdi
	swapgs
	jmp	.Lnative_iret


SYM_INNER_LABEL(restore_regs_and_return_to_kernel, SYM_L_GLOBAL)
#ifdef CONFIG_DEBUG_ENTRY
	
	testb	$3, CS(%rsp)
	jz	1f
	ud2
1:
#endif
	POP_REGS
	addq	$8, %rsp	
	
#ifdef CONFIG_XEN_PV
SYM_INNER_LABEL(early_xen_iret_patch, SYM_L_GLOBAL)
	ANNOTATE_NOENDBR
	.byte 0xe9
	.long .Lnative_iret - (. + 4)
#endif

.Lnative_iret:
	UNWIND_HINT_IRET_REGS
	
#ifdef CONFIG_X86_ESPFIX64
	testb	$4, (SS-RIP)(%rsp)
	jnz	native_irq_return_ldt
#endif

SYM_INNER_LABEL(native_irq_return_iret, SYM_L_GLOBAL)
	ANNOTATE_NOENDBR 
	
	iretq

#ifdef CONFIG_X86_ESPFIX64
native_irq_return_ldt:
	

	pushq	%rdi				
	swapgs					
	SWITCH_TO_KERNEL_CR3 scratch_reg=%rdi	

	movq	PER_CPU_VAR(espfix_waddr), %rdi
	movq	%rax, (0*8)(%rdi)		
	movq	(1*8)(%rsp), %rax		
	movq	%rax, (1*8)(%rdi)
	movq	(2*8)(%rsp), %rax		
	movq	%rax, (2*8)(%rdi)
	movq	(3*8)(%rsp), %rax		
	movq	%rax, (3*8)(%rdi)
	movq	(5*8)(%rsp), %rax		
	movq	%rax, (5*8)(%rdi)
	movq	(4*8)(%rsp), %rax		
	movq	%rax, (4*8)(%rdi)
	

	andl	$0xffff0000, %eax		

	
	orq	PER_CPU_VAR(espfix_stack), %rax

	SWITCH_TO_USER_CR3_STACK scratch_reg=%rdi
	swapgs					
	popq	%rdi				

	movq	%rax, %rsp
	UNWIND_HINT_IRET_REGS offset=8

	
	popq	%rax				

	
	jmp	native_irq_return_iret
#endif
SYM_CODE_END(common_interrupt_return)
_ASM_NOKPROBE(common_interrupt_return)


SYM_FUNC_START(asm_load_gs_index)
	FRAME_BEGIN
	swapgs
.Lgs_change:
	ANNOTATE_NOENDBR 
	movl	%edi, %gs
2:	ALTERNATIVE "", "mfence", X86_BUG_SWAPGS_FENCE
	swapgs
	FRAME_END
	RET

	
.Lbad_gs:
	swapgs					
.macro ZAP_GS
	
	movl $__USER_DS, %eax
	movl %eax, %gs
.endm
	ALTERNATIVE "", "ZAP_GS", X86_BUG_NULL_SEG
	xorl	%eax, %eax
	movl	%eax, %gs
	jmp	2b

	_ASM_EXTABLE(.Lgs_change, .Lbad_gs)

SYM_FUNC_END(asm_load_gs_index)
EXPORT_SYMBOL(asm_load_gs_index)

#ifdef CONFIG_XEN_PV

	__FUNC_ALIGN
SYM_CODE_START_LOCAL_NOALIGN(exc_xen_hypervisor_callback)


	UNWIND_HINT_FUNC
	movq	%rdi, %rsp			
	UNWIND_HINT_REGS

	call	xen_pv_evtchn_do_upcall

	jmp	error_return
SYM_CODE_END(exc_xen_hypervisor_callback)


	__FUNC_ALIGN
SYM_CODE_START_NOALIGN(xen_failsafe_callback)
	UNWIND_HINT_UNDEFINED
	ENDBR
	movl	%ds, %ecx
	cmpw	%cx, 0x10(%rsp)
	jne	1f
	movl	%es, %ecx
	cmpw	%cx, 0x18(%rsp)
	jne	1f
	movl	%fs, %ecx
	cmpw	%cx, 0x20(%rsp)
	jne	1f
	movl	%gs, %ecx
	cmpw	%cx, 0x28(%rsp)
	jne	1f
	
	movq	(%rsp), %rcx
	movq	8(%rsp), %r11
	addq	$0x30, %rsp
	pushq	$0				
	UNWIND_HINT_IRET_REGS offset=8
	jmp	asm_exc_general_protection
1:	
	movq	(%rsp), %rcx
	movq	8(%rsp), %r11
	addq	$0x30, %rsp
	UNWIND_HINT_IRET_REGS
	pushq	$-1 
	PUSH_AND_CLEAR_REGS
	ENCODE_FRAME_POINTER
	jmp	error_return
SYM_CODE_END(xen_failsafe_callback)
#endif 


SYM_CODE_START(paranoid_entry)
	ANNOTATE_NOENDBR
	UNWIND_HINT_FUNC
	PUSH_AND_CLEAR_REGS save_ret=1
	ENCODE_FRAME_POINTER 8

	
	SAVE_AND_SWITCH_TO_KERNEL_CR3 scratch_reg=%rax save_reg=%r14

	
	ALTERNATIVE "jmp .Lparanoid_entry_checkgs", "", X86_FEATURE_FSGSBASE

	
	SAVE_AND_SET_GSBASE scratch_reg=%rax save_reg=%rbx
	jmp .Lparanoid_gsbase_done

.Lparanoid_entry_checkgs:
	
	movl	$1, %ebx

	
	movl	$MSR_GS_BASE, %ecx
	rdmsr
	testl	%edx, %edx
	js	.Lparanoid_kernel_gsbase

	
	xorl	%ebx, %ebx
	swapgs
.Lparanoid_kernel_gsbase:
	FENCE_SWAPGS_KERNEL_ENTRY
.Lparanoid_gsbase_done:

	
	IBRS_ENTER save_reg=%r15
	UNTRAIN_RET_FROM_CALL

	RET
SYM_CODE_END(paranoid_entry)


SYM_CODE_START_LOCAL(paranoid_exit)
	UNWIND_HINT_REGS

	
	IBRS_EXIT save_reg=%r15

	
	RESTORE_CR3	scratch_reg=%rax save_reg=%r14

	
	ALTERNATIVE "jmp .Lparanoid_exit_checkgs", "", X86_FEATURE_FSGSBASE

	
	wrgsbase	%rbx
	jmp		restore_regs_and_return_to_kernel

.Lparanoid_exit_checkgs:
	
	testl		%ebx, %ebx
	jnz		restore_regs_and_return_to_kernel

	
	swapgs
	jmp		restore_regs_and_return_to_kernel
SYM_CODE_END(paranoid_exit)


SYM_CODE_START(error_entry)
	ANNOTATE_NOENDBR
	UNWIND_HINT_FUNC

	PUSH_AND_CLEAR_REGS save_ret=1
	ENCODE_FRAME_POINTER 8

	testb	$3, CS+8(%rsp)
	jz	.Lerror_kernelspace

	
	swapgs
	FENCE_SWAPGS_USER_ENTRY
	
	SWITCH_TO_KERNEL_CR3 scratch_reg=%rax
	IBRS_ENTER
	UNTRAIN_RET_FROM_CALL

	leaq	8(%rsp), %rdi			
	
	jmp	sync_regs

	
.Lerror_kernelspace:
	leaq	native_irq_return_iret(%rip), %rcx
	cmpq	%rcx, RIP+8(%rsp)
	je	.Lerror_bad_iret
	movl	%ecx, %eax			
	cmpq	%rax, RIP+8(%rsp)
	je	.Lbstep_iret
	cmpq	$.Lgs_change, RIP+8(%rsp)
	jne	.Lerror_entry_done_lfence

	
	swapgs

	
.Lerror_entry_done_lfence:
	FENCE_SWAPGS_KERNEL_ENTRY
	CALL_DEPTH_ACCOUNT
	leaq	8(%rsp), %rax			
	VALIDATE_UNRET_END
	RET

.Lbstep_iret:
	
	movq	%rcx, RIP+8(%rsp)
	

.Lerror_bad_iret:
	
	swapgs
	FENCE_SWAPGS_USER_ENTRY
	SWITCH_TO_KERNEL_CR3 scratch_reg=%rax
	IBRS_ENTER
	UNTRAIN_RET_FROM_CALL

	
	leaq	8(%rsp), %rdi			
	call	fixup_bad_iret
	mov	%rax, %rdi
	jmp	sync_regs
SYM_CODE_END(error_entry)

SYM_CODE_START_LOCAL(error_return)
	UNWIND_HINT_REGS
	DEBUG_ENTRY_ASSERT_IRQS_OFF
	testb	$3, CS(%rsp)
	jz	restore_regs_and_return_to_kernel
	jmp	swapgs_restore_regs_and_return_to_usermode
SYM_CODE_END(error_return)


SYM_CODE_START(asm_exc_nmi)
	UNWIND_HINT_IRET_ENTRY
	ENDBR

	

	ASM_CLAC
	cld

	
	pushq	%rdx

	testb	$3, CS-RIP+8(%rsp)
	jz	.Lnmi_from_kernel

	

	swapgs
	FENCE_SWAPGS_USER_ENTRY
	SWITCH_TO_KERNEL_CR3 scratch_reg=%rdx
	movq	%rsp, %rdx
	movq	PER_CPU_VAR(pcpu_hot + X86_top_of_stack), %rsp
	UNWIND_HINT_IRET_REGS base=%rdx offset=8
	pushq	5*8(%rdx)	
	pushq	4*8(%rdx)	
	pushq	3*8(%rdx)	
	pushq	2*8(%rdx)	
	pushq	1*8(%rdx)	
	UNWIND_HINT_IRET_REGS
	pushq   $-1		
	PUSH_AND_CLEAR_REGS rdx=(%rdx)
	ENCODE_FRAME_POINTER

	IBRS_ENTER
	UNTRAIN_RET

	

	movq	%rsp, %rdi
	movq	$-1, %rsi
	call	exc_nmi

	
	jmp	swapgs_restore_regs_and_return_to_usermode

.Lnmi_from_kernel:
	

	

	movq	$repeat_nmi, %rdx
	cmpq	8(%rsp), %rdx
	ja	1f
	movq	$end_repeat_nmi, %rdx
	cmpq	8(%rsp), %rdx
	ja	nested_nmi_out
1:

	
	cmpl	$1, -8(%rsp)
	je	nested_nmi

	
	lea	6*8(%rsp), %rdx
	
	cmpq	%rdx, 4*8(%rsp)
	
	ja	first_nmi

	subq	$EXCEPTION_STKSZ, %rdx
	cmpq	%rdx, 4*8(%rsp)
	
	jb	first_nmi

	

	testb	$(X86_EFLAGS_DF >> 8), (3*8 + 1)(%rsp)
	jz	first_nmi	

	

nested_nmi:
	
	subq	$8, %rsp
	leaq	-10*8(%rsp), %rdx
	pushq	$__KERNEL_DS
	pushq	%rdx
	pushfq
	pushq	$__KERNEL_CS
	pushq	$repeat_nmi

	
	addq	$(6*8), %rsp

nested_nmi_out:
	popq	%rdx

	
	iretq

first_nmi:
	
	movq	(%rsp), %rdx

	
	pushq	$0

	
	subq	$(5*8), %rsp

	
	.rept 5
	pushq	11*8(%rsp)
	.endr
	UNWIND_HINT_IRET_REGS

	

#ifdef CONFIG_DEBUG_ENTRY
	
	pushq	$0		
	pushq	%rsp		
	addq	$8, (%rsp)	
	pushfq			
	pushq	$__KERNEL_CS	
	pushq	$1f		
	iretq			
	UNWIND_HINT_IRET_REGS
1:
#endif

repeat_nmi:
	ANNOTATE_NOENDBR 
	
	movq	$1, 10*8(%rsp)		

	
	addq	$(10*8), %rsp
	.rept 5
	pushq	-6*8(%rsp)
	.endr
	subq	$(5*8), %rsp
end_repeat_nmi:
	ANNOTATE_NOENDBR 

	
	pushq	$-1				

	
	call	paranoid_entry
	UNWIND_HINT_REGS

	movq	%rsp, %rdi
	movq	$-1, %rsi
	call	exc_nmi

	
	IBRS_EXIT save_reg=%r15

	
	RESTORE_CR3 scratch_reg=%r15 save_reg=%r14

	
	ALTERNATIVE "jmp nmi_no_fsgsbase", "", X86_FEATURE_FSGSBASE

	wrgsbase	%rbx
	jmp	nmi_restore

nmi_no_fsgsbase:
	
	testl	%ebx, %ebx
	jnz	nmi_restore

nmi_swapgs:
	swapgs

nmi_restore:
	POP_REGS

	
	addq	$6*8, %rsp

	
	std
	movq	$0, 5*8(%rsp)		

	
	iretq
SYM_CODE_END(asm_exc_nmi)

#ifndef CONFIG_IA32_EMULATION

SYM_CODE_START(ignore_sysret)
	UNWIND_HINT_END_OF_STACK
	ENDBR
	mov	$-ENOSYS, %eax
	sysretl
SYM_CODE_END(ignore_sysret)
#endif

.pushsection .text, "ax"
	__FUNC_ALIGN
SYM_CODE_START_NOALIGN(rewind_stack_and_make_dead)
	UNWIND_HINT_FUNC
	
	xorl	%ebp, %ebp

	movq	PER_CPU_VAR(pcpu_hot + X86_top_of_stack), %rax
	leaq	-PTREGS_SIZE(%rax), %rsp
	UNWIND_HINT_REGS

	call	make_task_dead
SYM_CODE_END(rewind_stack_and_make_dead)
.popsection
