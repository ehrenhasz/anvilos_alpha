 
#include <linux/linkage.h>
#include <asm/unistd_32.h>
#include <asm/asm-offsets.h>

#ifndef SYSCALL_ENTER_KERNEL
#define	SYSCALL_ENTER_KERNEL	int $0x80
#endif

	.text
	.globl __kernel_sigreturn
	.type __kernel_sigreturn,@function
	nop  
	ALIGN
__kernel_sigreturn:
.LSTART_sigreturn:
	popl %eax		 
	movl $__NR_sigreturn, %eax
	SYSCALL_ENTER_KERNEL
.LEND_sigreturn:
SYM_INNER_LABEL(vdso32_sigreturn_landing_pad, SYM_L_GLOBAL)
	nop
	.size __kernel_sigreturn,.-.LSTART_sigreturn

	.globl __kernel_rt_sigreturn
	.type __kernel_rt_sigreturn,@function
	ALIGN
__kernel_rt_sigreturn:
.LSTART_rt_sigreturn:
	movl $__NR_rt_sigreturn, %eax
	SYSCALL_ENTER_KERNEL
.LEND_rt_sigreturn:
SYM_INNER_LABEL(vdso32_rt_sigreturn_landing_pad, SYM_L_GLOBAL)
	nop
	.size __kernel_rt_sigreturn,.-.LSTART_rt_sigreturn
	.previous

	.section .eh_frame,"a",@progbits
.LSTARTFRAMEDLSI1:
	.long .LENDCIEDLSI1-.LSTARTCIEDLSI1
.LSTARTCIEDLSI1:
	.long 0			 
	.byte 1			 
	.string "zRS"		 
	.uleb128 1		 
	.sleb128 -4		 
	.byte 8			 
	.uleb128 1		 
	.byte 0x1b		 
	.byte 0			 
	.align 4
.LENDCIEDLSI1:
	.long .LENDFDEDLSI1-.LSTARTFDEDLSI1  
.LSTARTFDEDLSI1:
	.long .LSTARTFDEDLSI1-.LSTARTFRAMEDLSI1  
	 
	.long .LSTART_sigreturn-1-.	 
	.long .LEND_sigreturn-.LSTART_sigreturn+1
	.uleb128 0			 
	 

#define do_cfa_expr(offset)						\
	.byte 0x0f;			 	\
	.uleb128 1f-0f;			 			\
0:	.byte 0x74;			 		\
	.sleb128 offset;		 		\
	.byte 0x06;			 		\
1:

#define do_expr(regno, offset)						\
	.byte 0x10;			 		\
	.uleb128 regno;			 			\
	.uleb128 1f-0f;			 			\
0:	.byte 0x74;			 		\
	.sleb128 offset;		 		\
1:

	do_cfa_expr(IA32_SIGCONTEXT_sp+4)
	do_expr(0, IA32_SIGCONTEXT_ax+4)
	do_expr(1, IA32_SIGCONTEXT_cx+4)
	do_expr(2, IA32_SIGCONTEXT_dx+4)
	do_expr(3, IA32_SIGCONTEXT_bx+4)
	do_expr(5, IA32_SIGCONTEXT_bp+4)
	do_expr(6, IA32_SIGCONTEXT_si+4)
	do_expr(7, IA32_SIGCONTEXT_di+4)
	do_expr(8, IA32_SIGCONTEXT_ip+4)

	.byte 0x42	 

	do_cfa_expr(IA32_SIGCONTEXT_sp)
	do_expr(0, IA32_SIGCONTEXT_ax)
	do_expr(1, IA32_SIGCONTEXT_cx)
	do_expr(2, IA32_SIGCONTEXT_dx)
	do_expr(3, IA32_SIGCONTEXT_bx)
	do_expr(5, IA32_SIGCONTEXT_bp)
	do_expr(6, IA32_SIGCONTEXT_si)
	do_expr(7, IA32_SIGCONTEXT_di)
	do_expr(8, IA32_SIGCONTEXT_ip)

	.align 4
.LENDFDEDLSI1:

	.long .LENDFDEDLSI2-.LSTARTFDEDLSI2  
.LSTARTFDEDLSI2:
	.long .LSTARTFDEDLSI2-.LSTARTFRAMEDLSI1  
	 
	.long .LSTART_rt_sigreturn-1-.	 
	.long .LEND_rt_sigreturn-.LSTART_rt_sigreturn+1
	.uleb128 0			 
	 

	do_cfa_expr(IA32_RT_SIGFRAME_sigcontext-4 + IA32_SIGCONTEXT_sp)
	do_expr(0, IA32_RT_SIGFRAME_sigcontext-4 + IA32_SIGCONTEXT_ax)
	do_expr(1, IA32_RT_SIGFRAME_sigcontext-4 + IA32_SIGCONTEXT_cx)
	do_expr(2, IA32_RT_SIGFRAME_sigcontext-4 + IA32_SIGCONTEXT_dx)
	do_expr(3, IA32_RT_SIGFRAME_sigcontext-4 + IA32_SIGCONTEXT_bx)
	do_expr(5, IA32_RT_SIGFRAME_sigcontext-4 + IA32_SIGCONTEXT_bp)
	do_expr(6, IA32_RT_SIGFRAME_sigcontext-4 + IA32_SIGCONTEXT_si)
	do_expr(7, IA32_RT_SIGFRAME_sigcontext-4 + IA32_SIGCONTEXT_di)
	do_expr(8, IA32_RT_SIGFRAME_sigcontext-4 + IA32_SIGCONTEXT_ip)

	.align 4
.LENDFDEDLSI2:
	.previous
