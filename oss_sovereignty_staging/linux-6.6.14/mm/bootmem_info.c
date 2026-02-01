
 
#include <linux/mm.h>
#include <linux/compiler.h>
#include <linux/memblock.h>
#include <linux/bootmem_info.h>
#include <linux/memory_hotplug.h>
#include <linux/kmemleak.h>

void get_page_bootmem(unsigned long info, struct page *page, unsigned long type)
{
	page->index = type;
	SetPagePrivate(page);
	set_page_private(page, info);
	page_ref_inc(page);
}

void put_page_bootmem(struct page *page)
{
	unsigned long type = page->index;

	BUG_ON(type < MEMORY_HOTPLUG_MIN_BOOTMEM_TYPE ||
	       type > MEMORY_HOTPLUG_MAX_BOOTMEM_TYPE);

	if (page_ref_dec_return(page) == 1) {
		page->index = 0;
		ClearPagePrivate(page);
		set_page_private(page, 0);
		INIT_LIST_HEAD(&page->lru);
		kmemleak_free_part(page_to_virt(page), PAGE_SIZE);
		free_reserved_page(page);
	}
}

#ifndef CONFIG_SPARSEMEM_VMEMMAP
static void __init register_page_bootmem_info_section(unsigned long start_pfn)
{
	unsigned long mapsize, section_nr, i;
	struct mem_section *ms;
	struct page *page, *memmap;
	struct mem_section_usage *usage;

	section_nr = pfn_to_section_nr(start_pfn);
	ms = __nr_to_section(section_nr);

	 
	memmap = sparse_decode_mem_map(ms->section_mem_map, section_nr);

	 
	page = virt_to_page(memmap);
	mapsize = sizeof(struct page) * PAGES_PER_SECTION;
	mapsize = PAGE_ALIGN(mapsize) >> PAGE_SHIFT;

	 
	for (i = 0; i < mapsize; i++, page++)
		get_page_bootmem(section_nr, page, SECTION_INFO);

	usage = ms->usage;
	page = virt_to_page(usage);

	mapsize = PAGE_ALIGN(mem_section_usage_size()) >> PAGE_SHIFT;

	for (i = 0; i < mapsize; i++, page++)
		get_page_bootmem(section_nr, page, MIX_SECTION_INFO);

}
#else  
static void __init register_page_bootmem_info_section(unsigned long start_pfn)
{
	unsigned long mapsize, section_nr, i;
	struct mem_section *ms;
	struct page *page, *memmap;
	struct mem_section_usage *usage;

	section_nr = pfn_to_section_nr(start_pfn);
	ms = __nr_to_section(section_nr);

	memmap = sparse_decode_mem_map(ms->section_mem_map, section_nr);

	register_page_bootmem_memmap(section_nr, memmap, PAGES_PER_SECTION);

	usage = ms->usage;
	page = virt_to_page(usage);

	mapsize = PAGE_ALIGN(mem_section_usage_size()) >> PAGE_SHIFT;

	for (i = 0; i < mapsize; i++, page++)
		get_page_bootmem(section_nr, page, MIX_SECTION_INFO);
}
#endif  

void __init register_page_bootmem_info_node(struct pglist_data *pgdat)
{
	unsigned long i, pfn, end_pfn, nr_pages;
	int node = pgdat->node_id;
	struct page *page;

	nr_pages = PAGE_ALIGN(sizeof(struct pglist_data)) >> PAGE_SHIFT;
	page = virt_to_page(pgdat);

	for (i = 0; i < nr_pages; i++, page++)
		get_page_bootmem(node, page, NODE_INFO);

	pfn = pgdat->node_start_pfn;
	end_pfn = pgdat_end_pfn(pgdat);

	 
	for (; pfn < end_pfn; pfn += PAGES_PER_SECTION) {
		 
		if (pfn_valid(pfn) && (early_pfn_to_nid(pfn) == node))
			register_page_bootmem_info_section(pfn);
	}
}
