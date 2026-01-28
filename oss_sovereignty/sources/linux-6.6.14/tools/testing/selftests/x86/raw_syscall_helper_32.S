
.global sys32_helper
sys32_helper:
	
	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	5*4(%esp), %eax	

	movl	1*4(%eax), %ebx
	movl	2*4(%eax), %ecx
	movl	3*4(%eax), %edx
	movl	4*4(%eax), %esi
	movl	5*4(%eax), %edi
	movl	6*4(%eax), %ebp
	movl	0*4(%eax), %eax

	call	*(6*4)(%esp)	

	
	pushl	%eax
	movl	6*4(%esp), %eax
	popl	0*4(%eax)
	movl	%ebx, 1*4(%eax)
	movl	%ecx, 2*4(%eax)
	movl	%edx, 3*4(%eax)
	movl	%esi, 4*4(%eax)
	movl	%edi, 5*4(%eax)
	movl	%ebp, 6*4(%eax)

	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret

	.type sys32_helper, @function
	.size sys32_helper, .-sys32_helper

.global int80_and_ret
int80_and_ret:
	int	$0x80
	ret

	.type int80_and_ret, @function
	.size int80_and_ret, .-int80_and_ret

.section .note.GNU-stack,"",%progbits
