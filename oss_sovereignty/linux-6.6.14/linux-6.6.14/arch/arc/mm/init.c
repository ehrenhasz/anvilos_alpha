#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/memblock.h>
#ifdef CONFIG_BLK_DEV_INITRD
#include <linux/initrd.h>
#endif
#include <linux/of_fdt.h>
#include <linux/swap.h>
#include <linux/module.h>
#include <linux/highmem.h>
#include <asm/page.h>
#include <asm/sections.h>
#include <asm/setup.h>
#include <asm/arcregs.h>
pgd_t swapper_pg_dir[PTRS_PER_PGD] __aligned(PAGE_SIZE);
char empty_zero_page[PAGE_SIZE] __aligned(PAGE_SIZE);
EXPORT_SYMBOL(empty_zero_page);
static const unsigned long low_mem_start = CONFIG_LINUX_RAM_BASE;
static unsigned long low_mem_sz;
#ifdef CONFIG_HIGHMEM
static unsigned long min_high_pfn, max_high_pfn;
static phys_addr_t high_mem_start;
static phys_addr_t high_mem_sz;
unsigned long arch_pfn_offset;
EXPORT_SYMBOL(arch_pfn_offset);
#endif
long __init arc_get_mem_sz(void)
{
	return low_mem_sz;
}
static int __init setup_mem_sz(char *str)
{
	low_mem_sz = memparse(str, NULL) & PAGE_MASK;
	pr_info("\"mem=%s\": mem sz set to %ldM\n", str, TO_MB(low_mem_sz));
	return 0;
}
early_param("mem", setup_mem_sz);
void __init early_init_dt_add_memory_arch(u64 base, u64 size)
{
	int in_use = 0;
	if (!low_mem_sz) {
		if (base != low_mem_start)
			panic("CONFIG_LINUX_RAM_BASE != DT memory { }");
		low_mem_sz = size;
		in_use = 1;
		memblock_add_node(base, size, 0, MEMBLOCK_NONE);
	} else {
#ifdef CONFIG_HIGHMEM
		high_mem_start = base;
		high_mem_sz = size;
		in_use = 1;
		memblock_add_node(base, size, 1, MEMBLOCK_NONE);
		memblock_reserve(base, size);
#endif
	}
	pr_info("Memory @ %llx [%lldM] %s\n",
		base, TO_MB(size), !in_use ? "Not used":"");
}
void __init setup_arch_memory(void)
{
	unsigned long max_zone_pfn[MAX_NR_ZONES] = { 0 };
	setup_initial_init_mm(_text, _etext, _edata, _end);
	min_low_pfn = virt_to_pfn((void *)CONFIG_LINUX_RAM_BASE);
	max_low_pfn = max_pfn = PFN_DOWN(low_mem_start + low_mem_sz);
	memblock_reserve(CONFIG_LINUX_LINK_BASE,
			 __pa(_end) - CONFIG_LINUX_LINK_BASE);
#ifdef CONFIG_BLK_DEV_INITRD
	if (phys_initrd_size) {
		memblock_reserve(phys_initrd_start, phys_initrd_size);
		initrd_start = (unsigned long)__va(phys_initrd_start);
		initrd_end = initrd_start + phys_initrd_size;
	}
#endif
	early_init_fdt_reserve_self();
	early_init_fdt_scan_reserved_mem();
	memblock_dump_all();
	max_zone_pfn[ZONE_NORMAL] = max_low_pfn;
#ifdef CONFIG_HIGHMEM
	min_high_pfn = PFN_DOWN(high_mem_start);
	max_high_pfn = PFN_DOWN(high_mem_start + high_mem_sz);
	max_zone_pfn[ZONE_HIGHMEM] = max_high_pfn;
	high_memory = (void *)(min_high_pfn << PAGE_SHIFT);
	arch_pfn_offset = min(min_low_pfn, min_high_pfn);
	kmap_init();
#else  
	max_mapnr = max_low_pfn - min_low_pfn;
#endif  
	free_area_init(max_zone_pfn);
}
static void __init highmem_init(void)
{
#ifdef CONFIG_HIGHMEM
	unsigned long tmp;
	memblock_phys_free(high_mem_start, high_mem_sz);
	for (tmp = min_high_pfn; tmp < max_high_pfn; tmp++)
		free_highmem_page(pfn_to_page(tmp));
#endif
}
void __init mem_init(void)
{
	memblock_free_all();
	highmem_init();
	BUILD_BUG_ON((PTRS_PER_PGD * sizeof(pgd_t)) > PAGE_SIZE);
	BUILD_BUG_ON((PTRS_PER_PUD * sizeof(pud_t)) > PAGE_SIZE);
	BUILD_BUG_ON((PTRS_PER_PMD * sizeof(pmd_t)) > PAGE_SIZE);
	BUILD_BUG_ON((PTRS_PER_PTE * sizeof(pte_t)) > PAGE_SIZE);
}
#ifdef CONFIG_HIGHMEM
int pfn_valid(unsigned long pfn)
{
	return (pfn >= min_high_pfn && pfn <= max_high_pfn) ||
		(pfn >= min_low_pfn && pfn <= max_low_pfn);
}
EXPORT_SYMBOL(pfn_valid);
#endif
