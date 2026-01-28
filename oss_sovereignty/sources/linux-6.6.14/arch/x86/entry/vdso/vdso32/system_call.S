


#include <linux/linkage.h>
#include <asm/dwarf2.h>
#include <asm/cpufeatures.h>
#include <asm/alternative.h>

	.text
	.globl __kernel_vsyscall
	.type __kernel_vsyscall,@function
	ALIGN
__kernel_vsyscall:
	CFI_STARTPROC
	
	pushl	%ecx
	CFI_ADJUST_CFA_OFFSET	4
	CFI_REL_OFFSET		ecx, 0
	pushl	%edx
	CFI_ADJUST_CFA_OFFSET	4
	CFI_REL_OFFSET		edx, 0
	pushl	%ebp
	CFI_ADJUST_CFA_OFFSET	4
	CFI_REL_OFFSET		ebp, 0

	#define SYSENTER_SEQUENCE	"movl %esp, %ebp; sysenter"
	#define SYSCALL_SEQUENCE	"movl %ecx, %ebp; syscall"

#ifdef CONFIG_X86_64
	
	ALTERNATIVE_2 "", SYSENTER_SEQUENCE, X86_FEATURE_SYSENTER32, \
	                  SYSCALL_SEQUENCE,  X86_FEATURE_SYSCALL32
#else
	ALTERNATIVE "", SYSENTER_SEQUENCE, X86_FEATURE_SEP
#endif

	
	int	$0x80
SYM_INNER_LABEL(int80_landing_pad, SYM_L_GLOBAL)

	
	popl	%ebp
	CFI_RESTORE		ebp
	CFI_ADJUST_CFA_OFFSET	-4
	popl	%edx
	CFI_RESTORE		edx
	CFI_ADJUST_CFA_OFFSET	-4
	popl	%ecx
	CFI_RESTORE		ecx
	CFI_ADJUST_CFA_OFFSET	-4
	RET
	CFI_ENDPROC

	.size __kernel_vsyscall,.-__kernel_vsyscall
	.previous
