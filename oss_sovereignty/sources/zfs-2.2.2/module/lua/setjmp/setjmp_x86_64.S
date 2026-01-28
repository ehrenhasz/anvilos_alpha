



#if defined(_KERNEL) && defined(__linux__)
#include <linux/linkage.h>
#endif


#ifdef __x86_64__

#define _ASM
#include <sys/asm_linkage.h>

ENTRY_ALIGN(setjmp, 8)
	movq	%rsp, 0(%rdi)
	movq	%rbp, 8(%rdi)
	movq	%rbx, 16(%rdi)
	movq	%r12, 24(%rdi)
	movq	%r13, 32(%rdi)
	movq	%r14, 40(%rdi)
	movq	%r15, 48(%rdi)
	movq	0(%rsp), %rdx		
	movq	%rdx, 56(%rdi)		
	xorl	%eax, %eax		
	RET
	SET_SIZE(setjmp)

ENTRY_ALIGN(longjmp, 8)
	movq	0(%rdi), %rsp
	movq	8(%rdi), %rbp
	movq	16(%rdi), %rbx
	movq	24(%rdi), %r12
	movq	32(%rdi), %r13
	movq	40(%rdi), %r14
	movq	48(%rdi), %r15
	movq	56(%rdi), %rdx		
	movq	%rdx, 0(%rsp)
	xorl	%eax, %eax
	incl	%eax			
	RET
	SET_SIZE(longjmp)

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif

#endif 
