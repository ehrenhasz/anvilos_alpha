


#include <linux/linkage.h>
#include <asm/pgtable_types.h>
#include <asm/page_types.h>
#include <asm/msr.h>
#include <asm/segment.h>
#include <asm/processor-flags.h>
#include <asm/realmode.h>
#include "realmode.h"

	.text
	.code16

.macro LOCK_AND_LOAD_REALMODE_ESP lock_pa=0
	
.Llock_rm\@:
	.if \lock_pa
        lock btsl       $0, pa_tr_lock
	.else
        lock btsl       $0, tr_lock
	.endif
        jnc             2f
        pause
        jmp             .Llock_rm\@
2:
	# Setup stack
	movl	$rm_stack_end, %esp
.endm

	.balign	PAGE_SIZE
SYM_CODE_START(trampoline_start)
	cli			# We should be safe anyway
	wbinvd

	LJMPW_RM(1f)
1:
	mov	%cs, %ax	# Code and data in the same place
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %ss

	LOCK_AND_LOAD_REALMODE_ESP

	call	verify_cpu		# Verify the cpu supports long mode
	testl   %eax, %eax		# Check for return code
	jnz	no_longmode

.Lswitch_to_protected:
	

	lidtl	tr_idt	# load idt with 0, 0
	lgdtl	tr_gdt	# load gdt with whatever is appropriate

	movw	$__KERNEL_DS, %dx	# Data segment descriptor

	# Enable protected mode
	movl	$(CR0_STATE & ~X86_CR0_PG), %eax
	movl	%eax, %cr0		# into protected mode

	# flush prefetch and jump to startup_32
	ljmpl	$__KERNEL32_CS, $pa_startup_32

no_longmode:
	hlt
	jmp no_longmode
SYM_CODE_END(trampoline_start)

#ifdef CONFIG_AMD_MEM_ENCRYPT

SYM_CODE_START(sev_es_trampoline_start)
	cli			# We should be safe anyway

	LJMPW_RM(1f)
1:
	mov	%cs, %ax	# Code and data in the same place
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %ss

	LOCK_AND_LOAD_REALMODE_ESP

	jmp	.Lswitch_to_protected
SYM_CODE_END(sev_es_trampoline_start)
#endif	

#include "../kernel/verify_cpu.S"

	.section ".text32","ax"
	.code32
	.balign 4
SYM_CODE_START(startup_32)
	movl	%edx, %ss
	addl	$pa_real_mode_base, %esp
	movl	%edx, %ds
	movl	%edx, %es
	movl	%edx, %fs
	movl	%edx, %gs

	
	btl	$TH_FLAGS_SME_ACTIVE_BIT, pa_tr_flags
	jnc	.Ldone
	movl	$MSR_AMD64_SYSCFG, %ecx
	rdmsr
	bts	$MSR_AMD64_SYSCFG_MEM_ENCRYPT_BIT, %eax
	jc	.Ldone

	
	wrmsr
.Ldone:

	movl	pa_tr_cr4, %eax
	movl	%eax, %cr4		# Enable PAE mode

	# Setup trampoline 4 level pagetables
	movl	$pa_trampoline_pgd, %eax
	movl	%eax, %cr3

	# Set up EFER
	movl	$MSR_EFER, %ecx
	rdmsr
	
	cmp	pa_tr_efer, %eax
	jne	.Lwrite_efer
	cmp	pa_tr_efer + 4, %edx
	je	.Ldone_efer
.Lwrite_efer:
	movl	pa_tr_efer, %eax
	movl	pa_tr_efer + 4, %edx
	wrmsr

.Ldone_efer:
	# Enable paging and in turn activate Long Mode.
	movl	$CR0_STATE, %eax
	movl	%eax, %cr0

	
	ljmpl	$__KERNEL_CS, $pa_startup_64
SYM_CODE_END(startup_32)

SYM_CODE_START(pa_trampoline_compat)
	
	LOCK_AND_LOAD_REALMODE_ESP lock_pa=1
	movw	$__KERNEL_DS, %dx

	movl	$(CR0_STATE & ~X86_CR0_PG), %eax
	movl	%eax, %cr0
	ljmpl   $__KERNEL32_CS, $pa_startup_32
SYM_CODE_END(pa_trampoline_compat)

	.section ".text64","ax"
	.code64
	.balign 4
SYM_CODE_START(startup_64)
	# Now jump into the kernel using virtual addresses
	jmpq	*tr_start(%rip)
SYM_CODE_END(startup_64)

SYM_CODE_START(trampoline_start64)
	
	lidt	tr_idt(%rip)
	lgdt	tr_gdt64(%rip)

	ljmpl	*tr_compat(%rip)
SYM_CODE_END(trampoline_start64)

	.section ".rodata","a"
	# Duplicate the global descriptor table
	# so the kernel can live anywhere
	.balign	16
SYM_DATA_START(tr_gdt)
	.short	tr_gdt_end - tr_gdt - 1	# gdt limit
	.long	pa_tr_gdt
	.short	0
	.quad	0x00cf9b000000ffff	# __KERNEL32_CS
	.quad	0x00af9b000000ffff	# __KERNEL_CS
	.quad	0x00cf93000000ffff	# __KERNEL_DS
SYM_DATA_END_LABEL(tr_gdt, SYM_L_LOCAL, tr_gdt_end)

SYM_DATA_START(tr_gdt64)
	.short	tr_gdt_end - tr_gdt - 1	# gdt limit
	.long	pa_tr_gdt
	.long	0
SYM_DATA_END(tr_gdt64)

SYM_DATA_START(tr_compat)
	.long	pa_trampoline_compat
	.short	__KERNEL32_CS
SYM_DATA_END(tr_compat)

	.bss
	.balign	PAGE_SIZE
SYM_DATA(trampoline_pgd, .space PAGE_SIZE)

	.balign	8
SYM_DATA_START(trampoline_header)
	SYM_DATA_LOCAL(tr_start,	.space 8)
	SYM_DATA(tr_efer,		.space 8)
	SYM_DATA(tr_cr4,		.space 4)
	SYM_DATA(tr_flags,		.space 4)
	SYM_DATA(tr_lock,		.space 4)
SYM_DATA_END(trampoline_header)

#include "trampoline_common.S"
