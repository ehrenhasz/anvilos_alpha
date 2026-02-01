 
 

#include <linux/linkage.h>
#include <asm/page_types.h>
#include <asm/kexec.h>
#include <asm/nospec-branch.h>
#include <asm/processor-flags.h>

 

#define PTR(x) (x << 2)

 
#define DATA(offset)		(KEXEC_CONTROL_CODE_MAX_SIZE+(offset))

 
#define ESP			DATA(0x0)
#define CR0			DATA(0x4)
#define CR3			DATA(0x8)
#define CR4			DATA(0xc)

 
#define CP_VA_CONTROL_PAGE	DATA(0x10)
#define CP_PA_PGD		DATA(0x14)
#define CP_PA_SWAP_PAGE		DATA(0x18)
#define CP_PA_BACKUP_PAGES_MAP	DATA(0x1c)

	.text
SYM_CODE_START_NOALIGN(relocate_kernel)
	 

	pushl	%ebx
	pushl	%esi
	pushl	%edi
	pushl	%ebp
	pushf

	movl	20+8(%esp), %ebp  
	movl	PTR(VA_CONTROL_PAGE)(%ebp), %edi
	movl	%esp, ESP(%edi)
	movl	%cr0, %eax
	movl	%eax, CR0(%edi)
	movl	%cr3, %eax
	movl	%eax, CR3(%edi)
	movl	%cr4, %eax
	movl	%eax, CR4(%edi)

	 
	movl  20+4(%esp), %ebx  
	movl  20+8(%esp), %ebp  
	movl  20+12(%esp), %edx  
	movl  20+16(%esp), %ecx  
	movl  20+20(%esp), %esi  

	 
	pushl $0
	popfl

	 
	movl	PTR(VA_CONTROL_PAGE)(%ebp), %edi
	movl	%edi, CP_VA_CONTROL_PAGE(%edi)
	movl	PTR(PA_PGD)(%ebp), %eax
	movl	%eax, CP_PA_PGD(%edi)
	movl	PTR(PA_SWAP_PAGE)(%ebp), %eax
	movl	%eax, CP_PA_SWAP_PAGE(%edi)
	movl	%ebx, CP_PA_BACKUP_PAGES_MAP(%edi)

	 
	movl	PTR(PA_CONTROL_PAGE)(%ebp), %edi

	 
	movl	PTR(PA_PGD)(%ebp), %eax
	movl	%eax, %cr3

	 
	lea	PAGE_SIZE(%edi), %esp

	 
	movl    %edi, %eax
	addl    $(identity_mapped - relocate_kernel), %eax
	pushl   %eax
	ANNOTATE_UNRET_SAFE
	ret
	int3
SYM_CODE_END(relocate_kernel)

SYM_CODE_START_LOCAL_NOALIGN(identity_mapped)
	 
	pushl	$0
	 
	pushl   %edx

	 
	movl	%cr0, %eax
	andl	$~(X86_CR0_PG | X86_CR0_AM | X86_CR0_WP | X86_CR0_TS | X86_CR0_EM), %eax
	orl	$(X86_CR0_PE), %eax
	movl	%eax, %cr0

	 
	testl	%ecx, %ecx
	jz	1f
	 
	xorl	%eax, %eax
	movl	%eax, %cr4

	jmp 1f
1:

	 
	xorl	%eax, %eax
	movl	%eax, %cr3

	movl	CP_PA_SWAP_PAGE(%edi), %eax
	pushl	%eax
	pushl	%ebx
	call	swap_pages
	addl	$8, %esp

	 
	xorl	%eax, %eax
	movl	%eax, %cr3

	 

	testl	%esi, %esi
	jnz 1f
	xorl	%edi, %edi
	xorl	%eax, %eax
	xorl	%ebx, %ebx
	xorl    %ecx, %ecx
	xorl    %edx, %edx
	xorl    %esi, %esi
	xorl    %ebp, %ebp
	ANNOTATE_UNRET_SAFE
	ret
	int3
1:
	popl	%edx
	movl	CP_PA_SWAP_PAGE(%edi), %esp
	addl	$PAGE_SIZE, %esp
2:
	ANNOTATE_RETPOLINE_SAFE
	call	*%edx

	 
	movl	0(%esp), %ebp
	call	1f
1:
	popl	%ebx
	subl	$(1b - relocate_kernel), %ebx
	movl	CP_VA_CONTROL_PAGE(%ebx), %edi
	lea	PAGE_SIZE(%ebx), %esp
	movl	CP_PA_SWAP_PAGE(%ebx), %eax
	movl	CP_PA_BACKUP_PAGES_MAP(%ebx), %edx
	pushl	%eax
	pushl	%edx
	call	swap_pages
	addl	$8, %esp
	movl	CP_PA_PGD(%ebx), %eax
	movl	%eax, %cr3
	movl	%cr0, %eax
	orl	$X86_CR0_PG, %eax
	movl	%eax, %cr0
	lea	PAGE_SIZE(%edi), %esp
	movl	%edi, %eax
	addl	$(virtual_mapped - relocate_kernel), %eax
	pushl	%eax
	ANNOTATE_UNRET_SAFE
	ret
	int3
SYM_CODE_END(identity_mapped)

SYM_CODE_START_LOCAL_NOALIGN(virtual_mapped)
	movl	CR4(%edi), %eax
	movl	%eax, %cr4
	movl	CR3(%edi), %eax
	movl	%eax, %cr3
	movl	CR0(%edi), %eax
	movl	%eax, %cr0
	movl	ESP(%edi), %esp
	movl	%ebp, %eax

	popf
	popl	%ebp
	popl	%edi
	popl	%esi
	popl	%ebx
	ANNOTATE_UNRET_SAFE
	ret
	int3
SYM_CODE_END(virtual_mapped)

	 
SYM_CODE_START_LOCAL_NOALIGN(swap_pages)
	movl	8(%esp), %edx
	movl	4(%esp), %ecx
	pushl	%ebp
	pushl	%ebx
	pushl	%edi
	pushl	%esi
	movl	%ecx, %ebx
	jmp	1f

0:	 
	movl	(%ebx), %ecx
	addl	$4, %ebx
1:
	testb	$0x1, %cl      
	jz	2f
	movl	%ecx,	%edi
	andl	$0xfffff000, %edi
	jmp     0b
2:
	testb	$0x2, %cl     
	jz	2f
	movl	%ecx,	%ebx
	andl	$0xfffff000, %ebx
	jmp     0b
2:
	testb   $0x4, %cl     
	jz      2f
	jmp     3f
2:
	testb   $0x8, %cl     
	jz      0b	      
	movl    %ecx,   %esi  
	andl    $0xfffff000, %esi

	movl	%edi, %eax
	movl	%esi, %ebp

	movl	%edx, %edi
	movl    $1024, %ecx
	rep ; movsl

	movl	%ebp, %edi
	movl	%eax, %esi
	movl	$1024, %ecx
	rep ; movsl

	movl	%eax, %edi
	movl	%edx, %esi
	movl	$1024, %ecx
	rep ; movsl

	lea	PAGE_SIZE(%ebp), %esi
	jmp     0b
3:
	popl	%esi
	popl	%edi
	popl	%ebx
	popl	%ebp
	ANNOTATE_UNRET_SAFE
	ret
	int3
SYM_CODE_END(swap_pages)

	.globl kexec_control_code_size
.set kexec_control_code_size, . - relocate_kernel
