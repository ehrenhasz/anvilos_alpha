#ifndef __ASM_CSKY_VDSO_H
#define __ASM_CSKY_VDSO_H
#include <linux/types.h>
#ifndef GENERIC_TIME_VSYSCALL
struct vdso_data {
};
#endif
#define VDSO_SYMBOL(base, name)							\
({										\
	extern const char __vdso_##name[];					\
	(void __user *)((unsigned long)(base) + __vdso_##name);			\
})
#endif  
