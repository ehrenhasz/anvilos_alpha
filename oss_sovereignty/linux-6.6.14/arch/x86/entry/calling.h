#include <linux/jump_label.h>
#include <asm/unwind_hints.h>
#include <asm/cpufeatures.h>
#include <asm/page_types.h>
#include <asm/percpu.h>
#include <asm/asm-offsets.h>
#include <asm/processor-flags.h>
#include <asm/ptrace-abi.h>
#include <asm/msr.h>
#include <asm/nospec-branch.h>
#ifdef CONFIG_X86_64
.macro PUSH_REGS rdx=%rdx rcx=%rcx rax=%rax save_ret=0
	.if \save_ret
	pushq	%rsi		 
	movq	8(%rsp), %rsi	 
	movq	%rdi, 8(%rsp)	 
	.else
	pushq   %rdi		 
	pushq   %rsi		 
	.endif
	pushq	\rdx		 
	pushq   \rcx		 
	pushq   \rax		 
	pushq   %r8		 
	pushq   %r9		 
	pushq   %r10		 
	pushq   %r11		 
	pushq	%rbx		 
	pushq	%rbp		 
	pushq	%r12		 
	pushq	%r13		 
	pushq	%r14		 
	pushq	%r15		 
	UNWIND_HINT_REGS
	.if \save_ret
	pushq	%rsi		 
	.endif
.endm
.macro CLEAR_REGS
	xorl	%esi,  %esi	 
	xorl	%edx,  %edx	 
	xorl	%ecx,  %ecx	 
	xorl	%r8d,  %r8d	 
	xorl	%r9d,  %r9d	 
	xorl	%r10d, %r10d	 
	xorl	%r11d, %r11d	 
	xorl	%ebx,  %ebx	 
	xorl	%ebp,  %ebp	 
	xorl	%r12d, %r12d	 
	xorl	%r13d, %r13d	 
	xorl	%r14d, %r14d	 
	xorl	%r15d, %r15d	 
.endm
.macro PUSH_AND_CLEAR_REGS rdx=%rdx rcx=%rcx rax=%rax save_ret=0
	PUSH_REGS rdx=\rdx, rcx=\rcx, rax=\rax, save_ret=\save_ret
	CLEAR_REGS
.endm
.macro POP_REGS pop_rdi=1
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rax
	popq %rcx
	popq %rdx
	popq %rsi
	.if \pop_rdi
	popq %rdi
	.endif
.endm
#ifdef CONFIG_PAGE_TABLE_ISOLATION
#define PTI_USER_PGTABLE_BIT		PAGE_SHIFT
#define PTI_USER_PGTABLE_MASK		(1 << PTI_USER_PGTABLE_BIT)
#define PTI_USER_PCID_BIT		X86_CR3_PTI_PCID_USER_BIT
#define PTI_USER_PCID_MASK		(1 << PTI_USER_PCID_BIT)
#define PTI_USER_PGTABLE_AND_PCID_MASK  (PTI_USER_PCID_MASK | PTI_USER_PGTABLE_MASK)
.macro SET_NOFLUSH_BIT	reg:req
	bts	$X86_CR3_PCID_NOFLUSH_BIT, \reg
.endm
.macro ADJUST_KERNEL_CR3 reg:req
	ALTERNATIVE "", "SET_NOFLUSH_BIT \reg", X86_FEATURE_PCID
	andq    $(~PTI_USER_PGTABLE_AND_PCID_MASK), \reg
.endm
.macro SWITCH_TO_KERNEL_CR3 scratch_reg:req
	ALTERNATIVE "jmp .Lend_\@", "", X86_FEATURE_PTI
	mov	%cr3, \scratch_reg
	ADJUST_KERNEL_CR3 \scratch_reg
	mov	\scratch_reg, %cr3
.Lend_\@:
.endm
#define THIS_CPU_user_pcid_flush_mask   \
	PER_CPU_VAR(cpu_tlbstate) + TLB_STATE_user_pcid_flush_mask
.macro SWITCH_TO_USER_CR3_NOSTACK scratch_reg:req scratch_reg2:req
	ALTERNATIVE "jmp .Lend_\@", "", X86_FEATURE_PTI
	mov	%cr3, \scratch_reg
	ALTERNATIVE "jmp .Lwrcr3_\@", "", X86_FEATURE_PCID
	movq	\scratch_reg, \scratch_reg2
	andq	$(0x7FF), \scratch_reg		 
	bt	\scratch_reg, THIS_CPU_user_pcid_flush_mask
	jnc	.Lnoflush_\@
	btr	\scratch_reg, THIS_CPU_user_pcid_flush_mask
	movq	\scratch_reg2, \scratch_reg
	jmp	.Lwrcr3_pcid_\@
.Lnoflush_\@:
	movq	\scratch_reg2, \scratch_reg
	SET_NOFLUSH_BIT \scratch_reg
.Lwrcr3_pcid_\@:
	orq	$(PTI_USER_PCID_MASK), \scratch_reg
.Lwrcr3_\@:
	orq     $(PTI_USER_PGTABLE_MASK), \scratch_reg
	mov	\scratch_reg, %cr3
.Lend_\@:
.endm
.macro SWITCH_TO_USER_CR3_STACK	scratch_reg:req
	pushq	%rax
	SWITCH_TO_USER_CR3_NOSTACK scratch_reg=\scratch_reg scratch_reg2=%rax
	popq	%rax
.endm
.macro SAVE_AND_SWITCH_TO_KERNEL_CR3 scratch_reg:req save_reg:req
	ALTERNATIVE "jmp .Ldone_\@", "", X86_FEATURE_PTI
	movq	%cr3, \scratch_reg
	movq	\scratch_reg, \save_reg
	bt	$PTI_USER_PGTABLE_BIT, \scratch_reg
	jnc	.Ldone_\@
	ADJUST_KERNEL_CR3 \scratch_reg
	movq	\scratch_reg, %cr3
.Ldone_\@:
.endm
.macro RESTORE_CR3 scratch_reg:req save_reg:req
	ALTERNATIVE "jmp .Lend_\@", "", X86_FEATURE_PTI
	ALTERNATIVE "jmp .Lwrcr3_\@", "", X86_FEATURE_PCID
	bt	$PTI_USER_PGTABLE_BIT, \save_reg
	jnc	.Lnoflush_\@
	movq	\save_reg, \scratch_reg
	andq	$(0x7FF), \scratch_reg
	bt	\scratch_reg, THIS_CPU_user_pcid_flush_mask
	jnc	.Lnoflush_\@
	btr	\scratch_reg, THIS_CPU_user_pcid_flush_mask
	jmp	.Lwrcr3_\@
.Lnoflush_\@:
	SET_NOFLUSH_BIT \save_reg
.Lwrcr3_\@:
	movq	\save_reg, %cr3
.Lend_\@:
.endm
#else  
.macro SWITCH_TO_KERNEL_CR3 scratch_reg:req
.endm
.macro SWITCH_TO_USER_CR3_NOSTACK scratch_reg:req scratch_reg2:req
.endm
.macro SWITCH_TO_USER_CR3_STACK scratch_reg:req
.endm
.macro SAVE_AND_SWITCH_TO_KERNEL_CR3 scratch_reg:req save_reg:req
.endm
.macro RESTORE_CR3 scratch_reg:req save_reg:req
.endm
#endif
.macro IBRS_ENTER save_reg
#ifdef CONFIG_CPU_IBRS_ENTRY
	ALTERNATIVE "jmp .Lend_\@", "", X86_FEATURE_KERNEL_IBRS
	movl	$MSR_IA32_SPEC_CTRL, %ecx
.ifnb \save_reg
	rdmsr
	shl	$32, %rdx
	or	%rdx, %rax
	mov	%rax, \save_reg
	test	$SPEC_CTRL_IBRS, %eax
	jz	.Ldo_wrmsr_\@
	lfence
	jmp	.Lend_\@
.Ldo_wrmsr_\@:
.endif
	movq	PER_CPU_VAR(x86_spec_ctrl_current), %rdx
	movl	%edx, %eax
	shr	$32, %rdx
	wrmsr
.Lend_\@:
#endif
.endm
.macro IBRS_EXIT save_reg
#ifdef CONFIG_CPU_IBRS_ENTRY
	ALTERNATIVE "jmp .Lend_\@", "", X86_FEATURE_KERNEL_IBRS
	movl	$MSR_IA32_SPEC_CTRL, %ecx
.ifnb \save_reg
	mov	\save_reg, %rdx
.else
	movq	PER_CPU_VAR(x86_spec_ctrl_current), %rdx
	andl	$(~SPEC_CTRL_IBRS), %edx
.endif
	movl	%edx, %eax
	shr	$32, %rdx
	wrmsr
.Lend_\@:
#endif
.endm
.macro FENCE_SWAPGS_USER_ENTRY
	ALTERNATIVE "", "lfence", X86_FEATURE_FENCE_SWAPGS_USER
.endm
.macro FENCE_SWAPGS_KERNEL_ENTRY
	ALTERNATIVE "", "lfence", X86_FEATURE_FENCE_SWAPGS_KERNEL
.endm
.macro STACKLEAK_ERASE_NOCLOBBER
#ifdef CONFIG_GCC_PLUGIN_STACKLEAK
	PUSH_AND_CLEAR_REGS
	call stackleak_erase
	POP_REGS
#endif
.endm
.macro SAVE_AND_SET_GSBASE scratch_reg:req save_reg:req
	rdgsbase \save_reg
	GET_PERCPU_BASE \scratch_reg
	wrgsbase \scratch_reg
.endm
#else  
# undef		UNWIND_HINT_IRET_REGS
# define	UNWIND_HINT_IRET_REGS
#endif  
.macro STACKLEAK_ERASE
#ifdef CONFIG_GCC_PLUGIN_STACKLEAK
	call stackleak_erase
#endif
.endm
#ifdef CONFIG_SMP
.macro LOAD_CPU_AND_NODE_SEG_LIMIT reg:req
	movq	$__CPUNODE_SEG, \reg
	lsl	\reg, \reg
.endm
.macro GET_PERCPU_BASE reg:req
	LOAD_CPU_AND_NODE_SEG_LIMIT \reg
	andq	$VDSO_CPUNODE_MASK, \reg
	movq	__per_cpu_offset(, \reg, 8), \reg
.endm
#else
.macro GET_PERCPU_BASE reg:req
	movq	pcpu_unit_offsets(%rip), \reg
.endm
#endif  
