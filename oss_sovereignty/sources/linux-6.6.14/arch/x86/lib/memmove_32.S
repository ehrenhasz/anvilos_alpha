

#include <linux/linkage.h>
#include <asm/export.h>

SYM_FUNC_START(memmove)

.set dest_in, %eax
.set dest, %edi
.set src_in, %edx
.set src, %esi
.set n, %ecx
.set tmp0, %edx
.set tmp0w, %dx
.set tmp1, %ebx
.set tmp1w, %bx
.set tmp2, %eax
.set tmp3b, %cl


	pushl	%ebp
	movl	%esp, %ebp	

	pushl	%ebx
	pushl	%edi
	pushl	%esi
	pushl	%eax		

	movl src_in, src
	movl dest_in, dest

	
	cmpl	$0x10, n
	jb	.Lmove_16B

	
	cmpl	dest, src
	jb	.Lbackwards_header

	
	cmpl	$680, n
	jb	.Ltoo_small_forwards
	
	movl	src, tmp0
	xorl	dest, tmp0
	andl	$0xff, tmp0
	jz	.Lforward_movs
.Ltoo_small_forwards:
	subl	$0x10, n

	
.Lmove_16B_forwards_loop:
	subl	$0x10, n
	movl	0*4(src), tmp0
	movl	1*4(src), tmp1
	movl	tmp0, 0*4(dest)
	movl	tmp1, 1*4(dest)
	movl	2*4(src), tmp0
	movl	3*4(src), tmp1
	movl	tmp0, 2*4(dest)
	movl	tmp1, 3*4(dest)
	leal	0x10(src), src
	leal	0x10(dest), dest
	jae	.Lmove_16B_forwards_loop
	addl	$0x10, n
	jmp	.Lmove_16B

	
.p2align 4
.Lforward_movs:
	movl	-4(src, n), tmp0
	leal	-4(dest, n), tmp1
	shrl	$2, n
	rep	movsl
	movl	tmp0, (tmp1)
	jmp	.Ldone

	
.p2align 4
.Lbackwards_movs:
	movl	(src), tmp0
	movl	dest, tmp1
	leal	-4(src, n), src
	leal	-4(dest, n), dest
	shrl	$2, n
	std
	rep	movsl
	movl	tmp0,(tmp1)
	cld
	jmp	.Ldone

	
.p2align 4
.Lbackwards_header:
	cmpl	$680, n
	jb	.Ltoo_small_backwards
	movl	src, tmp0
	xorl	dest, tmp0
	andl	$0xff, tmp0
	jz	.Lbackwards_movs

	
.Ltoo_small_backwards:
	addl	n, src
	addl	n, dest
	subl	$0x10, n

	
.Lmove_16B_backwards_loop:
	subl	$0x10, n

	movl	-1*4(src), tmp0
	movl	-2*4(src), tmp1
	movl	tmp0, -1*4(dest)
	movl	tmp1, -2*4(dest)
	movl	-3*4(src), tmp0
	movl	-4*4(src), tmp1
	movl	tmp0, -3*4(dest)
	movl	tmp1, -4*4(dest)
	leal	-0x10(src), src
	leal	-0x10(dest), dest
	jae	.Lmove_16B_backwards_loop
	
	addl	$0x10, n
	subl	n, src
	subl	n, dest

	
.p2align 4
.Lmove_16B:
	cmpl	$8, n
	jb	.Lmove_8B
	movl	0*4(src), tmp0
	movl	1*4(src), tmp1
	movl	-2*4(src, n), tmp2
	movl	-1*4(src, n), src

	movl	tmp0, 0*4(dest)
	movl	tmp1, 1*4(dest)
	movl	tmp2, -2*4(dest, n)
	movl	src, -1*4(dest, n)
	jmp	.Ldone

	
.p2align 4
.Lmove_8B:
	cmpl	$4, n
	jb	.Lmove_4B
	movl	0*4(src), tmp0
	movl	-1*4(src, n), tmp1
	movl	tmp0, 0*4(dest)
	movl	tmp1, -1*4(dest, n)
	jmp	.Ldone

	
.p2align 4
.Lmove_4B:
	cmpl	$2, n
	jb	.Lmove_1B
	movw	0*2(src), tmp0w
	movw	-1*2(src, n), tmp1w
	movw	tmp0w, 0*2(dest)
	movw	tmp1w, -1*2(dest, n)
	jmp	.Ldone

	
.p2align 4
.Lmove_1B:
	cmpl	$1, n
	jb	.Ldone
	movb	(src), tmp3b
	movb	tmp3b, (dest)
.p2align 4
.Ldone:
	popl	dest_in	
	
	popl	%esi
	popl	%edi
	popl	%ebx
	popl	%ebp

	RET
SYM_FUNC_END(memmove)
EXPORT_SYMBOL(memmove)
