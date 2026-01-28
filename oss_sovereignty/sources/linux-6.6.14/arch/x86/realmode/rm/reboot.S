
#include <linux/linkage.h>
#include <asm/segment.h>
#include <asm/page_types.h>
#include <asm/processor-flags.h>
#include <asm/msr-index.h>
#include "realmode.h"


	.section ".text32", "ax"
	.code32
SYM_CODE_START(machine_real_restart_asm)

#ifdef CONFIG_X86_64
	
	movl	$__KERNEL_DS, %eax
	movl	%eax, %ds
	lgdtl	pa_tr_gdt

	
	movl	%cr0, %eax
	andl	$~X86_CR0_PG, %eax
	movl	%eax, %cr0
	ljmpl	$__KERNEL32_CS, $pa_machine_real_restart_paging_off

SYM_INNER_LABEL(machine_real_restart_paging_off, SYM_L_GLOBAL)
	xorl	%eax, %eax
	xorl	%edx, %edx
	movl	$MSR_EFER, %ecx
	wrmsr

	movl	%edi, %eax
	
#endif 
	
	
	lidtl	pa_machine_real_restart_idt

	
	lgdtl	pa_machine_real_restart_gdt

	
	movl	$16, %ecx
	movl	%ecx, %ds
	movl	%ecx, %es
	movl	%ecx, %fs
	movl	%ecx, %gs
	movl	%ecx, %ss
	ljmpw	$8, $1f
SYM_CODE_END(machine_real_restart_asm)


	.text
	.code16

	.balign	16
machine_real_restart_asm16:
1:
	xorl	%ecx, %ecx
	movl	%cr0, %edx
	andl	$0x00000011, %edx
	orl	$0x60000000, %edx
	movl	%edx, %cr0
	movl	%ecx, %cr3
	movl	%cr0, %edx
	testl	$0x60000000, %edx	
	jz	2f
	wbinvd
2:
	andb	$0x10, %dl
	movl	%edx, %cr0
	LJMPW_RM(3f)
3:
	andw	%ax, %ax
	jz	bios

apm:
	movw	$0x1000, %ax
	movw	%ax, %ss
	movw	$0xf000, %sp
	movw	$0x5307, %ax
	movw	$0x0001, %bx
	movw	$0x0003, %cx
	int	$0x15
	

bios:
	ljmpw	$0xf000, $0xfff0

	.section ".rodata", "a"

	.balign	16
SYM_DATA_START(machine_real_restart_idt)
	.word	0xffff		
	.long	0		
SYM_DATA_END(machine_real_restart_idt)

	.balign	16
SYM_DATA_START(machine_real_restart_gdt)
	
	.word	0xffff		
	.long	pa_machine_real_restart_gdt
	.word	0

	
	.word	0xffff		
	.long	0x9b000000 + pa_real_mode_base
	.word	0

	
	.quad	GDT_ENTRY(0x0093, 0x100, 0xffff)
SYM_DATA_END(machine_real_restart_gdt)
