 
 
#include <linux/linkage.h>
#include <asm/segment.h>
#include <asm/msr-index.h>
#include <asm/page_types.h>
#include <asm/pgtable_types.h>
#include <asm/processor-flags.h>
#include "realmode.h"
#include "wakeup.h"

	.code16

 
	.section ".data", "aw"

	.balign	16
SYM_DATA_START(wakeup_header)
	video_mode:	.short	0	 
	pmode_entry:	.long	0
	pmode_cs:	.short	__KERNEL_CS
	pmode_cr0:	.long	0	 
	pmode_cr3:	.long	0	 
	pmode_cr4:	.long	0	 
	pmode_efer:	.quad	0	 
	pmode_gdt:	.quad	0
	pmode_misc_en:	.quad	0	 
	pmode_behavior:	.long	0	 
	realmode_flags:	.long	0
	real_magic:	.long	0
	signature:	.long	WAKEUP_HEADER_SIGNATURE
SYM_DATA_END(wakeup_header)

	.text
	.code16

	.balign	16
SYM_CODE_START(wakeup_start)
	cli
	cld

	LJMPW_RM(3f)
3:
	 
	movw	$16, %cx
	lgdtl	%cs:wakeup_gdt
	movl	%cr0, %eax
	orb	$X86_CR0_PE, %al
	movl	%eax, %cr0
	ljmpw	$8, $2f
2:
	movw	%cx, %ds
	movw	%cx, %es
	movw	%cx, %ss
	movw	%cx, %fs
	movw	%cx, %gs

	andb	$~X86_CR0_PE, %al
	movl	%eax, %cr0
	LJMPW_RM(3f)
3:
	 
	movw	%cs, %ax
	movw	%ax, %ss
	movl	$rm_stack_end, %esp
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs

	lidtl	.Lwakeup_idt

	 
	pushl $0
	popfl

	 
	movl	signature, %eax
	cmpl	$WAKEUP_HEADER_SIGNATURE, %eax
	jne	bogus_real_magic

	 
	movl	end_signature, %eax
	cmpl	$REALMODE_END_SIGNATURE, %eax
	jne	bogus_real_magic

	 
	calll	main

	 
	movl	pmode_behavior, %edi
	btl	$WAKEUP_BEHAVIOR_RESTORE_MISC_ENABLE, %edi
	jnc	1f

	movl	pmode_misc_en, %eax
	movl	pmode_misc_en + 4, %edx
	movl	$MSR_IA32_MISC_ENABLE, %ecx
	wrmsr
1:

	 

#ifndef CONFIG_64BIT
	 
	movl	pmode_cr3, %eax
	movl	%eax, %cr3

	btl	$WAKEUP_BEHAVIOR_RESTORE_CR4, %edi
	jnc	1f
	movl	pmode_cr4, %eax
	movl	%eax, %cr4
1:
	btl	$WAKEUP_BEHAVIOR_RESTORE_EFER, %edi
	jnc	1f
	movl	pmode_efer, %eax
	movl	pmode_efer + 4, %edx
	movl	$MSR_EFER, %ecx
	wrmsr
1:

	lgdtl	pmode_gdt

	 
	movl	pmode_entry, %eax
	movl	pmode_cr0, %ecx
	movl	%ecx, %cr0
	ljmpl	$__KERNEL_CS, $pa_startup_32
	 
#else
	jmp	trampoline_start
#endif
SYM_CODE_END(wakeup_start)

bogus_real_magic:
1:
	hlt
	jmp	1b

	.section ".rodata","a"

	 

	.balign	16
SYM_DATA_START(wakeup_gdt)
	.word	3*8-1		 
	.long	pa_wakeup_gdt
	.word	0

	.word	0xffff		 
	.long	0x9b000000 + pa_real_mode_base
	.word	0x008f		 

	.word	0xffff		 
	.long	0x93000000 + pa_real_mode_base
	.word	0x008f		 
SYM_DATA_END(wakeup_gdt)

	.section ".rodata","a"
	.balign	8

	 
	.balign	16
SYM_DATA_START_LOCAL(.Lwakeup_idt)
	.word	0xffff		 
	.long	0		 
	.word	0
SYM_DATA_END(.Lwakeup_idt)
