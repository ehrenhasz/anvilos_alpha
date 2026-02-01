 
 

#include <linux/linkage.h>
#include <asm/msr.h>
#include <asm/page_types.h>
#include <asm/processor-flags.h>
#include <asm/segment.h>

	.code64
	.text
 
SYM_FUNC_START(startup_64_mixed_mode)
	lea	efi32_boot_args(%rip), %rdx
	mov	0(%rdx), %edi
	mov	4(%rdx), %esi
#ifdef CONFIG_EFI_HANDOVER_PROTOCOL
	mov	8(%rdx), %edx		 
	test	%edx, %edx
	jnz	efi_stub_entry
#endif
	 
	sub	$40, %rsp
	mov	%rdi, %rcx		 
	mov	%rsi, %rdx
	jmp	efi_pe_entry
SYM_FUNC_END(startup_64_mixed_mode)

SYM_FUNC_START(__efi64_thunk)
	push	%rbp
	push	%rbx

	movl	%ds, %eax
	push	%rax
	movl	%es, %eax
	push	%rax
	movl	%ss, %eax
	push	%rax

	 
	movq	0x30(%rsp), %rbp
	movq	0x38(%rsp), %rbx
	movq	0x40(%rsp), %rax

	 
	subq	$64, %rsp
	movl	%esi, 0x0(%rsp)
	movl	%edx, 0x4(%rsp)
	movl	%ecx, 0x8(%rsp)
	movl	%r8d, 0xc(%rsp)
	movl	%r9d, 0x10(%rsp)
	movl	%ebp, 0x14(%rsp)
	movl	%ebx, 0x18(%rsp)
	movl	%eax, 0x1c(%rsp)

	leaq	0x20(%rsp), %rbx
	sgdt	(%rbx)
	sidt	16(%rbx)

	leaq	1f(%rip), %rbp

	 
	lidt	efi32_boot_idt(%rip)
	lgdt	efi32_boot_gdt(%rip)

	movzwl	efi32_boot_ds(%rip), %edx
	movzwq	efi32_boot_cs(%rip), %rax
	pushq	%rax
	leaq	efi_enter32(%rip), %rax
	pushq	%rax
	lretq

1:	addq	$64, %rsp
	movq	%rdi, %rax

	pop	%rbx
	movl	%ebx, %ss
	pop	%rbx
	movl	%ebx, %es
	pop	%rbx
	movl	%ebx, %ds
	 
	xorl	%ebx, %ebx
	movl	%ebx, %fs
	movl	%ebx, %gs

	pop	%rbx
	pop	%rbp
	RET
SYM_FUNC_END(__efi64_thunk)

	.code32
#ifdef CONFIG_EFI_HANDOVER_PROTOCOL
SYM_FUNC_START(efi32_stub_entry)
	call	1f
1:	popl	%ecx

	 
	xorl	%eax, %eax
	leal	(_bss - 1b)(%ecx), %edi
	leal	(_ebss - 1b)(%ecx), %ecx
	subl	%edi, %ecx
	shrl	$2, %ecx
	cld
	rep	stosl

	add	$0x4, %esp		 
	popl	%ecx
	popl	%edx
	popl	%esi
	jmp	efi32_entry
SYM_FUNC_END(efi32_stub_entry)
#endif

 
SYM_FUNC_START_LOCAL(efi_enter32)
	 
	movl	%edx, %ds
	movl	%edx, %es
	movl	%edx, %fs
	movl	%edx, %gs
	movl	%edx, %ss

	 
	movl	%cr3, %eax
	movl	%eax, %cr3

	 
	movl	%cr0, %eax
	btrl	$X86_CR0_PG_BIT, %eax
	movl	%eax, %cr0

	 
	movl	$MSR_EFER, %ecx
	rdmsr
	btrl	$_EFER_LME, %eax
	wrmsr

	call	*%edi

	 
	movl	%eax, %edi

	 
	cli

	lidtl	16(%ebx)
	lgdtl	(%ebx)

	movl	%cr4, %eax
	btsl	$(X86_CR4_PAE_BIT), %eax
	movl	%eax, %cr4

	movl	%cr3, %eax
	movl	%eax, %cr3

	movl	$MSR_EFER, %ecx
	rdmsr
	btsl	$_EFER_LME, %eax
	wrmsr

	xorl	%eax, %eax
	lldt	%ax

	pushl	$__KERNEL_CS
	pushl	%ebp

	 
	movl	%cr0, %eax
	btsl	$X86_CR0_PG_BIT, %eax
	movl	%eax, %cr0
	lret
SYM_FUNC_END(efi_enter32)

 
SYM_FUNC_START_LOCAL(efi32_entry)
	call	1f
1:	pop	%ebx

	 
	sgdtl	(efi32_boot_gdt - 1b)(%ebx)
	movw	%cs, (efi32_boot_cs - 1b)(%ebx)
	movw	%ds, (efi32_boot_ds - 1b)(%ebx)

	 
	sidtl	(efi32_boot_idt - 1b)(%ebx)

	 
	leal	(efi32_boot_args - 1b)(%ebx), %ebx
	movl	%ecx, 0(%ebx)
	movl	%edx, 4(%ebx)
	movl	%esi, 8(%ebx)
	movb	$0x0, 12(%ebx)           

	 
	movl	%cr0, %eax
	btrl	$X86_CR0_PG_BIT, %eax
	movl	%eax, %cr0

	jmp	startup_32
SYM_FUNC_END(efi32_entry)

 
SYM_FUNC_START(efi32_pe_entry)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx				 
	pushl	%edi

	call	verify_cpu			 
	testl	%eax, %eax
	movl	$0x80000003, %eax		 
	jnz	2f

	movl	8(%ebp), %ecx			 
	movl	12(%ebp), %edx			 
	xorl	%esi, %esi
	jmp	efi32_entry			 
						 

2:	popl	%edi				 
	popl	%ebx
	leave
	RET
SYM_FUNC_END(efi32_pe_entry)

#ifdef CONFIG_EFI_HANDOVER_PROTOCOL
	.org	efi32_stub_entry + 0x200
	.code64
SYM_FUNC_START_NOALIGN(efi64_stub_entry)
	jmp	efi_handover_entry
SYM_FUNC_END(efi64_stub_entry)
#endif

	.data
	.balign	8
SYM_DATA_START_LOCAL(efi32_boot_gdt)
	.word	0
	.quad	0
SYM_DATA_END(efi32_boot_gdt)

SYM_DATA_START_LOCAL(efi32_boot_idt)
	.word	0
	.quad	0
SYM_DATA_END(efi32_boot_idt)

SYM_DATA_LOCAL(efi32_boot_cs, .word 0)
SYM_DATA_LOCAL(efi32_boot_ds, .word 0)
SYM_DATA_LOCAL(efi32_boot_args, .long 0, 0, 0)
SYM_DATA(efi_is64, .byte 1)
