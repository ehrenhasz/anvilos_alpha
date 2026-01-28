#ifndef __RISCV_ASM_VDSO_DATA_H
#define __RISCV_ASM_VDSO_DATA_H
#include <linux/types.h>
#include <vdso/datapage.h>
#include <asm/hwprobe.h>
struct arch_vdso_data {
	__u64 all_cpu_hwprobe_values[RISCV_HWPROBE_MAX_KEY + 1];
	__u8 homogeneous_cpus;
};
#endif  
