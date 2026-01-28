


	.text

	.global call32_from_64
	.type call32_from_64, @function
call32_from_64:
	
	

	
	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	pushfq

	
	mov %rsp,(%rdi)
	mov %rdi,%rsp

	
	pushq $0x23  
	pushq $1f
	lretq

1:
	.code32
	
	call *%esi
	
	jmp $0x33,$1f
	.code64

1:
	
	mov (%rsp),%rsp

	
	popfq
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx

	ret

.size call32_from_64, .-call32_from_64

.section .note.GNU-stack,"",%progbits
