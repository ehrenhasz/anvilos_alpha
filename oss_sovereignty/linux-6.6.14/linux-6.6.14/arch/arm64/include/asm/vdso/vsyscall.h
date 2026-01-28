#ifndef __ASM_VDSO_VSYSCALL_H
#define __ASM_VDSO_VSYSCALL_H
#ifndef __ASSEMBLY__
#include <linux/timekeeper_internal.h>
#include <vdso/datapage.h>
#define VDSO_PRECISION_MASK	~(0xFF00ULL<<48)
extern struct vdso_data *vdso_data;
static __always_inline
struct vdso_data *__arm64_get_k_vdso_data(void)
{
	return vdso_data;
}
#define __arch_get_k_vdso_data __arm64_get_k_vdso_data
static __always_inline
void __arm64_update_vsyscall(struct vdso_data *vdata, struct timekeeper *tk)
{
	vdata[CS_HRES_COARSE].mask	= VDSO_PRECISION_MASK;
	vdata[CS_RAW].mask		= VDSO_PRECISION_MASK;
}
#define __arch_update_vsyscall __arm64_update_vsyscall
#include <asm-generic/vdso/vsyscall.h>
#endif  
#endif  
