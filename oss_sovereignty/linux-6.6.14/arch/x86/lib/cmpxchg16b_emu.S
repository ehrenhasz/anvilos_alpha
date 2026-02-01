 
#include <linux/linkage.h>
#include <asm/percpu.h>
#include <asm/processor-flags.h>

.text

 
SYM_FUNC_START(this_cpu_cmpxchg16b_emu)

	pushfq
	cli

	 
	cmpq	PER_CPU_VAR(0(%rsi)), %rax
	jne	.Lnot_same
	cmpq	PER_CPU_VAR(8(%rsi)), %rdx
	jne	.Lnot_same

	 
	movq	%rbx, PER_CPU_VAR(0(%rsi))
	movq	%rcx, PER_CPU_VAR(8(%rsi))

	 
	orl	$X86_EFLAGS_ZF, (%rsp)

	popfq
	RET

.Lnot_same:
	 

	 
	movq	PER_CPU_VAR(0(%rsi)), %rax
	movq	PER_CPU_VAR(8(%rsi)), %rdx

	 
	andl	$(~X86_EFLAGS_ZF), (%rsp)

	popfq
	RET

SYM_FUNC_END(this_cpu_cmpxchg16b_emu)
