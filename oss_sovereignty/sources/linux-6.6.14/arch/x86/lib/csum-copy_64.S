
#include <linux/linkage.h>
#include <asm/errno.h>
#include <asm/asm.h>



	.macro source
10:
	_ASM_EXTABLE_UA(10b, .Lfault)
	.endm

	.macro dest
20:
	_ASM_EXTABLE_UA(20b, .Lfault)
	.endm

SYM_FUNC_START(csum_partial_copy_generic)
	subq  $5*8, %rsp
	movq  %rbx, 0*8(%rsp)
	movq  %r12, 1*8(%rsp)
	movq  %r14, 2*8(%rsp)
	movq  %r13, 3*8(%rsp)
	movq  %r15, 4*8(%rsp)

	movl  $-1, %eax
	xorl  %r9d, %r9d
	movl  %edx, %ecx
	cmpl  $8, %ecx
	jb    .Lshort

	testb  $7, %sil
	jne   .Lunaligned
.Laligned:
	movl  %ecx, %r12d

	shrq  $6, %r12
	jz	.Lhandle_tail       

	clc

	
	
	
	
	.p2align 4
.Lloop:
	source
	movq  (%rdi), %rbx
	source
	movq  8(%rdi), %r8
	source
	movq  16(%rdi), %r11
	source
	movq  24(%rdi), %rdx

	source
	movq  32(%rdi), %r10
	source
	movq  40(%rdi), %r15
	source
	movq  48(%rdi), %r14
	source
	movq  56(%rdi), %r13

30:
	
	_ASM_EXTABLE(30b, 2f)
	prefetcht0 5*64(%rdi)
2:
	adcq  %rbx, %rax
	adcq  %r8, %rax
	adcq  %r11, %rax
	adcq  %rdx, %rax
	adcq  %r10, %rax
	adcq  %r15, %rax
	adcq  %r14, %rax
	adcq  %r13, %rax

	decl %r12d

	dest
	movq %rbx, (%rsi)
	dest
	movq %r8, 8(%rsi)
	dest
	movq %r11, 16(%rsi)
	dest
	movq %rdx, 24(%rsi)

	dest
	movq %r10, 32(%rsi)
	dest
	movq %r15, 40(%rsi)
	dest
	movq %r14, 48(%rsi)
	dest
	movq %r13, 56(%rsi)

	leaq 64(%rdi), %rdi
	leaq 64(%rsi), %rsi

	jnz	.Lloop

	adcq  %r9, %rax

	
.Lhandle_tail:
	
	movq %rcx, %r10
	andl $63, %ecx
	shrl $3, %ecx
	jz	.Lfold
	clc
	.p2align 4
.Lloop_8:
	source
	movq (%rdi), %rbx
	adcq %rbx, %rax
	decl %ecx
	dest
	movq %rbx, (%rsi)
	leaq 8(%rsi), %rsi 
	leaq 8(%rdi), %rdi
	jnz	.Lloop_8
	adcq %r9, %rax	

.Lfold:
	
	movl %eax, %ebx
	shrq $32, %rax
	addl %ebx, %eax
	adcl %r9d, %eax

	
.Lhandle_7:
	movl %r10d, %ecx
	andl $7, %ecx
.L1:				
	shrl $1, %ecx
	jz   .Lhandle_1
	movl $2, %edx
	xorl %ebx, %ebx
	clc
	.p2align 4
.Lloop_1:
	source
	movw (%rdi), %bx
	adcl %ebx, %eax
	decl %ecx
	dest
	movw %bx, (%rsi)
	leaq 2(%rdi), %rdi
	leaq 2(%rsi), %rsi
	jnz .Lloop_1
	adcl %r9d, %eax	

	
.Lhandle_1:
	testb $1, %r10b
	jz    .Lende
	xorl  %ebx, %ebx
	source
	movb (%rdi), %bl
	dest
	movb %bl, (%rsi)
	addl %ebx, %eax
	adcl %r9d, %eax		

.Lende:
	testq %r10, %r10
	js  .Lwas_odd
.Lout:
	movq 0*8(%rsp), %rbx
	movq 1*8(%rsp), %r12
	movq 2*8(%rsp), %r14
	movq 3*8(%rsp), %r13
	movq 4*8(%rsp), %r15
	addq $5*8, %rsp
	RET
.Lshort:
	movl %ecx, %r10d
	jmp  .L1
.Lunaligned:
	xorl %ebx, %ebx
	testb $1, %sil
	jne  .Lodd
1:	testb $2, %sil
	je   2f
	source
	movw (%rdi), %bx
	dest
	movw %bx, (%rsi)
	leaq 2(%rdi), %rdi
	subq $2, %rcx
	leaq 2(%rsi), %rsi
	addq %rbx, %rax
2:	testb $4, %sil
	je .Laligned
	source
	movl (%rdi), %ebx
	dest
	movl %ebx, (%rsi)
	leaq 4(%rdi), %rdi
	subq $4, %rcx
	leaq 4(%rsi), %rsi
	addq %rbx, %rax
	jmp .Laligned

.Lodd:
	source
	movb (%rdi), %bl
	dest
	movb %bl, (%rsi)
	leaq 1(%rdi), %rdi
	leaq 1(%rsi), %rsi
	
	leaq -1(%rcx, %rcx), %rcx
	rorq $1, %rcx
	shll $8, %ebx
	addq %rbx, %rax
	jmp 1b

.Lwas_odd:
	roll $8, %eax
	jmp .Lout

	
.Lfault:
	xorl %eax, %eax
	jmp  .Lout
SYM_FUNC_END(csum_partial_copy_generic)
