#ifndef __ASM_VDSO_H
#define __ASM_VDSO_H
#define VDSO_LBASE	0x0
#define __VVAR_PAGES    2
#ifndef __ASSEMBLY__
#include <generated/vdso-offsets.h>
#ifdef CONFIG_COMPAT_VDSO
#include <generated/vdso32-offsets.h>
#endif
#define VDSO_SYMBOL(base, name)						   \
({									   \
	(void *)(vdso_offset_##name - VDSO_LBASE + (unsigned long)(base)); \
})
extern char vdso_start[], vdso_end[];
extern char vdso32_start[], vdso32_end[];
#endif  
#endif  
