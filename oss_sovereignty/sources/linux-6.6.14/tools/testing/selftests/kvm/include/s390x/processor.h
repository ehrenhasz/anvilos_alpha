

#ifndef SELFTEST_KVM_PROCESSOR_H
#define SELFTEST_KVM_PROCESSOR_H

#include <linux/compiler.h>


#define REGION_ENTRY_ORIGIN	~0xfffUL 
#define REGION_ENTRY_PROTECT	0x200	 
#define REGION_ENTRY_NOEXEC	0x100	 
#define REGION_ENTRY_OFFSET	0xc0	 
#define REGION_ENTRY_INVALID	0x20	 
#define REGION_ENTRY_TYPE	0x0c	 
#define REGION_ENTRY_LENGTH	0x03	 


#define PAGE_INVALID	0x400		
#define PAGE_PROTECT	0x200		
#define PAGE_NOEXEC	0x100		


static inline void cpu_relax(void)
{
	barrier();
}

#endif
