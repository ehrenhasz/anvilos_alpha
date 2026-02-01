 
 

 

 
	.text

#include <linux/init.h>
#include <linux/linkage.h>
#include <asm/segment.h>
#include <asm/page_types.h>
#include <asm/boot.h>
#include <asm/asm-offsets.h>
#include <asm/bootparam.h>

 
	.hidden _bss
	.hidden _ebss
	.hidden _end

	__HEAD
SYM_FUNC_START(startup_32)
	cld
	cli

 
	leal	(BP_scratch+4)(%esi), %esp
	call	1f
1:	popl	%edx
	addl	$_GLOBAL_OFFSET_TABLE_+(.-1b), %edx

	 
	leal	gdt@GOTOFF(%edx), %eax
	movl	%eax, 2(%eax)
	lgdt	(%eax)

	 
	movl	$__BOOT_DS, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %fs
	movl	%eax, %gs
	movl	%eax, %ss

 

#ifdef CONFIG_RELOCATABLE
	leal	startup_32@GOTOFF(%edx), %ebx
	movl	BP_kernel_alignment(%esi), %eax
	decl	%eax
	addl    %eax, %ebx
	notl	%eax
	andl    %eax, %ebx
	cmpl	$LOAD_PHYSICAL_ADDR, %ebx
	jae	1f
#endif
	movl	$LOAD_PHYSICAL_ADDR, %ebx
1:

	movl	%ebx, %ebp	 
	 
	addl    BP_init_size(%esi), %ebx
	subl    $_end@GOTOFF, %ebx

	 
	leal	boot_stack_end@GOTOFF(%ebx), %esp

	 
	pushl	$0
	popfl

 
	pushl	%esi
	leal	(_bss@GOTOFF-4)(%edx), %esi
	leal	(_bss@GOTOFF-4)(%ebx), %edi
	movl	$(_bss - startup_32), %ecx
	shrl	$2, %ecx
	std
	rep	movsl
	cld
	popl	%esi

	 
	leal	gdt@GOTOFF(%ebx), %eax
	movl	%eax, 2(%eax)
	lgdt	(%eax)

 
	leal	.Lrelocated@GOTOFF(%ebx), %eax
	jmp	*%eax
SYM_FUNC_END(startup_32)

	.text
SYM_FUNC_START_LOCAL_NOALIGN(.Lrelocated)

 
	xorl	%eax, %eax
	leal	_bss@GOTOFF(%ebx), %edi
	leal	_ebss@GOTOFF(%ebx), %ecx
	subl	%edi, %ecx
	shrl	$2, %ecx
	rep	stosl

 
	 

	pushl	%ebp			 
	pushl	%esi			 
	call	extract_kernel		 
	addl	$24, %esp

 
	xorl	%ebx, %ebx
	jmp	*%eax
SYM_FUNC_END(.Lrelocated)

	.data
	.balign	8
SYM_DATA_START_LOCAL(gdt)
	.word	gdt_end - gdt - 1
	.long	0
	.word	0
	.quad	0x0000000000000000	 
	.quad	0x00cf9a000000ffff	 
	.quad	0x00cf92000000ffff	 
SYM_DATA_END_LABEL(gdt, SYM_L_LOCAL, gdt_end)

 
	.bss
	.balign 4
boot_stack:
	.fill BOOT_STACK_SIZE, 1, 0
boot_stack_end:
