


#include <linux/linkage.h>

#include <asm/processor-flags.h>
#include <asm/msr.h>
#include <asm/asm-offsets.h>
#include <asm/segment.h>
#include <asm/trapnr.h>

	.text
	.code32
SYM_FUNC_START(get_sev_encryption_bit)
	push	%ebx

	movl	$0x80000000, %eax	
	cpuid
	cmpl	$0x8000001f, %eax	
	jb	.Lno_sev

	
	movl	$0x8000001f, %eax
	cpuid
	bt	$1, %eax		
	jnc	.Lno_sev

	movl	$MSR_AMD64_SEV, %ecx	
	rdmsr
	bt	$MSR_AMD64_SEV_ENABLED_BIT, %eax	
	jnc	.Lno_sev

	movl	%ebx, %eax
	andl	$0x3f, %eax		
	jmp	.Lsev_exit

.Lno_sev:
	xor	%eax, %eax

.Lsev_exit:
	pop	%ebx
	RET
SYM_FUNC_END(get_sev_encryption_bit)


SYM_CODE_START_LOCAL(sev_es_req_cpuid)
	shll	$30, %eax
	orl     $0x00000004, %eax
	movl    $MSR_AMD64_SEV_ES_GHCB, %ecx
	wrmsr
	rep; vmmcall		# VMGEXIT
	rdmsr

	
	movl	%eax, %ecx
	andl	$0x3ffff000, %ecx	# Bits [12-29] MBZ
	jnz	2f

	
	andl    $0xfff, %eax
	cmpl    $5, %eax
	jne	2f

	
	xorl	%eax, %eax
1:
	RET
2:
	movl	$-1, %eax
	jmp	1b
SYM_CODE_END(sev_es_req_cpuid)

SYM_CODE_START_LOCAL(startup32_vc_handler)
	pushl	%eax
	pushl	%ebx
	pushl	%ecx
	pushl	%edx

	
	movl	%eax, %ebx

	
	cmpl	$0x72, 16(%esp)
	jne	.Lfail

	movl	$0, %eax		# Request CPUID[fn].EAX
	movl	%ebx, %edx		# CPUID fn
	call	sev_es_req_cpuid	# Call helper
	testl	%eax, %eax		# Check return code
	jnz	.Lfail
	movl	%edx, 12(%esp)		# Store result

	movl	$1, %eax		# Request CPUID[fn].EBX
	movl	%ebx, %edx		# CPUID fn
	call	sev_es_req_cpuid	# Call helper
	testl	%eax, %eax		# Check return code
	jnz	.Lfail
	movl	%edx, 8(%esp)		# Store result

	movl	$2, %eax		# Request CPUID[fn].ECX
	movl	%ebx, %edx		# CPUID fn
	call	sev_es_req_cpuid	# Call helper
	testl	%eax, %eax		# Check return code
	jnz	.Lfail
	movl	%edx, 4(%esp)		# Store result

	movl	$3, %eax		# Request CPUID[fn].EDX
	movl	%ebx, %edx		# CPUID fn
	call	sev_es_req_cpuid	# Call helper
	testl	%eax, %eax		# Check return code
	jnz	.Lfail
	movl	%edx, 0(%esp)		# Store result

	

	
	cmpl    $0x80000000, %ebx
	jne     .Lcheck_sev
	cmpl    $0x8000001f, 12(%esp)
	jb      .Lfail
	jmp     .Ldone

.Lcheck_sev:
	
	cmpl    $0x8000001f, %ebx
	jne     .Ldone
	btl     $1, 12(%esp)
	jnc     .Lfail

.Ldone:
	popl	%edx
	popl	%ecx
	popl	%ebx
	popl	%eax

	
	addl	$4, %esp

	
	addl	$2, (%esp)

	iret
.Lfail:
	
	movl    $0x100, %eax
	xorl    %edx, %edx
	movl    $MSR_AMD64_SEV_ES_GHCB, %ecx
	wrmsr
	rep; vmmcall

	
	hlt
	jmp .Lfail
SYM_CODE_END(startup32_vc_handler)


SYM_FUNC_START_LOCAL(startup32_set_idt_entry)
	
	leal	(%ecx, %edx, 8), %ecx

	
	movl    %eax, %edx
	andl    $0x0000ffff, %edx		# Target code segment offset [15:0]
	orl	$(__KERNEL32_CS << 16), %edx	# Target code segment selector

	
	movl    %edx, (%ecx)

	
	movl    %eax, %edx
	andl    $0xffff0000, %edx	# Target code segment offset [31:16]
	orl     $0x00008e00, %edx	# Present, Type 32-bit Interrupt Gate

	
	movl    %edx, 4(%ecx)

	RET
SYM_FUNC_END(startup32_set_idt_entry)

SYM_FUNC_START(startup32_load_idt)
	push	%ebp
	push	%ebx

	call	1f
1:	pop	%ebp

	leal    (boot32_idt - 1b)(%ebp), %ebx

	
	leal    (startup32_vc_handler - 1b)(%ebp), %eax
	movl    $X86_TRAP_VC, %edx
	movl	%ebx, %ecx
	call    startup32_set_idt_entry

	
	leal	(boot32_idt_desc - 1b)(%ebp), %ecx
	movl	%ebx, 2(%ecx)
	lidt    (%ecx)

	pop	%ebx
	pop	%ebp
	RET
SYM_FUNC_END(startup32_load_idt)


SYM_FUNC_START(startup32_check_sev_cbit)
	pushl	%ebx
	pushl	%ebp

	call	0f
0:	popl	%ebp

	
	movl	(sev_status - 0b)(%ebp), %eax
	testl	%eax, %eax
	jz	4f

	
1:	rdrand	%eax
	jnc	1b
2:	rdrand	%ebx
	jnc	2b

	
	leal	(sev_check_data - 0b)(%ebp), %ebp
	movl	%eax, 0(%ebp)
	movl	%ebx, 4(%ebp)

	
	movl	%cr0, %edx			 
	movl	$(X86_CR0_PG | X86_CR0_PE), %ecx 
	movl	%ecx, %cr0

	cmpl	%eax, 0(%ebp)
	jne	3f
	cmpl	%ebx, 4(%ebp)
	jne	3f

	movl	%edx, %cr0	

	jmp	4f

3:	
	hlt
	jmp	3b

4:
	popl	%ebp
	popl	%ebx
	RET
SYM_FUNC_END(startup32_check_sev_cbit)

	.code64

#include "../../kernel/sev_verify_cbit.S"

	.data

	.balign	8
SYM_DATA(sme_me_mask,		.quad 0)
SYM_DATA(sev_status,		.quad 0)
SYM_DATA(sev_check_data,	.quad 0)

SYM_DATA_START_LOCAL(boot32_idt)
	.rept	32
	.quad	0
	.endr
SYM_DATA_END(boot32_idt)

SYM_DATA_START_LOCAL(boot32_idt_desc)
	.word	. - boot32_idt - 1
	.long	0
SYM_DATA_END(boot32_idt_desc)
