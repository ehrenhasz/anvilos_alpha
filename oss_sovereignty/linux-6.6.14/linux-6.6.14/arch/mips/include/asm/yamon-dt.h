#ifndef __MIPS_ASM_YAMON_DT_H__
#define __MIPS_ASM_YAMON_DT_H__
#include <linux/types.h>
struct yamon_mem_region {
	phys_addr_t	start;
	phys_addr_t	size;
	phys_addr_t	discard;
};
extern __init int yamon_dt_append_cmdline(void *fdt);
extern __init int yamon_dt_append_memory(void *fdt,
					const struct yamon_mem_region *regions);
extern __init int yamon_dt_serial_config(void *fdt);
#endif  
