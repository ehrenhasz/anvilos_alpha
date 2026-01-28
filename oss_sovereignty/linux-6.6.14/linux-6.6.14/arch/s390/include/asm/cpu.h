#ifndef _ASM_S390_CPU_H
#define _ASM_S390_CPU_H
#ifndef __ASSEMBLY__
#include <linux/types.h>
#include <linux/jump_label.h>
struct cpuid
{
	unsigned int version :	8;
	unsigned int ident   : 24;
	unsigned int machine : 16;
	unsigned int unused  : 16;
} __attribute__ ((packed, aligned(8)));
DECLARE_STATIC_KEY_FALSE(cpu_has_bear);
#endif  
#endif  
