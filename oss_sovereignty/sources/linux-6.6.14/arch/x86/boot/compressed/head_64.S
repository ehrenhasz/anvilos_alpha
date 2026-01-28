





	.code32
	.text

#include <linux/init.h>
#include <linux/linkage.h>
#include <asm/segment.h>
#include <asm/boot.h>
#include <asm/msr.h>
#include <asm/processor-flags.h>
#include <asm/asm-offsets.h>
#include <asm/bootparam.h>
#include <asm/desc_defs.h>
#include <asm/trapnr.h>
#include "pgtable.h"


#undef __ALIGN
#define __ALIGN		.balign	16, 0x90


	.hidden _bss
	.hidden _ebss
	.hidden _end

	__HEAD


#define rva(X) ((X) - startup_32)

	.code32
SYM_FUNC_START(startup_32)
	
	cld
	cli


	leal	(BP_scratch+4)(%esi), %esp
	call	1f
1:	popl	%ebp
	subl	$ rva(1b), %ebp

	
	leal	rva(gdt)(%ebp), %eax
	movl	%eax, 2(%eax)
	lgdt	(%eax)

	
	movl	$__BOOT_DS, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %fs
	movl	%eax, %gs
	movl	%eax, %ss

	
	leal	rva(boot_stack_end)(%ebp), %esp

	pushl	$__KERNEL32_CS
	leal	rva(1f)(%ebp), %eax
	pushl	%eax
	lretl
1:

	
#ifdef CONFIG_AMD_MEM_ENCRYPT
	call	startup32_load_idt
#endif

	
	call	verify_cpu
	testl	%eax, %eax
	jnz	.Lno_longmode



#ifdef CONFIG_RELOCATABLE
	movl	%ebp, %ebx
	movl	BP_kernel_alignment(%esi), %eax
	decl	%eax
	addl	%eax, %ebx
	notl	%eax
	andl	%eax, %ebx
	cmpl	$LOAD_PHYSICAL_ADDR, %ebx
	jae	1f
#endif
	movl	$LOAD_PHYSICAL_ADDR, %ebx
1:

	
	addl	BP_init_size(%esi), %ebx
	subl	$ rva(_end), %ebx



	
	movl	%cr4, %eax
	orl	$X86_CR4_PAE, %eax
	movl	%eax, %cr4

 
	
	xorl	%edx, %edx
#ifdef	CONFIG_AMD_MEM_ENCRYPT
	call	get_sev_encryption_bit
	xorl	%edx, %edx
	testl	%eax, %eax
	jz	1f
	subl	$32, %eax	
	bts	%eax, %edx	
	
	movl	$1, rva(sev_status)(%ebp)
1:
#endif

	
	leal	rva(pgtable)(%ebx), %edi
	xorl	%eax, %eax
	movl	$(BOOT_INIT_PGT_SIZE/4), %ecx
	rep	stosl

	
	leal	rva(pgtable + 0)(%ebx), %edi
	leal	0x1007 (%edi), %eax
	movl	%eax, 0(%edi)
	addl	%edx, 4(%edi)

	
	leal	rva(pgtable + 0x1000)(%ebx), %edi
	leal	0x1007(%edi), %eax
	movl	$4, %ecx
1:	movl	%eax, 0x00(%edi)
	addl	%edx, 0x04(%edi)
	addl	$0x00001000, %eax
	addl	$8, %edi
	decl	%ecx
	jnz	1b

	
	leal	rva(pgtable + 0x2000)(%ebx), %edi
	movl	$0x00000183, %eax
	movl	$2048, %ecx
1:	movl	%eax, 0(%edi)
	addl	%edx, 4(%edi)
	addl	$0x00200000, %eax
	addl	$8, %edi
	decl	%ecx
	jnz	1b

	
	leal	rva(pgtable)(%ebx), %eax
	movl	%eax, %cr3

	
	movl	$MSR_EFER, %ecx
	rdmsr
	btsl	$_EFER_LME, %eax
	wrmsr

	
	xorl	%eax, %eax
	lldt	%ax
	movl    $__BOOT_TSS, %eax
	ltr	%ax

#ifdef CONFIG_AMD_MEM_ENCRYPT
	
	call	startup32_check_sev_cbit
#endif

	
	leal	rva(startup_64)(%ebp), %eax
#ifdef CONFIG_EFI_MIXED
	cmpb	$1, rva(efi_is64)(%ebp)
	je	1f
	leal	rva(startup_64_mixed_mode)(%ebp), %eax
1:
#endif

	pushl	$__KERNEL_CS
	pushl	%eax

	
	movl	$CR0_STATE, %eax
	movl	%eax, %cr0

	
	lret
SYM_FUNC_END(startup_32)

	.code64
	.org 0x200
SYM_CODE_START(startup_64)
	

	cld
	cli

	
	xorl	%eax, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %ss
	movl	%eax, %fs
	movl	%eax, %gs

	

	
#ifdef CONFIG_RELOCATABLE
	leaq	startup_32(%rip) , %rbp
	movl	BP_kernel_alignment(%rsi), %eax
	decl	%eax
	addq	%rax, %rbp
	notq	%rax
	andq	%rax, %rbp
	cmpq	$LOAD_PHYSICAL_ADDR, %rbp
	jae	1f
#endif
	movq	$LOAD_PHYSICAL_ADDR, %rbp
1:

	
	movl	BP_init_size(%rsi), %ebx
	subl	$ rva(_end), %ebx
	addq	%rbp, %rbx

	
	leaq	rva(boot_stack_end)(%rbx), %rsp

	

	
	leaq	gdt64(%rip), %rax
	addq	%rax, 2(%rax)
	lgdt	(%rax)

	
	pushq	$__KERNEL_CS
	leaq	.Lon_kernel_cs(%rip), %rax
	pushq	%rax
	lretq

.Lon_kernel_cs:
	
	movq	%rsi, %r15

	call	load_stage1_idt

#ifdef CONFIG_AMD_MEM_ENCRYPT
	
	movq	%r15, %rdi
	call	sev_enable
#endif

	
	movq	%r15, %rdi
	leaq	rva(top_pgtable)(%rbx), %rsi
	call	configure_5level_paging

	
	pushq	$0
	popfq


	leaq	(_bss-8)(%rip), %rsi
	leaq	rva(_bss-8)(%rbx), %rdi
	movl	$(_bss - startup_32), %ecx
	shrl	$3, %ecx
	std
	rep	movsq
	cld

	
	leaq	rva(gdt64)(%rbx), %rax
	leaq	rva(gdt)(%rbx), %rdx
	movq	%rdx, 2(%rax)
	lgdt	(%rax)


	leaq	rva(.Lrelocated)(%rbx), %rax
	jmp	*%rax
SYM_CODE_END(startup_64)

	.text
SYM_FUNC_START_LOCAL_NOALIGN(.Lrelocated)


	xorl	%eax, %eax
	leaq    _bss(%rip), %rdi
	leaq    _ebss(%rip), %rcx
	subq	%rdi, %rcx
	shrq	$3, %rcx
	rep	stosq

	call	load_stage2_idt

	
	movq	%r15, %rdi
	call	initialize_identity_maps


	
	movq	%r15, %rdi
	movq	%rbp, %rsi
	call	extract_kernel		


	movq	%r15, %rsi
	jmp	*%rax
SYM_FUNC_END(.Lrelocated)


	.section ".rodata", "a", @progbits
SYM_CODE_START(trampoline_32bit_src)
	
	pushq	%r15
	pushq	%r14
	pushq	%r13
	pushq	%r12
	pushq	%rbp
	pushq	%rbx

	
	movq	%rsp, %rbx
	shrq	$32, %rbx

	
	pushq	$__KERNEL32_CS
	leaq	0f(%rip), %rax
	pushq	%rax
	lretq

	
.Lret:
	shlq	$32, %rbx
	orq	%rbx, %rsp

	
	popq	%rbx
	popq	%rbp
	popq	%r12
	popq	%r13
	popq	%r14
	popq	%r15
	retq

	.code32
0:
	
	movl	%cr0, %eax
	btrl	$X86_CR0_PG_BIT, %eax
	movl	%eax, %cr0

	
	movl	%edi, %cr3

	
	movl	$MSR_EFER, %ecx
	rdmsr
	btsl	$_EFER_LME, %eax
	
	jc	1f
	wrmsr
1:
	
	movl	%cr4, %eax
	btcl	$X86_CR4_LA57_BIT, %eax
	movl	%eax, %cr4

	
	movl	%cr0, %eax
	btsl	$X86_CR0_PG_BIT, %eax
	movl	%eax, %cr0

	
.Ljmp:	ljmpl	$__KERNEL_CS, $(.Lret - trampoline_32bit_src)
SYM_CODE_END(trampoline_32bit_src)


SYM_DATA(trampoline_ljmp_imm_offset, .word  .Ljmp + 1 - trampoline_32bit_src)

	
	.org	trampoline_32bit_src + TRAMPOLINE_32BIT_CODE_SIZE

	.text
SYM_FUNC_START_LOCAL_NOALIGN(.Lno_longmode)
	
1:
	hlt
	jmp     1b
SYM_FUNC_END(.Lno_longmode)

	.globl	verify_cpu
#include "../../kernel/verify_cpu.S"

	.data
SYM_DATA_START_LOCAL(gdt64)
	.word	gdt_end - gdt - 1
	.quad   gdt - gdt64
SYM_DATA_END(gdt64)
	.balign	8
SYM_DATA_START_LOCAL(gdt)
	.word	gdt_end - gdt - 1
	.long	0
	.word	0
	.quad	0x00cf9a000000ffff	
	.quad	0x00af9a000000ffff	
	.quad	0x00cf92000000ffff	
	.quad	0x0080890000000000	
	.quad   0x0000000000000000	
SYM_DATA_END_LABEL(gdt, SYM_L_LOCAL, gdt_end)

SYM_DATA_START(boot_idt_desc)
	.word	boot_idt_end - boot_idt - 1
	.quad	0
SYM_DATA_END(boot_idt_desc)
	.balign 8
SYM_DATA_START(boot_idt)
	.rept	BOOT_IDT_ENTRIES
	.quad	0
	.quad	0
	.endr
SYM_DATA_END_LABEL(boot_idt, SYM_L_GLOBAL, boot_idt_end)


	.bss
	.balign 4
SYM_DATA_START_LOCAL(boot_stack)
	.fill BOOT_STACK_SIZE, 1, 0
	.balign 16
SYM_DATA_END_LABEL(boot_stack, SYM_L_LOCAL, boot_stack_end)


	.section ".pgtable","aw",@nobits
	.balign 4096
SYM_DATA_LOCAL(pgtable,		.fill BOOT_PGT_SIZE, 1, 0)


SYM_DATA_LOCAL(top_pgtable,	.fill PAGE_SIZE, 1, 0)
