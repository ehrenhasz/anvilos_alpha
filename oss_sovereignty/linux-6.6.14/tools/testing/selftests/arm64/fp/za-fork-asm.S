 
 

#include "sme-inst.h"

.arch_extension sve

#define MAGIC     42

#define MAXVL     2048
#define MAXVL_B   (MAXVL / 8)

.pushsection .text
.data
.align 4
scratch:
	.space	MAXVL_B
.popsection

.globl fork_test
fork_test:
	smstart_za

	 
	 
	ldr	x0, =scratch
	mov	x1, #MAGIC
	str	x1, [x0]
	mov	w12, wzr
	_ldr_za 12, 0			 

	 
	b	fork_test_c

.globl verify_fork
verify_fork:
	 
	mrs	x0, S3_3_C4_C2_2
	and	x1, x0, #3
	cmp	x1, #2
	beq	1f
	mov	x0, xzr
	b	100f
1:

	 
	ldr	x0, =scratch
	mov	w12, wzr
	_str_za 12, 0			 
	ldr	x1, [x0]
	cmp	x1, #MAGIC
	beq	2f
	mov	x0, xzr
	b	100f

2:
	 
	mov	x0, #1
100:
	ret

