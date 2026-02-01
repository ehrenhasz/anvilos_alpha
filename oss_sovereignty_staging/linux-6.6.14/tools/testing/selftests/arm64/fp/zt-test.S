 
 
 
 
 
 
 

#include <asm/unistd.h>
#include "assembler.h"
#include "asm-offsets.h"
#include "sme-inst.h"

.arch_extension sve

#define ZT_SZ	512
#define ZT_B	(ZT_SZ / 8)

 
 
.pushsection .text
.data
.align 4
ztref:
	.space	ZT_B
scratch:
	.space	ZT_B
.popsection


 
 
 

 
 
 
 
 

function pattern
	mov	w3, wzr
	bfi	w3, w0, #8, #16		 
	bfi	w3, w1, #24, #8		 

	ldr	x0, =scratch
	mov	w1, #ZT_B / 4

0:	str	w3, [x0], #4
	add	w3, w3, #1		 
	subs	w1, w1, #1
	b.ne	0b

	ret
endfunction

 
 
 
function setup_zt
	mov	x4, x30

	bl	pattern			 
	ldr	x0, =ztref
	ldr	x1, =scratch
	mov	x2, #ZT_B
	bl	memcpy

	ldr	x0, =ztref
	_ldr_zt 0			 

	ret	x4
endfunction

 
 
 
 
function memcmp
	cbz	x2, 2f

	stp	x0, x1, [sp, #-0x20]!
	str	x2, [sp, #0x10]

	mov	x5, #0
0:	ldrb	w3, [x0, x5]
	ldrb	w4, [x1, x5]
	add	x5, x5, #1
	cmp	w3, w4
	b.ne	1f
	subs	x2, x2, #1
	b.ne	0b

1:	ldr	x2, [sp, #0x10]
	ldp	x0, x1, [sp], #0x20
	b.ne	barf

2:	ret
endfunction

 
 
function check_zt
	mov	x3, x30

	ldr	x0, =scratch		 
	mov	x1, #ZT_B
	bl	memfill_ae

	ldr	x0, =scratch
	_str_zt 0

	ldr	x0, =ztref
	ldr	x1, =scratch
	mov	x2, #ZT_B
	mov	x30, x3
	b	memcmp
endfunction

 
 
function irritator_handler
	 
	ldr	x0, [x2, #ucontext_regs + 8 * 23]
	add	x0, x0, #1
	str	x0, [x2, #ucontext_regs + 8 * 23]

	 
#if 0
	adr	x0, .text + (irritator_handler - .text) / 16 * 16
	movi	v0.8b, #1
	movi	v9.16b, #2
	movi	v31.8b, #3
#endif

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

	smstart_za

	 
	mov	x8, #__NR_getpid
	svc	#0
	mov	x20, x0

	puts	"PID:\t"
	mov	x0, x20
	bl	putdecn

	mov	x22, #0		 
.Ltest_loop:
	mov	x0, x20
	mov	x1, x22
	bl	setup_zt

	mov	x8, #__NR_sched_yield	 
	svc	#0

	mrs	x0, S3_3_C4_C2_2	 
	and	x1, x0, #3
	cmp	x1, #2
	b.ne	svcr_barf

	bl	check_zt

	add	x22, x22, #1	 
	b	.Ltest_loop

.Labort:
	mov	x0, #0
	mov	x1, #SIGABRT
	mov	x8, #__NR_kill
	svc	#0
endfunction

function barf
 
 
 
 
 
	smstop
	mov	x10, x0	 
	mov	x11, x1	 
	mov	x12, x2	 

	puts	"Mismatch: PID="
	mov	x0, x20
	bl	putdec
	puts	", iteration="
	mov	x0, x22
	bl	putdec
	puts	"\tExpected ["
	mov	x0, x10
	mov	x1, x12
	bl	dumphex
	puts	"]\n\tGot      ["
	mov	x0, x11
	mov	x1, x12
	bl	dumphex
	puts	"]\n"

	mov	x8, #__NR_getpid
	svc	#0
 
 
 
 
 
	mov	x1, #SIGABRT
	mov	x8, #__NR_kill
	svc	#0
 
 
 
endfunction

function svcr_barf
	mov	x10, x0

	puts	"Bad SVCR: "
	mov	x0, x10
	bl	putdecn

	mov	x8, #__NR_exit
	mov	x1, #1
	svc	#0
endfunction
