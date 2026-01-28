#ifndef __ASM_VDSO_H
#define __ASM_VDSO_H
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <vdso/datapage.h>
#include <asm/barrier.h>
struct loongarch_vdso_info {
	void *vdso;
	unsigned long size;
	unsigned long offset_sigreturn;
	struct vm_special_mapping code_mapping;
	struct vm_special_mapping data_mapping;
};
extern struct loongarch_vdso_info vdso_info;
#endif  
