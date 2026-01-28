

#include <asm/asm-offsets.h>
#include <asm/current.h>
#include <asm/errno.h>
#include <asm/ia32_unistd.h>
#include <asm/thread_info.h>
#include <asm/segment.h>
#include <asm/irqflags.h>
#include <asm/asm.h>
#include <asm/smap.h>
#include <asm/nospec-branch.h>
#include <linux/linkage.h>
#include <linux/err.h>

#include "calling.h"

	.section .entry.text, "ax"


SYM_CODE_START(entry_SYSENTER_compat)
	UNWIND_HINT_ENTRY
	ENDBR
	
	swapgs

	pushq	%rax
	SWITCH_TO_KERNEL_CR3 scratch_reg=%rax
	popq	%rax

	movq	PER_CPU_VAR(pcpu_hot + X86_top_of_stack), %rsp

	
	pushq	$__USER_DS		
	pushq	$0			

	
	pushfq				
	pushq	$__USER32_CS		
	pushq	$0			
SYM_INNER_LABEL(entry_SYSENTER_compat_after_hwframe, SYM_L_GLOBAL)

	
	movl	%eax, %eax

	pushq	%rax			
	PUSH_AND_CLEAR_REGS rax=$-ENOSYS
	UNWIND_HINT_REGS

	cld

	IBRS_ENTER
	UNTRAIN_RET

	
	testl	$X86_EFLAGS_NT|X86_EFLAGS_AC|X86_EFLAGS_TF, EFLAGS(%rsp)
	jnz	.Lsysenter_fix_flags
.Lsysenter_flags_fixed:

	movq	%rsp, %rdi
	call	do_SYSENTER_32
	
	ALTERNATIVE "testl %eax, %eax; jz swapgs_restore_regs_and_return_to_usermode", \
		    "jmp swapgs_restore_regs_and_return_to_usermode", X86_FEATURE_XENPV
	jmp	sysret32_from_system_call

.Lsysenter_fix_flags:
	pushq	$X86_EFLAGS_FIXED
	popfq
	jmp	.Lsysenter_flags_fixed
SYM_INNER_LABEL(__end_entry_SYSENTER_compat, SYM_L_GLOBAL)
SYM_CODE_END(entry_SYSENTER_compat)


SYM_CODE_START(entry_SYSCALL_compat)
	UNWIND_HINT_ENTRY
	ENDBR
	
	swapgs

	
	movl	%esp, %r8d

	
	SWITCH_TO_KERNEL_CR3 scratch_reg=%rsp

	
	movq	PER_CPU_VAR(pcpu_hot + X86_top_of_stack), %rsp

SYM_INNER_LABEL(entry_SYSCALL_compat_safe_stack, SYM_L_GLOBAL)
	ANNOTATE_NOENDBR

	
	pushq	$__USER_DS		
	pushq	%r8			
	pushq	%r11			
	pushq	$__USER32_CS		
	pushq	%rcx			
SYM_INNER_LABEL(entry_SYSCALL_compat_after_hwframe, SYM_L_GLOBAL)
	movl	%eax, %eax		
	pushq	%rax			
	PUSH_AND_CLEAR_REGS rcx=%rbp rax=$-ENOSYS
	UNWIND_HINT_REGS

	IBRS_ENTER
	UNTRAIN_RET

	movq	%rsp, %rdi
	call	do_fast_syscall_32
	
	ALTERNATIVE "testl %eax, %eax; jz swapgs_restore_regs_and_return_to_usermode", \
		    "jmp swapgs_restore_regs_and_return_to_usermode", X86_FEATURE_XENPV

	
sysret32_from_system_call:
	
	STACKLEAK_ERASE

	IBRS_EXIT

	movq	RBX(%rsp), %rbx		
	movq	RBP(%rsp), %rbp		
	movq	EFLAGS(%rsp), %r11	
	movq	RIP(%rsp), %rcx		
	addq	$RAX, %rsp		
	popq	%rax			
	popq	%rdx			
	popq	%rdx			
	popq	%rsi			
	popq	%rdi			

        
	movq	RSP-ORIG_RAX(%rsp), %rsp
SYM_INNER_LABEL(entry_SYSRETL_compat_unsafe_stack, SYM_L_GLOBAL)
	ANNOTATE_NOENDBR

	
	SWITCH_TO_USER_CR3_NOSTACK scratch_reg=%r8 scratch_reg2=%r9

	xorl	%r8d, %r8d
	xorl	%r9d, %r9d
	xorl	%r10d, %r10d
	swapgs
	sysretl
SYM_INNER_LABEL(entry_SYSRETL_compat_end, SYM_L_GLOBAL)
	ANNOTATE_NOENDBR
	int3
SYM_CODE_END(entry_SYSCALL_compat)
