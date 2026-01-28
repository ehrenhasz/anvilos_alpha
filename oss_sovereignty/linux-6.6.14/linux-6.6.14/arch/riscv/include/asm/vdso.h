#ifndef _ASM_RISCV_VDSO_H
#define _ASM_RISCV_VDSO_H
#ifdef CONFIG_MMU
#define __VVAR_PAGES    2
#ifndef __ASSEMBLY__
#include <generated/vdso-offsets.h>
#define VDSO_SYMBOL(base, name)							\
	(void __user *)((unsigned long)(base) + __vdso_##name##_offset)
#ifdef CONFIG_COMPAT
#include <generated/compat_vdso-offsets.h>
#define COMPAT_VDSO_SYMBOL(base, name)						\
	(void __user *)((unsigned long)(base) + compat__vdso_##name##_offset)
extern char compat_vdso_start[], compat_vdso_end[];
#endif  
extern char vdso_start[], vdso_end[];
#endif  
#endif  
#endif  
