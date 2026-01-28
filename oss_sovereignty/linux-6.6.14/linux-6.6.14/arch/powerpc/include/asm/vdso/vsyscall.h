#ifndef _ASM_POWERPC_VDSO_VSYSCALL_H
#define _ASM_POWERPC_VDSO_VSYSCALL_H
#ifndef __ASSEMBLY__
#include <linux/timekeeper_internal.h>
#include <asm/vdso_datapage.h>
static __always_inline
struct vdso_data *__arch_get_k_vdso_data(void)
{
	return vdso_data->data;
}
#define __arch_get_k_vdso_data __arch_get_k_vdso_data
#include <asm-generic/vdso/vsyscall.h>
#endif  
#endif  
