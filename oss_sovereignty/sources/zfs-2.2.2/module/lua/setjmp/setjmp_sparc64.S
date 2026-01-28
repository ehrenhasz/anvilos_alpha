

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
	.asciz "@(#)_setjmp.s	8.1 (Berkeley) 6/4/93"
#else
	RCSID("$NetBSD: _setjmp.S,v 1.4 1998/10/08 02:27:59 eeh Exp $")
#endif
#endif 

#define	_JB_FP		0x0
#define	_JB_PC		0x8
#define	_JB_SP		0x10

	.register %g2,#ignore
	.register %g3,#ignore

#define	ENTRY(x)		\
	.text ;			\
	.balign 32 ;		\
	.globl	x ;		\
	.type	x,@function ;	\
x:

#define	END(x)			\
	.size x, . - x



ENTRY(setjmp)
	stx	%sp, [%o0 + _JB_SP]
	stx	%o7, [%o0 + _JB_PC]
	stx	%fp, [%o0 + _JB_FP]
	retl
	 clr	%o0
END(setjmp)

ENTRY(longjmp)
	mov	1, %g1
	movrnz	%o1, %o1, %g1
	mov	%o0, %g2
	ldx	[%g2 + _JB_FP], %g3
1:	cmp	%fp, %g3
	bl,a	1b
	 restore
	be,a	2f
	 ldx	[%g2 + _JB_SP], %o0

.Lbotch:
	illtrap

2:	cmp	%o0, %sp
	bge,a	3f
	 mov	%o0, %sp
	b,a	.Lbotch
	 nop
3:	ldx	[%g2 + _JB_PC], %o7
	retl
	 mov	%g1, %o0
END(longjmp)

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
