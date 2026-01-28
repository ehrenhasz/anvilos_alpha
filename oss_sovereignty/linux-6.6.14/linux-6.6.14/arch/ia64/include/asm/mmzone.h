#ifndef _ASM_IA64_MMZONE_H
#define _ASM_IA64_MMZONE_H
#include <linux/numa.h>
#include <asm/page.h>
#include <asm/meminit.h>
#ifdef CONFIG_NUMA
static inline int pfn_to_nid(unsigned long pfn)
{
	extern int paddr_to_nid(unsigned long);
	int nid = paddr_to_nid(pfn << PAGE_SHIFT);
	if (nid < 0)
		return 0;
	else
		return nid;
}
#define MAX_PHYSNODE_ID		2048
#endif  
#define NR_NODE_MEMBLKS		(MAX_NUMNODES * 4)
#endif  
