
 
#include <linux/bug.h>
#include <linux/rethook.h>
#include <linux/kprobes.h>
#include <linux/objtool.h>

#include "kprobes/common.h"

__visible void arch_rethook_trampoline_callback(struct pt_regs *regs);

#ifndef ANNOTATE_NOENDBR
#define ANNOTATE_NOENDBR
#endif

 
asm(
	".text\n"
	".global arch_rethook_trampoline\n"
	".type arch_rethook_trampoline, @function\n"
	"arch_rethook_trampoline:\n"
#ifdef CONFIG_X86_64
	ANNOTATE_NOENDBR	 
	 
	"	pushq $arch_rethook_trampoline\n"
	UNWIND_HINT_FUNC
	"       pushq $" __stringify(__KERNEL_DS) "\n"
	 
	"	pushq %rsp\n"
	"	pushfq\n"
	SAVE_REGS_STRING
	"	movq %rsp, %rdi\n"
	"	call arch_rethook_trampoline_callback\n"
	RESTORE_REGS_STRING
	 
	"	addq $16, %rsp\n"
	"	popfq\n"
#else
	 
	"	pushl $arch_rethook_trampoline\n"
	UNWIND_HINT_FUNC
	"	pushl %ss\n"
	 
	"	pushl %esp\n"
	"	pushfl\n"
	SAVE_REGS_STRING
	"	movl %esp, %eax\n"
	"	call arch_rethook_trampoline_callback\n"
	RESTORE_REGS_STRING
	 
	"	addl $8, %esp\n"
	"	popfl\n"
#endif
	ASM_RET
	".size arch_rethook_trampoline, .-arch_rethook_trampoline\n"
);
NOKPROBE_SYMBOL(arch_rethook_trampoline);

 
__used __visible void arch_rethook_trampoline_callback(struct pt_regs *regs)
{
	unsigned long *frame_pointer;

	 
	regs->cs = __KERNEL_CS;
#ifdef CONFIG_X86_32
	regs->gs = 0;
#endif
	regs->ip = (unsigned long)&arch_rethook_trampoline;
	regs->orig_ax = ~0UL;
	regs->sp += 2*sizeof(long);
	frame_pointer = (long *)(regs + 1);

	 
	rethook_trampoline_handler(regs, (unsigned long)frame_pointer);

	 
	*(unsigned long *)&regs->ss = regs->flags;
}
NOKPROBE_SYMBOL(arch_rethook_trampoline_callback);

 
STACK_FRAME_NON_STANDARD_FP(arch_rethook_trampoline);

 
void arch_rethook_fixup_return(struct pt_regs *regs,
			       unsigned long correct_ret_addr)
{
	unsigned long *frame_pointer = (void *)(regs + 1);

	 
	*frame_pointer = correct_ret_addr;
}
NOKPROBE_SYMBOL(arch_rethook_fixup_return);

void arch_rethook_prepare(struct rethook_node *rh, struct pt_regs *regs, bool mcount)
{
	unsigned long *stack = (unsigned long *)regs->sp;

	rh->ret_addr = stack[0];
	rh->frame = regs->sp;

	 
	stack[0] = (unsigned long) arch_rethook_trampoline;
}
NOKPROBE_SYMBOL(arch_rethook_prepare);
