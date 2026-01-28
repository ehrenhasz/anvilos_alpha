
#ifndef _MEMBLOCK_ALLOC_EXACT_NID_H
#define _MEMBLOCK_ALLOC_EXACT_NID_H

#include "common.h"

int memblock_alloc_exact_nid_checks(void);
int __memblock_alloc_exact_nid_numa_checks(void);

#ifdef CONFIG_NUMA
static inline int memblock_alloc_exact_nid_numa_checks(void)
{
	__memblock_alloc_exact_nid_numa_checks();
	return 0;
}

#else
static inline int memblock_alloc_exact_nid_numa_checks(void)
{
	return 0;
}

#endif 

#endif
