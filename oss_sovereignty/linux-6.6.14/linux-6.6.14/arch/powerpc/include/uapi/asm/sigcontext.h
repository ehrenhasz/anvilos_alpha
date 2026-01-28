#ifndef _ASM_POWERPC_SIGCONTEXT_H
#define _ASM_POWERPC_SIGCONTEXT_H
#include <linux/compiler.h>
#include <asm/ptrace.h>
#ifdef __powerpc64__
#include <asm/elf.h>
#endif
struct sigcontext {
	unsigned long	_unused[4];
	int		signal;
#ifdef __powerpc64__
	int		_pad0;
#endif
	unsigned long	handler;
	unsigned long	oldmask;
#ifdef __KERNEL__
	struct user_pt_regs __user *regs;
#else
	struct pt_regs	*regs;
#endif
#ifdef __powerpc64__
	elf_gregset_t	gp_regs;
	elf_fpregset_t	fp_regs;
	elf_vrreg_t	__user *v_regs;
	long		vmx_reserve[ELF_NVRREG + ELF_NVRREG + 1 + 32];
#endif
};
#endif  
