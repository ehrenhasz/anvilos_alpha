 
 


#include <linux/linkage.h>
#include <linux/threads.h>
#include <linux/init.h>
#include <linux/pgtable.h>
#include <asm/segment.h>
#include <asm/page.h>
#include <asm/msr.h>
#include <asm/cache.h>
#include <asm/processor-flags.h>
#include <asm/percpu.h>
#include <asm/nops.h>
#include "../entry/calling.h"
#include <asm/export.h>
#include <asm/nospec-branch.h>
#include <asm/apicdef.h>
#include <asm/fixmap.h>
#include <asm/smp.h>

 
#define l4_index(x)	(((x) >> 39) & 511)
#define pud_index(x)	(((x) >> PUD_SHIFT) & (PTRS_PER_PUD-1))

L4_PAGE_OFFSET = l4_index(__PAGE_OFFSET_BASE_L4)
L4_START_KERNEL = l4_index(__START_KERNEL_map)

L3_START_KERNEL = pud_index(__START_KERNEL_map)

	.text
	__HEAD
	.code64
SYM_CODE_START_NOALIGN(startup_64)
	UNWIND_HINT_END_OF_STACK
	 
	mov	%rsi, %r15

	 
	leaq	(__end_init_task - PTREGS_SIZE)(%rip), %rsp

	leaq	_text(%rip), %rdi

	 
	movl	$MSR_GS_BASE, %ecx
	leaq	INIT_PER_CPU_VAR(fixed_percpu_data)(%rip), %rdx
	movl	%edx, %eax
	shrq	$32,  %rdx
	wrmsr

	call	startup_64_setup_env

	 
	pushq	$__KERNEL_CS
	leaq	.Lon_kernel_cs(%rip), %rax
	pushq	%rax
	lretq

.Lon_kernel_cs:
	UNWIND_HINT_END_OF_STACK

#ifdef CONFIG_AMD_MEM_ENCRYPT
	 
	movq	%r15, %rdi
	call	sme_enable
#endif

	 
	call verify_cpu

	 
	leaq	_text(%rip), %rdi
	movq	%r15, %rsi
	call	__startup_64

	 
	addq	$(early_top_pgt - __START_KERNEL_map), %rax
	jmp 1f
SYM_CODE_END(startup_64)

SYM_CODE_START(secondary_startup_64)
	UNWIND_HINT_END_OF_STACK
	ANNOTATE_NOENDBR
	 

	 
	call verify_cpu

	 
SYM_INNER_LABEL(secondary_startup_64_no_verify, SYM_L_GLOBAL)
	UNWIND_HINT_END_OF_STACK
	ANNOTATE_NOENDBR

	 
	xorq	%r15, %r15

	 
#ifdef CONFIG_AMD_MEM_ENCRYPT
	movq	sme_me_mask, %rax
#else
	xorq	%rax, %rax
#endif

	 
	addq	$(init_top_pgt - __START_KERNEL_map), %rax
1:

#ifdef CONFIG_X86_MCE
	 
	movq	%cr4, %rcx
	andl	$X86_CR4_MCE, %ecx
#else
	movl	$0, %ecx
#endif

	 
	orl	$(X86_CR4_PAE | X86_CR4_PGE), %ecx
#ifdef CONFIG_X86_5LEVEL
	testl	$1, __pgtable_l5_enabled(%rip)
	jz	1f
	orl	$X86_CR4_LA57, %ecx
1:
#endif
	movq	%rcx, %cr4

	 
	addq	phys_base(%rip), %rax

	 
	movq	%rax, %rdi
	call	sev_verify_cbit

	 
	movq	%rax, %cr3

	 
	movq	%cr4, %rcx
	movq	%rcx, %rax
	xorq	$X86_CR4_PGE, %rcx
	movq	%rcx, %cr4
	movq	%rax, %cr4

	 
	movq	$1f, %rax
	ANNOTATE_RETPOLINE_SAFE
	jmp	*%rax
1:
	UNWIND_HINT_END_OF_STACK
	ANNOTATE_NOENDBR  

#ifdef CONFIG_SMP
	 
	movl	smpboot_control(%rip), %ecx
	testl	$STARTUP_READ_APICID, %ecx
	jnz	.Lread_apicid
	 
	andl	$(~STARTUP_PARALLEL_MASK), %ecx
	jmp	.Lsetup_cpu

.Lread_apicid:
	 
	mov	$MSR_IA32_APICBASE, %ecx
	rdmsr
	testl	$X2APIC_ENABLE, %eax
	jnz	.Lread_apicid_msr

#ifdef CONFIG_X86_X2APIC
	 
	cmpl	$0, x2apic_mode(%rip)
	jz	.Lread_apicid_mmio

	 
	orl	$X2APIC_ENABLE, %eax
	wrmsr
	jmp	.Lread_apicid_msr
#endif

.Lread_apicid_mmio:
	 
	movq	apic_mmio_base(%rip), %rcx
	addq	$APIC_ID, %rcx
	movl	(%rcx), %eax
	shr	$24, %eax
	jmp	.Llookup_AP

.Lread_apicid_msr:
	mov	$APIC_X2APIC_ID_MSR, %ecx
	rdmsr

.Llookup_AP:
	 
	xorq	%rcx, %rcx
	leaq	cpuid_to_apicid(%rip), %rbx

.Lfind_cpunr:
	cmpl	(%rbx,%rcx,4), %eax
	jz	.Lsetup_cpu
	inc	%ecx
#ifdef CONFIG_FORCE_NR_CPUS
	cmpl	$NR_CPUS, %ecx
#else
	cmpl	nr_cpu_ids(%rip), %ecx
#endif
	jb	.Lfind_cpunr

	 
	movq	trampoline_lock(%rip), %rax
	movl	$0, (%rax)

1:	cli
	hlt
	jmp	1b

.Lsetup_cpu:
	 
	movq	__per_cpu_offset(,%rcx,8), %rdx
#else
	xorl	%edx, %edx  
#endif  

	 
	movq	pcpu_hot + X86_current_task(%rdx), %rax
	movq	TASK_threadsp(%rax), %rsp

	 
	movq	trampoline_lock(%rip), %rax
	testq	%rax, %rax
	jz	.Lsetup_gdt
	movl	$0, (%rax)

.Lsetup_gdt:
	 
	subq	$16, %rsp
	movw	$(GDT_SIZE-1), (%rsp)
	leaq	gdt_page(%rdx), %rax
	movq	%rax, 2(%rsp)
	lgdt	(%rsp)
	addq	$16, %rsp

	 
	xorl %eax,%eax
	movl %eax,%ds
	movl %eax,%ss
	movl %eax,%es

	 
	movl %eax,%fs
	movl %eax,%gs

	 
	movl	$MSR_GS_BASE,%ecx
#ifndef CONFIG_SMP
	leaq	INIT_PER_CPU_VAR(fixed_percpu_data)(%rip), %rdx
#endif
	movl	%edx, %eax
	shrq	$32, %rdx
	wrmsr

	 
	call	early_setup_idt

	 
	movl	$0x80000001, %eax
	cpuid
	movl	%edx,%edi

	 
	movl	$MSR_EFER, %ecx
	rdmsr
	 
	movl    %eax, %edx
	btsl	$_EFER_SCE, %eax	 
	btl	$20,%edi		 
	jnc     1f
	btsl	$_EFER_NX, %eax
	btsq	$_PAGE_BIT_NX,early_pmd_flags(%rip)

	 
1:	cmpl	%edx, %eax
	je	1f
	xor	%edx, %edx
	wrmsr				 
1:
	 
	movl	$CR0_STATE, %eax
	 
	movq	%rax, %cr0

	 
	pushq $0
	popfq

	 
	movq	%r15, %rdi

.Ljump_to_C_code:
	 
	pushq	$.Lafter_lret	# put return address on stack for unwinder
	xorl	%ebp, %ebp	# clear frame pointer
	movq	initial_code(%rip), %rax
	pushq	$__KERNEL_CS	# set correct cs
	pushq	%rax		# target address in negative space
	lretq
.Lafter_lret:
	ANNOTATE_NOENDBR
SYM_CODE_END(secondary_startup_64)

#include "verify_cpu.S"
#include "sev_verify_cbit.S"

#if defined(CONFIG_HOTPLUG_CPU) && defined(CONFIG_AMD_MEM_ENCRYPT)
 
SYM_CODE_START(soft_restart_cpu)
	ANNOTATE_NOENDBR
	UNWIND_HINT_END_OF_STACK

	 
	movq	PER_CPU_VAR(pcpu_hot) + X86_current_task, %rcx
	movq	TASK_threadsp(%rcx), %rsp

	jmp	.Ljump_to_C_code
SYM_CODE_END(soft_restart_cpu)
#endif

#ifdef CONFIG_AMD_MEM_ENCRYPT
 
SYM_CODE_START_NOALIGN(vc_boot_ghcb)
	UNWIND_HINT_IRET_REGS offset=8
	ENDBR

	 
	PUSH_AND_CLEAR_REGS

	 
	movq    %rsp, %rdi
	movq	ORIG_RAX(%rsp), %rsi
	movq	initial_vc_handler(%rip), %rax
	ANNOTATE_RETPOLINE_SAFE
	call	*%rax

	 
	POP_REGS

	 
	addq    $8, %rsp

	iretq
SYM_CODE_END(vc_boot_ghcb)
#endif

	 
	__REFDATA
	.balign	8
SYM_DATA(initial_code,	.quad x86_64_start_kernel)
#ifdef CONFIG_AMD_MEM_ENCRYPT
SYM_DATA(initial_vc_handler,	.quad handle_vc_boot_ghcb)
#endif

SYM_DATA(trampoline_lock, .quad 0);
	__FINITDATA

	__INIT
SYM_CODE_START(early_idt_handler_array)
	i = 0
	.rept NUM_EXCEPTION_VECTORS
	.if ((EXCEPTION_ERRCODE_MASK >> i) & 1) == 0
		UNWIND_HINT_IRET_REGS
		ENDBR
		pushq $0	# Dummy error code, to make stack frame uniform
	.else
		UNWIND_HINT_IRET_REGS offset=8
		ENDBR
	.endif
	pushq $i		# 72(%rsp) Vector number
	jmp early_idt_handler_common
	UNWIND_HINT_IRET_REGS
	i = i + 1
	.fill early_idt_handler_array + i*EARLY_IDT_HANDLER_SIZE - ., 1, 0xcc
	.endr
SYM_CODE_END(early_idt_handler_array)
	ANNOTATE_NOENDBR  

SYM_CODE_START_LOCAL(early_idt_handler_common)
	UNWIND_HINT_IRET_REGS offset=16
	 
	cld

	incl early_recursion_flag(%rip)

	 
	pushq %rsi				 
	movq 8(%rsp), %rsi			 
	movq %rdi, 8(%rsp)			 
	pushq %rdx				 
	pushq %rcx				 
	pushq %rax				 
	pushq %r8				 
	pushq %r9				 
	pushq %r10				 
	pushq %r11				 
	pushq %rbx				 
	pushq %rbp				 
	pushq %r12				 
	pushq %r13				 
	pushq %r14				 
	pushq %r15				 
	UNWIND_HINT_REGS

	movq %rsp,%rdi		 
	call do_early_exception

	decl early_recursion_flag(%rip)
	jmp restore_regs_and_return_to_kernel
SYM_CODE_END(early_idt_handler_common)

#ifdef CONFIG_AMD_MEM_ENCRYPT
 
SYM_CODE_START_NOALIGN(vc_no_ghcb)
	UNWIND_HINT_IRET_REGS offset=8
	ENDBR

	 
	PUSH_AND_CLEAR_REGS

	 
	movq    %rsp, %rdi
	movq	ORIG_RAX(%rsp), %rsi
	call    do_vc_no_ghcb

	 
	POP_REGS

	 
	addq    $8, %rsp

	 
	iretq
SYM_CODE_END(vc_no_ghcb)
#endif

#define SYM_DATA_START_PAGE_ALIGNED(name)			\
	SYM_START(name, SYM_L_GLOBAL, .balign PAGE_SIZE)

#ifdef CONFIG_PAGE_TABLE_ISOLATION
 
#define PTI_USER_PGD_FILL	512
 
#define SYM_DATA_START_PTI_ALIGNED(name) \
	SYM_START(name, SYM_L_GLOBAL, .balign 2 * PAGE_SIZE)
#else
#define SYM_DATA_START_PTI_ALIGNED(name) \
	SYM_DATA_START_PAGE_ALIGNED(name)
#define PTI_USER_PGD_FILL	0
#endif

 
#define PMDS(START, PERM, COUNT)			\
	i = 0 ;						\
	.rept (COUNT) ;					\
	.quad	(START) + (i << PMD_SHIFT) + (PERM) ;	\
	i = i + 1 ;					\
	.endr

	__INITDATA
	.balign 4

SYM_DATA_START_PTI_ALIGNED(early_top_pgt)
	.fill	512,8,0
	.fill	PTI_USER_PGD_FILL,8,0
SYM_DATA_END(early_top_pgt)

SYM_DATA_START_PAGE_ALIGNED(early_dynamic_pgts)
	.fill	512*EARLY_DYNAMIC_PAGE_TABLES,8,0
SYM_DATA_END(early_dynamic_pgts)

SYM_DATA(early_recursion_flag, .long 0)

	.data

#if defined(CONFIG_XEN_PV) || defined(CONFIG_PVH)
SYM_DATA_START_PTI_ALIGNED(init_top_pgt)
	.quad   level3_ident_pgt - __START_KERNEL_map + _KERNPG_TABLE_NOENC
	.org    init_top_pgt + L4_PAGE_OFFSET*8, 0
	.quad   level3_ident_pgt - __START_KERNEL_map + _KERNPG_TABLE_NOENC
	.org    init_top_pgt + L4_START_KERNEL*8, 0
	 
	.quad   level3_kernel_pgt - __START_KERNEL_map + _PAGE_TABLE_NOENC
	.fill	PTI_USER_PGD_FILL,8,0
SYM_DATA_END(init_top_pgt)

SYM_DATA_START_PAGE_ALIGNED(level3_ident_pgt)
	.quad	level2_ident_pgt - __START_KERNEL_map + _KERNPG_TABLE_NOENC
	.fill	511, 8, 0
SYM_DATA_END(level3_ident_pgt)
SYM_DATA_START_PAGE_ALIGNED(level2_ident_pgt)
	 
	PMDS(0, __PAGE_KERNEL_IDENT_LARGE_EXEC, PTRS_PER_PMD)
SYM_DATA_END(level2_ident_pgt)
#else
SYM_DATA_START_PTI_ALIGNED(init_top_pgt)
	.fill	512,8,0
	.fill	PTI_USER_PGD_FILL,8,0
SYM_DATA_END(init_top_pgt)
#endif

#ifdef CONFIG_X86_5LEVEL
SYM_DATA_START_PAGE_ALIGNED(level4_kernel_pgt)
	.fill	511,8,0
	.quad	level3_kernel_pgt - __START_KERNEL_map + _PAGE_TABLE_NOENC
SYM_DATA_END(level4_kernel_pgt)
#endif

SYM_DATA_START_PAGE_ALIGNED(level3_kernel_pgt)
	.fill	L3_START_KERNEL,8,0
	 
	.quad	level2_kernel_pgt - __START_KERNEL_map + _KERNPG_TABLE_NOENC
	.quad	level2_fixmap_pgt - __START_KERNEL_map + _PAGE_TABLE_NOENC
SYM_DATA_END(level3_kernel_pgt)

SYM_DATA_START_PAGE_ALIGNED(level2_kernel_pgt)
	 
	PMDS(0, __PAGE_KERNEL_LARGE_EXEC, KERNEL_IMAGE_SIZE/PMD_SIZE)
SYM_DATA_END(level2_kernel_pgt)

SYM_DATA_START_PAGE_ALIGNED(level2_fixmap_pgt)
	.fill	(512 - 4 - FIXMAP_PMD_NUM),8,0
	pgtno = 0
	.rept (FIXMAP_PMD_NUM)
	.quad level1_fixmap_pgt + (pgtno << PAGE_SHIFT) - __START_KERNEL_map \
		+ _PAGE_TABLE_NOENC;
	pgtno = pgtno + 1
	.endr
	 
	.fill	4,8,0
SYM_DATA_END(level2_fixmap_pgt)

SYM_DATA_START_PAGE_ALIGNED(level1_fixmap_pgt)
	.rept (FIXMAP_PMD_NUM)
	.fill	512,8,0
	.endr
SYM_DATA_END(level1_fixmap_pgt)

#undef PMDS

	.data
	.align 16

SYM_DATA(smpboot_control,		.long 0)

	.align 16
 
SYM_DATA(phys_base, .quad 0x0)
EXPORT_SYMBOL(phys_base)

#include "../../x86/xen/xen-head.S"

	__PAGE_ALIGNED_BSS
SYM_DATA_START_PAGE_ALIGNED(empty_zero_page)
	.skip PAGE_SIZE
SYM_DATA_END(empty_zero_page)
EXPORT_SYMBOL(empty_zero_page)

