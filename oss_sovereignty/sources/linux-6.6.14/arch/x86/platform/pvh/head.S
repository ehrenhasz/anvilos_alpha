



	.code32
	.text
#define _pa(x)          ((x) - __START_KERNEL_map)

#include <linux/elfnote.h>
#include <linux/init.h>
#include <linux/linkage.h>
#include <asm/segment.h>
#include <asm/asm.h>
#include <asm/boot.h>
#include <asm/processor-flags.h>
#include <asm/msr.h>
#include <asm/nospec-branch.h>
#include <xen/interface/elfnote.h>

	__HEAD



#define PVH_GDT_ENTRY_CS	1
#define PVH_GDT_ENTRY_DS	2
#define PVH_CS_SEL		(PVH_GDT_ENTRY_CS * 8)
#define PVH_DS_SEL		(PVH_GDT_ENTRY_DS * 8)

SYM_CODE_START_LOCAL(pvh_start_xen)
	UNWIND_HINT_END_OF_STACK
	cld

	lgdt (_pa(gdt))

	mov $PVH_DS_SEL,%eax
	mov %eax,%ds
	mov %eax,%es
	mov %eax,%ss

	
	mov $_pa(pvh_start_info), %edi
	mov %ebx, %esi
	mov _pa(pvh_start_info_sz), %ecx
	shr $2,%ecx
	rep
	movsl

	mov $_pa(early_stack_end), %esp

	
	mov %cr4, %eax
	orl $X86_CR4_PAE, %eax
	mov %eax, %cr4

#ifdef CONFIG_X86_64
	
	mov $MSR_EFER, %ecx
	rdmsr
	btsl $_EFER_LME, %eax
	wrmsr

	
	mov $_pa(init_top_pgt), %eax
	mov %eax, %cr3
	mov $(X86_CR0_PG | X86_CR0_PE), %eax
	mov %eax, %cr0

	
	ljmp $PVH_CS_SEL, $_pa(1f)

	
	.code64
1:
	
	mov $MSR_GS_BASE,%ecx
	mov $_pa(canary), %eax
	xor %edx, %edx
	wrmsr

	call xen_prepare_pvh

	
	mov $_pa(pvh_bootparams), %rsi
	mov $_pa(startup_64), %rax
	ANNOTATE_RETPOLINE_SAFE
	jmp *%rax

#else 

	call mk_early_pgtbl_32

	mov $_pa(initial_page_table), %eax
	mov %eax, %cr3

	mov %cr0, %eax
	or $(X86_CR0_PG | X86_CR0_PE), %eax
	mov %eax, %cr0

	ljmp $PVH_CS_SEL, $1f
1:
	call xen_prepare_pvh
	mov $_pa(pvh_bootparams), %esi

	
	ljmp $PVH_CS_SEL, $_pa(2f)
2:
	mov %cr0, %eax
	and $~X86_CR0_PG, %eax
	mov %eax, %cr0
	mov %cr4, %eax
	and $~X86_CR4_PAE, %eax
	mov %eax, %cr4

	ljmp $PVH_CS_SEL, $_pa(startup_32)
#endif
SYM_CODE_END(pvh_start_xen)

	.section ".init.data","aw"
	.balign 8
SYM_DATA_START_LOCAL(gdt)
	.word gdt_end - gdt_start
	.long _pa(gdt_start)
	.word 0
SYM_DATA_END(gdt)
SYM_DATA_START_LOCAL(gdt_start)
	.quad 0x0000000000000000            
#ifdef CONFIG_X86_64
	.quad GDT_ENTRY(0xa09a, 0, 0xfffff) 
#else
	.quad GDT_ENTRY(0xc09a, 0, 0xfffff) 
#endif
	.quad GDT_ENTRY(0xc092, 0, 0xfffff) 
SYM_DATA_END_LABEL(gdt_start, SYM_L_LOCAL, gdt_end)

	.balign 16
SYM_DATA_LOCAL(canary, .fill 48, 1, 0)

SYM_DATA_START_LOCAL(early_stack)
	.fill BOOT_STACK_SIZE, 1, 0
SYM_DATA_END_LABEL(early_stack, SYM_L_LOCAL, early_stack_end)

	ELFNOTE(Xen, XEN_ELFNOTE_PHYS32_ENTRY,
	             _ASM_PTR (pvh_start_xen - __START_KERNEL_map))
