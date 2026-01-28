


	.text
	.code32

	.global call64_from_32
	.type call32_from_64, @function

	
call64_from_32:
	
	mov	4(%esp), %eax

	
	push	%ecx
	push	%edx
	push	%esi
	push	%edi

	
	jmp	$0x33,$1f
1:	.code64

	
	call	*%rax

	
	push	$0x23  
	.code32; push $1f; .code64 
	lretq
1:	.code32

	pop	%edi
	pop	%esi
	pop	%edx
	pop	%ecx

	ret

.size call64_from_32, .-call64_from_32

.section .note.GNU-stack,"",%progbits
