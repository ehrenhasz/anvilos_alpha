



#define	ENTRY(x) \
	.text; \
	.balign	8; \
	.globl	x; \
	.type	x, @function; \
x:

#define	SET_SIZE(x) \
	.size	x, [.-x]


#ifdef __i386__

	ENTRY(setjmp)			
	movl	%ebp, 0(%eax)		
	movl	%ebx, 4(%eax)		
	movl	%esi, 8(%eax)		
	movl	%edi, 12(%eax)		
	movl	%esp, 16(%eax)		
	movl	(%esp), %ecx		
	movl	%ecx, 20(%eax)		
	subl	%eax, %eax		
	ret
	SET_SIZE(setjmp)

	ENTRY(longjmp)			
	movl	0(%eax), %ebp		
	movl	4(%eax), %ebx		
	movl	8(%eax), %esi		
	movl	12(%eax), %edi		
	movl	16(%eax), %esp		
	movl	20(%eax), %ecx		
	addl	$4, %esp		
	jmp	*%ecx			
	SET_SIZE(longjmp)

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif

#endif 
