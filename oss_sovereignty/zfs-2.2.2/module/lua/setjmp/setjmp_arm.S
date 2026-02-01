 


#if defined(__arm__) && !defined(__aarch64__)

#if defined(__thumb2__)
#define	_FUNC_MODE	.code 16; .thumb_func
#else
#define	_FUNC_MODE	.code 32
#endif

#define	ENTRY(x) \
	.text; \
	.syntax unified; \
	.balign 2; \
	.global x; \
	.type x,#function; \
	_FUNC_MODE; \
x:

#define	END(x) \
	.size x, . - x;

#define	RET	bx	lr


 
ENTRY(setjmp)
#if defined(__thumb2__)
	mov	ip, sp
	stmia	r0, {r4-r12,r14}
#else
	stmia	r0, {r4-r14}
#endif
	mov	r0, #0x00000000
	RET
END(setjmp)

ENTRY(longjmp)
#if defined(__thumb2__)
	ldmia	r0, {r4-r12,r14}
	mov	sp, ip
#else
	ldmia	r0, {r4-r14}
#endif
	mov	r0, #0x00000001
	RET
END(longjmp)

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif

#endif
