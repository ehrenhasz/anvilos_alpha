#ifndef __ASM_OPENRISC_CPUINFO_H
#define __ASM_OPENRISC_CPUINFO_H
struct cpuinfo_or1k {
	u32 clock_frequency;
	u32 icache_size;
	u32 icache_block_size;
	u32 icache_ways;
	u32 dcache_size;
	u32 dcache_block_size;
	u32 dcache_ways;
	u16 coreid;
};
extern struct cpuinfo_or1k cpuinfo_or1k[NR_CPUS];
extern void setup_cpuinfo(void);
#endif  
