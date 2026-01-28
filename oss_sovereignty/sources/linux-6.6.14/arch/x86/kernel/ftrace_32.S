


#include <linux/linkage.h>
#include <asm/page_types.h>
#include <asm/segment.h>
#include <asm/export.h>
#include <asm/ftrace.h>
#include <asm/nospec-branch.h>
#include <asm/frame.h>
#include <asm/asm-offsets.h>

#ifdef CONFIG_FRAME_POINTER
# define MCOUNT_FRAME			1	
#else
# define MCOUNT_FRAME			0	
#endif

SYM_FUNC_START(__fentry__)
	RET
SYM_FUNC_END(__fentry__)
EXPORT_SYMBOL(__fentry__)

SYM_CODE_START(ftrace_caller)

#ifdef CONFIG_FRAME_POINTER
	
	pushl	4(%esp)				
	pushl	%ebp
	movl	%esp, %ebp
	pushl	2*4(%esp)			

	
	pushl	%ebp
	movl	%esp, %ebp
#endif
	pushl	%eax
	pushl	%ecx
	pushl	%edx
	pushl	$0				

#ifdef CONFIG_FRAME_POINTER
	
	movl	4*4(%esp), %edx
#else
	
	lea	4*4(%esp), %edx
#endif

	movl	(MCOUNT_FRAME+4)*4(%esp), %eax	
	
	movl	4(%edx), %edx			

	movl	function_trace_op, %ecx
	subl	$MCOUNT_INSN_SIZE, %eax

.globl ftrace_call
ftrace_call:
	call	ftrace_stub

	addl	$4, %esp			
	popl	%edx
	popl	%ecx
	popl	%eax
#ifdef CONFIG_FRAME_POINTER
	popl	%ebp
	addl	$4,%esp				
	popl	%ebp				
	addl	$4, %esp			
#endif
.Lftrace_ret:
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
.globl ftrace_graph_call
ftrace_graph_call:
	jmp	ftrace_stub
#endif


SYM_INNER_LABEL_ALIGN(ftrace_stub, SYM_L_WEAK)
	RET
SYM_CODE_END(ftrace_caller)

SYM_CODE_START(ftrace_regs_caller)
	
	subl	$3*4, %esp	# RET-IP + 3 gaps
	pushl	%ss		# ss
	pushl	%esp		# points at ss
	addl	$5*4, (%esp)	#   make it point at <previous context>
	pushfl			# flags
	pushl	$__KERNEL_CS	# cs
	pushl	7*4(%esp)	# ip <- RET-IP
	pushl	$0		# orig_eax

	pushl	%gs
	pushl	%fs
	pushl	%es
	pushl	%ds

	pushl	%eax
	pushl	%ebp
	pushl	%edi
	pushl	%esi
	pushl	%edx
	pushl	%ecx
	pushl	%ebx

	ENCODE_FRAME_POINTER

	movl	PT_EIP(%esp), %eax	# 1st argument: IP
	subl	$MCOUNT_INSN_SIZE, %eax
	movl	21*4(%esp), %edx	# 2nd argument: parent ip
	movl	function_trace_op, %ecx	# 3rd argument: ftrace_pos
	pushl	%esp			# 4th argument: pt_regs

SYM_INNER_LABEL(ftrace_regs_call, SYM_L_GLOBAL)
	call	ftrace_stub

	addl	$4, %esp		# skip 4th argument

	
	movl	PT_OLDESP(%esp), %eax
	movl	PT_EIP(%esp), %ecx
	movl	%ecx, -4(%eax)

	
	movl	PT_EAX(%esp), %ecx
	movl	%ecx, -8(%eax)

	popl	%ebx
	popl	%ecx
	popl	%edx
	popl	%esi
	popl	%edi
	popl	%ebp

	lea	-8(%eax), %esp
	popl	%eax

	jmp	.Lftrace_ret
SYM_CODE_END(ftrace_regs_caller)

SYM_FUNC_START(ftrace_stub_direct_tramp)
	CALL_DEPTH_ACCOUNT
	RET
SYM_FUNC_END(ftrace_stub_direct_tramp)

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
SYM_CODE_START(ftrace_graph_caller)
	pushl	%eax
	pushl	%ecx
	pushl	%edx
	movl	3*4(%esp), %eax
	
	lea	4*4(%esp), %edx
	movl	$0, %ecx
	subl	$MCOUNT_INSN_SIZE, %eax
	call	prepare_ftrace_return
	popl	%edx
	popl	%ecx
	popl	%eax
	RET
SYM_CODE_END(ftrace_graph_caller)

.globl return_to_handler
return_to_handler:
	pushl	$0
	pushl	%edx
	pushl	%eax
	movl	%esp, %eax
	call	ftrace_return_to_handler
	movl	%eax, %ecx
	popl	%eax
	popl	%edx
	addl	$4, %esp		# skip ebp
	JMP_NOSPEC ecx
#endif
