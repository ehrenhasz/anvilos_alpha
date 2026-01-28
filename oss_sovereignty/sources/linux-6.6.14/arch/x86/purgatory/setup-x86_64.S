

#include <linux/linkage.h>
#include <asm/purgatory.h>

	.text
	.balign 16
	.code64

SYM_CODE_START(purgatory_start)
	
	lgdt	gdt(%rip)

	
	movl	$0x18, %eax	
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %ss
	movl	%eax, %fs
	movl	%eax, %gs

	
	leaq	lstack_end(%rip), %rsp

	
	call purgatory
	jmp	entry64
SYM_CODE_END(purgatory_start)

	.section ".rodata"
	.balign 16
SYM_DATA_START_LOCAL(gdt)
	
	.word	gdt_end - gdt - 1
	.quad	gdt
	.word	0, 0, 0

	
	.word	0xFFFF, 0x0000, 0x9A00, 0x00AF

	
	.word	0xFFFF, 0x0000, 0x9200, 0x00CF
SYM_DATA_END_LABEL(gdt, SYM_L_LOCAL, gdt_end)

	.bss
	.balign 4096
SYM_DATA_START_LOCAL(lstack)
	.skip 4096
SYM_DATA_END_LABEL(lstack, SYM_L_LOCAL, lstack_end)
