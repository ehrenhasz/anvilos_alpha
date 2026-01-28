


#include <linux/linkage.h>
#include <asm/export.h>
#include <asm/msr-index.h>

.pushsection .noinstr.text, "ax"

SYM_FUNC_START(entry_ibpb)
	movl	$MSR_IA32_PRED_CMD, %ecx
	movl	$PRED_CMD_IBPB, %eax
	xorl	%edx, %edx
	wrmsr
	RET
SYM_FUNC_END(entry_ibpb)

EXPORT_SYMBOL_GPL(entry_ibpb);

.popsection
