 
 
 
 
 
 
 
 
 
 
 

#include <asm/unistd.h>
#include "assembler.h"
#include "asm-offsets.h"

#define NVR	32
#define MAXVL_B	(128 / 8)

.macro _vldr Vn:req, Xt:req
	ld1	{v\Vn\().2d}, [x\Xt]
.endm

.macro _vstr Vn:req, Xt:req
	st1	{v\Vn\().2d}, [x\Xt]
.endm

 
 
 
 
 
define_accessor setv, NVR, _vldr
define_accessor getv, NVR, _vstr

 
.pushsection .text
.data
.align 4
vref:
	.space	MAXVL_B * NVR
scratch:
	.space	MAXVL_B
.popsection

 
 
 
 
function pattern
	orr	w1, w0, w1, lsl #16
	orr	w2, w1, w2, lsl #28

	ldr	x0, =scratch
	mov	w1, #MAXVL_B / 4

0:	str	w2, [x0], #4
	add	w2, w2, #(1 << 22)
	subs	w1, w1, #1
	bne	0b

	ret
endfunction

 
.macro _adrv xd, xn, nrtmp
	ldr	\xd, =vref
	mov	x\nrtmp, #16
	madd	\xd, x\nrtmp, \xn, \xd
.endm

 
 
 
 
function setup_vreg
	mov	x4, x30

	mov	x6, x1
	bl	pattern
	_adrv	x0, x6, 2
	mov	x5, x0
	ldr	x1, =scratch
	bl	memcpy

	mov	x0, x6
	mov	x1, x5
	bl	setv

	ret	x4
endfunction

 
 
 
 
function memcmp
	cbz	x2, 1f

	mov	x5, #0
0:	ldrb	w3, [x0, x5]
	ldrb	w4, [x1, x5]
	add	x5, x5, #1
	cmp	w3, w4
	b.ne	barf
	subs	x2, x2, #1
	b.ne	0b

1:	ret
endfunction

 
 
 
function check_vreg
	mov	x3, x30

	_adrv	x5, x0, 6
	mov	x4, x0
	ldr	x7, =scratch

	mov	x0, x7
	mov	x1, x6
	bl	memfill_ae

	mov	x0, x4
	mov	x1, x7
	bl	getv

	mov	x0, x5
	mov	x1, x7
	mov	x2, x6
	mov	x30, x3
	b	memcmp
endfunction

 
 
function irritator_handler
	 
	ldr	x0, [x2, #ucontext_regs + 8 * 23]
	add	x0, x0, #1
	str	x0, [x2, #ucontext_regs + 8 * 23]

	 
	adr	x0, .text + (irritator_handler - .text) / 16 * 16
	movi	v0.8b, #7
	movi	v9.16b, #9
	movi	v31.8b, #31

	ret
endfunction

function tickle_handler
	 
	ldr	x0, [x2, #ucontext_regs + 8 * 23]
	add	x0, x0, #1
	str	x0, [x2, #ucontext_regs + 8 * 23]

	ret
endfunction

function terminate_handler
	mov	w21, w0
	mov	x20, x2

	puts	"Terminated by signal "
	mov	w0, w21
	bl	putdec
	puts	", no error, iterations="
	ldr	x0, [x20, #ucontext_regs + 8 * 22]
	bl	putdec
	puts	", signals="
	ldr	x0, [x20, #ucontext_regs + 8 * 23]
	bl	putdecn

	mov	x0, #0
	mov	x8, #__NR_exit
	svc	#0
endfunction

 
 
 
 
function setsignal
	str	x30, [sp, #-((sa_sz + 15) / 16 * 16 + 16)]!

	mov	w4, w0
	mov	x5, x1
	mov	w6, w2

	add	x0, sp, #16
	mov	x1, #sa_sz
	bl	memclr

	mov	w0, w4
	add	x1, sp, #16
	str	w6, [x1, #sa_flags]
	str	x5, [x1, #sa_handler]
	mov	x2, #0
	mov	x3, #sa_mask_sz
	mov	x8, #__NR_rt_sigaction
	svc	#0

	cbz	w0, 1f

	puts	"sigaction failure\n"
	b	.Labort

1:	ldr	x30, [sp], #((sa_sz + 15) / 16 * 16 + 16)
	ret
endfunction

 
.globl _start
function _start
	mov	x23, #0		 

	mov	w0, #SIGINT
	adr	x1, terminate_handler
	mov	w2, #SA_SIGINFO
	bl	setsignal

	mov	w0, #SIGTERM
	adr	x1, terminate_handler
	mov	w2, #SA_SIGINFO
	bl	setsignal

	mov	w0, #SIGUSR1
	adr	x1, irritator_handler
	mov	w2, #SA_SIGINFO
	orr	w2, w2, #SA_NODEFER
	bl	setsignal

	mov	w0, #SIGUSR2
	adr	x1, tickle_handler
	mov	w2, #SA_SIGINFO
	orr	w2, w2, #SA_NODEFER
	bl	setsignal

	 

	mov	x19, #128
	cmp	x19, #128
	b.lo	1f
	cmp	x19, #2048
	b.hi	1f
	tst	x19, #(8 - 1)
	b.eq	2f

1:	puts	"Bad vector length: "
	mov	x0, x19
	bl	putdecn
	b	.Labort

2:	puts	"Vector length:\t"
	mov	x0, x19
	bl	putdec
	puts	" bits\n"

	 

	mov	x8, #__NR_getpid
	svc	#0
	mov	x20, x0

	puts	"PID:\t"
	mov	x0, x20
	bl	putdecn

	mov	x22, #0		 
.Ltest_loop:

	mov	x21, #0		 
0:	mov	x0, x20
	mov	x1, x21
	and	x2, x22, #0xf
	bl	setup_vreg
	add	x21, x21, #1
	cmp	x21, #NVR
	b.lo	0b

 
	mov	x8, #__NR_sched_yield	 
	svc	#0

	mov	x21, #0
0:	mov	x0, x21
	bl	check_vreg
	add	x21, x21, #1
	cmp	x21, #NVR
	b.lo	0b

	add	x22, x22, #1
	b	.Ltest_loop

.Labort:
	mov	x0, #0
	mov	x1, #SIGABRT
	mov	x8, #__NR_kill
	svc	#0
endfunction

function barf
	mov	x10, x0	 
	mov	x11, x1	 
	mov	x12, x2	 

	puts	"Mismatch: PID="
	mov	x0, x20
	bl	putdec
	puts	", iteration="
	mov	x0, x22
	bl	putdec
	puts	", reg="
	mov	x0, x21
	bl	putdecn
	puts	"\tExpected ["
	mov	x0, x10
	mov	x1, x12
	bl	dumphex
	puts	"]\n\tGot      ["
	mov	x0, x11
	mov	x1, x12
	bl	dumphex
	puts	"]\n"

	mov	x8, #__NR_exit
	mov	x1, #1
	svc	#0
endfunction
