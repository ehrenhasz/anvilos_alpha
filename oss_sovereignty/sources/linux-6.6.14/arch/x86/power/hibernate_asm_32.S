


#include <linux/linkage.h>
#include <asm/segment.h>
#include <asm/page_types.h>
#include <asm/asm-offsets.h>
#include <asm/processor-flags.h>
#include <asm/frame.h>

.text

SYM_FUNC_START(swsusp_arch_suspend)
	movl %esp, saved_context_esp
	movl %ebx, saved_context_ebx
	movl %ebp, saved_context_ebp
	movl %esi, saved_context_esi
	movl %edi, saved_context_edi
	pushfl
	popl saved_context_eflags

	
	movl	%cr3, %eax
	movl	%eax, restore_cr3

	FRAME_BEGIN
	call swsusp_save
	FRAME_END
	RET
SYM_FUNC_END(swsusp_arch_suspend)

SYM_CODE_START(restore_image)
	
	movl	restore_jump_address, %ebx
	movl	restore_cr3, %ebp

	movl	mmu_cr4_features, %ecx

	
	movl	relocated_restore_code, %eax
	jmpl	*%eax
SYM_CODE_END(restore_image)


SYM_CODE_START(core_restore_code)
	movl	temp_pgt, %eax
	movl	%eax, %cr3

	jecxz	1f	# cr4 Pentium and higher, skip if zero
	andl	$~(X86_CR4_PGE), %ecx
	movl	%ecx, %cr4;  # turn off PGE
	movl	%cr3, %eax;  # flush TLB
	movl	%eax, %cr3
1:
	movl	restore_pblist, %edx
	.p2align 4,,7

copy_loop:
	testl	%edx, %edx
	jz	done

	movl	pbe_address(%edx), %esi
	movl	pbe_orig_address(%edx), %edi

	movl	$(PAGE_SIZE >> 2), %ecx
	rep
	movsl

	movl	pbe_next(%edx), %edx
	jmp	copy_loop
	.p2align 4,,7

done:
	jmpl	*%ebx
SYM_CODE_END(core_restore_code)

	
	.align PAGE_SIZE
SYM_FUNC_START(restore_registers)
	
	movl	%ebp, %cr3
	movl	mmu_cr4_features, %ecx
	jecxz	1f	# cr4 Pentium and higher, skip if zero
	movl	%ecx, %cr4;  # turn PGE back on
1:

	movl saved_context_esp, %esp
	movl saved_context_ebp, %ebp
	movl saved_context_ebx, %ebx
	movl saved_context_esi, %esi
	movl saved_context_edi, %edi

	pushl saved_context_eflags
	popfl

	
	movl $saved_context, %eax
	lgdt saved_context_gdt_desc(%eax)

	xorl	%eax, %eax

	
	movl	%eax, in_suspend

	RET
SYM_FUNC_END(restore_registers)
