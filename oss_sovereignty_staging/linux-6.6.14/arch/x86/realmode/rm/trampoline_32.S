 
 

#include <linux/linkage.h>
#include <asm/segment.h>
#include <asm/page_types.h>
#include "realmode.h"

	.text
	.code16

	.balign	PAGE_SIZE
SYM_CODE_START(trampoline_start)
	wbinvd			# Needed for NUMA-Q should be harmless for others

	LJMPW_RM(1f)
1:
	mov	%cs, %ax	# Code and data in the same place
	mov	%ax, %ds

	cli			# We should be safe anyway

	movl	tr_start, %eax	# where we need to go

	 
	lidtl	tr_idt			# load idt with 0, 0
	lgdtl	tr_gdt			# load gdt with whatever is appropriate

	movw	$1, %dx			# protected mode (PE) bit
	lmsw	%dx			# into protected mode

	ljmpl	$__BOOT_CS, $pa_startup_32
SYM_CODE_END(trampoline_start)

	.section ".text32","ax"
	.code32
SYM_CODE_START(startup_32)			# note: also used from wakeup_asm.S
	jmp	*%eax
SYM_CODE_END(startup_32)

	.bss
	.balign 8
SYM_DATA_START(trampoline_header)
	SYM_DATA_LOCAL(tr_start,	.space 4)
	SYM_DATA_LOCAL(tr_gdt_pad,	.space 2)
	SYM_DATA_LOCAL(tr_gdt,		.space 6)
SYM_DATA_END(trampoline_header)
	
#include "trampoline_common.S"
