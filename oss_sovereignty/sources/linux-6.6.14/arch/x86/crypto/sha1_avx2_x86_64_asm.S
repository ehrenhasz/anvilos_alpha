



#include <linux/linkage.h>

#define	CTX	%rdi	
#define BUF	%rsi	
#define CNT	%rdx	

#define	REG_A	%ecx
#define	REG_B	%esi
#define	REG_C	%edi
#define	REG_D	%eax
#define	REG_E	%edx
#define	REG_TB	%ebx
#define	REG_TA	%r12d
#define	REG_RA	%rcx
#define	REG_RB	%rsi
#define	REG_RC	%rdi
#define	REG_RD	%rax
#define	REG_RE	%rdx
#define	REG_RTA	%r12
#define	REG_RTB	%rbx
#define	REG_T1	%r11d
#define	xmm_mov	vmovups
#define	avx2_zeroupper	vzeroupper
#define	RND_F1	1
#define	RND_F2	2
#define	RND_F3	3

.macro REGALLOC
	.set A, REG_A
	.set B, REG_B
	.set C, REG_C
	.set D, REG_D
	.set E, REG_E
	.set TB, REG_TB
	.set TA, REG_TA

	.set RA, REG_RA
	.set RB, REG_RB
	.set RC, REG_RC
	.set RD, REG_RD
	.set RE, REG_RE

	.set RTA, REG_RTA
	.set RTB, REG_RTB

	.set T1, REG_T1
.endm

#define HASH_PTR	%r9
#define BLOCKS_CTR	%r8
#define BUFFER_PTR	%r10
#define BUFFER_PTR2	%r13

#define PRECALC_BUF	%r14
#define WK_BUF		%r15

#define W_TMP		%xmm0
#define WY_TMP		%ymm0
#define WY_TMP2		%ymm9

# AVX2 variables
#define WY0		%ymm3
#define WY4		%ymm5
#define WY08		%ymm7
#define WY12		%ymm8
#define WY16		%ymm12
#define WY20		%ymm13
#define WY24		%ymm14
#define WY28		%ymm15

#define YMM_SHUFB_BSWAP	%ymm10


#define W_SIZE		(80*2*2 +16)

#define WK(t)	((((t) % 80) / 4)*32 + ( (t) % 4)*4 + ((t)/80)*16 )(WK_BUF)
#define PRECALC_WK(t)	((t)*2*2)(PRECALC_BUF)


.macro UPDATE_HASH  hash, val
	add	\hash, \val
	mov	\val, \hash
.endm

.macro PRECALC_RESET_WY
	.set WY_00, WY0
	.set WY_04, WY4
	.set WY_08, WY08
	.set WY_12, WY12
	.set WY_16, WY16
	.set WY_20, WY20
	.set WY_24, WY24
	.set WY_28, WY28
	.set WY_32, WY_00
.endm

.macro PRECALC_ROTATE_WY
	
	.set WY_32, WY_28
	.set WY_28, WY_24
	.set WY_24, WY_20
	.set WY_20, WY_16
	.set WY_16, WY_12
	.set WY_12, WY_08
	.set WY_08, WY_04
	.set WY_04, WY_00
	.set WY_00, WY_32

	
	.set WY, WY_00
	.set WY_minus_04, WY_04
	.set WY_minus_08, WY_08
	.set WY_minus_12, WY_12
	.set WY_minus_16, WY_16
	.set WY_minus_20, WY_20
	.set WY_minus_24, WY_24
	.set WY_minus_28, WY_28
	.set WY_minus_32, WY
.endm

.macro PRECALC_00_15
	.if (i == 0) # Initialize and rotate registers
		PRECALC_RESET_WY
		PRECALC_ROTATE_WY
	.endif

	
	.if   ((i & 7) == 0)
		
		vmovdqu (i * 2)(BUFFER_PTR), W_TMP
	.elseif ((i & 7) == 1)
		vinsertf128 $1, ((i-1) * 2)(BUFFER_PTR2),\
			 WY_TMP, WY_TMP
	.elseif ((i & 7) == 2)
		vpshufb YMM_SHUFB_BSWAP, WY_TMP, WY
	.elseif ((i & 7) == 4)
		vpaddd  K_XMM + K_XMM_AR(%rip), WY, WY_TMP
	.elseif ((i & 7) == 7)
		vmovdqu  WY_TMP, PRECALC_WK(i&~7)

		PRECALC_ROTATE_WY
	.endif
.endm

.macro PRECALC_16_31
	
	.if   ((i & 7) == 0)
		
		
		vpalignr	$8, WY_minus_16, WY_minus_12, WY
		vpsrldq	$4, WY_minus_04, WY_TMP               
	.elseif ((i & 7) == 1)
		vpxor	WY_minus_08, WY, WY
		vpxor	WY_minus_16, WY_TMP, WY_TMP
	.elseif ((i & 7) == 2)
		vpxor	WY_TMP, WY, WY
		vpslldq	$12, WY, WY_TMP2
	.elseif ((i & 7) == 3)
		vpslld	$1, WY, WY_TMP
		vpsrld	$31, WY, WY
	.elseif ((i & 7) == 4)
		vpor	WY, WY_TMP, WY_TMP
		vpslld	$2, WY_TMP2, WY
	.elseif ((i & 7) == 5)
		vpsrld	$30, WY_TMP2, WY_TMP2
		vpxor	WY, WY_TMP, WY_TMP
	.elseif ((i & 7) == 7)
		vpxor	WY_TMP2, WY_TMP, WY
		vpaddd  K_XMM + K_XMM_AR(%rip), WY, WY_TMP
		vmovdqu	WY_TMP, PRECALC_WK(i&~7)

		PRECALC_ROTATE_WY
	.endif
.endm

.macro PRECALC_32_79
	

	.if   ((i & 7) == 0)
	
		vpalignr	$8, WY_minus_08, WY_minus_04, WY_TMP
	.elseif ((i & 7) == 1)
		
		vpxor	WY_minus_28, WY, WY
	.elseif ((i & 7) == 2)
		vpxor	WY_minus_16, WY_TMP, WY_TMP
	.elseif ((i & 7) == 3)
		vpxor	WY_TMP, WY, WY
	.elseif ((i & 7) == 4)
		vpslld	$2, WY, WY_TMP
	.elseif ((i & 7) == 5)
		vpsrld	$30, WY, WY
		vpor	WY, WY_TMP, WY
	.elseif ((i & 7) == 7)
		vpaddd  K_XMM + K_XMM_AR(%rip), WY, WY_TMP
		vmovdqu	WY_TMP, PRECALC_WK(i&~7)

		PRECALC_ROTATE_WY
	.endif
.endm

.macro PRECALC r, s
	.set i, \r

	.if (i < 40)
		.set K_XMM, 32*0
	.elseif (i < 80)
		.set K_XMM, 32*1
	.elseif (i < 120)
		.set K_XMM, 32*2
	.else
		.set K_XMM, 32*3
	.endif

	.if (i<32)
		PRECALC_00_15	\s
	.elseif (i<64)
		PRECALC_16_31	\s
	.elseif (i < 160)
		PRECALC_32_79	\s
	.endif
.endm

.macro ROTATE_STATE
	.set T_REG, E
	.set E, D
	.set D, C
	.set C, B
	.set B, TB
	.set TB, A
	.set A, T_REG

	.set T_REG, RE
	.set RE, RD
	.set RD, RC
	.set RC, RB
	.set RB, RTB
	.set RTB, RA
	.set RA, T_REG
.endm



.macro RND_FUN f, r
	.if (\f == RND_F1)
		ROUND_F1	\r
	.elseif (\f == RND_F2)
		ROUND_F2	\r
	.elseif (\f == RND_F3)
		ROUND_F3	\r
	.endif
.endm

.macro RR r
	.set round_id, (\r % 80)

	.if (round_id == 0)        
		.set ROUND_FUNC, RND_F1
		mov	B, TB

		rorx	$(32-30), B, B    
		andn	D, TB, T1
		and	C, TB
		xor	T1, TB
	.endif

	RND_FUN ROUND_FUNC, \r
	ROTATE_STATE

	.if   (round_id == 18)
		.set ROUND_FUNC, RND_F2
	.elseif (round_id == 38)
		.set ROUND_FUNC, RND_F3
	.elseif (round_id == 58)
		.set ROUND_FUNC, RND_F2
	.endif

	.set round_id, ( (\r+1) % 80)

	RND_FUN ROUND_FUNC, (\r+1)
	ROTATE_STATE
.endm

.macro ROUND_F1 r
	add	WK(\r), E

	andn	C, A, T1			
	lea	(RE,RTB), E		

	rorx	$(32-5), A, TA		
	rorx	$(32-30),A, TB		

	PRECALC	(\r)			

	
	and	B, A			
	xor	T1, A			

	lea	(RE,RTA), E		
.endm

.macro ROUND_F2 r
	add	WK(\r), E
	lea	(RE,RTB), E		

	
	rorx	$(32-5), A, TA		
	.if ((round_id) < 79)
		rorx	$(32-30), A, TB	
	.endif
	PRECALC	(\r)			

	.if ((round_id) < 79)
		xor	B, A
	.endif

	add	TA, E			

	.if ((round_id) < 79)
		xor	C, A
	.endif
.endm

.macro ROUND_F3 r
	add	WK(\r), E
	PRECALC	(\r)			

	lea	(RE,RTB), E		

	mov	B, T1
	or	A, T1

	rorx	$(32-5), A, TA		
	rorx	$(32-30), A, TB		

	
	and	C, T1
	and	B, A
	or	T1, A

	add	TA, E			

.endm


.macro ADD_IF_GE a, b, c, d
	mov     \a, RTA
	add     $\d, RTA
	cmp     $\c, \b
	cmovge  RTA, \a
.endm


.macro SHA1_PIPELINED_MAIN_BODY

	REGALLOC

	mov	(HASH_PTR), A
	mov	4(HASH_PTR), B
	mov	8(HASH_PTR), C
	mov	12(HASH_PTR), D
	mov	16(HASH_PTR), E

	mov	%rsp, PRECALC_BUF
	lea	(2*4*80+32)(%rsp), WK_BUF

	# Precalc WK for first 2 blocks
	ADD_IF_GE BUFFER_PTR2, BLOCKS_CTR, 2, 64
	.set i, 0
	.rept    160
		PRECALC i
		.set i, i + 1
	.endr

	
	ADD_IF_GE BUFFER_PTR, BLOCKS_CTR, 3, 128
	ADD_IF_GE BUFFER_PTR2, BLOCKS_CTR, 4, 128
	xchg	WK_BUF, PRECALC_BUF

	.align 32
.L_loop:
	
	test BLOCKS_CTR, BLOCKS_CTR
	jnz .L_begin
	.align 32
	jmp	.L_end
	.align 32
.L_begin:

	
	.set j, 0
	.rept 5
		RR	j
		.set j, j+2
	.endr

	
	.rept 25
		RR	j
		.set j, j+2
	.endr

	
	sub $1, BLOCKS_CTR
	
	ADD_IF_GE BUFFER_PTR, BLOCKS_CTR, 4, 128
	
	.rept 10
		RR	j
		.set j, j+2
	.endr

	UPDATE_HASH	(HASH_PTR), A
	UPDATE_HASH	4(HASH_PTR), TB
	UPDATE_HASH	8(HASH_PTR), C
	UPDATE_HASH	12(HASH_PTR), D
	UPDATE_HASH	16(HASH_PTR), E

	test	BLOCKS_CTR, BLOCKS_CTR
	jz	.L_loop

	mov	TB, B

	
	

	.set j, 0
	.rept 10
		RR	j+80
		.set j, j+2
	.endr

	
	.rept 10
		RR	j+80
		.set j, j+2
	.endr

	
	.rept 10
		RR	j+80
		.set j, j+2
	.endr

	
	sub     $1, BLOCKS_CTR
	
	ADD_IF_GE BUFFER_PTR2, BLOCKS_CTR, 4, 128

	
	.rept 10
		RR	j+80
		.set j, j+2
	.endr

	UPDATE_HASH	(HASH_PTR), A
	UPDATE_HASH	4(HASH_PTR), TB
	UPDATE_HASH	8(HASH_PTR), C
	UPDATE_HASH	12(HASH_PTR), D
	UPDATE_HASH	16(HASH_PTR), E

	
	mov	A, TA
	mov	TB, A
	mov	C, TB
	mov	E, C
	mov	D, B
	mov	TA, D

	REGALLOC

	xchg	WK_BUF, PRECALC_BUF

	jmp	.L_loop

	.align 32
.L_end:

.endm

.macro SHA1_VECTOR_ASM  name
	SYM_FUNC_START(\name)

	push	%rbx
	push	%r12
	push	%r13
	push	%r14
	push	%r15

	RESERVE_STACK  = (W_SIZE*4 + 8+24)

	
	push	%rbp
	mov	%rsp, %rbp
	and	$~(0x20-1), %rsp
	sub	$RESERVE_STACK, %rsp

	avx2_zeroupper

	
	mov	CTX, HASH_PTR
	mov	BUF, BUFFER_PTR

	mov	BUF, BUFFER_PTR2
	mov	CNT, BLOCKS_CTR

	xmm_mov	BSWAP_SHUFB_CTL(%rip), YMM_SHUFB_BSWAP

	SHA1_PIPELINED_MAIN_BODY

	avx2_zeroupper

	mov	%rbp, %rsp
	pop	%rbp

	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%rbx

	RET

	SYM_FUNC_END(\name)
.endm

.section .rodata

#define K1 0x5a827999
#define K2 0x6ed9eba1
#define K3 0x8f1bbcdc
#define K4 0xca62c1d6

.align 128
K_XMM_AR:
	.long K1, K1, K1, K1
	.long K1, K1, K1, K1
	.long K2, K2, K2, K2
	.long K2, K2, K2, K2
	.long K3, K3, K3, K3
	.long K3, K3, K3, K3
	.long K4, K4, K4, K4
	.long K4, K4, K4, K4

BSWAP_SHUFB_CTL:
	.long 0x00010203
	.long 0x04050607
	.long 0x08090a0b
	.long 0x0c0d0e0f
	.long 0x00010203
	.long 0x04050607
	.long 0x08090a0b
	.long 0x0c0d0e0f
.text

SHA1_VECTOR_ASM     sha1_transform_avx2
