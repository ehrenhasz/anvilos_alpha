

#include <linux/linkage.h>
#include <asm/export.h>
#include <asm/errno.h>
#include <asm/enclu.h>

#include "extable.h"


#define SGX_ENCLAVE_OFFSET_OF_RUN		16


#define SGX_ENCLAVE_RUN_TCS			0
#define SGX_ENCLAVE_RUN_LEAF			8
#define SGX_ENCLAVE_RUN_EXCEPTION_VECTOR	12
#define SGX_ENCLAVE_RUN_EXCEPTION_ERROR_CODE	14
#define SGX_ENCLAVE_RUN_EXCEPTION_ADDR		16
#define SGX_ENCLAVE_RUN_USER_HANDLER		24
#define SGX_ENCLAVE_RUN_USER_DATA		32	
#define SGX_ENCLAVE_RUN_RESERVED_START		40
#define SGX_ENCLAVE_RUN_RESERVED_END		256

.code64
.section .text, "ax"

SYM_FUNC_START(__vdso_sgx_enter_enclave)
	
	.cfi_startproc
	push	%rbp
	.cfi_adjust_cfa_offset	8
	.cfi_rel_offset		%rbp, 0
	mov	%rsp, %rbp
	.cfi_def_cfa_register	%rbp
	push	%rbx
	.cfi_rel_offset		%rbx, -8

	mov	%ecx, %eax
.Lenter_enclave:
	
	cmp	$EENTER, %eax
	jb	.Linvalid_input
	cmp	$ERESUME, %eax
	ja	.Linvalid_input

	mov	SGX_ENCLAVE_OFFSET_OF_RUN(%rbp), %rcx

	
	mov	$SGX_ENCLAVE_RUN_RESERVED_START, %rbx
1:
	cmpq	$0, (%rcx, %rbx)
	jne	.Linvalid_input
	add	$8, %rbx
	cmpq	$SGX_ENCLAVE_RUN_RESERVED_END, %rbx
	jne	1b

	
	mov	SGX_ENCLAVE_RUN_TCS(%rcx), %rbx
	lea	.Lasync_exit_pointer(%rip), %rcx

	
.Lasync_exit_pointer:
.Lenclu_eenter_eresume:
	enclu

	
	mov	SGX_ENCLAVE_OFFSET_OF_RUN(%rbp), %rbx

	
	movl	$EEXIT, SGX_ENCLAVE_RUN_LEAF(%rbx)

	
.Lhandle_exit:
	cmpq	$0, SGX_ENCLAVE_RUN_USER_HANDLER(%rbx)
	jne	.Linvoke_userspace_handler

	
	xor	%eax, %eax

.Lout:
	pop	%rbx
	leave
	.cfi_def_cfa		%rsp, 8
	RET

	
	.cfi_def_cfa		%rbp, 16

.Linvalid_input:
	mov	$(-EINVAL), %eax
	jmp	.Lout

.Lhandle_exception:
	mov	SGX_ENCLAVE_OFFSET_OF_RUN(%rbp), %rbx

	
	mov	%eax, (SGX_ENCLAVE_RUN_LEAF)(%rbx)
	mov	%di,  (SGX_ENCLAVE_RUN_EXCEPTION_VECTOR)(%rbx)
	mov	%si,  (SGX_ENCLAVE_RUN_EXCEPTION_ERROR_CODE)(%rbx)
	mov	%rdx, (SGX_ENCLAVE_RUN_EXCEPTION_ADDR)(%rbx)
	jmp	.Lhandle_exit

.Linvoke_userspace_handler:
	
	mov	%rsp, %rcx

	
	mov	%rbx, %rax

	
	mov	%rsp, %rbx
	and	$0xf, %rbx

	
	and	$-0x10, %rsp
	push	%rax

	
	push	%rax

	
	cld

	
	mov	SGX_ENCLAVE_RUN_USER_HANDLER(%rax), %rax
	lfence
	call	*%rax

	
	lea	0x10(%rsp, %rbx), %rsp

	
	cmp	$0, %eax
	jle	.Lout
	jmp	.Lenter_enclave

	.cfi_endproc

_ASM_VDSO_EXTABLE_HANDLE(.Lenclu_eenter_eresume, .Lhandle_exception)

SYM_FUNC_END(__vdso_sgx_enter_enclave)
