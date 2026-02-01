 
#include <asm/asm-offsets.h>
#include <asm/asm.h>
#include <asm/frame.h>
#include <asm/unwind_hints.h>

#include <linux/linkage.h>
#include <linux/bits.h>
#include <linux/errno.h>

#include "../../virt/vmx/tdx/tdxcall.S"

 
#define TDX_RDX		BIT(2)
#define TDX_RBX		BIT(3)
#define TDX_RSI		BIT(6)
#define TDX_RDI		BIT(7)
#define TDX_R8		BIT(8)
#define TDX_R9		BIT(9)
#define TDX_R10		BIT(10)
#define TDX_R11		BIT(11)
#define TDX_R12		BIT(12)
#define TDX_R13		BIT(13)
#define TDX_R14		BIT(14)
#define TDX_R15		BIT(15)

 
#define TDVMCALL_EXPOSE_REGS_MASK	\
	( TDX_RDX | TDX_RBX | TDX_RSI | TDX_RDI | TDX_R8  | TDX_R9  | \
	  TDX_R10 | TDX_R11 | TDX_R12 | TDX_R13 | TDX_R14 | TDX_R15 )

.section .noinstr.text, "ax"

 
SYM_FUNC_START(__tdx_module_call)
	FRAME_BEGIN
	TDX_MODULE_CALL host=0
	FRAME_END
	RET
SYM_FUNC_END(__tdx_module_call)

 
.macro TDX_HYPERCALL ret:req
	FRAME_BEGIN

	 
	push %r15
	push %r14
	push %r13
	push %r12
	push %rbx

	 
	movq %rdi, %rax

	 
	movq TDX_HYPERCALL_r8(%rax),  %r8
	movq TDX_HYPERCALL_r9(%rax),  %r9
	movq TDX_HYPERCALL_r10(%rax), %r10
	movq TDX_HYPERCALL_r11(%rax), %r11
	movq TDX_HYPERCALL_r12(%rax), %r12
	movq TDX_HYPERCALL_r13(%rax), %r13
	movq TDX_HYPERCALL_r14(%rax), %r14
	movq TDX_HYPERCALL_r15(%rax), %r15
	movq TDX_HYPERCALL_rdi(%rax), %rdi
	movq TDX_HYPERCALL_rsi(%rax), %rsi
	movq TDX_HYPERCALL_rbx(%rax), %rbx
	movq TDX_HYPERCALL_rdx(%rax), %rdx

	push %rax

	 
	 
	xor %eax, %eax

	movl $TDVMCALL_EXPOSE_REGS_MASK, %ecx

	tdcall

	 
	testq %rax, %rax
	jne .Lpanic\@

	pop %rax

	.if \ret
	movq %r8,  TDX_HYPERCALL_r8(%rax)
	movq %r9,  TDX_HYPERCALL_r9(%rax)
	movq %r10, TDX_HYPERCALL_r10(%rax)
	movq %r11, TDX_HYPERCALL_r11(%rax)
	movq %r12, TDX_HYPERCALL_r12(%rax)
	movq %r13, TDX_HYPERCALL_r13(%rax)
	movq %r14, TDX_HYPERCALL_r14(%rax)
	movq %r15, TDX_HYPERCALL_r15(%rax)
	movq %rdi, TDX_HYPERCALL_rdi(%rax)
	movq %rsi, TDX_HYPERCALL_rsi(%rax)
	movq %rbx, TDX_HYPERCALL_rbx(%rax)
	movq %rdx, TDX_HYPERCALL_rdx(%rax)
	.endif

	 
	movq %r10, %rax

	 
	xor %r8d,  %r8d
	xor %r9d,  %r9d
	xor %r10d, %r10d
	xor %r11d, %r11d
	xor %rdi,  %rdi
	xor %rsi,  %rsi
	xor %rdx,  %rdx

	 
	pop %rbx
	pop %r12
	pop %r13
	pop %r14
	pop %r15

	FRAME_END

	RET
.Lpanic\@:
	call __tdx_hypercall_failed
	 
	REACHABLE
	jmp .Lpanic\@
.endm

 
SYM_FUNC_START(__tdx_hypercall)
	TDX_HYPERCALL ret=0
SYM_FUNC_END(__tdx_hypercall)

 
SYM_FUNC_START(__tdx_hypercall_ret)
	TDX_HYPERCALL ret=1
SYM_FUNC_END(__tdx_hypercall_ret)
