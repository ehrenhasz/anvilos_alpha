


#include <linux/linkage.h>
#include <linux/objtool.h>
#include <asm/page_types.h>
#include <asm/segment.h>

	.text
	.code64
SYM_FUNC_START(__efi64_thunk)
STACK_FRAME_NON_STANDARD __efi64_thunk
	push	%rbp
	push	%rbx

	
	movq	%rsp, %rax
	movq	efi_mixed_mode_stack_pa(%rip), %rsp
	push	%rax

	
	subq	$0x24, %rsp
	movq	0x18(%rax), %rbp
	movq	0x20(%rax), %rbx
	movq	0x28(%rax), %rax
	movl	%ebp, 0x18(%rsp)
	movl	%ebx, 0x1c(%rsp)
	movl	%eax, 0x20(%rsp)

	
	movq	$__START_KERNEL_map, %rax
	subq	phys_base(%rip), %rax

	leaq	1f(%rip), %rbp
	leaq	2f(%rip), %rbx
	subq	%rax, %rbp
	subq	%rax, %rbx

	movl	%ebx, 0x0(%rsp)		
	movl	%esi, 0x4(%rsp)
	movl	%edx, 0x8(%rsp)
	movl	%ecx, 0xc(%rsp)
	movl	%r8d, 0x10(%rsp)
	movl	%r9d, 0x14(%rsp)

	
	pushq	$__KERNEL32_CS
	pushq	%rdi			
	lretq

	
	
	
	RET
SYM_FUNC_END(__efi64_thunk)

	.section ".rodata", "a", @progbits
	.balign	16
SYM_DATA_START(__efi64_thunk_ret_tramp)
1:	movq	0x20(%rsp), %rsp
	pop	%rbx
	pop	%rbp
	ret
	int3

	.code32
2:	pushl	$__KERNEL_CS
	pushl	%ebp
	lret
SYM_DATA_END(__efi64_thunk_ret_tramp)

	.bss
	.balign 8
SYM_DATA(efi_mixed_mode_stack_pa, .quad 0)
