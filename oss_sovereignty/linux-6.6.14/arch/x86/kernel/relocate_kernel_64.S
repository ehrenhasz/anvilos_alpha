 
 

#include <linux/linkage.h>
#include <asm/page_types.h>
#include <asm/kexec.h>
#include <asm/processor-flags.h>
#include <asm/pgtable_types.h>
#include <asm/nospec-branch.h>
#include <asm/unwind_hints.h>

 

#define PTR(x) (x << 3)
#define PAGE_ATTR (_PAGE_PRESENT | _PAGE_RW | _PAGE_ACCESSED | _PAGE_DIRTY)

 
#define DATA(offset)		(KEXEC_CONTROL_CODE_MAX_SIZE+(offset))

 
#define RSP			DATA(0x0)
#define CR0			DATA(0x8)
#define CR3			DATA(0x10)
#define CR4			DATA(0x18)

 
#define CP_PA_TABLE_PAGE	DATA(0x20)
#define CP_PA_SWAP_PAGE		DATA(0x28)
#define CP_PA_BACKUP_PAGES_MAP	DATA(0x30)

	.text
	.align PAGE_SIZE
	.code64
SYM_CODE_START_NOALIGN(relocate_range)
SYM_CODE_START_NOALIGN(relocate_kernel)
	UNWIND_HINT_END_OF_STACK
	ANNOTATE_NOENDBR
	 

	 
	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
	pushf

	movq	PTR(VA_CONTROL_PAGE)(%rsi), %r11
	movq	%rsp, RSP(%r11)
	movq	%cr0, %rax
	movq	%rax, CR0(%r11)
	movq	%cr3, %rax
	movq	%rax, CR3(%r11)
	movq	%cr4, %rax
	movq	%rax, CR4(%r11)

	 
	movq	%rax, %r13

	 
	pushq $0
	popfq

	 
	movq	%r8, %r12

	 
	movq	PTR(PA_CONTROL_PAGE)(%rsi), %r8

	 
	movq	PTR(PA_TABLE_PAGE)(%rsi), %r9

	 
	movq	PTR(PA_SWAP_PAGE)(%rsi), %r10

	 
	movq	%r9, CP_PA_TABLE_PAGE(%r11)
	movq	%r10, CP_PA_SWAP_PAGE(%r11)
	movq	%rdi, CP_PA_BACKUP_PAGES_MAP(%r11)

	 
	movq	%r9, %cr3

	 
	lea	PAGE_SIZE(%r8), %rsp

	 
	addq	$(identity_mapped - relocate_kernel), %r8
	pushq	%r8
	ANNOTATE_UNRET_SAFE
	ret
	int3
SYM_CODE_END(relocate_kernel)

SYM_CODE_START_LOCAL_NOALIGN(identity_mapped)
	UNWIND_HINT_END_OF_STACK
	 
	pushq	$0
	 
	pushq   %rdx

	 
	movq	%cr4, %rax
	andq	$~(X86_CR4_CET), %rax
	movq	%rax, %cr4

	 
	movq	%cr0, %rax
	andq	$~(X86_CR0_AM | X86_CR0_WP | X86_CR0_TS | X86_CR0_EM), %rax
	orl	$(X86_CR0_PG | X86_CR0_PE), %eax
	movq	%rax, %cr0

	 
	movl	$X86_CR4_PAE, %eax
	testq	$X86_CR4_LA57, %r13
	jz	1f
	orl	$X86_CR4_LA57, %eax
1:
	movq	%rax, %cr4

	jmp 1f
1:

	 
	movq	%r9, %cr3

	 
	testq	%r12, %r12
	jz 1f
	wbinvd
1:

	movq	%rcx, %r11
	call	swap_pages

	 
	movq	%cr3, %rax
	movq	%rax, %cr3

	 

	testq	%r11, %r11
	jnz 1f
	xorl	%eax, %eax
	xorl	%ebx, %ebx
	xorl    %ecx, %ecx
	xorl    %edx, %edx
	xorl    %esi, %esi
	xorl    %edi, %edi
	xorl    %ebp, %ebp
	xorl	%r8d, %r8d
	xorl	%r9d, %r9d
	xorl	%r10d, %r10d
	xorl	%r11d, %r11d
	xorl	%r12d, %r12d
	xorl	%r13d, %r13d
	xorl	%r14d, %r14d
	xorl	%r15d, %r15d

	ANNOTATE_UNRET_SAFE
	ret
	int3

1:
	popq	%rdx
	leaq	PAGE_SIZE(%r10), %rsp
	ANNOTATE_RETPOLINE_SAFE
	call	*%rdx

	 
	movq	0(%rsp), %rbp
	leaq	relocate_kernel(%rip), %r8
	movq	CP_PA_SWAP_PAGE(%r8), %r10
	movq	CP_PA_BACKUP_PAGES_MAP(%r8), %rdi
	movq	CP_PA_TABLE_PAGE(%r8), %rax
	movq	%rax, %cr3
	lea	PAGE_SIZE(%r8), %rsp
	call	swap_pages
	movq	$virtual_mapped, %rax
	pushq	%rax
	ANNOTATE_UNRET_SAFE
	ret
	int3
SYM_CODE_END(identity_mapped)

SYM_CODE_START_LOCAL_NOALIGN(virtual_mapped)
	UNWIND_HINT_END_OF_STACK
	ANNOTATE_NOENDBR  
	movq	RSP(%r8), %rsp
	movq	CR4(%r8), %rax
	movq	%rax, %cr4
	movq	CR3(%r8), %rax
	movq	CR0(%r8), %r8
	movq	%rax, %cr3
	movq	%r8, %cr0
	movq	%rbp, %rax

	popf
	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbp
	popq	%rbx
	ANNOTATE_UNRET_SAFE
	ret
	int3
SYM_CODE_END(virtual_mapped)

	 
SYM_CODE_START_LOCAL_NOALIGN(swap_pages)
	UNWIND_HINT_END_OF_STACK
	movq	%rdi, %rcx	 
	xorl	%edi, %edi
	xorl	%esi, %esi
	jmp	1f

0:	 

	movq	(%rbx), %rcx
	addq	$8,	%rbx
1:
	testb	$0x1,	%cl    
	jz	2f
	movq	%rcx,	%rdi
	andq	$0xfffffffffffff000, %rdi
	jmp	0b
2:
	testb	$0x2,	%cl    
	jz	2f
	movq	%rcx,   %rbx
	andq	$0xfffffffffffff000, %rbx
	jmp	0b
2:
	testb	$0x4,	%cl    
	jz	2f
	jmp	3f
2:
	testb	$0x8,	%cl    
	jz	0b	       
	movq	%rcx,   %rsi   
	andq	$0xfffffffffffff000, %rsi

	movq	%rdi, %rdx
	movq	%rsi, %rax

	movq	%r10, %rdi
	movl	$512, %ecx
	rep ; movsq

	movq	%rax, %rdi
	movq	%rdx, %rsi
	movl	$512, %ecx
	rep ; movsq

	movq	%rdx, %rdi
	movq	%r10, %rsi
	movl	$512, %ecx
	rep ; movsq

	lea	PAGE_SIZE(%rax), %rsi
	jmp	0b
3:
	ANNOTATE_UNRET_SAFE
	ret
	int3
SYM_CODE_END(swap_pages)

	.skip KEXEC_CONTROL_CODE_MAX_SIZE - (. - relocate_kernel), 0xcc
SYM_CODE_END(relocate_range);
