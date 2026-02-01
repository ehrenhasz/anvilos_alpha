 
 

#include <linux/linkage.h>
#include <linux/err.h>
#include <asm/thread_info.h>
#include <asm/irqflags.h>
#include <asm/errno.h>
#include <asm/segment.h>
#include <asm/smp.h>
#include <asm/percpu.h>
#include <asm/processor-flags.h>
#include <asm/irq_vectors.h>
#include <asm/cpufeatures.h>
#include <asm/alternative.h>
#include <asm/asm.h>
#include <asm/smap.h>
#include <asm/frame.h>
#include <asm/trapnr.h>
#include <asm/nospec-branch.h>

#include "calling.h"

	.section .entry.text, "ax"

#define PTI_SWITCH_MASK         (1 << PAGE_SHIFT)

 
.macro SWITCH_TO_USER_CR3 scratch_reg:req
	ALTERNATIVE "jmp .Lend_\@", "", X86_FEATURE_PTI

	movl	%cr3, \scratch_reg
	orl	$PTI_SWITCH_MASK, \scratch_reg
	movl	\scratch_reg, %cr3
.Lend_\@:
.endm

.macro BUG_IF_WRONG_CR3 no_user_check=0
#ifdef CONFIG_DEBUG_ENTRY
	ALTERNATIVE "jmp .Lend_\@", "", X86_FEATURE_PTI
	.if \no_user_check == 0
	 
	testl	$USER_SEGMENT_RPL_MASK, PT_CS(%esp)
	jz	.Lend_\@
	.endif
	 
	movl	%cr3, %eax
	testl	$PTI_SWITCH_MASK, %eax
	jnz	.Lend_\@
	 
	ud2
.Lend_\@:
#endif
.endm

 
.macro SWITCH_TO_KERNEL_CR3 scratch_reg:req
	ALTERNATIVE "jmp .Lend_\@", "", X86_FEATURE_PTI
	movl	%cr3, \scratch_reg
	 
	testl	$PTI_SWITCH_MASK, \scratch_reg
	jz	.Lend_\@
	andl	$(~PTI_SWITCH_MASK), \scratch_reg
	movl	\scratch_reg, %cr3
	 
	orl	$PTI_SWITCH_MASK, \scratch_reg
.Lend_\@:
.endm

#define CS_FROM_ENTRY_STACK	(1 << 31)
#define CS_FROM_USER_CR3	(1 << 30)
#define CS_FROM_KERNEL		(1 << 29)
#define CS_FROM_ESPFIX		(1 << 28)

.macro FIXUP_FRAME
	 
	andl	$0x0000ffff, 4*4(%esp)

#ifdef CONFIG_VM86
	testl	$X86_EFLAGS_VM, 5*4(%esp)
	jnz	.Lfrom_usermode_no_fixup_\@
#endif
	testl	$USER_SEGMENT_RPL_MASK, 4*4(%esp)
	jnz	.Lfrom_usermode_no_fixup_\@

	orl	$CS_FROM_KERNEL, 4*4(%esp)

	 

	pushl	%ss		# ss
	pushl	%esp		# sp (points at ss)
	addl	$7*4, (%esp)	# point sp back at the previous context
	pushl	7*4(%esp)	# flags
	pushl	7*4(%esp)	# cs
	pushl	7*4(%esp)	# ip
	pushl	7*4(%esp)	# orig_eax
	pushl	7*4(%esp)	# gs / function
	pushl	7*4(%esp)	# fs
.Lfrom_usermode_no_fixup_\@:
.endm

.macro IRET_FRAME
	 
	testl $CS_FROM_KERNEL, 1*4(%esp)
	jz .Lfinished_frame_\@

	 
	pushl	%eax
	pushl	%ecx
	movl	5*4(%esp), %eax		# (modified) regs->sp

	movl	4*4(%esp), %ecx		# flags
	movl	%ecx, %ss:-1*4(%eax)

	movl	3*4(%esp), %ecx		# cs
	andl	$0x0000ffff, %ecx
	movl	%ecx, %ss:-2*4(%eax)

	movl	2*4(%esp), %ecx		# ip
	movl	%ecx, %ss:-3*4(%eax)

	movl	1*4(%esp), %ecx		# eax
	movl	%ecx, %ss:-4*4(%eax)

	popl	%ecx
	lea	-4*4(%eax), %esp
	popl	%eax
.Lfinished_frame_\@:
.endm

.macro SAVE_ALL pt_regs_ax=%eax switch_stacks=0 skip_gs=0 unwind_espfix=0
	cld
.if \skip_gs == 0
	pushl	$0
.endif
	pushl	%fs

	pushl	%eax
	movl	$(__KERNEL_PERCPU), %eax
	movl	%eax, %fs
.if \unwind_espfix > 0
	UNWIND_ESPFIX_STACK
.endif
	popl	%eax

	FIXUP_FRAME
	pushl	%es
	pushl	%ds
	pushl	\pt_regs_ax
	pushl	%ebp
	pushl	%edi
	pushl	%esi
	pushl	%edx
	pushl	%ecx
	pushl	%ebx
	movl	$(__USER_DS), %edx
	movl	%edx, %ds
	movl	%edx, %es
	 
.if \switch_stacks > 0
	SWITCH_TO_KERNEL_STACK
.endif
.endm

.macro SAVE_ALL_NMI cr3_reg:req unwind_espfix=0
	SAVE_ALL unwind_espfix=\unwind_espfix

	BUG_IF_WRONG_CR3

	 
	SWITCH_TO_KERNEL_CR3 scratch_reg=\cr3_reg

.Lend_\@:
.endm

.macro RESTORE_INT_REGS
	popl	%ebx
	popl	%ecx
	popl	%edx
	popl	%esi
	popl	%edi
	popl	%ebp
	popl	%eax
.endm

.macro RESTORE_REGS pop=0
	RESTORE_INT_REGS
1:	popl	%ds
2:	popl	%es
3:	popl	%fs
4:	addl	$(4 + \pop), %esp	 
	IRET_FRAME

	 
	_ASM_EXTABLE_TYPE(1b, 2b, EX_TYPE_POP_ZERO|EX_REG_DS)
	_ASM_EXTABLE_TYPE(2b, 3b, EX_TYPE_POP_ZERO|EX_REG_ES)
	_ASM_EXTABLE_TYPE(3b, 4b, EX_TYPE_POP_ZERO|EX_REG_FS)
.endm

.macro RESTORE_ALL_NMI cr3_reg:req pop=0
	 
	ALTERNATIVE "jmp .Lswitched_\@", "", X86_FEATURE_PTI

	testl	$PTI_SWITCH_MASK, \cr3_reg
	jz	.Lswitched_\@

	 
	movl	\cr3_reg, %cr3

.Lswitched_\@:

	BUG_IF_WRONG_CR3

	RESTORE_REGS pop=\pop
.endm

.macro CHECK_AND_APPLY_ESPFIX
#ifdef CONFIG_X86_ESPFIX32
#define GDT_ESPFIX_OFFSET (GDT_ENTRY_ESPFIX_SS * 8)
#define GDT_ESPFIX_SS PER_CPU_VAR(gdt_page) + GDT_ESPFIX_OFFSET

	ALTERNATIVE	"jmp .Lend_\@", "", X86_BUG_ESPFIX

	movl	PT_EFLAGS(%esp), %eax		# mix EFLAGS, SS and CS
	 
	movb	PT_OLDSS(%esp), %ah
	movb	PT_CS(%esp), %al
	andl	$(X86_EFLAGS_VM | (SEGMENT_TI_MASK << 8) | SEGMENT_RPL_MASK), %eax
	cmpl	$((SEGMENT_LDT << 8) | USER_RPL), %eax
	jne	.Lend_\@	# returning to user-space with LDT SS

	 
	mov	%esp, %edx			 
	mov	PT_OLDESP(%esp), %eax		 
	mov	%dx, %ax			 
	sub	%eax, %edx			 
	shr	$16, %edx
	mov	%dl, GDT_ESPFIX_SS + 4		 
	mov	%dh, GDT_ESPFIX_SS + 7		 
	pushl	$__ESPFIX_SS
	pushl	%eax				 
	 
	cli
	lss	(%esp), %esp			 
.Lend_\@:
#endif  
.endm

 

.macro SWITCH_TO_KERNEL_STACK

	BUG_IF_WRONG_CR3

	SWITCH_TO_KERNEL_CR3 scratch_reg=%eax

	 

	 
	movl	PER_CPU_VAR(cpu_entry_area), %ecx
	addl	$CPU_ENTRY_AREA_entry_stack + SIZEOF_entry_stack, %ecx
	subl	%esp, %ecx	 
	cmpl	$SIZEOF_entry_stack, %ecx
	jae	.Lend_\@

	 
	movl	%esp, %esi
	movl	%esi, %edi

	 
	andl	$(MASK_entry_stack), %edi
	addl	$(SIZEOF_entry_stack), %edi

	 
	movl	TSS_entry2task_stack(%edi), %edi

	 
#ifdef CONFIG_VM86
	movl	PT_EFLAGS(%esp), %ecx		# mix EFLAGS and CS
	movb	PT_CS(%esp), %cl
	andl	$(X86_EFLAGS_VM | SEGMENT_RPL_MASK), %ecx
#else
	movl	PT_CS(%esp), %ecx
	andl	$SEGMENT_RPL_MASK, %ecx
#endif
	cmpl	$USER_RPL, %ecx
	jb	.Lentry_from_kernel_\@

	 
	movl	$PTREGS_SIZE, %ecx

#ifdef CONFIG_VM86
	testl	$X86_EFLAGS_VM, PT_EFLAGS(%esi)
	jz	.Lcopy_pt_regs_\@

	 
	addl	$(4 * 4), %ecx

#endif
.Lcopy_pt_regs_\@:

	 
	subl	%ecx, %edi

	 
	movl	%edi, %esp

	 
	shrl	$2, %ecx
	cld
	rep movsl

	jmp .Lend_\@

.Lentry_from_kernel_\@:

	 

	 
	movl	%esi, %ecx

	 
	andl	$(MASK_entry_stack), %ecx
	addl	$(SIZEOF_entry_stack), %ecx

	 
	sub	%esi, %ecx

	 
	orl	$CS_FROM_ENTRY_STACK, PT_CS(%esp)

	 
	testl	$PTI_SWITCH_MASK, %eax
	jz	.Lcopy_pt_regs_\@
	orl	$CS_FROM_USER_CR3, PT_CS(%esp)

	 
	jmp .Lcopy_pt_regs_\@

.Lend_\@:
.endm

 
.macro SWITCH_TO_ENTRY_STACK

	 
	movl	$PTREGS_SIZE, %ecx

#ifdef CONFIG_VM86
	testl	$(X86_EFLAGS_VM), PT_EFLAGS(%esp)
	jz	.Lcopy_pt_regs_\@

	 
	addl    $(4 * 4), %ecx

.Lcopy_pt_regs_\@:
#endif

	 
	movl	PER_CPU_VAR(cpu_tss_rw + TSS_sp0), %edi
	subl	%ecx, %edi
	movl	%esp, %esi

	 
	movl	%edi, %ebx

	 
	shrl	$2, %ecx
	cld
	rep movsl

	 
	movl	%ebx, %esp

.Lend_\@:
.endm

 
.macro PARANOID_EXIT_TO_KERNEL_MODE

	 
	testl	$CS_FROM_ENTRY_STACK, PT_CS(%esp)
	jz	.Lend_\@

	 

	 
	andl	$(~CS_FROM_ENTRY_STACK), PT_CS(%esp)

	 
	movl	%esp, %esi
	movl	PER_CPU_VAR(cpu_tss_rw + TSS_sp0), %edi

	 
	movl	PER_CPU_VAR(cpu_tss_rw + TSS_sp1), %ecx
	subl	%esi, %ecx

	 
	subl	%ecx, %edi

	 
	movl	%edi, %ebx

	 
	shrl	$2, %ecx
	cld
	rep movsl

	 
	movl	%ebx, %esp

	 
	testl	$CS_FROM_USER_CR3, PT_CS(%esp)
	jz	.Lend_\@

	 
	andl	$(~CS_FROM_USER_CR3), PT_CS(%esp)

	SWITCH_TO_USER_CR3 scratch_reg=%eax

.Lend_\@:
.endm

 
.macro idtentry vector asmsym cfunc has_error_code:req
SYM_CODE_START(\asmsym)
	ASM_CLAC
	cld

	.if \has_error_code == 0
		pushl	$0		 
	.endif

	 
	pushl	$\cfunc
	 
	jmp	handle_exception
SYM_CODE_END(\asmsym)
.endm

.macro idtentry_irq vector cfunc
	.p2align CONFIG_X86_L1_CACHE_SHIFT
SYM_CODE_START_LOCAL(asm_\cfunc)
	ASM_CLAC
	SAVE_ALL switch_stacks=1
	ENCODE_FRAME_POINTER
	movl	%esp, %eax
	movl	PT_ORIG_EAX(%esp), %edx		 
	movl	$-1, PT_ORIG_EAX(%esp)		 
	call	\cfunc
	jmp	handle_exception_return
SYM_CODE_END(asm_\cfunc)
.endm

.macro idtentry_sysvec vector cfunc
	idtentry \vector asm_\cfunc \cfunc has_error_code=0
.endm

 
	.align 16
	.globl __irqentry_text_start
__irqentry_text_start:

#include <asm/idtentry.h>

	.align 16
	.globl __irqentry_text_end
__irqentry_text_end:

 
.pushsection .text, "ax"
SYM_CODE_START(__switch_to_asm)
	 
	pushl	%ebp
	pushl	%ebx
	pushl	%edi
	pushl	%esi
	 
	pushfl

	 
	movl	%esp, TASK_threadsp(%eax)
	movl	TASK_threadsp(%edx), %esp

#ifdef CONFIG_STACKPROTECTOR
	movl	TASK_stack_canary(%edx), %ebx
	movl	%ebx, PER_CPU_VAR(__stack_chk_guard)
#endif

	 
	FILL_RETURN_BUFFER %ebx, RSB_CLEAR_LOOPS, X86_FEATURE_RSB_CTXSW

	 
	popfl
	 
	popl	%esi
	popl	%edi
	popl	%ebx
	popl	%ebp

	jmp	__switch_to
SYM_CODE_END(__switch_to_asm)
.popsection

 
.pushsection .text, "ax"
SYM_CODE_START(ret_from_fork_asm)
	movl	%esp, %edx	 

	 
	pushl	$.Lsyscall_32_done

	FRAME_BEGIN
	 
	movl	%ebx, %ecx	 
	pushl	%edi		 
	call	ret_from_fork
	addl	$4, %esp
	FRAME_END

	RET
SYM_CODE_END(ret_from_fork_asm)
.popsection

SYM_ENTRY(__begin_SYSENTER_singlestep_region, SYM_L_GLOBAL, SYM_A_NONE)
 

 
SYM_FUNC_START(entry_SYSENTER_32)
	 
	pushfl
	pushl	%eax
	BUG_IF_WRONG_CR3 no_user_check=1
	SWITCH_TO_KERNEL_CR3 scratch_reg=%eax
	popl	%eax
	popfl

	 
	movl	TSS_entry2task_stack(%esp), %esp

.Lsysenter_past_esp:
	pushl	$__USER_DS		 
	pushl	$0			 
	pushfl				 
	pushl	$__USER_CS		 
	pushl	$0			 
	pushl	%eax			 
	SAVE_ALL pt_regs_ax=$-ENOSYS	 

	 
	testl	$X86_EFLAGS_NT|X86_EFLAGS_AC|X86_EFLAGS_TF, PT_EFLAGS(%esp)
	jnz	.Lsysenter_fix_flags
.Lsysenter_flags_fixed:

	movl	%esp, %eax
	call	do_SYSENTER_32
	testl	%eax, %eax
	jz	.Lsyscall_32_done

	STACKLEAK_ERASE

	 

	 

	 
	movl	PER_CPU_VAR(cpu_tss_rw + TSS_sp0), %eax
	subl	$(2*4), %eax

	 
	movl	PT_EFLAGS(%esp), %edi
	movl	PT_EAX(%esp), %esi
	movl	%edi, (%eax)
	movl	%esi, 4(%eax)

	 
	movl	PT_EIP(%esp), %edx	 
	movl	PT_OLDESP(%esp), %ecx	 
1:	mov	PT_FS(%esp), %fs

	popl	%ebx			 
	addl	$2*4, %esp		 
	popl	%esi			 
	popl	%edi			 
	popl	%ebp			 

	 
	movl	%eax, %esp

	 
	SWITCH_TO_USER_CR3 scratch_reg=%eax

	 
	btrl	$X86_EFLAGS_IF_BIT, (%esp)
	BUG_IF_WRONG_CR3 no_user_check=1
	popfl
	popl	%eax

	 
	sti
	sysexit

2:	movl    $0, PT_FS(%esp)
	jmp     1b
	_ASM_EXTABLE(1b, 2b)

.Lsysenter_fix_flags:
	pushl	$X86_EFLAGS_FIXED
	popfl
	jmp	.Lsysenter_flags_fixed
SYM_ENTRY(__end_SYSENTER_singlestep_region, SYM_L_GLOBAL, SYM_A_NONE)
SYM_FUNC_END(entry_SYSENTER_32)

 
SYM_FUNC_START(entry_INT80_32)
	ASM_CLAC
	pushl	%eax			 

	SAVE_ALL pt_regs_ax=$-ENOSYS switch_stacks=1	 

	movl	%esp, %eax
	call	do_int80_syscall_32
.Lsyscall_32_done:
	STACKLEAK_ERASE

restore_all_switch_stack:
	SWITCH_TO_ENTRY_STACK
	CHECK_AND_APPLY_ESPFIX

	 
	SWITCH_TO_USER_CR3 scratch_reg=%eax

	BUG_IF_WRONG_CR3

	 
	RESTORE_REGS pop=4			# skip orig_eax/error_code
.Lirq_return:
	 
	iret

.Lasm_iret_error:
	pushl	$0				# no error code
	pushl	$iret_error

#ifdef CONFIG_DEBUG_ENTRY
	 
	pushl	%eax
	SWITCH_TO_USER_CR3 scratch_reg=%eax
	popl	%eax
#endif

	jmp	handle_exception

	_ASM_EXTABLE(.Lirq_return, .Lasm_iret_error)
SYM_FUNC_END(entry_INT80_32)

.macro FIXUP_ESPFIX_STACK
 
#ifdef CONFIG_X86_ESPFIX32
	 
	pushl	%ecx
	subl	$2*4, %esp
	sgdt	(%esp)
	movl	2(%esp), %ecx				 
	 
	mov	%cs:GDT_ESPFIX_OFFSET + 4(%ecx), %al	 
	mov	%cs:GDT_ESPFIX_OFFSET + 7(%ecx), %ah	 
	shl	$16, %eax
	addl	$2*4, %esp
	popl	%ecx
	addl	%esp, %eax			 
	pushl	$__KERNEL_DS
	pushl	%eax
	lss	(%esp), %esp			 
#endif
.endm

.macro UNWIND_ESPFIX_STACK
	 
#ifdef CONFIG_X86_ESPFIX32
	movl	%ss, %eax
	 
	cmpw	$__ESPFIX_SS, %ax
	jne	.Lno_fixup_\@
	 
	FIXUP_ESPFIX_STACK
.Lno_fixup_\@:
#endif
.endm

SYM_CODE_START_LOCAL_NOALIGN(handle_exception)
	 
	SAVE_ALL switch_stacks=1 skip_gs=1 unwind_espfix=1
	ENCODE_FRAME_POINTER

	movl	PT_GS(%esp), %edi		# get the function address

	 
	movl	PT_ORIG_EAX(%esp), %edx		# get the error code
	movl	$-1, PT_ORIG_EAX(%esp)		# no syscall to restart

	movl	%esp, %eax			# pt_regs pointer
	CALL_NOSPEC edi

handle_exception_return:
#ifdef CONFIG_VM86
	movl	PT_EFLAGS(%esp), %eax		# mix EFLAGS and CS
	movb	PT_CS(%esp), %al
	andl	$(X86_EFLAGS_VM | SEGMENT_RPL_MASK), %eax
#else
	 
	movl	PT_CS(%esp), %eax
	andl	$SEGMENT_RPL_MASK, %eax
#endif
	cmpl	$USER_RPL, %eax			# returning to v8086 or userspace ?
	jnb	ret_to_user

	PARANOID_EXIT_TO_KERNEL_MODE
	BUG_IF_WRONG_CR3
	RESTORE_REGS 4
	jmp	.Lirq_return

ret_to_user:
	movl	%esp, %eax
	jmp	restore_all_switch_stack
SYM_CODE_END(handle_exception)

SYM_CODE_START(asm_exc_double_fault)
1:
	 

	clts				 
	pushl	$X86_EFLAGS_FIXED
	popfl				 

	call	doublefault_shim

	 
1:
	hlt
	jmp 1b
SYM_CODE_END(asm_exc_double_fault)

 
SYM_CODE_START(asm_exc_nmi)
	ASM_CLAC

#ifdef CONFIG_X86_ESPFIX32
	 
	pushl	%eax
	movl	%ss, %eax
	cmpw	$__ESPFIX_SS, %ax
	popl	%eax
	je	.Lnmi_espfix_stack
#endif

	pushl	%eax				# pt_regs->orig_ax
	SAVE_ALL_NMI cr3_reg=%edi
	ENCODE_FRAME_POINTER
	xorl	%edx, %edx			# zero error code
	movl	%esp, %eax			# pt_regs pointer

	 
	movl	PER_CPU_VAR(cpu_entry_area), %ecx
	addl	$CPU_ENTRY_AREA_entry_stack + SIZEOF_entry_stack, %ecx
	subl	%eax, %ecx	 
	cmpl	$SIZEOF_entry_stack, %ecx
	jb	.Lnmi_from_sysenter_stack

	 
	call	exc_nmi
	jmp	.Lnmi_return

.Lnmi_from_sysenter_stack:
	 
	movl	%esp, %ebx
	movl	PER_CPU_VAR(pcpu_hot + X86_top_of_stack), %esp
	call	exc_nmi
	movl	%ebx, %esp

.Lnmi_return:
#ifdef CONFIG_X86_ESPFIX32
	testl	$CS_FROM_ESPFIX, PT_CS(%esp)
	jnz	.Lnmi_from_espfix
#endif

	CHECK_AND_APPLY_ESPFIX
	RESTORE_ALL_NMI cr3_reg=%edi pop=4
	jmp	.Lirq_return

#ifdef CONFIG_X86_ESPFIX32
.Lnmi_espfix_stack:
	 
	pushl	%ss
	pushl	%esp
	addl	$4, (%esp)

	 
	pushl	4*4(%esp)	# flags
	pushl	4*4(%esp)	# cs
	pushl	4*4(%esp)	# ip

	pushl	%eax		# orig_ax

	SAVE_ALL_NMI cr3_reg=%edi unwind_espfix=1
	ENCODE_FRAME_POINTER

	 
	xorl	$(CS_FROM_ESPFIX | CS_FROM_KERNEL), PT_CS(%esp)

	xorl	%edx, %edx			# zero error code
	movl	%esp, %eax			# pt_regs pointer
	jmp	.Lnmi_from_sysenter_stack

.Lnmi_from_espfix:
	RESTORE_ALL_NMI cr3_reg=%edi
	 
	lss	(1+5+6)*4(%esp), %esp			# back to espfix stack
	jmp	.Lirq_return
#endif
SYM_CODE_END(asm_exc_nmi)

.pushsection .text, "ax"
SYM_CODE_START(rewind_stack_and_make_dead)
	 
	xorl	%ebp, %ebp

	movl	PER_CPU_VAR(pcpu_hot + X86_top_of_stack), %esi
	leal	-TOP_OF_KERNEL_STACK_PADDING-PTREGS_SIZE(%esi), %esp

	call	make_task_dead
1:	jmp 1b
SYM_CODE_END(rewind_stack_and_make_dead)
.popsection
