


#include <linux/linkage.h>
#include <asm/asm.h>

#ifndef CONFIG_UML

#ifdef CONFIG_X86_MCE


SYM_FUNC_START(copy_mc_fragile)
	cmpl $8, %edx
	
	jb .L_no_whole_words

	
	testl $7, %esi
	
	jz .L_8byte_aligned

	
	movl %esi, %ecx
	andl $7, %ecx
	subl $8, %ecx
	negl %ecx
	subl %ecx, %edx
.L_read_leading_bytes:
	movb (%rsi), %al
.L_write_leading_bytes:
	movb %al, (%rdi)
	incq %rsi
	incq %rdi
	decl %ecx
	jnz .L_read_leading_bytes

.L_8byte_aligned:
	movl %edx, %ecx
	andl $7, %edx
	shrl $3, %ecx
	jz .L_no_whole_words

.L_read_words:
	movq (%rsi), %r8
.L_write_words:
	movq %r8, (%rdi)
	addq $8, %rsi
	addq $8, %rdi
	decl %ecx
	jnz .L_read_words

	
.L_no_whole_words:
	andl %edx, %edx
	jz .L_done_memcpy_trap

	
	movl %edx, %ecx
.L_read_trailing_bytes:
	movb (%rsi), %al
.L_write_trailing_bytes:
	movb %al, (%rdi)
	incq %rsi
	incq %rdi
	decl %ecx
	jnz .L_read_trailing_bytes

	
.L_done_memcpy_trap:
	xorl %eax, %eax
.L_done:
	RET

	
.E_read_words:
	shll	$3, %ecx
.E_leading_bytes:
	addl	%edx, %ecx
.E_trailing_bytes:
	mov	%ecx, %eax
	jmp	.L_done

	
.E_write_words:
	shll	$3, %ecx
	addl	%edx, %ecx
	movl	%ecx, %edx
	jmp copy_mc_fragile_handle_tail

	_ASM_EXTABLE_TYPE(.L_read_leading_bytes, .E_leading_bytes, EX_TYPE_DEFAULT_MCE_SAFE)
	_ASM_EXTABLE_TYPE(.L_read_words, .E_read_words, EX_TYPE_DEFAULT_MCE_SAFE)
	_ASM_EXTABLE_TYPE(.L_read_trailing_bytes, .E_trailing_bytes, EX_TYPE_DEFAULT_MCE_SAFE)
	_ASM_EXTABLE(.L_write_leading_bytes, .E_leading_bytes)
	_ASM_EXTABLE(.L_write_words, .E_write_words)
	_ASM_EXTABLE(.L_write_trailing_bytes, .E_trailing_bytes)

SYM_FUNC_END(copy_mc_fragile)
#endif 


SYM_FUNC_START(copy_mc_enhanced_fast_string)
	movq %rdi, %rax
	movq %rdx, %rcx
.L_copy:
	rep movsb
	
	xorl %eax, %eax
	RET

.E_copy:
	
	movq %rcx, %rax
	RET

	_ASM_EXTABLE_TYPE(.L_copy, .E_copy, EX_TYPE_DEFAULT_MCE_SAFE)

SYM_FUNC_END(copy_mc_enhanced_fast_string)
#endif 
