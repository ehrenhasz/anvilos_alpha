 
 
#ifndef __SOC_RENESAS_RCAR_GEN4_SYSC_H__
#define __SOC_RENESAS_RCAR_GEN4_SYSC_H__

#include <linux/types.h>

 
#define PD_CPU		BIT(0)	 
#define PD_SCU		BIT(1)	 
#define PD_NO_CR	BIT(2)	 

#define PD_CPU_NOCR	(PD_CPU | PD_NO_CR)  
#define PD_ALWAYS_ON	PD_NO_CR	   

 
struct rcar_gen4_sysc_area {
	const char *name;
	u8 pdr;			 
	s8 parent;		 
	u8 flags;		 
};

 
struct rcar_gen4_sysc_info {
	const struct rcar_gen4_sysc_area *areas;
	unsigned int num_areas;
};

extern const struct rcar_gen4_sysc_info r8a779a0_sysc_info;
extern const struct rcar_gen4_sysc_info r8a779f0_sysc_info;
extern const struct rcar_gen4_sysc_info r8a779g0_sysc_info;

#endif  
