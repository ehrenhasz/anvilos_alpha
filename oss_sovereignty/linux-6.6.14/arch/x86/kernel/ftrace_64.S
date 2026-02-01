 
 

#include <linux/cfi_types.h>
#include <linux/linkage.h>
#include <asm/asm-offsets.h>
#include <asm/ptrace.h>
#include <asm/ftrace.h>
#include <asm/export.h>
#include <asm/nospec-branch.h>
#include <asm/unwind_hints.h>
#include <asm/frame.h>

	.code64
	.section .text, "ax"

#ifdef CONFIG_FRAME_POINTER
 
#  define MCOUNT_FRAME_SIZE	(8+16*2)
#else
 
# define MCOUNT_FRAME_SIZE	0
#endif  

 
#define MCOUNT_REG_SIZE		(FRAME_SIZE + MCOUNT_FRAME_SIZE)

 

 
.macro save_mcount_regs added=0

#ifdef CONFIG_FRAME_POINTER
	 
	pushq %rbp

	 

	 
	pushq \added+8*2(%rsp)
	pushq %rbp
	movq %rsp, %rbp
	 
	pushq \added+8*3(%rsp)
	pushq %rbp
	movq %rsp, %rbp
#endif  

	 
	subq $(FRAME_SIZE), %rsp
	movq %rax, RAX(%rsp)
	movq %rcx, RCX(%rsp)
	movq %rdx, RDX(%rsp)
	movq %rsi, RSI(%rsp)
	movq %rdi, RDI(%rsp)
	movq %r8, R8(%rsp)
	movq %r9, R9(%rsp)
	movq $0, ORIG_RAX(%rsp)
	 
#ifdef CONFIG_FRAME_POINTER
	movq MCOUNT_REG_SIZE-8(%rsp), %rdx
#else
	movq %rbp, %rdx
#endif
	movq %rdx, RBP(%rsp)

	 
	movq MCOUNT_REG_SIZE+8+\added(%rsp), %rsi

	  
	movq MCOUNT_REG_SIZE+\added(%rsp), %rdi
	movq %rdi, RIP(%rsp)

	 
	subq $MCOUNT_INSN_SIZE, %rdi
	.endm

.macro restore_mcount_regs save=0

	 
	movq RBP(%rsp), %rbp

	movq R9(%rsp), %r9
	movq R8(%rsp), %r8
	movq RDI(%rsp), %rdi
	movq RSI(%rsp), %rsi
	movq RDX(%rsp), %rdx
	movq RCX(%rsp), %rcx
	movq RAX(%rsp), %rax

	addq $MCOUNT_REG_SIZE-\save, %rsp

	.endm

SYM_TYPED_FUNC_START(ftrace_stub)
	CALL_DEPTH_ACCOUNT
	RET
SYM_FUNC_END(ftrace_stub)

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
SYM_TYPED_FUNC_START(ftrace_stub_graph)
	CALL_DEPTH_ACCOUNT
	RET
SYM_FUNC_END(ftrace_stub_graph)
#endif

#ifdef CONFIG_DYNAMIC_FTRACE

SYM_FUNC_START(__fentry__)
	CALL_DEPTH_ACCOUNT
	RET
SYM_FUNC_END(__fentry__)
EXPORT_SYMBOL(__fentry__)

SYM_FUNC_START(ftrace_caller)
	 
	save_mcount_regs

	CALL_DEPTH_ACCOUNT

	 
	leaq MCOUNT_REG_SIZE+8(%rsp), %rcx
	movq %rcx, RSP(%rsp)

SYM_INNER_LABEL(ftrace_caller_op_ptr, SYM_L_GLOBAL)
	ANNOTATE_NOENDBR
	 
	movq function_trace_op(%rip), %rdx

	 
	leaq (%rsp), %rcx

	 
	movq $0, CS(%rsp)

	 
	CALL_DEPTH_ACCOUNT

SYM_INNER_LABEL(ftrace_call, SYM_L_GLOBAL)
	ANNOTATE_NOENDBR
	call ftrace_stub

	 
	movq RIP(%rsp), %rax
	movq %rax, MCOUNT_REG_SIZE(%rsp)

	restore_mcount_regs

	 
SYM_INNER_LABEL(ftrace_caller_end, SYM_L_GLOBAL)
	ANNOTATE_NOENDBR
	RET
SYM_FUNC_END(ftrace_caller);
STACK_FRAME_NON_STANDARD_FP(ftrace_caller)

SYM_FUNC_START(ftrace_regs_caller)
	 
	pushfq

	 
	save_mcount_regs 8
	 

	CALL_DEPTH_ACCOUNT

SYM_INNER_LABEL(ftrace_regs_caller_op_ptr, SYM_L_GLOBAL)
	ANNOTATE_NOENDBR
	 
	movq function_trace_op(%rip), %rdx

	 
	movq %r15, R15(%rsp)
	movq %r14, R14(%rsp)
	movq %r13, R13(%rsp)
	movq %r12, R12(%rsp)
	movq %r11, R11(%rsp)
	movq %r10, R10(%rsp)
	movq %rbx, RBX(%rsp)
	 
	movq MCOUNT_REG_SIZE(%rsp), %rcx
	movq %rcx, EFLAGS(%rsp)
	 
	movq $__KERNEL_DS, %rcx
	movq %rcx, SS(%rsp)
	movq $__KERNEL_CS, %rcx
	movq %rcx, CS(%rsp)
	 
	leaq MCOUNT_REG_SIZE+8*2(%rsp), %rcx
	movq %rcx, RSP(%rsp)

	ENCODE_FRAME_POINTER

	 
	leaq (%rsp), %rcx

	 
	CALL_DEPTH_ACCOUNT

SYM_INNER_LABEL(ftrace_regs_call, SYM_L_GLOBAL)
	ANNOTATE_NOENDBR
	call ftrace_stub

	 
	movq EFLAGS(%rsp), %rax
	movq %rax, MCOUNT_REG_SIZE(%rsp)

	 
	movq RIP(%rsp), %rax
	movq %rax, MCOUNT_REG_SIZE+8(%rsp)

	 
	movq R15(%rsp), %r15
	movq R14(%rsp), %r14
	movq R13(%rsp), %r13
	movq R12(%rsp), %r12
	movq R10(%rsp), %r10
	movq RBX(%rsp), %rbx

	movq ORIG_RAX(%rsp), %rax
	movq %rax, MCOUNT_REG_SIZE-8(%rsp)

	 
	testq	%rax, %rax
SYM_INNER_LABEL(ftrace_regs_caller_jmp, SYM_L_GLOBAL)
	ANNOTATE_NOENDBR
	jnz	1f

	restore_mcount_regs
	 
	popfq

	 
SYM_INNER_LABEL(ftrace_regs_caller_end, SYM_L_GLOBAL)
	ANNOTATE_NOENDBR
	RET

	 
1:	movq MCOUNT_REG_SIZE(%rsp), %rdi
	movq %rdi, MCOUNT_REG_SIZE-8(%rsp)
	movq %rax, MCOUNT_REG_SIZE(%rsp)

	restore_mcount_regs 8
	 
	popfq
	UNWIND_HINT_FUNC

	 
	ANNOTATE_INTRA_FUNCTION_CALL
	CALL .Ldo_rebalance
	int3
.Ldo_rebalance:
	add $8, %rsp
	ALTERNATIVE __stringify(RET), \
		    __stringify(ANNOTATE_UNRET_SAFE; ret; int3), \
		    X86_FEATURE_CALL_DEPTH

SYM_FUNC_END(ftrace_regs_caller)
STACK_FRAME_NON_STANDARD_FP(ftrace_regs_caller)

SYM_FUNC_START(ftrace_stub_direct_tramp)
	CALL_DEPTH_ACCOUNT
	RET
SYM_FUNC_END(ftrace_stub_direct_tramp)

#else  

SYM_FUNC_START(__fentry__)
	CALL_DEPTH_ACCOUNT

	cmpq $ftrace_stub, ftrace_trace_function
	jnz trace
	RET

trace:
	 
	save_mcount_regs

	 
	movq ftrace_trace_function, %r8
	CALL_NOSPEC r8
	restore_mcount_regs

	jmp ftrace_stub
SYM_FUNC_END(__fentry__)
EXPORT_SYMBOL(__fentry__)
STACK_FRAME_NON_STANDARD_FP(__fentry__)

#endif  

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
SYM_CODE_START(return_to_handler)
	UNWIND_HINT_UNDEFINED
	ANNOTATE_NOENDBR
	subq  $24, %rsp

	 
	movq %rax, (%rsp)
	movq %rdx, 8(%rsp)
	movq %rbp, 16(%rsp)
	movq %rsp, %rdi

	call ftrace_return_to_handler

	movq %rax, %rdi
	movq 8(%rsp), %rdx
	movq (%rsp), %rax

	addq $24, %rsp
	 
	ANNOTATE_INTRA_FUNCTION_CALL
	call .Ldo_rop
	int3
.Ldo_rop:
	mov %rdi, (%rsp)
	ALTERNATIVE __stringify(RET), \
		    __stringify(ANNOTATE_UNRET_SAFE; ret; int3), \
		    X86_FEATURE_CALL_DEPTH
SYM_CODE_END(return_to_handler)
#endif
