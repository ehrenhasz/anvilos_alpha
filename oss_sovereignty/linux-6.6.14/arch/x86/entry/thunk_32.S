 
 
	#include <linux/linkage.h>
	#include <asm/asm.h>
	#include <asm/export.h>

	 
	.macro THUNK name, func, put_ret_addr_in_eax=0
SYM_CODE_START_NOALIGN(\name)
	pushl %eax
	pushl %ecx
	pushl %edx

	.if \put_ret_addr_in_eax
	 
	movl 3*4(%esp), %eax
	.endif

	call \func
	popl %edx
	popl %ecx
	popl %eax
	RET
	_ASM_NOKPROBE(\name)
SYM_CODE_END(\name)
	.endm

	THUNK preempt_schedule_thunk, preempt_schedule
	THUNK preempt_schedule_notrace_thunk, preempt_schedule_notrace
	EXPORT_SYMBOL(preempt_schedule_thunk)
	EXPORT_SYMBOL(preempt_schedule_notrace_thunk)

