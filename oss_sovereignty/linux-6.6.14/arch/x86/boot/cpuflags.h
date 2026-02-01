 
#ifndef BOOT_CPUFLAGS_H
#define BOOT_CPUFLAGS_H

#include <asm/cpufeatures.h>
#include <asm/processor-flags.h>

struct cpu_features {
	int level;		 
	int family;		 
	int model;
	u32 flags[NCAPINTS];
};

extern struct cpu_features cpu;
extern u32 cpu_vendor[3];

int has_eflag(unsigned long mask);
void get_cpuflags(void);
void cpuid_count(u32 id, u32 count, u32 *a, u32 *b, u32 *c, u32 *d);

#endif
