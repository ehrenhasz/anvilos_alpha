#ifndef _SPARC64_CPUDATA_H
#define _SPARC64_CPUDATA_H
#ifndef __ASSEMBLY__
typedef struct {
	unsigned int	__softirq_pending;  
	unsigned int	__nmi_count;
	unsigned long	clock_tick;	 
	unsigned long	__pad;
	unsigned int	irq0_irqs;
	unsigned int	__pad2;
	unsigned int	dcache_size;
	unsigned int	dcache_line_size;
	unsigned int	icache_size;
	unsigned int	icache_line_size;
	unsigned int	ecache_size;
	unsigned int	ecache_line_size;
	unsigned short	sock_id;	 
	unsigned short	core_id;
	unsigned short  max_cache_id;	 
	signed short	proc_id;	 
} cpuinfo_sparc;
DECLARE_PER_CPU(cpuinfo_sparc, __cpu_data);
#define cpu_data(__cpu)		per_cpu(__cpu_data, (__cpu))
#define local_cpu_data()	(*this_cpu_ptr(&__cpu_data))
#endif  
#include <asm/trap_block.h>
#endif  
