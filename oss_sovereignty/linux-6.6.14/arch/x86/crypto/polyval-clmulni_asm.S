 
 
 

#include <linux/linkage.h>
#include <asm/frame.h>

#define STRIDE_BLOCKS 8

#define GSTAR %xmm7
#define PL %xmm8
#define PH %xmm9
#define TMP_XMM %xmm11
#define LO %xmm12
#define HI %xmm13
#define MI %xmm14
#define SUM %xmm15

#define KEY_POWERS %rdi
#define MSG %rsi
#define BLOCKS_LEFT %rdx
#define ACCUMULATOR %rcx
#define TMP %rax

.section    .rodata.cst16.gstar, "aM", @progbits, 16
.align 16

.Lgstar:
	.quad 0xc200000000000000, 0xc200000000000000

.text

 
.macro schoolbook1 count
	.set i, 0
	.rept (\count)
		schoolbook1_iteration i 0
		.set i, (i +1)
	.endr
.endm

 
.macro schoolbook1_iteration i xor_sum
	movups (16*\i)(MSG), %xmm0
	.if (\i == 0 && \xor_sum == 1)
		pxor SUM, %xmm0
	.endif
	vpclmulqdq $0x01, (16*\i)(KEY_POWERS), %xmm0, %xmm2
	vpclmulqdq $0x00, (16*\i)(KEY_POWERS), %xmm0, %xmm1
	vpclmulqdq $0x10, (16*\i)(KEY_POWERS), %xmm0, %xmm3
	vpclmulqdq $0x11, (16*\i)(KEY_POWERS), %xmm0, %xmm4
	vpxor %xmm2, MI, MI
	vpxor %xmm1, LO, LO
	vpxor %xmm4, HI, HI
	vpxor %xmm3, MI, MI
.endm

 
.macro schoolbook1_noload
	vpclmulqdq $0x01, %xmm0, %xmm1, MI
	vpclmulqdq $0x10, %xmm0, %xmm1, %xmm2
	vpclmulqdq $0x00, %xmm0, %xmm1, LO
	vpclmulqdq $0x11, %xmm0, %xmm1, HI
	vpxor %xmm2, MI, MI
.endm

 
.macro schoolbook2
	vpslldq $8, MI, PL
	vpsrldq $8, MI, PH
	pxor LO, PL
	pxor HI, PH
.endm

 
.macro montgomery_reduction dest
	vpclmulqdq $0x00, PL, GSTAR, TMP_XMM	# TMP_XMM = T_1 : T_0 = P_0 * g*(x)
	pshufd $0b01001110, TMP_XMM, TMP_XMM	# TMP_XMM = T_0 : T_1
	pxor PL, TMP_XMM			# TMP_XMM = P_1 + T_0 : P_0 + T_1
	pxor TMP_XMM, PH			# PH = P_3 + P_1 + T_0 : P_2 + P_0 + T_1
	pclmulqdq $0x11, GSTAR, TMP_XMM		# TMP_XMM = V_1 : V_0 = V = [(P_1 + T_0) * g*(x)]
	vpxor TMP_XMM, PH, \dest
.endm

 
.macro full_stride reduce
	pxor LO, LO
	pxor HI, HI
	pxor MI, MI

	schoolbook1_iteration 7 0
	.if \reduce
		vpclmulqdq $0x00, PL, GSTAR, TMP_XMM
	.endif

	schoolbook1_iteration 6 0
	.if \reduce
		pshufd $0b01001110, TMP_XMM, TMP_XMM
	.endif

	schoolbook1_iteration 5 0
	.if \reduce
		pxor PL, TMP_XMM
	.endif

	schoolbook1_iteration 4 0
	.if \reduce
		pxor TMP_XMM, PH
	.endif

	schoolbook1_iteration 3 0
	.if \reduce
		pclmulqdq $0x11, GSTAR, TMP_XMM
	.endif

	schoolbook1_iteration 2 0
	.if \reduce
		vpxor TMP_XMM, PH, SUM
	.endif

	schoolbook1_iteration 1 0

	schoolbook1_iteration 0 1

	addq $(8*16), MSG
	schoolbook2
.endm

 
.macro partial_stride
	mov BLOCKS_LEFT, TMP
	shlq $4, TMP
	addq $(16*STRIDE_BLOCKS), KEY_POWERS
	subq TMP, KEY_POWERS

	movups (MSG), %xmm0
	pxor SUM, %xmm0
	movaps (KEY_POWERS), %xmm1
	schoolbook1_noload
	dec BLOCKS_LEFT
	addq $16, MSG
	addq $16, KEY_POWERS

	test $4, BLOCKS_LEFT
	jz .Lpartial4BlocksDone
	schoolbook1 4
	addq $(4*16), MSG
	addq $(4*16), KEY_POWERS
.Lpartial4BlocksDone:
	test $2, BLOCKS_LEFT
	jz .Lpartial2BlocksDone
	schoolbook1 2
	addq $(2*16), MSG
	addq $(2*16), KEY_POWERS
.Lpartial2BlocksDone:
	test $1, BLOCKS_LEFT
	jz .LpartialDone
	schoolbook1 1
.LpartialDone:
	schoolbook2
	montgomery_reduction SUM
.endm

 
SYM_FUNC_START(clmul_polyval_mul)
	FRAME_BEGIN
	vmovdqa .Lgstar(%rip), GSTAR
	movups (%rdi), %xmm0
	movups (%rsi), %xmm1
	schoolbook1_noload
	schoolbook2
	montgomery_reduction SUM
	movups SUM, (%rdi)
	FRAME_END
	RET
SYM_FUNC_END(clmul_polyval_mul)

 
SYM_FUNC_START(clmul_polyval_update)
	FRAME_BEGIN
	vmovdqa .Lgstar(%rip), GSTAR
	movups (ACCUMULATOR), SUM
	subq $STRIDE_BLOCKS, BLOCKS_LEFT
	js .LstrideLoopExit
	full_stride 0
	subq $STRIDE_BLOCKS, BLOCKS_LEFT
	js .LstrideLoopExitReduce
.LstrideLoop:
	full_stride 1
	subq $STRIDE_BLOCKS, BLOCKS_LEFT
	jns .LstrideLoop
.LstrideLoopExitReduce:
	montgomery_reduction SUM
.LstrideLoopExit:
	add $STRIDE_BLOCKS, BLOCKS_LEFT
	jz .LskipPartial
	partial_stride
.LskipPartial:
	movups SUM, (ACCUMULATOR)
	FRAME_END
	RET
SYM_FUNC_END(clmul_polyval_update)
