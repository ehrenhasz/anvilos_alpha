








#include <asm/unistd.h>
#include "assembler.h"

.arch_extension sve

.macro test_loop per_loop
	mov	x10, x20
	mov	x8, #__NR_getpid
	mrs	x11, CNTVCT_EL0
1:
	\per_loop
	svc	#0
	sub	x10, x10, #1
	cbnz	x10, 1b

	mrs	x12, CNTVCT_EL0
	sub	x0, x12, x11
	bl	putdec
	puts	"\n"
.endm


.globl _start
function _start
	puts	"Iterations per test: "
	mov	x20, #10000
	lsl	x20, x20, #8
	mov	x0, x20
	bl	putdec
	puts	"\n"

	
	puts	"No SVE: "
	test_loop

	
	mrs	x0, ID_AA64PFR0_EL1
	ubfx	x0, x0, #32, #4
	cbnz	x0, 1f
	puts	"System does not support SVE\n"
	b	out
1:

	
	puts	"SVE VL: "
	rdvl	x0, #8
	bl	putdec
	puts	"\n"

	puts	"SVE used once: "
	test_loop

	
	puts	"SVE used per syscall: "
	test_loop "rdvl x0, #8"

	
out:
	mov	x0, #0
	mov	x8, #__NR_exit
	svc	#0
