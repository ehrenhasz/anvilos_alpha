




	.code16
	.section ".inittext","ax"
	.globl	intcall
	.type	intcall, @function
intcall:
	
	cmpb	%al, 3f
	je	1f
	movb	%al, 3f
	jmp	1f		
1:
	
	pushfl
	pushw	%fs
	pushw	%gs
	pushal

	
	subw	$44, %sp
	movw	%dx, %si
	movw	%sp, %di
	movw	$11, %cx
	rep; movsl

	
	popal
	popw	%gs
	popw	%fs
	popw	%es
	popw	%ds
	popfl

	
	.byte	0xcd		
3:	.byte	0

	
	pushfl
	pushw	%ds
	pushw	%es
	pushw	%fs
	pushw	%gs
	pushal

	
	cld
	movzwl	%sp, %esp
	movw	%cs, %ax
	movw	%ax, %ds
	movw	%ax, %es

	
	movw	68(%esp), %di	
	andw	%di, %di
	jz	4f
	movw	%sp, %si
	movw	$11, %cx
	rep; movsl
4:	addw	$44, %sp

	
	popal
	popw	%gs
	popw	%fs
	popfl
	retl
	.size	intcall, .-intcall
