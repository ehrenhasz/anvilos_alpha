#ifndef _ASM_ARCH_CPU_H
#define _ASM_ARCH_CPU_H
#include "common.h"
struct davinci_id {
	u8	variant;	 
	u16	part_no;	 
	u16	manufacturer;	 
	u32	cpu_id;
	char	*name;
};
#define	DAVINCI_CPU_ID_DA830		0x08300000
#define	DAVINCI_CPU_ID_DA850		0x08500000
#endif
