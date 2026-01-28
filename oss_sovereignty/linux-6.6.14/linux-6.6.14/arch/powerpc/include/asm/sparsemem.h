#ifndef _ASM_POWERPC_SPARSEMEM_H
#define _ASM_POWERPC_SPARSEMEM_H 1
#ifdef __KERNEL__
#ifdef CONFIG_SPARSEMEM
#define SECTION_SIZE_BITS       24
#endif  
#ifdef CONFIG_MEMORY_HOTPLUG
extern int remove_section_mapping(unsigned long start, unsigned long end);
extern int memory_add_physaddr_to_nid(u64 start);
#define memory_add_physaddr_to_nid memory_add_physaddr_to_nid
#ifdef CONFIG_NUMA
extern int hot_add_scn_to_nid(unsigned long scn_addr);
#else
static inline int hot_add_scn_to_nid(unsigned long scn_addr)
{
	return 0;
}
#endif  
#endif  
#endif  
#endif  
