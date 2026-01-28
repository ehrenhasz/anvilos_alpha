


#include <asm/unistd.h>

.section        .rodata, "a"
call_fmt:
	.asciz "Calling sigreturn with fake sigframe sized:%zd at SP @%08lX\n"

.text

.globl fake_sigreturn


fake_sigreturn:
	stp	x29, x30, [sp, #-16]!
	mov	x29, sp

	mov	x20, x0
	mov	x21, x1
	mov	x22, x2

	
	add	x0, x21, x22
	add	x0, x0, #15
	bic	x0, x0, #15 
	sub	sp, sp, x0
	add	x23, sp, x22 

	ldr	x0, =call_fmt
	mov	x1, x21
	mov	x2, x23
	bl	printf

	
	mov	x0, x23
	mov	x1, x20
	mov	x2, x21
	bl	memcpy

	
	ldr	x0, current
	str	x23, [x0]
	
	mov	sp, x23

	mov	x8, #__NR_rt_sigreturn
	svc	#0

	
	b	.
