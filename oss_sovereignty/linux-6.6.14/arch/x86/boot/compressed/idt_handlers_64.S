 
 

#include <asm/segment.h>

 
#include "../../entry/calling.h"

.macro EXCEPTION_HANDLER name function error_code=0
SYM_FUNC_START(\name)

	 
	.if \error_code == 0
	pushq   $0
	.endif

	pushq   %rdi
	pushq   %rsi
	pushq   %rdx
	pushq   %rcx
	pushq   %rax
	pushq   %r8
	pushq   %r9
	pushq   %r10
	pushq   %r11
	pushq   %rbx
	pushq   %rbp
	pushq   %r12
	pushq   %r13
	pushq   %r14
	pushq   %r15

	 
	movq    %rsp, %rdi
	 
	movq	ORIG_RAX(%rsp), %rsi
	call    \function

	 
	popq    %r15
	popq    %r14
	popq    %r13
	popq    %r12
	popq    %rbp
	popq    %rbx
	popq    %r11
	popq    %r10
	popq    %r9
	popq    %r8
	popq    %rax
	popq    %rcx
	popq    %rdx
	popq    %rsi
	popq    %rdi

	 
	addq    $8, %rsp

	iretq
SYM_FUNC_END(\name)
	.endm

	.text
	.code64

EXCEPTION_HANDLER	boot_page_fault do_boot_page_fault error_code=1

#ifdef CONFIG_AMD_MEM_ENCRYPT
EXCEPTION_HANDLER	boot_stage1_vc do_vc_no_ghcb		error_code=1
EXCEPTION_HANDLER	boot_stage2_vc do_boot_stage2_vc	error_code=1
#endif
