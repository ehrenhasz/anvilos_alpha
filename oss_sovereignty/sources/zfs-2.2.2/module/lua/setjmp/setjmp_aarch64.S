


#ifdef __aarch64__

#define	ENTRY(sym) \
	.text; \
	.globl	sym; \
	.balign	2; \
	.type	sym,#function; \
sym:

#define	END(sym) \
	.size	sym, . - sym


ENTRY(setjmp)
	
	mov	x8, sp
	str	x8, [x0], #8

	
	stp	x19, x20, [x0], #16
	stp	x21, x22, [x0], #16
	stp	x23, x24, [x0], #16
	stp	x25, x26, [x0], #16
	stp	x27, x28, [x0], #16
	stp	x29, x30, [x0], #16

	
	mov	x0, #0
	ret
END(setjmp)

ENTRY(longjmp)
	
	ldr	x8, [x0], #8
	mov	sp, x8

	
	ldp	x19, x20, [x0], #16
	ldp	x21, x22, [x0], #16
	ldp	x23, x24, [x0], #16
	ldp	x25, x26, [x0], #16
	ldp	x27, x28, [x0], #16
	ldp	x29, x30, [x0], #16

	
	mov	x0, x1
	ret
END(longjmp)

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif

#endif 
