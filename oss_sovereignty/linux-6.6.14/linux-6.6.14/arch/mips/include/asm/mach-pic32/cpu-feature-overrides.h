#ifndef __ASM_MACH_PIC32_CPU_FEATURE_OVERRIDES_H
#define __ASM_MACH_PIC32_CPU_FEATURE_OVERRIDES_H
#ifdef CONFIG_CPU_MIPS32
#define cpu_has_vint		1
#define cpu_has_veic		0
#define cpu_has_tlb		1
#define cpu_has_4kex		1
#define cpu_has_4k_cache	1
#define cpu_has_fpu		0
#define cpu_has_counter		1
#define cpu_has_llsc		1
#define cpu_has_nofpuex		0
#define cpu_icache_snoops_remote_store 1
#endif
#ifdef CONFIG_CPU_MIPS64
#error This platform does not support 64bit.
#endif
#endif  
