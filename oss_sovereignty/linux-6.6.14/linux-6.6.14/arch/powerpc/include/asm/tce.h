#ifndef _ASM_POWERPC_TCE_H
#define _ASM_POWERPC_TCE_H
#ifdef __KERNEL__
#include <asm/iommu.h>
#define TCE_VB			0
#define TCE_PCI			1
#define TCE_ENTRY_SIZE		8		 
#define TCE_VALID		0x800		 
#define TCE_ALLIO		0x400		 
#define TCE_PCI_WRITE		0x2		 
#define TCE_PCI_READ		0x1		 
#define TCE_VB_WRITE		0x1		 
#endif  
#endif  
