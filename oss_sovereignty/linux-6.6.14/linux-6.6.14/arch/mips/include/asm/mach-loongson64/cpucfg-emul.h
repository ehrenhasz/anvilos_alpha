#ifndef _ASM_MACH_LOONGSON64_CPUCFG_EMUL_H_
#define _ASM_MACH_LOONGSON64_CPUCFG_EMUL_H_
#include <asm/cpu-info.h>
#ifdef CONFIG_CPU_LOONGSON3_CPUCFG_EMULATION
#include <loongson_regs.h>
#define LOONGSON_FPREV_MASK 0x7
void loongson3_cpucfg_synthesize_data(struct cpuinfo_mips *c);
static inline bool loongson3_cpucfg_emulation_enabled(struct cpuinfo_mips *c)
{
	return c->loongson3_cpucfg_data[0] != 0;
}
static inline u32 loongson3_cpucfg_read_synthesized(struct cpuinfo_mips *c,
	__u64 sel)
{
	switch (sel) {
	case LOONGSON_CFG0:
		return c->processor_id;
	case LOONGSON_CFG1:
	case LOONGSON_CFG2:
	case LOONGSON_CFG3:
		return c->loongson3_cpucfg_data[sel - 1];
	case LOONGSON_CFG4:
	case LOONGSON_CFG5:
		return 0;
	case LOONGSON_CFG6:
		return 0;
	case LOONGSON_CFG7:
		return 0;
	}
	return 0;
}
#else
static inline void loongson3_cpucfg_synthesize_data(struct cpuinfo_mips *c)
{
}
static inline bool loongson3_cpucfg_emulation_enabled(struct cpuinfo_mips *c)
{
	return false;
}
static inline u32 loongson3_cpucfg_read_synthesized(struct cpuinfo_mips *c,
	__u64 sel)
{
	return 0;
}
#endif
#endif  
