#ifndef _ASM_CPUFEATURE_H
#define _ASM_CPUFEATURE_H
#include <linux/bitmap.h>
#include <asm/hwcap.h>
struct riscv_cpuinfo {
	unsigned long mvendorid;
	unsigned long marchid;
	unsigned long mimpid;
};
struct riscv_isainfo {
	DECLARE_BITMAP(isa, RISCV_ISA_EXT_MAX);
};
DECLARE_PER_CPU(struct riscv_cpuinfo, riscv_cpuinfo);
DECLARE_PER_CPU(long, misaligned_access_speed);
extern struct riscv_isainfo hart_isa[NR_CPUS];
void check_unaligned_access(int cpu);
#endif
