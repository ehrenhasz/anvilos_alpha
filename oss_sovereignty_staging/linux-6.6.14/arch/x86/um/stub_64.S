 
#include <as-layout.h>

.section .__syscall_stub, "ax"
	.globl batch_syscall_stub
batch_syscall_stub:
	 
	mov	%rsp, %rbx
	add	$0x10, %rsp
again:
	 
	mov	0x0(%rsp), %rax

	 
	 
	mov	%rax, 8(%rbx)
	cmp	$0, %rax
	jz	done

	 
	mov	%rsp, 8(%rbx)

	 
	add	%rax, %rsp

	 
	pop	%rax

	 
	pop	%rdi
	pop	%rsi
	pop	%rdx
	pop	%r10
 	pop	%r8
	pop	%r9

	 
	syscall

	 
	pop	%rcx
	cmp	%rcx, %rax
	je	again

done:
	 
	mov	%rax, (%rbx)

	 
	int3
