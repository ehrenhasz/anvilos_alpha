#ifndef __ASM_VDSO_VSYSCALL_H
#define __ASM_VDSO_VSYSCALL_H
#ifndef __ASSEMBLY__
#include <linux/timekeeper_internal.h>
#include <vdso/datapage.h>
extern struct vdso_data *vdso_data;
static __always_inline
struct vdso_data *__loongarch_get_k_vdso_data(void)
{
	return vdso_data;
}
#define __arch_get_k_vdso_data __loongarch_get_k_vdso_data
#include <asm-generic/vdso/vsyscall.h>
#endif  
#endif  
