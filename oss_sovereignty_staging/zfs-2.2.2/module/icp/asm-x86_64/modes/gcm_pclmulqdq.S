 

 
 

 

 


#if defined(lint) || defined(__lint)	 

#include <sys/types.h>

void
gcm_mul_pclmulqdq(uint64_t *x_in, uint64_t *y, uint64_t *res) {
	(void) x_in, (void) y, (void) res;
}

#elif defined(HAVE_PCLMULQDQ)	 

#define _ASM
#include <sys/asm_linkage.h>

 

 
 
SECTION_STATIC
.balign XMM_ALIGN
.Lbyte_swap16_mask:
	.byte	15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0


 

ENTRY_NP(gcm_mul_pclmulqdq)
	 
	 
	 
	movdqu	(%rdi), %xmm0	 
	movdqu	(%rsi), %xmm1	 

	 
	 
	 
	lea	.Lbyte_swap16_mask(%rip), %rax
	movups	(%rax), %xmm10
	pshufb	%xmm10, %xmm0
	pshufb	%xmm10, %xmm1


	 
	 
	 
	movdqu	%xmm0, %xmm3
	pclmulqdq $0, %xmm1, %xmm3	 

	movdqu	%xmm0, %xmm4
	pclmulqdq $16, %xmm1, %xmm4	 

	movdqu	%xmm0, %xmm5
	pclmulqdq $1, %xmm1, %xmm5	 
	movdqu	%xmm0, %xmm6
	pclmulqdq $17, %xmm1, %xmm6	 

	pxor	%xmm5, %xmm4	 

	movdqu	%xmm4, %xmm5	 
	psrldq	$8, %xmm4	 
	pslldq	$8, %xmm5	 
	pxor	%xmm5, %xmm3
	pxor	%xmm4, %xmm6	 
				 
				 

	 
	 
	movdqu	%xmm3, %xmm7
	movdqu	%xmm6, %xmm8
	pslld	$1, %xmm3
	pslld	$1, %xmm6
	psrld	$31, %xmm7
	psrld	$31, %xmm8
	movdqu	%xmm7, %xmm9
	pslldq	$4, %xmm8
	pslldq	$4, %xmm7
	psrldq	$12, %xmm9
	por	%xmm7, %xmm3
	por	%xmm8, %xmm6
	por	%xmm9, %xmm6

	 
	 
	 
	 
	 
	movdqu	%xmm3, %xmm7
	movdqu	%xmm3, %xmm8
	movdqu	%xmm3, %xmm9
	pslld	$31, %xmm7	 
	pslld	$30, %xmm8	 
	pslld	$25, %xmm9	 
	pxor	%xmm8, %xmm7	 
	pxor	%xmm9, %xmm7
	movdqu	%xmm7, %xmm8
	pslldq	$12, %xmm7
	psrldq	$4, %xmm8
	pxor	%xmm7, %xmm3	 

	 
	 
	 
	 
	 
	movdqu	%xmm3, %xmm2
	movdqu	%xmm3, %xmm4	 
	movdqu	%xmm3, %xmm5
	psrld	$1, %xmm2
	psrld	$2, %xmm4	 
	psrld	$7, %xmm5	 
	pxor	%xmm4, %xmm2	 
	pxor	%xmm5, %xmm2
	pxor	%xmm8, %xmm2
	pxor	%xmm2, %xmm3
	pxor	%xmm3, %xmm6	 

	 
	 
	 
	pshufb	%xmm10, %xmm6	 

	 
	 
	 
	movdqu	%xmm6, (%rdx)	 


	 
	 
	 
	RET
	SET_SIZE(gcm_mul_pclmulqdq)

#endif	 

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
