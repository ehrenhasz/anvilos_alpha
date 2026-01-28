
#include <asm/asm-offsets.h>
#include <asm/tdx.h>


#define tdcall		.byte 0x66,0x0f,0x01,0xcc
#define seamcall	.byte 0x66,0x0f,0x01,0xcf


.macro TDX_MODULE_CALL host:req
	

	
	push %r12

	
	push %r9

	
	
	mov %rdi, %rax
	
	mov %r8,  %r9
	
	mov %rcx, %r8
	
	mov %rsi, %rcx
	

	.if \host
	seamcall
	
	jnc .Lno_vmfailinvalid
	mov $TDX_SEAMCALL_VMFAILINVALID, %rax
.Lno_vmfailinvalid:

	.else
	tdcall
	.endif

	
	pop %r12

	
	test %r12, %r12
	jz .Lno_output_struct

	
	movq %rcx, TDX_MODULE_rcx(%r12)
	movq %rdx, TDX_MODULE_rdx(%r12)
	movq %r8,  TDX_MODULE_r8(%r12)
	movq %r9,  TDX_MODULE_r9(%r12)
	movq %r10, TDX_MODULE_r10(%r12)
	movq %r11, TDX_MODULE_r11(%r12)

.Lno_output_struct:
	
	pop %r12
.endm
