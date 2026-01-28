#ifndef _ASM_FIXMAP_H
#define _ASM_FIXMAP_H
#define TMPALIAS_SIZE_BITS	22	 
#define TMPALIAS_MAP_START	((__PAGE_OFFSET) - (2 << TMPALIAS_SIZE_BITS))
#define FIXMAP_SIZE		(FIX_BITMAP_COUNT << PAGE_SHIFT)
#define FIXMAP_START		(TMPALIAS_MAP_START - FIXMAP_SIZE)
#define KERNEL_MAP_START	(GATEWAY_PAGE_SIZE)
#define KERNEL_MAP_END		(FIXMAP_START)
#ifndef __ASSEMBLY__
enum fixed_addresses {
	FIX_TEXT_POKE0,
	FIX_TEXT_KEXEC,
	FIX_BITMAP_COUNT
};
extern void *parisc_vmalloc_start;
#define PCXL_DMA_MAP_SIZE	(8*1024*1024)
#define VMALLOC_START		((unsigned long)parisc_vmalloc_start)
#define VMALLOC_END		(KERNEL_MAP_END)
#define __fix_to_virt(_x) (FIXMAP_START + ((_x) << PAGE_SHIFT))
void set_fixmap(enum fixed_addresses idx, phys_addr_t phys);
void clear_fixmap(enum fixed_addresses idx);
#endif  
#endif  
