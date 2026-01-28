
#include "common.h"

	.data
	.balign 8
success_message:
	.ascii "success: switch_endian_test\n\0"

	.balign 8
failure_message:
	.ascii "failure: switch_endian_test\n\0"

	.section ".toc"
	.balign 8
pattern:
	.8byte 0x5555AAAA5555AAAA

	.text
FUNC_START(_start)
	
	ld	r15, pattern@TOC(%r2)

	
	lis	r3, 0x00FF
	ori	r3, r3, 0xF000
	mtcr	r3

	
	mr	r3, r15
	addi	r4, r15, 4

	addi	r5, r15, 32
	mtlr	r5

	addi	r5, r15, 5
	addi	r6, r15, 6
	addi	r7, r15, 7
	addi	r8, r15, 8

	

	addi	r13, r15, 13
	addi	r14, r15, 14

	

	addi	r16, r15, 16
	addi	r17, r15, 17
	addi	r18, r15, 18
	addi	r19, r15, 19
	addi	r20, r15, 20
	addi	r21, r15, 21
	addi	r22, r15, 22
	addi	r23, r15, 23
	addi	r24, r15, 24
	addi	r25, r15, 25
	addi	r26, r15, 26
	addi	r27, r15, 27
	addi	r28, r15, 28
	addi	r29, r15, 29
	addi	r30, r15, 30
	addi	r31, r15, 31

	
	li r0, __NR_switch_endian
	sc

	tdi   0, 0, 0x48	
	b     .Lfail	  	

#include "check-reversed.S"

	
	.long	0x02000044	

#include "check.S"

	ld	r4, success_message@got(%r2)
	li	r5, 28	
	li	r14, 0	
.Lout:
	li	r0, __NR_write
	li	r3, 1	
	sc
	li      r0, __NR_exit
	mr	r3, r14
	sc
	b       .

.Lfail:
	ld	r4, failure_message@got(%r2)
	li	r5, 28	
	li	r14, 1
	b	.Lout
