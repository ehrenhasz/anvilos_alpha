 
 

#include <linux/linkage.h>
#include <linux/init.h>
#include <asm/asm-offsets.h>
#include <asm/page_types.h>

	__INIT
SYM_FUNC_START(efi_call_svam)
	push	%ebp
	movl	%esp, %ebp
	push	%ebx

	push	16(%esp)
	push	16(%esp)
	push	%ecx
	push	%edx
	movl	%eax, %ebx		 

	 
	movl	$1f, %edx
	subl	$__PAGE_OFFSET, %edx
	jmp	*%edx
1:

	 
	movl	%cr0, %edx
	andl	$0x7fffffff, %edx
	movl	%edx, %cr0

	 
	subl	$__PAGE_OFFSET, %esp

	 
	movl	(%eax), %eax
	call	*EFI_svam(%eax)

	 
	movl	(%ebx), %ecx
	movl	36(%esp), %edx		 
	movl	%ecx, (%edx)

	 
	movl	%cr0, %edx
	orl	$0x80000000, %edx
	movl	%edx, %cr0

	movl	16(%esp), %ebx
	leave
	RET
SYM_FUNC_END(efi_call_svam)
