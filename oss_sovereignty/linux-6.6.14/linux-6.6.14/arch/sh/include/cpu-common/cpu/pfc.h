#ifndef __ARCH_SH_CPU_PFC_H__
#define __ARCH_SH_CPU_PFC_H__
#include <linux/types.h>
struct resource;
int sh_pfc_register(const char *name,
		    struct resource *resource, u32 num_resources);
#endif  
