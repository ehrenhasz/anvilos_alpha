#ifndef __ASM_VDSO_H
#define __ASM_VDSO_H
#include <linux/mm_types.h>
#include <vdso/datapage.h>
#include <asm/barrier.h>
struct mips_vdso_image {
	void *data;
	unsigned long size;
	unsigned long off_sigreturn;
	unsigned long off_rt_sigreturn;
	struct vm_special_mapping mapping;
};
extern struct mips_vdso_image vdso_image;
#ifdef CONFIG_MIPS32_O32
extern struct mips_vdso_image vdso_image_o32;
#endif
#ifdef CONFIG_MIPS32_N32
extern struct mips_vdso_image vdso_image_n32;
#endif
union mips_vdso_data {
	struct vdso_data data[CS_BASES];
	u8 page[PAGE_SIZE];
};
#endif  
