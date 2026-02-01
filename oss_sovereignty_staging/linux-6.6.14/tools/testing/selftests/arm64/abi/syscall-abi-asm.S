 
 
 
 

 
 
 
 
 
 
 
 
 
 
 
 
 
 
 

#include "syscall-abi.h"

.arch_extension sve

#define ID_AA64SMFR0_EL1_SMEver_SHIFT           56
#define ID_AA64SMFR0_EL1_SMEver_WIDTH           4

 
.macro _ldr_za nw, nxbase, offset=0
	.inst	0xe1000000			\
		| (((\nw) & 3) << 13)		\
		| ((\nxbase) << 5)		\
		| ((\offset) & 7)
.endm

 
.macro _str_za nw, nxbase, offset=0
	.inst	0xe1200000			\
		| (((\nw) & 3) << 13)		\
		| ((\nxbase) << 5)		\
		| ((\offset) & 7)
.endm

 
.macro _ldr_zt nx
	.inst	0xe11f8000			\
		| (((\nx) & 0x1f) << 5)
.endm

 
.macro _str_zt nx
	.inst	0xe13f8000			\
		| (((\nx) & 0x1f) << 5)
.endm

.globl do_syscall
do_syscall:
	 
	stp	x29, x30, [sp, #-112]!
	mov	x29, sp
	stp	x0, x1, [sp, #16]
	stp	x19, x20, [sp, #32]
	stp	x21, x22, [sp, #48]
	stp	x23, x24, [sp, #64]
	stp	x25, x26, [sp, #80]
	stp	x27, x28, [sp, #96]

	 
	cbz	x1, 1f
	adrp	x2, svcr_in
	ldr	x2, [x2, :lo12:svcr_in]
	msr	S3_3_C4_C2_2, x2
1:

	 
	tbz	x2, #SVCR_ZA_SHIFT, 1f
	mov	w12, #0
	ldr	x2, =za_in
2:	_ldr_za 12, 2
	add	x2, x2, x1
	add	x12, x12, #1
	cmp	x1, x12
	bne	2b

	 
	mrs	x2, S3_0_C0_C4_5	 
	ubfx	x2, x2, #ID_AA64SMFR0_EL1_SMEver_SHIFT, \
			 #ID_AA64SMFR0_EL1_SMEver_WIDTH
	cbz	x2, 1f
	adrp	x2, zt_in
	add	x2, x2, :lo12:zt_in
	_ldr_zt 2
1:

	 
	ldr	x2, =gpr_in
	add	x2, x2, #64
	ldp	x8, x9, [x2], #16
	ldp	x10, x11, [x2], #16
	ldp	x12, x13, [x2], #16
	ldp	x14, x15, [x2], #16
	ldp	x16, x17, [x2], #16
	ldp	x18, x19, [x2], #16
	ldp	x20, x21, [x2], #16
	ldp	x22, x23, [x2], #16
	ldp	x24, x25, [x2], #16
	ldp	x26, x27, [x2], #16
	ldr	x28, [x2], #8
	str	x29, [x2], #8		 
	str	x30, [x2], #8		 

	 
	cbnz	x0, 1f
	ldr	x2, =svcr_in
	tbnz	x2, #SVCR_SM_SHIFT, 1f

	ldr	x2, =fpr_in
	ldp	q0, q1, [x2]
	ldp	q2, q3, [x2, #16 * 2]
	ldp	q4, q5, [x2, #16 * 4]
	ldp	q6, q7, [x2, #16 * 6]
	ldp	q8, q9, [x2, #16 * 8]
	ldp	q10, q11, [x2, #16 * 10]
	ldp	q12, q13, [x2, #16 * 12]
	ldp	q14, q15, [x2, #16 * 14]
	ldp	q16, q17, [x2, #16 * 16]
	ldp	q18, q19, [x2, #16 * 18]
	ldp	q20, q21, [x2, #16 * 20]
	ldp	q22, q23, [x2, #16 * 22]
	ldp	q24, q25, [x2, #16 * 24]
	ldp	q26, q27, [x2, #16 * 26]
	ldp	q28, q29, [x2, #16 * 28]
	ldp	q30, q31, [x2, #16 * 30]

	b	2f
1:

	 

	ldr	x2, =z_in
	ldr	z0, [x2, #0, MUL VL]
	ldr	z1, [x2, #1, MUL VL]
	ldr	z2, [x2, #2, MUL VL]
	ldr	z3, [x2, #3, MUL VL]
	ldr	z4, [x2, #4, MUL VL]
	ldr	z5, [x2, #5, MUL VL]
	ldr	z6, [x2, #6, MUL VL]
	ldr	z7, [x2, #7, MUL VL]
	ldr	z8, [x2, #8, MUL VL]
	ldr	z9, [x2, #9, MUL VL]
	ldr	z10, [x2, #10, MUL VL]
	ldr	z11, [x2, #11, MUL VL]
	ldr	z12, [x2, #12, MUL VL]
	ldr	z13, [x2, #13, MUL VL]
	ldr	z14, [x2, #14, MUL VL]
	ldr	z15, [x2, #15, MUL VL]
	ldr	z16, [x2, #16, MUL VL]
	ldr	z17, [x2, #17, MUL VL]
	ldr	z18, [x2, #18, MUL VL]
	ldr	z19, [x2, #19, MUL VL]
	ldr	z20, [x2, #20, MUL VL]
	ldr	z21, [x2, #21, MUL VL]
	ldr	z22, [x2, #22, MUL VL]
	ldr	z23, [x2, #23, MUL VL]
	ldr	z24, [x2, #24, MUL VL]
	ldr	z25, [x2, #25, MUL VL]
	ldr	z26, [x2, #26, MUL VL]
	ldr	z27, [x2, #27, MUL VL]
	ldr	z28, [x2, #28, MUL VL]
	ldr	z29, [x2, #29, MUL VL]
	ldr	z30, [x2, #30, MUL VL]
	ldr	z31, [x2, #31, MUL VL]

	 
	 
	ldr	x2, =ffr_in
	ldr	p0, [x2]
	ldr	x2, [x2, #0]
	cbz	x2, 1f
	wrffr	p0.b
1:

	ldr	x2, =p_in
	ldr	p0, [x2, #0, MUL VL]
	ldr	p1, [x2, #1, MUL VL]
	ldr	p2, [x2, #2, MUL VL]
	ldr	p3, [x2, #3, MUL VL]
	ldr	p4, [x2, #4, MUL VL]
	ldr	p5, [x2, #5, MUL VL]
	ldr	p6, [x2, #6, MUL VL]
	ldr	p7, [x2, #7, MUL VL]
	ldr	p8, [x2, #8, MUL VL]
	ldr	p9, [x2, #9, MUL VL]
	ldr	p10, [x2, #10, MUL VL]
	ldr	p11, [x2, #11, MUL VL]
	ldr	p12, [x2, #12, MUL VL]
	ldr	p13, [x2, #13, MUL VL]
	ldr	p14, [x2, #14, MUL VL]
	ldr	p15, [x2, #15, MUL VL]
2:

	 
	svc	#0

	 
	ldr	x2, =gpr_out
	add	x2, x2, #64
	stp	x8, x9, [x2], #16
	stp	x10, x11, [x2], #16
	stp	x12, x13, [x2], #16
	stp	x14, x15, [x2], #16
	stp	x16, x17, [x2], #16
	stp	x18, x19, [x2], #16
	stp	x20, x21, [x2], #16
	stp	x22, x23, [x2], #16
	stp	x24, x25, [x2], #16
	stp	x26, x27, [x2], #16
	stp	x28, x29, [x2], #16
	str	x30, [x2]

	 
	ldp	x0, x1, [sp, #16]

	 
	ldr	x2, =fpr_out
	stp	q0, q1, [x2]
	stp	q2, q3, [x2, #16 * 2]
	stp	q4, q5, [x2, #16 * 4]
	stp	q6, q7, [x2, #16 * 6]
	stp	q8, q9, [x2, #16 * 8]
	stp	q10, q11, [x2, #16 * 10]
	stp	q12, q13, [x2, #16 * 12]
	stp	q14, q15, [x2, #16 * 14]
	stp	q16, q17, [x2, #16 * 16]
	stp	q18, q19, [x2, #16 * 18]
	stp	q20, q21, [x2, #16 * 20]
	stp	q22, q23, [x2, #16 * 22]
	stp	q24, q25, [x2, #16 * 24]
	stp	q26, q27, [x2, #16 * 26]
	stp	q28, q29, [x2, #16 * 28]
	stp	q30, q31, [x2, #16 * 30]

	 
	cbz	x1, 1f
	mrs	x2, S3_3_C4_C2_2
	adrp	x3, svcr_out
	str	x2, [x3, :lo12:svcr_out]
1:

	 
	tbz	x2, #SVCR_ZA_SHIFT, 1f
	mov	w12, #0
	ldr	x2, =za_out
2:	_str_za 12, 2
	add	x2, x2, x1
	add	x12, x12, #1
	cmp	x1, x12
	bne	2b

	 
	mrs	x2, S3_0_C0_C4_5	 
	ubfx	x2, x2, #ID_AA64SMFR0_EL1_SMEver_SHIFT, \
			#ID_AA64SMFR0_EL1_SMEver_WIDTH
	cbz	x2, 1f
	adrp	x2, zt_out
	add	x2, x2, :lo12:zt_out
	_str_zt 2
1:

	 
	cbz	x0, 1f

	ldr	x2, =z_out
	str	z0, [x2, #0, MUL VL]
	str	z1, [x2, #1, MUL VL]
	str	z2, [x2, #2, MUL VL]
	str	z3, [x2, #3, MUL VL]
	str	z4, [x2, #4, MUL VL]
	str	z5, [x2, #5, MUL VL]
	str	z6, [x2, #6, MUL VL]
	str	z7, [x2, #7, MUL VL]
	str	z8, [x2, #8, MUL VL]
	str	z9, [x2, #9, MUL VL]
	str	z10, [x2, #10, MUL VL]
	str	z11, [x2, #11, MUL VL]
	str	z12, [x2, #12, MUL VL]
	str	z13, [x2, #13, MUL VL]
	str	z14, [x2, #14, MUL VL]
	str	z15, [x2, #15, MUL VL]
	str	z16, [x2, #16, MUL VL]
	str	z17, [x2, #17, MUL VL]
	str	z18, [x2, #18, MUL VL]
	str	z19, [x2, #19, MUL VL]
	str	z20, [x2, #20, MUL VL]
	str	z21, [x2, #21, MUL VL]
	str	z22, [x2, #22, MUL VL]
	str	z23, [x2, #23, MUL VL]
	str	z24, [x2, #24, MUL VL]
	str	z25, [x2, #25, MUL VL]
	str	z26, [x2, #26, MUL VL]
	str	z27, [x2, #27, MUL VL]
	str	z28, [x2, #28, MUL VL]
	str	z29, [x2, #29, MUL VL]
	str	z30, [x2, #30, MUL VL]
	str	z31, [x2, #31, MUL VL]

	ldr	x2, =p_out
	str	p0, [x2, #0, MUL VL]
	str	p1, [x2, #1, MUL VL]
	str	p2, [x2, #2, MUL VL]
	str	p3, [x2, #3, MUL VL]
	str	p4, [x2, #4, MUL VL]
	str	p5, [x2, #5, MUL VL]
	str	p6, [x2, #6, MUL VL]
	str	p7, [x2, #7, MUL VL]
	str	p8, [x2, #8, MUL VL]
	str	p9, [x2, #9, MUL VL]
	str	p10, [x2, #10, MUL VL]
	str	p11, [x2, #11, MUL VL]
	str	p12, [x2, #12, MUL VL]
	str	p13, [x2, #13, MUL VL]
	str	p14, [x2, #14, MUL VL]
	str	p15, [x2, #15, MUL VL]

	 
	ldr	x2, =ffr_in
	ldr	x2, [x2, #0]
	cbz	x2, 1f
	ldr	x2, =ffr_out
	rdffr	p0.b
	str	p0, [x2]
1:

	 
	ldp	x19, x20, [sp, #32]
	ldp	x21, x22, [sp, #48]
	ldp	x23, x24, [sp, #64]
	ldp	x25, x26, [sp, #80]
	ldp	x27, x28, [sp, #96]
	ldp	x29, x30, [sp], #112

	 
	cbz	x1, 1f
	msr	S3_3_C4_C2_2, xzr
1:

	ret
