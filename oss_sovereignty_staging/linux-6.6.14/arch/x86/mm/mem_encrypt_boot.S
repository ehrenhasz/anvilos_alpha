 
 

#include <linux/linkage.h>
#include <linux/pgtable.h>
#include <asm/page.h>
#include <asm/processor-flags.h>
#include <asm/msr-index.h>
#include <asm/nospec-branch.h>

	.text
	.code64
SYM_FUNC_START(sme_encrypt_execute)

	 

	push	%rbp
	movq	%rsp, %rbp		 

	 
	movq	%rcx, %rax		 
	leaq	PAGE_SIZE(%rax), %rsp	 
	addq	$PAGE_SIZE, %rax	 

	push	%r12
	movq	%rdi, %r10		 
	movq	%rsi, %r11		 
	movq	%rdx, %r12		 

	 
	movq	%rax, %rdi				 
	leaq	__enc_copy(%rip), %rsi			 
	movq	$(.L__enc_copy_end - __enc_copy), %rcx	 
	rep	movsb

	 
	movq	%r10, %rdi		 
	movq	%r11, %rsi		 
	movq	%r8, %rdx		 
	movq	%r12, %rcx		 
	movq	%rax, %r8		 
	addq	$PAGE_SIZE, %r8		 

	ANNOTATE_RETPOLINE_SAFE
	call	*%rax			 

	pop	%r12

	movq	%rbp, %rsp		 
	pop	%rbp

	 
	ANNOTATE_UNRET_SAFE
	ret
	int3
SYM_FUNC_END(sme_encrypt_execute)

SYM_FUNC_START(__enc_copy)
 
	 
	mov	%rdx, %cr3

	 
	mov	%cr4, %rdx
	andq	$~X86_CR4_PGE, %rdx
	mov	%rdx, %cr4
	orq	$X86_CR4_PGE, %rdx
	mov	%rdx, %cr4

	push	%r15
	push	%r12

	movq	%rcx, %r9		 
	movq	%rdi, %r10		 
	movq	%rsi, %r11		 

	 
	movl	$MSR_IA32_CR_PAT, %ecx
	rdmsr
	mov	%rdx, %r15		 
	andl	$0xffff00ff, %edx	 
	orl	$0x00000500, %edx	 
	wrmsr

	wbinvd				 

	 
	movq	$PMD_SIZE, %r12
1:
	cmpq	%r12, %r9
	jnb	2f
	movq	%r9, %r12

2:
	movq	%r11, %rsi		 
	movq	%r8, %rdi		 
	movq	%r12, %rcx
	rep	movsb

	movq	%r8, %rsi		 
	movq	%r10, %rdi		 
	movq	%r12, %rcx
	rep	movsb

	addq	%r12, %r11
	addq	%r12, %r10
	subq	%r12, %r9		 
	jnz	1b			 

	 
	movl	$MSR_IA32_CR_PAT, %ecx
	rdmsr
	mov	%r15, %rdx		 
	wrmsr

	pop	%r12
	pop	%r15

	 
	ANNOTATE_UNRET_SAFE
	ret
	int3
.L__enc_copy_end:
SYM_FUNC_END(__enc_copy)
