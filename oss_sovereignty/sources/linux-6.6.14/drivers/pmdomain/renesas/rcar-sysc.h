

#ifndef __SOC_RENESAS_RCAR_SYSC_H__
#define __SOC_RENESAS_RCAR_SYSC_H__

#include <linux/types.h>



#define PD_CPU		BIT(0)	
#define PD_SCU		BIT(1)	
#define PD_NO_CR	BIT(2)	

#define PD_CPU_CR	PD_CPU		  
#define PD_CPU_NOCR	PD_CPU | PD_NO_CR 
#define PD_ALWAYS_ON	PD_NO_CR	  




struct rcar_sysc_area {
	const char *name;
	u16 chan_offs;		
	u8 chan_bit;		
	u8 isr_bit;		
	s8 parent;		
	u8 flags;		
};




struct rcar_sysc_info {
	int (*init)(void);	
	const struct rcar_sysc_area *areas;
	unsigned int num_areas;
	
	u32 extmask_offs;	
	u32 extmask_val;	
};

extern const struct rcar_sysc_info r8a7742_sysc_info;
extern const struct rcar_sysc_info r8a7743_sysc_info;
extern const struct rcar_sysc_info r8a7745_sysc_info;
extern const struct rcar_sysc_info r8a77470_sysc_info;
extern const struct rcar_sysc_info r8a774a1_sysc_info;
extern const struct rcar_sysc_info r8a774b1_sysc_info;
extern const struct rcar_sysc_info r8a774c0_sysc_info;
extern const struct rcar_sysc_info r8a774e1_sysc_info;
extern const struct rcar_sysc_info r8a7779_sysc_info;
extern const struct rcar_sysc_info r8a7790_sysc_info;
extern const struct rcar_sysc_info r8a7791_sysc_info;
extern const struct rcar_sysc_info r8a7792_sysc_info;
extern const struct rcar_sysc_info r8a7794_sysc_info;
extern struct rcar_sysc_info r8a7795_sysc_info;
extern const struct rcar_sysc_info r8a77960_sysc_info;
extern const struct rcar_sysc_info r8a77961_sysc_info;
extern const struct rcar_sysc_info r8a77965_sysc_info;
extern const struct rcar_sysc_info r8a77970_sysc_info;
extern const struct rcar_sysc_info r8a77980_sysc_info;
extern const struct rcar_sysc_info r8a77990_sysc_info;
extern const struct rcar_sysc_info r8a77995_sysc_info;


    

extern void rcar_sysc_nullify(struct rcar_sysc_area *areas,
			      unsigned int num_areas, u8 id);

#endif 
