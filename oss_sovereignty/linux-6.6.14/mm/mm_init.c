
 
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/export.h>
#include <linux/memory.h>
#include <linux/notifier.h>
#include <linux/sched.h>
#include <linux/mman.h>
#include <linux/memblock.h>
#include <linux/page-isolation.h>
#include <linux/padata.h>
#include <linux/nmi.h>
#include <linux/buffer_head.h>
#include <linux/kmemleak.h>
#include <linux/kfence.h>
#include <linux/page_ext.h>
#include <linux/pti.h>
#include <linux/pgtable.h>
#include <linux/swap.h>
#include <linux/cma.h>
#include "internal.h"
#include "slab.h"
#include "shuffle.h"

#include <asm/setup.h>

#ifdef CONFIG_DEBUG_MEMORY_INIT
int __meminitdata mminit_loglevel;

 
void __init mminit_verify_zonelist(void)
{
	int nid;

	if (mminit_loglevel < MMINIT_VERIFY)
		return;

	for_each_online_node(nid) {
		pg_data_t *pgdat = NODE_DATA(nid);
		struct zone *zone;
		struct zoneref *z;
		struct zonelist *zonelist;
		int i, listid, zoneid;

		BUILD_BUG_ON(MAX_ZONELISTS > 2);
		for (i = 0; i < MAX_ZONELISTS * MAX_NR_ZONES; i++) {

			 
			zoneid = i % MAX_NR_ZONES;
			listid = i / MAX_NR_ZONES;
			zonelist = &pgdat->node_zonelists[listid];
			zone = &pgdat->node_zones[zoneid];
			if (!populated_zone(zone))
				continue;

			 
			printk(KERN_DEBUG "mminit::zonelist %s %d:%s = ",
				listid > 0 ? "thisnode" : "general", nid,
				zone->name);

			 
			for_each_zone_zonelist(zone, z, zonelist, zoneid)
				pr_cont("%d:%s ", zone_to_nid(zone), zone->name);
			pr_cont("\n");
		}
	}
}

void __init mminit_verify_pageflags_layout(void)
{
	int shift, width;
	unsigned long or_mask, add_mask;

	shift = BITS_PER_LONG;
	width = shift - SECTIONS_WIDTH - NODES_WIDTH - ZONES_WIDTH
		- LAST_CPUPID_SHIFT - KASAN_TAG_WIDTH - LRU_GEN_WIDTH - LRU_REFS_WIDTH;
	mminit_dprintk(MMINIT_TRACE, "pageflags_layout_widths",
		"Section %d Node %d Zone %d Lastcpupid %d Kasantag %d Gen %d Tier %d Flags %d\n",
		SECTIONS_WIDTH,
		NODES_WIDTH,
		ZONES_WIDTH,
		LAST_CPUPID_WIDTH,
		KASAN_TAG_WIDTH,
		LRU_GEN_WIDTH,
		LRU_REFS_WIDTH,
		NR_PAGEFLAGS);
	mminit_dprintk(MMINIT_TRACE, "pageflags_layout_shifts",
		"Section %d Node %d Zone %d Lastcpupid %d Kasantag %d\n",
		SECTIONS_SHIFT,
		NODES_SHIFT,
		ZONES_SHIFT,
		LAST_CPUPID_SHIFT,
		KASAN_TAG_WIDTH);
	mminit_dprintk(MMINIT_TRACE, "pageflags_layout_pgshifts",
		"Section %lu Node %lu Zone %lu Lastcpupid %lu Kasantag %lu\n",
		(unsigned long)SECTIONS_PGSHIFT,
		(unsigned long)NODES_PGSHIFT,
		(unsigned long)ZONES_PGSHIFT,
		(unsigned long)LAST_CPUPID_PGSHIFT,
		(unsigned long)KASAN_TAG_PGSHIFT);
	mminit_dprintk(MMINIT_TRACE, "pageflags_layout_nodezoneid",
		"Node/Zone ID: %lu -> %lu\n",
		(unsigned long)(ZONEID_PGOFF + ZONEID_SHIFT),
		(unsigned long)ZONEID_PGOFF);
	mminit_dprintk(MMINIT_TRACE, "pageflags_layout_usage",
		"location: %d -> %d layout %d -> %d unused %d -> %d page-flags\n",
		shift, width, width, NR_PAGEFLAGS, NR_PAGEFLAGS, 0);
#ifdef NODE_NOT_IN_PAGE_FLAGS
	mminit_dprintk(MMINIT_TRACE, "pageflags_layout_nodeflags",
		"Node not in page flags");
#endif
#ifdef LAST_CPUPID_NOT_IN_PAGE_FLAGS
	mminit_dprintk(MMINIT_TRACE, "pageflags_layout_nodeflags",
		"Last cpupid not in page flags");
#endif

	if (SECTIONS_WIDTH) {
		shift -= SECTIONS_WIDTH;
		BUG_ON(shift != SECTIONS_PGSHIFT);
	}
	if (NODES_WIDTH) {
		shift -= NODES_WIDTH;
		BUG_ON(shift != NODES_PGSHIFT);
	}
	if (ZONES_WIDTH) {
		shift -= ZONES_WIDTH;
		BUG_ON(shift != ZONES_PGSHIFT);
	}

	 
	or_mask = (ZONES_MASK << ZONES_PGSHIFT) |
			(NODES_MASK << NODES_PGSHIFT) |
			(SECTIONS_MASK << SECTIONS_PGSHIFT);
	add_mask = (ZONES_MASK << ZONES_PGSHIFT) +
			(NODES_MASK << NODES_PGSHIFT) +
			(SECTIONS_MASK << SECTIONS_PGSHIFT);
	BUG_ON(or_mask != add_mask);
}

static __init int set_mminit_loglevel(char *str)
{
	get_option(&str, &mminit_loglevel);
	return 0;
}
early_param("mminit_loglevel", set_mminit_loglevel);
#endif  

struct kobject *mm_kobj;

#ifdef CONFIG_SMP
s32 vm_committed_as_batch = 32;

void mm_compute_batch(int overcommit_policy)
{
	u64 memsized_batch;
	s32 nr = num_present_cpus();
	s32 batch = max_t(s32, nr*2, 32);
	unsigned long ram_pages = totalram_pages();

	 
	if (overcommit_policy == OVERCOMMIT_NEVER)
		memsized_batch = min_t(u64, ram_pages/nr/256, INT_MAX);
	else
		memsized_batch = min_t(u64, ram_pages/nr/4, INT_MAX);

	vm_committed_as_batch = max_t(s32, memsized_batch, batch);
}

static int __meminit mm_compute_batch_notifier(struct notifier_block *self,
					unsigned long action, void *arg)
{
	switch (action) {
	case MEM_ONLINE:
	case MEM_OFFLINE:
		mm_compute_batch(sysctl_overcommit_memory);
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

static int __init mm_compute_batch_init(void)
{
	mm_compute_batch(sysctl_overcommit_memory);
	hotplug_memory_notifier(mm_compute_batch_notifier, MM_COMPUTE_BATCH_PRI);
	return 0;
}

__initcall(mm_compute_batch_init);

#endif

static int __init mm_sysfs_init(void)
{
	mm_kobj = kobject_create_and_add("mm", kernel_kobj);
	if (!mm_kobj)
		return -ENOMEM;

	return 0;
}
postcore_initcall(mm_sysfs_init);

static unsigned long arch_zone_lowest_possible_pfn[MAX_NR_ZONES] __initdata;
static unsigned long arch_zone_highest_possible_pfn[MAX_NR_ZONES] __initdata;
static unsigned long zone_movable_pfn[MAX_NUMNODES] __initdata;

static unsigned long required_kernelcore __initdata;
static unsigned long required_kernelcore_percent __initdata;
static unsigned long required_movablecore __initdata;
static unsigned long required_movablecore_percent __initdata;

static unsigned long nr_kernel_pages __initdata;
static unsigned long nr_all_pages __initdata;
static unsigned long dma_reserve __initdata;

static bool deferred_struct_pages __meminitdata;

static DEFINE_PER_CPU(struct per_cpu_nodestat, boot_nodestats);

static int __init cmdline_parse_core(char *p, unsigned long *core,
				     unsigned long *percent)
{
	unsigned long long coremem;
	char *endptr;

	if (!p)
		return -EINVAL;

	 
	coremem = simple_strtoull(p, &endptr, 0);
	if (*endptr == '%') {
		 
		WARN_ON(coremem > 100);

		*percent = coremem;
	} else {
		coremem = memparse(p, &p);
		 
		WARN_ON((coremem >> PAGE_SHIFT) > ULONG_MAX);

		*core = coremem >> PAGE_SHIFT;
		*percent = 0UL;
	}
	return 0;
}

bool mirrored_kernelcore __initdata_memblock;

 
static int __init cmdline_parse_kernelcore(char *p)
{
	 
	if (parse_option_str(p, "mirror")) {
		mirrored_kernelcore = true;
		return 0;
	}

	return cmdline_parse_core(p, &required_kernelcore,
				  &required_kernelcore_percent);
}
early_param("kernelcore", cmdline_parse_kernelcore);

 
static int __init cmdline_parse_movablecore(char *p)
{
	return cmdline_parse_core(p, &required_movablecore,
				  &required_movablecore_percent);
}
early_param("movablecore", cmdline_parse_movablecore);

 
static unsigned long __init early_calculate_totalpages(void)
{
	unsigned long totalpages = 0;
	unsigned long start_pfn, end_pfn;
	int i, nid;

	for_each_mem_pfn_range(i, MAX_NUMNODES, &start_pfn, &end_pfn, &nid) {
		unsigned long pages = end_pfn - start_pfn;

		totalpages += pages;
		if (pages)
			node_set_state(nid, N_MEMORY);
	}
	return totalpages;
}

 
static void __init find_usable_zone_for_movable(void)
{
	int zone_index;
	for (zone_index = MAX_NR_ZONES - 1; zone_index >= 0; zone_index--) {
		if (zone_index == ZONE_MOVABLE)
			continue;

		if (arch_zone_highest_possible_pfn[zone_index] >
				arch_zone_lowest_possible_pfn[zone_index])
			break;
	}

	VM_BUG_ON(zone_index == -1);
	movable_zone = zone_index;
}

 
static void __init find_zone_movable_pfns_for_nodes(void)
{
	int i, nid;
	unsigned long usable_startpfn;
	unsigned long kernelcore_node, kernelcore_remaining;
	 
	nodemask_t saved_node_state = node_states[N_MEMORY];
	unsigned long totalpages = early_calculate_totalpages();
	int usable_nodes = nodes_weight(node_states[N_MEMORY]);
	struct memblock_region *r;

	 
	find_usable_zone_for_movable();

	 
	if (movable_node_is_enabled()) {
		for_each_mem_region(r) {
			if (!memblock_is_hotpluggable(r))
				continue;

			nid = memblock_get_region_node(r);

			usable_startpfn = PFN_DOWN(r->base);
			zone_movable_pfn[nid] = zone_movable_pfn[nid] ?
				min(usable_startpfn, zone_movable_pfn[nid]) :
				usable_startpfn;
		}

		goto out2;
	}

	 
	if (mirrored_kernelcore) {
		bool mem_below_4gb_not_mirrored = false;

		if (!memblock_has_mirror()) {
			pr_warn("The system has no mirror memory, ignore kernelcore=mirror.\n");
			goto out;
		}

		for_each_mem_region(r) {
			if (memblock_is_mirror(r))
				continue;

			nid = memblock_get_region_node(r);

			usable_startpfn = memblock_region_memory_base_pfn(r);

			if (usable_startpfn < PHYS_PFN(SZ_4G)) {
				mem_below_4gb_not_mirrored = true;
				continue;
			}

			zone_movable_pfn[nid] = zone_movable_pfn[nid] ?
				min(usable_startpfn, zone_movable_pfn[nid]) :
				usable_startpfn;
		}

		if (mem_below_4gb_not_mirrored)
			pr_warn("This configuration results in unmirrored kernel memory.\n");

		goto out2;
	}

	 
	if (required_kernelcore_percent)
		required_kernelcore = (totalpages * 100 * required_kernelcore_percent) /
				       10000UL;
	if (required_movablecore_percent)
		required_movablecore = (totalpages * 100 * required_movablecore_percent) /
					10000UL;

	 
	if (required_movablecore) {
		unsigned long corepages;

		 
		required_movablecore =
			roundup(required_movablecore, MAX_ORDER_NR_PAGES);
		required_movablecore = min(totalpages, required_movablecore);
		corepages = totalpages - required_movablecore;

		required_kernelcore = max(required_kernelcore, corepages);
	}

	 
	if (!required_kernelcore || required_kernelcore >= totalpages)
		goto out;

	 
	usable_startpfn = arch_zone_lowest_possible_pfn[movable_zone];

restart:
	 
	kernelcore_node = required_kernelcore / usable_nodes;
	for_each_node_state(nid, N_MEMORY) {
		unsigned long start_pfn, end_pfn;

		 
		if (required_kernelcore < kernelcore_node)
			kernelcore_node = required_kernelcore / usable_nodes;

		 
		kernelcore_remaining = kernelcore_node;

		 
		for_each_mem_pfn_range(i, nid, &start_pfn, &end_pfn, NULL) {
			unsigned long size_pages;

			start_pfn = max(start_pfn, zone_movable_pfn[nid]);
			if (start_pfn >= end_pfn)
				continue;

			 
			if (start_pfn < usable_startpfn) {
				unsigned long kernel_pages;
				kernel_pages = min(end_pfn, usable_startpfn)
								- start_pfn;

				kernelcore_remaining -= min(kernel_pages,
							kernelcore_remaining);
				required_kernelcore -= min(kernel_pages,
							required_kernelcore);

				 
				if (end_pfn <= usable_startpfn) {

					 
					zone_movable_pfn[nid] = end_pfn;
					continue;
				}
				start_pfn = usable_startpfn;
			}

			 
			size_pages = end_pfn - start_pfn;
			if (size_pages > kernelcore_remaining)
				size_pages = kernelcore_remaining;
			zone_movable_pfn[nid] = start_pfn + size_pages;

			 
			required_kernelcore -= min(required_kernelcore,
								size_pages);
			kernelcore_remaining -= size_pages;
			if (!kernelcore_remaining)
				break;
		}
	}

	 
	usable_nodes--;
	if (usable_nodes && required_kernelcore > usable_nodes)
		goto restart;

out2:
	 
	for (nid = 0; nid < MAX_NUMNODES; nid++) {
		unsigned long start_pfn, end_pfn;

		zone_movable_pfn[nid] =
			roundup(zone_movable_pfn[nid], MAX_ORDER_NR_PAGES);

		get_pfn_range_for_nid(nid, &start_pfn, &end_pfn);
		if (zone_movable_pfn[nid] >= end_pfn)
			zone_movable_pfn[nid] = 0;
	}

out:
	 
	node_states[N_MEMORY] = saved_node_state;
}

static void __meminit __init_single_page(struct page *page, unsigned long pfn,
				unsigned long zone, int nid)
{
	mm_zero_struct_page(page);
	set_page_links(page, zone, nid, pfn);
	init_page_count(page);
	page_mapcount_reset(page);
	page_cpupid_reset_last(page);
	page_kasan_tag_reset(page);

	INIT_LIST_HEAD(&page->lru);
#ifdef WANT_PAGE_VIRTUAL
	 
	if (!is_highmem_idx(zone))
		set_page_address(page, __va(pfn << PAGE_SHIFT));
#endif
}

#ifdef CONFIG_NUMA
 
struct mminit_pfnnid_cache {
	unsigned long last_start;
	unsigned long last_end;
	int last_nid;
};

static struct mminit_pfnnid_cache early_pfnnid_cache __meminitdata;

 
static int __meminit __early_pfn_to_nid(unsigned long pfn,
					struct mminit_pfnnid_cache *state)
{
	unsigned long start_pfn, end_pfn;
	int nid;

	if (state->last_start <= pfn && pfn < state->last_end)
		return state->last_nid;

	nid = memblock_search_pfn_nid(pfn, &start_pfn, &end_pfn);
	if (nid != NUMA_NO_NODE) {
		state->last_start = start_pfn;
		state->last_end = end_pfn;
		state->last_nid = nid;
	}

	return nid;
}

int __meminit early_pfn_to_nid(unsigned long pfn)
{
	static DEFINE_SPINLOCK(early_pfn_lock);
	int nid;

	spin_lock(&early_pfn_lock);
	nid = __early_pfn_to_nid(pfn, &early_pfnnid_cache);
	if (nid < 0)
		nid = first_online_node;
	spin_unlock(&early_pfn_lock);

	return nid;
}

int hashdist = HASHDIST_DEFAULT;

static int __init set_hashdist(char *str)
{
	if (!str)
		return 0;
	hashdist = simple_strtoul(str, &str, 0);
	return 1;
}
__setup("hashdist=", set_hashdist);

static inline void fixup_hashdist(void)
{
	if (num_node_state(N_MEMORY) == 1)
		hashdist = 0;
}
#else
static inline void fixup_hashdist(void) {}
#endif  

#ifdef CONFIG_DEFERRED_STRUCT_PAGE_INIT
static inline void pgdat_set_deferred_range(pg_data_t *pgdat)
{
	pgdat->first_deferred_pfn = ULONG_MAX;
}

 
static inline bool __meminit early_page_initialised(unsigned long pfn, int nid)
{
	if (node_online(nid) && pfn >= NODE_DATA(nid)->first_deferred_pfn)
		return false;

	return true;
}

 
static bool __meminit
defer_init(int nid, unsigned long pfn, unsigned long end_pfn)
{
	static unsigned long prev_end_pfn, nr_initialised;

	if (early_page_ext_enabled())
		return false;
	 
	if (prev_end_pfn != end_pfn) {
		prev_end_pfn = end_pfn;
		nr_initialised = 0;
	}

	 
	if (end_pfn < pgdat_end_pfn(NODE_DATA(nid)))
		return false;

	if (NODE_DATA(nid)->first_deferred_pfn != ULONG_MAX)
		return true;
	 
	nr_initialised++;
	if ((nr_initialised > PAGES_PER_SECTION) &&
	    (pfn & (PAGES_PER_SECTION - 1)) == 0) {
		NODE_DATA(nid)->first_deferred_pfn = pfn;
		return true;
	}
	return false;
}

static void __meminit init_reserved_page(unsigned long pfn, int nid)
{
	pg_data_t *pgdat;
	int zid;

	if (early_page_initialised(pfn, nid))
		return;

	pgdat = NODE_DATA(nid);

	for (zid = 0; zid < MAX_NR_ZONES; zid++) {
		struct zone *zone = &pgdat->node_zones[zid];

		if (zone_spans_pfn(zone, pfn))
			break;
	}
	__init_single_page(pfn_to_page(pfn), pfn, zid, nid);
}
#else
static inline void pgdat_set_deferred_range(pg_data_t *pgdat) {}

static inline bool early_page_initialised(unsigned long pfn, int nid)
{
	return true;
}

static inline bool defer_init(int nid, unsigned long pfn, unsigned long end_pfn)
{
	return false;
}

static inline void init_reserved_page(unsigned long pfn, int nid)
{
}
#endif  

 
void __meminit reserve_bootmem_region(phys_addr_t start,
				      phys_addr_t end, int nid)
{
	unsigned long start_pfn = PFN_DOWN(start);
	unsigned long end_pfn = PFN_UP(end);

	for (; start_pfn < end_pfn; start_pfn++) {
		if (pfn_valid(start_pfn)) {
			struct page *page = pfn_to_page(start_pfn);

			init_reserved_page(start_pfn, nid);

			 
			INIT_LIST_HEAD(&page->lru);

			 
			__SetPageReserved(page);
		}
	}
}

 
static bool __meminit
overlap_memmap_init(unsigned long zone, unsigned long *pfn)
{
	static struct memblock_region *r;

	if (mirrored_kernelcore && zone == ZONE_MOVABLE) {
		if (!r || *pfn >= memblock_region_memory_end_pfn(r)) {
			for_each_mem_region(r) {
				if (*pfn < memblock_region_memory_end_pfn(r))
					break;
			}
		}
		if (*pfn >= memblock_region_memory_base_pfn(r) &&
		    memblock_is_mirror(r)) {
			*pfn = memblock_region_memory_end_pfn(r);
			return true;
		}
	}
	return false;
}

 
static void __init init_unavailable_range(unsigned long spfn,
					  unsigned long epfn,
					  int zone, int node)
{
	unsigned long pfn;
	u64 pgcnt = 0;

	for (pfn = spfn; pfn < epfn; pfn++) {
		if (!pfn_valid(pageblock_start_pfn(pfn))) {
			pfn = pageblock_end_pfn(pfn) - 1;
			continue;
		}
		__init_single_page(pfn_to_page(pfn), pfn, zone, node);
		__SetPageReserved(pfn_to_page(pfn));
		pgcnt++;
	}

	if (pgcnt)
		pr_info("On node %d, zone %s: %lld pages in unavailable ranges",
			node, zone_names[zone], pgcnt);
}

 
void __meminit memmap_init_range(unsigned long size, int nid, unsigned long zone,
		unsigned long start_pfn, unsigned long zone_end_pfn,
		enum meminit_context context,
		struct vmem_altmap *altmap, int migratetype)
{
	unsigned long pfn, end_pfn = start_pfn + size;
	struct page *page;

	if (highest_memmap_pfn < end_pfn - 1)
		highest_memmap_pfn = end_pfn - 1;

#ifdef CONFIG_ZONE_DEVICE
	 
	if (zone == ZONE_DEVICE) {
		if (!altmap)
			return;

		if (start_pfn == altmap->base_pfn)
			start_pfn += altmap->reserve;
		end_pfn = altmap->base_pfn + vmem_altmap_offset(altmap);
	}
#endif

	for (pfn = start_pfn; pfn < end_pfn; ) {
		 
		if (context == MEMINIT_EARLY) {
			if (overlap_memmap_init(zone, &pfn))
				continue;
			if (defer_init(nid, pfn, zone_end_pfn)) {
				deferred_struct_pages = true;
				break;
			}
		}

		page = pfn_to_page(pfn);
		__init_single_page(page, pfn, zone, nid);
		if (context == MEMINIT_HOTPLUG)
			__SetPageReserved(page);

		 
		if (pageblock_aligned(pfn)) {
			set_pageblock_migratetype(page, migratetype);
			cond_resched();
		}
		pfn++;
	}
}

static void __init memmap_init_zone_range(struct zone *zone,
					  unsigned long start_pfn,
					  unsigned long end_pfn,
					  unsigned long *hole_pfn)
{
	unsigned long zone_start_pfn = zone->zone_start_pfn;
	unsigned long zone_end_pfn = zone_start_pfn + zone->spanned_pages;
	int nid = zone_to_nid(zone), zone_id = zone_idx(zone);

	start_pfn = clamp(start_pfn, zone_start_pfn, zone_end_pfn);
	end_pfn = clamp(end_pfn, zone_start_pfn, zone_end_pfn);

	if (start_pfn >= end_pfn)
		return;

	memmap_init_range(end_pfn - start_pfn, nid, zone_id, start_pfn,
			  zone_end_pfn, MEMINIT_EARLY, NULL, MIGRATE_MOVABLE);

	if (*hole_pfn < start_pfn)
		init_unavailable_range(*hole_pfn, start_pfn, zone_id, nid);

	*hole_pfn = end_pfn;
}

static void __init memmap_init(void)
{
	unsigned long start_pfn, end_pfn;
	unsigned long hole_pfn = 0;
	int i, j, zone_id = 0, nid;

	for_each_mem_pfn_range(i, MAX_NUMNODES, &start_pfn, &end_pfn, &nid) {
		struct pglist_data *node = NODE_DATA(nid);

		for (j = 0; j < MAX_NR_ZONES; j++) {
			struct zone *zone = node->node_zones + j;

			if (!populated_zone(zone))
				continue;

			memmap_init_zone_range(zone, start_pfn, end_pfn,
					       &hole_pfn);
			zone_id = j;
		}
	}

#ifdef CONFIG_SPARSEMEM
	 
	end_pfn = round_up(end_pfn, PAGES_PER_SECTION);
	if (hole_pfn < end_pfn)
#endif
		init_unavailable_range(hole_pfn, end_pfn, zone_id, nid);
}

#ifdef CONFIG_ZONE_DEVICE
static void __ref __init_zone_device_page(struct page *page, unsigned long pfn,
					  unsigned long zone_idx, int nid,
					  struct dev_pagemap *pgmap)
{

	__init_single_page(page, pfn, zone_idx, nid);

	 
	__SetPageReserved(page);

	 
	page->pgmap = pgmap;
	page->zone_device_data = NULL;

	 
	if (pageblock_aligned(pfn)) {
		set_pageblock_migratetype(page, MIGRATE_MOVABLE);
		cond_resched();
	}

	 
	if (pgmap->type == MEMORY_DEVICE_PRIVATE ||
	    pgmap->type == MEMORY_DEVICE_COHERENT)
		set_page_count(page, 0);
}

 
static inline unsigned long compound_nr_pages(struct vmem_altmap *altmap,
					      struct dev_pagemap *pgmap)
{
	if (!vmemmap_can_optimize(altmap, pgmap))
		return pgmap_vmemmap_nr(pgmap);

	return VMEMMAP_RESERVE_NR * (PAGE_SIZE / sizeof(struct page));
}

static void __ref memmap_init_compound(struct page *head,
				       unsigned long head_pfn,
				       unsigned long zone_idx, int nid,
				       struct dev_pagemap *pgmap,
				       unsigned long nr_pages)
{
	unsigned long pfn, end_pfn = head_pfn + nr_pages;
	unsigned int order = pgmap->vmemmap_shift;

	__SetPageHead(head);
	for (pfn = head_pfn + 1; pfn < end_pfn; pfn++) {
		struct page *page = pfn_to_page(pfn);

		__init_zone_device_page(page, pfn, zone_idx, nid, pgmap);
		prep_compound_tail(head, pfn - head_pfn);
		set_page_count(page, 0);

		 
		if (pfn == head_pfn + 1)
			prep_compound_head(head, order);
	}
}

void __ref memmap_init_zone_device(struct zone *zone,
				   unsigned long start_pfn,
				   unsigned long nr_pages,
				   struct dev_pagemap *pgmap)
{
	unsigned long pfn, end_pfn = start_pfn + nr_pages;
	struct pglist_data *pgdat = zone->zone_pgdat;
	struct vmem_altmap *altmap = pgmap_altmap(pgmap);
	unsigned int pfns_per_compound = pgmap_vmemmap_nr(pgmap);
	unsigned long zone_idx = zone_idx(zone);
	unsigned long start = jiffies;
	int nid = pgdat->node_id;

	if (WARN_ON_ONCE(!pgmap || zone_idx != ZONE_DEVICE))
		return;

	 
	if (altmap) {
		start_pfn = altmap->base_pfn + vmem_altmap_offset(altmap);
		nr_pages = end_pfn - start_pfn;
	}

	for (pfn = start_pfn; pfn < end_pfn; pfn += pfns_per_compound) {
		struct page *page = pfn_to_page(pfn);

		__init_zone_device_page(page, pfn, zone_idx, nid, pgmap);

		if (pfns_per_compound == 1)
			continue;

		memmap_init_compound(page, pfn, zone_idx, nid, pgmap,
				     compound_nr_pages(altmap, pgmap));
	}

	pr_debug("%s initialised %lu pages in %ums\n", __func__,
		nr_pages, jiffies_to_msecs(jiffies - start));
}
#endif

 
static void __init adjust_zone_range_for_zone_movable(int nid,
					unsigned long zone_type,
					unsigned long node_end_pfn,
					unsigned long *zone_start_pfn,
					unsigned long *zone_end_pfn)
{
	 
	if (zone_movable_pfn[nid]) {
		 
		if (zone_type == ZONE_MOVABLE) {
			*zone_start_pfn = zone_movable_pfn[nid];
			*zone_end_pfn = min(node_end_pfn,
				arch_zone_highest_possible_pfn[movable_zone]);

		 
		} else if (!mirrored_kernelcore &&
			*zone_start_pfn < zone_movable_pfn[nid] &&
			*zone_end_pfn > zone_movable_pfn[nid]) {
			*zone_end_pfn = zone_movable_pfn[nid];

		 
		} else if (*zone_start_pfn >= zone_movable_pfn[nid])
			*zone_start_pfn = *zone_end_pfn;
	}
}

 
unsigned long __init __absent_pages_in_range(int nid,
				unsigned long range_start_pfn,
				unsigned long range_end_pfn)
{
	unsigned long nr_absent = range_end_pfn - range_start_pfn;
	unsigned long start_pfn, end_pfn;
	int i;

	for_each_mem_pfn_range(i, nid, &start_pfn, &end_pfn, NULL) {
		start_pfn = clamp(start_pfn, range_start_pfn, range_end_pfn);
		end_pfn = clamp(end_pfn, range_start_pfn, range_end_pfn);
		nr_absent -= end_pfn - start_pfn;
	}
	return nr_absent;
}

 
unsigned long __init absent_pages_in_range(unsigned long start_pfn,
							unsigned long end_pfn)
{
	return __absent_pages_in_range(MAX_NUMNODES, start_pfn, end_pfn);
}

 
static unsigned long __init zone_absent_pages_in_node(int nid,
					unsigned long zone_type,
					unsigned long zone_start_pfn,
					unsigned long zone_end_pfn)
{
	unsigned long nr_absent;

	 
	if (zone_start_pfn == zone_end_pfn)
		return 0;

	nr_absent = __absent_pages_in_range(nid, zone_start_pfn, zone_end_pfn);

	 
	if (mirrored_kernelcore && zone_movable_pfn[nid]) {
		unsigned long start_pfn, end_pfn;
		struct memblock_region *r;

		for_each_mem_region(r) {
			start_pfn = clamp(memblock_region_memory_base_pfn(r),
					  zone_start_pfn, zone_end_pfn);
			end_pfn = clamp(memblock_region_memory_end_pfn(r),
					zone_start_pfn, zone_end_pfn);

			if (zone_type == ZONE_MOVABLE &&
			    memblock_is_mirror(r))
				nr_absent += end_pfn - start_pfn;

			if (zone_type == ZONE_NORMAL &&
			    !memblock_is_mirror(r))
				nr_absent += end_pfn - start_pfn;
		}
	}

	return nr_absent;
}

 
static unsigned long __init zone_spanned_pages_in_node(int nid,
					unsigned long zone_type,
					unsigned long node_start_pfn,
					unsigned long node_end_pfn,
					unsigned long *zone_start_pfn,
					unsigned long *zone_end_pfn)
{
	unsigned long zone_low = arch_zone_lowest_possible_pfn[zone_type];
	unsigned long zone_high = arch_zone_highest_possible_pfn[zone_type];

	 
	*zone_start_pfn = clamp(node_start_pfn, zone_low, zone_high);
	*zone_end_pfn = clamp(node_end_pfn, zone_low, zone_high);
	adjust_zone_range_for_zone_movable(nid, zone_type, node_end_pfn,
					   zone_start_pfn, zone_end_pfn);

	 
	if (*zone_end_pfn < node_start_pfn || *zone_start_pfn > node_end_pfn)
		return 0;

	 
	*zone_end_pfn = min(*zone_end_pfn, node_end_pfn);
	*zone_start_pfn = max(*zone_start_pfn, node_start_pfn);

	 
	return *zone_end_pfn - *zone_start_pfn;
}

static void __init reset_memoryless_node_totalpages(struct pglist_data *pgdat)
{
	struct zone *z;

	for (z = pgdat->node_zones; z < pgdat->node_zones + MAX_NR_ZONES; z++) {
		z->zone_start_pfn = 0;
		z->spanned_pages = 0;
		z->present_pages = 0;
#if defined(CONFIG_MEMORY_HOTPLUG)
		z->present_early_pages = 0;
#endif
	}

	pgdat->node_spanned_pages = 0;
	pgdat->node_present_pages = 0;
	pr_debug("On node %d totalpages: 0\n", pgdat->node_id);
}

static void __init calculate_node_totalpages(struct pglist_data *pgdat,
						unsigned long node_start_pfn,
						unsigned long node_end_pfn)
{
	unsigned long realtotalpages = 0, totalpages = 0;
	enum zone_type i;

	for (i = 0; i < MAX_NR_ZONES; i++) {
		struct zone *zone = pgdat->node_zones + i;
		unsigned long zone_start_pfn, zone_end_pfn;
		unsigned long spanned, absent;
		unsigned long real_size;

		spanned = zone_spanned_pages_in_node(pgdat->node_id, i,
						     node_start_pfn,
						     node_end_pfn,
						     &zone_start_pfn,
						     &zone_end_pfn);
		absent = zone_absent_pages_in_node(pgdat->node_id, i,
						   zone_start_pfn,
						   zone_end_pfn);

		real_size = spanned - absent;

		if (spanned)
			zone->zone_start_pfn = zone_start_pfn;
		else
			zone->zone_start_pfn = 0;
		zone->spanned_pages = spanned;
		zone->present_pages = real_size;
#if defined(CONFIG_MEMORY_HOTPLUG)
		zone->present_early_pages = real_size;
#endif

		totalpages += spanned;
		realtotalpages += real_size;
	}

	pgdat->node_spanned_pages = totalpages;
	pgdat->node_present_pages = realtotalpages;
	pr_debug("On node %d totalpages: %lu\n", pgdat->node_id, realtotalpages);
}

static unsigned long __init calc_memmap_size(unsigned long spanned_pages,
						unsigned long present_pages)
{
	unsigned long pages = spanned_pages;

	 
	if (spanned_pages > present_pages + (present_pages >> 4) &&
	    IS_ENABLED(CONFIG_SPARSEMEM))
		pages = present_pages;

	return PAGE_ALIGN(pages * sizeof(struct page)) >> PAGE_SHIFT;
}

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
static void pgdat_init_split_queue(struct pglist_data *pgdat)
{
	struct deferred_split *ds_queue = &pgdat->deferred_split_queue;

	spin_lock_init(&ds_queue->split_queue_lock);
	INIT_LIST_HEAD(&ds_queue->split_queue);
	ds_queue->split_queue_len = 0;
}
#else
static void pgdat_init_split_queue(struct pglist_data *pgdat) {}
#endif

#ifdef CONFIG_COMPACTION
static void pgdat_init_kcompactd(struct pglist_data *pgdat)
{
	init_waitqueue_head(&pgdat->kcompactd_wait);
}
#else
static void pgdat_init_kcompactd(struct pglist_data *pgdat) {}
#endif

static void __meminit pgdat_init_internals(struct pglist_data *pgdat)
{
	int i;

	pgdat_resize_init(pgdat);
	pgdat_kswapd_lock_init(pgdat);

	pgdat_init_split_queue(pgdat);
	pgdat_init_kcompactd(pgdat);

	init_waitqueue_head(&pgdat->kswapd_wait);
	init_waitqueue_head(&pgdat->pfmemalloc_wait);

	for (i = 0; i < NR_VMSCAN_THROTTLE; i++)
		init_waitqueue_head(&pgdat->reclaim_wait[i]);

	pgdat_page_ext_init(pgdat);
	lruvec_init(&pgdat->__lruvec);
}

static void __meminit zone_init_internals(struct zone *zone, enum zone_type idx, int nid,
							unsigned long remaining_pages)
{
	atomic_long_set(&zone->managed_pages, remaining_pages);
	zone_set_nid(zone, nid);
	zone->name = zone_names[idx];
	zone->zone_pgdat = NODE_DATA(nid);
	spin_lock_init(&zone->lock);
	zone_seqlock_init(zone);
	zone_pcp_init(zone);
}

static void __meminit zone_init_free_lists(struct zone *zone)
{
	unsigned int order, t;
	for_each_migratetype_order(order, t) {
		INIT_LIST_HEAD(&zone->free_area[order].free_list[t]);
		zone->free_area[order].nr_free = 0;
	}

#ifdef CONFIG_UNACCEPTED_MEMORY
	INIT_LIST_HEAD(&zone->unaccepted_pages);
#endif
}

void __meminit init_currently_empty_zone(struct zone *zone,
					unsigned long zone_start_pfn,
					unsigned long size)
{
	struct pglist_data *pgdat = zone->zone_pgdat;
	int zone_idx = zone_idx(zone) + 1;

	if (zone_idx > pgdat->nr_zones)
		pgdat->nr_zones = zone_idx;

	zone->zone_start_pfn = zone_start_pfn;

	mminit_dprintk(MMINIT_TRACE, "memmap_init",
			"Initialising map node %d zone %lu pfns %lu -> %lu\n",
			pgdat->node_id,
			(unsigned long)zone_idx(zone),
			zone_start_pfn, (zone_start_pfn + size));

	zone_init_free_lists(zone);
	zone->initialized = 1;
}

#ifndef CONFIG_SPARSEMEM
 
static unsigned long __init usemap_size(unsigned long zone_start_pfn, unsigned long zonesize)
{
	unsigned long usemapsize;

	zonesize += zone_start_pfn & (pageblock_nr_pages-1);
	usemapsize = roundup(zonesize, pageblock_nr_pages);
	usemapsize = usemapsize >> pageblock_order;
	usemapsize *= NR_PAGEBLOCK_BITS;
	usemapsize = roundup(usemapsize, BITS_PER_LONG);

	return usemapsize / BITS_PER_BYTE;
}

static void __ref setup_usemap(struct zone *zone)
{
	unsigned long usemapsize = usemap_size(zone->zone_start_pfn,
					       zone->spanned_pages);
	zone->pageblock_flags = NULL;
	if (usemapsize) {
		zone->pageblock_flags =
			memblock_alloc_node(usemapsize, SMP_CACHE_BYTES,
					    zone_to_nid(zone));
		if (!zone->pageblock_flags)
			panic("Failed to allocate %ld bytes for zone %s pageblock flags on node %d\n",
			      usemapsize, zone->name, zone_to_nid(zone));
	}
}
#else
static inline void setup_usemap(struct zone *zone) {}
#endif  

#ifdef CONFIG_HUGETLB_PAGE_SIZE_VARIABLE

 
void __init set_pageblock_order(void)
{
	unsigned int order = MAX_ORDER;

	 
	if (pageblock_order)
		return;

	 
	if (HPAGE_SHIFT > PAGE_SHIFT && HUGETLB_PAGE_ORDER < order)
		order = HUGETLB_PAGE_ORDER;

	 
	pageblock_order = order;
}
#else  

 
void __init set_pageblock_order(void)
{
}

#endif  

 
#ifdef CONFIG_MEMORY_HOTPLUG
void __ref free_area_init_core_hotplug(struct pglist_data *pgdat)
{
	int nid = pgdat->node_id;
	enum zone_type z;
	int cpu;

	pgdat_init_internals(pgdat);

	if (pgdat->per_cpu_nodestats == &boot_nodestats)
		pgdat->per_cpu_nodestats = alloc_percpu(struct per_cpu_nodestat);

	 
	pgdat->nr_zones = 0;
	pgdat->kswapd_order = 0;
	pgdat->kswapd_highest_zoneidx = 0;
	pgdat->node_start_pfn = 0;
	pgdat->node_present_pages = 0;

	for_each_online_cpu(cpu) {
		struct per_cpu_nodestat *p;

		p = per_cpu_ptr(pgdat->per_cpu_nodestats, cpu);
		memset(p, 0, sizeof(*p));
	}

	 
	for (z = 0; z < MAX_NR_ZONES; z++) {
		struct zone *zone = pgdat->node_zones + z;

		zone->present_pages = 0;
		zone_init_internals(zone, z, nid, 0);
	}
}
#endif

 
static void __init free_area_init_core(struct pglist_data *pgdat)
{
	enum zone_type j;
	int nid = pgdat->node_id;

	pgdat_init_internals(pgdat);
	pgdat->per_cpu_nodestats = &boot_nodestats;

	for (j = 0; j < MAX_NR_ZONES; j++) {
		struct zone *zone = pgdat->node_zones + j;
		unsigned long size, freesize, memmap_pages;

		size = zone->spanned_pages;
		freesize = zone->present_pages;

		 
		memmap_pages = calc_memmap_size(size, freesize);
		if (!is_highmem_idx(j)) {
			if (freesize >= memmap_pages) {
				freesize -= memmap_pages;
				if (memmap_pages)
					pr_debug("  %s zone: %lu pages used for memmap\n",
						 zone_names[j], memmap_pages);
			} else
				pr_warn("  %s zone: %lu memmap pages exceeds freesize %lu\n",
					zone_names[j], memmap_pages, freesize);
		}

		 
		if (j == 0 && freesize > dma_reserve) {
			freesize -= dma_reserve;
			pr_debug("  %s zone: %lu pages reserved\n", zone_names[0], dma_reserve);
		}

		if (!is_highmem_idx(j))
			nr_kernel_pages += freesize;
		 
		else if (nr_kernel_pages > memmap_pages * 2)
			nr_kernel_pages -= memmap_pages;
		nr_all_pages += freesize;

		 
		zone_init_internals(zone, j, nid, freesize);

		if (!size)
			continue;

		setup_usemap(zone);
		init_currently_empty_zone(zone, zone->zone_start_pfn, size);
	}
}

void __init *memmap_alloc(phys_addr_t size, phys_addr_t align,
			  phys_addr_t min_addr, int nid, bool exact_nid)
{
	void *ptr;

	if (exact_nid)
		ptr = memblock_alloc_exact_nid_raw(size, align, min_addr,
						   MEMBLOCK_ALLOC_ACCESSIBLE,
						   nid);
	else
		ptr = memblock_alloc_try_nid_raw(size, align, min_addr,
						 MEMBLOCK_ALLOC_ACCESSIBLE,
						 nid);

	if (ptr && size > 0)
		page_init_poison(ptr, size);

	return ptr;
}

#ifdef CONFIG_FLATMEM
static void __init alloc_node_mem_map(struct pglist_data *pgdat)
{
	unsigned long __maybe_unused start = 0;
	unsigned long __maybe_unused offset = 0;

	 
	if (!pgdat->node_spanned_pages)
		return;

	start = pgdat->node_start_pfn & ~(MAX_ORDER_NR_PAGES - 1);
	offset = pgdat->node_start_pfn - start;
	 
	if (!pgdat->node_mem_map) {
		unsigned long size, end;
		struct page *map;

		 
		end = pgdat_end_pfn(pgdat);
		end = ALIGN(end, MAX_ORDER_NR_PAGES);
		size =  (end - start) * sizeof(struct page);
		map = memmap_alloc(size, SMP_CACHE_BYTES, MEMBLOCK_LOW_LIMIT,
				   pgdat->node_id, false);
		if (!map)
			panic("Failed to allocate %ld bytes for node %d memory map\n",
			      size, pgdat->node_id);
		pgdat->node_mem_map = map + offset;
	}
	pr_debug("%s: node %d, pgdat %08lx, node_mem_map %08lx\n",
				__func__, pgdat->node_id, (unsigned long)pgdat,
				(unsigned long)pgdat->node_mem_map);
#ifndef CONFIG_NUMA
	 
	if (pgdat == NODE_DATA(0)) {
		mem_map = NODE_DATA(0)->node_mem_map;
		if (page_to_pfn(mem_map) != pgdat->node_start_pfn)
			mem_map -= offset;
	}
#endif
}
#else
static inline void alloc_node_mem_map(struct pglist_data *pgdat) { }
#endif  

 
void __init get_pfn_range_for_nid(unsigned int nid,
			unsigned long *start_pfn, unsigned long *end_pfn)
{
	unsigned long this_start_pfn, this_end_pfn;
	int i;

	*start_pfn = -1UL;
	*end_pfn = 0;

	for_each_mem_pfn_range(i, nid, &this_start_pfn, &this_end_pfn, NULL) {
		*start_pfn = min(*start_pfn, this_start_pfn);
		*end_pfn = max(*end_pfn, this_end_pfn);
	}

	if (*start_pfn == -1UL)
		*start_pfn = 0;
}

static void __init free_area_init_node(int nid)
{
	pg_data_t *pgdat = NODE_DATA(nid);
	unsigned long start_pfn = 0;
	unsigned long end_pfn = 0;

	 
	WARN_ON(pgdat->nr_zones || pgdat->kswapd_highest_zoneidx);

	get_pfn_range_for_nid(nid, &start_pfn, &end_pfn);

	pgdat->node_id = nid;
	pgdat->node_start_pfn = start_pfn;
	pgdat->per_cpu_nodestats = NULL;

	if (start_pfn != end_pfn) {
		pr_info("Initmem setup node %d [mem %#018Lx-%#018Lx]\n", nid,
			(u64)start_pfn << PAGE_SHIFT,
			end_pfn ? ((u64)end_pfn << PAGE_SHIFT) - 1 : 0);

		calculate_node_totalpages(pgdat, start_pfn, end_pfn);
	} else {
		pr_info("Initmem setup node %d as memoryless\n", nid);

		reset_memoryless_node_totalpages(pgdat);
	}

	alloc_node_mem_map(pgdat);
	pgdat_set_deferred_range(pgdat);

	free_area_init_core(pgdat);
	lru_gen_init_pgdat(pgdat);
}

 
static void __init check_for_memory(pg_data_t *pgdat)
{
	enum zone_type zone_type;

	for (zone_type = 0; zone_type <= ZONE_MOVABLE - 1; zone_type++) {
		struct zone *zone = &pgdat->node_zones[zone_type];
		if (populated_zone(zone)) {
			if (IS_ENABLED(CONFIG_HIGHMEM))
				node_set_state(pgdat->node_id, N_HIGH_MEMORY);
			if (zone_type <= ZONE_NORMAL)
				node_set_state(pgdat->node_id, N_NORMAL_MEMORY);
			break;
		}
	}
}

#if MAX_NUMNODES > 1
 
void __init setup_nr_node_ids(void)
{
	unsigned int highest;

	highest = find_last_bit(node_possible_map.bits, MAX_NUMNODES);
	nr_node_ids = highest + 1;
}
#endif

 
static bool arch_has_descending_max_zone_pfns(void)
{
	return IS_ENABLED(CONFIG_ARC) && !IS_ENABLED(CONFIG_ARC_HAS_PAE40);
}

 
void __init free_area_init(unsigned long *max_zone_pfn)
{
	unsigned long start_pfn, end_pfn;
	int i, nid, zone;
	bool descending;

	 
	memset(arch_zone_lowest_possible_pfn, 0,
				sizeof(arch_zone_lowest_possible_pfn));
	memset(arch_zone_highest_possible_pfn, 0,
				sizeof(arch_zone_highest_possible_pfn));

	start_pfn = PHYS_PFN(memblock_start_of_DRAM());
	descending = arch_has_descending_max_zone_pfns();

	for (i = 0; i < MAX_NR_ZONES; i++) {
		if (descending)
			zone = MAX_NR_ZONES - i - 1;
		else
			zone = i;

		if (zone == ZONE_MOVABLE)
			continue;

		end_pfn = max(max_zone_pfn[zone], start_pfn);
		arch_zone_lowest_possible_pfn[zone] = start_pfn;
		arch_zone_highest_possible_pfn[zone] = end_pfn;

		start_pfn = end_pfn;
	}

	 
	memset(zone_movable_pfn, 0, sizeof(zone_movable_pfn));
	find_zone_movable_pfns_for_nodes();

	 
	pr_info("Zone ranges:\n");
	for (i = 0; i < MAX_NR_ZONES; i++) {
		if (i == ZONE_MOVABLE)
			continue;
		pr_info("  %-8s ", zone_names[i]);
		if (arch_zone_lowest_possible_pfn[i] ==
				arch_zone_highest_possible_pfn[i])
			pr_cont("empty\n");
		else
			pr_cont("[mem %#018Lx-%#018Lx]\n",
				(u64)arch_zone_lowest_possible_pfn[i]
					<< PAGE_SHIFT,
				((u64)arch_zone_highest_possible_pfn[i]
					<< PAGE_SHIFT) - 1);
	}

	 
	pr_info("Movable zone start for each node\n");
	for (i = 0; i < MAX_NUMNODES; i++) {
		if (zone_movable_pfn[i])
			pr_info("  Node %d: %#018Lx\n", i,
			       (u64)zone_movable_pfn[i] << PAGE_SHIFT);
	}

	 
	pr_info("Early memory node ranges\n");
	for_each_mem_pfn_range(i, MAX_NUMNODES, &start_pfn, &end_pfn, &nid) {
		pr_info("  node %3d: [mem %#018Lx-%#018Lx]\n", nid,
			(u64)start_pfn << PAGE_SHIFT,
			((u64)end_pfn << PAGE_SHIFT) - 1);
		subsection_map_init(start_pfn, end_pfn - start_pfn);
	}

	 
	mminit_verify_pageflags_layout();
	setup_nr_node_ids();
	set_pageblock_order();

	for_each_node(nid) {
		pg_data_t *pgdat;

		if (!node_online(nid)) {
			pr_info("Initializing node %d as memoryless\n", nid);

			 
			pgdat = arch_alloc_nodedata(nid);
			if (!pgdat)
				panic("Cannot allocate %zuB for node %d.\n",
				       sizeof(*pgdat), nid);
			arch_refresh_nodedata(nid, pgdat);
			free_area_init_node(nid);

			 
			continue;
		}

		pgdat = NODE_DATA(nid);
		free_area_init_node(nid);

		 
		if (pgdat->node_present_pages)
			node_set_state(nid, N_MEMORY);
		check_for_memory(pgdat);
	}

	memmap_init();

	 
	fixup_hashdist();
}

 
unsigned long __init node_map_pfn_alignment(void)
{
	unsigned long accl_mask = 0, last_end = 0;
	unsigned long start, end, mask;
	int last_nid = NUMA_NO_NODE;
	int i, nid;

	for_each_mem_pfn_range(i, MAX_NUMNODES, &start, &end, &nid) {
		if (!start || last_nid < 0 || last_nid == nid) {
			last_nid = nid;
			last_end = end;
			continue;
		}

		 
		mask = ~((1 << __ffs(start)) - 1);
		while (mask && last_end <= (start & (mask << 1)))
			mask <<= 1;

		 
		accl_mask |= mask;
	}

	 
	return ~accl_mask + 1;
}

#ifdef CONFIG_DEFERRED_STRUCT_PAGE_INIT
static void __init deferred_free_range(unsigned long pfn,
				       unsigned long nr_pages)
{
	struct page *page;
	unsigned long i;

	if (!nr_pages)
		return;

	page = pfn_to_page(pfn);

	 
	if (nr_pages == MAX_ORDER_NR_PAGES && IS_MAX_ORDER_ALIGNED(pfn)) {
		for (i = 0; i < nr_pages; i += pageblock_nr_pages)
			set_pageblock_migratetype(page + i, MIGRATE_MOVABLE);
		__free_pages_core(page, MAX_ORDER);
		return;
	}

	 
	accept_memory(PFN_PHYS(pfn), PFN_PHYS(pfn + nr_pages));

	for (i = 0; i < nr_pages; i++, page++, pfn++) {
		if (pageblock_aligned(pfn))
			set_pageblock_migratetype(page, MIGRATE_MOVABLE);
		__free_pages_core(page, 0);
	}
}

 
static atomic_t pgdat_init_n_undone __initdata;
static __initdata DECLARE_COMPLETION(pgdat_init_all_done_comp);

static inline void __init pgdat_init_report_one_done(void)
{
	if (atomic_dec_and_test(&pgdat_init_n_undone))
		complete(&pgdat_init_all_done_comp);
}

 
static inline bool __init deferred_pfn_valid(unsigned long pfn)
{
	if (IS_MAX_ORDER_ALIGNED(pfn) && !pfn_valid(pfn))
		return false;
	return true;
}

 
static void __init deferred_free_pages(unsigned long pfn,
				       unsigned long end_pfn)
{
	unsigned long nr_free = 0;

	for (; pfn < end_pfn; pfn++) {
		if (!deferred_pfn_valid(pfn)) {
			deferred_free_range(pfn - nr_free, nr_free);
			nr_free = 0;
		} else if (IS_MAX_ORDER_ALIGNED(pfn)) {
			deferred_free_range(pfn - nr_free, nr_free);
			nr_free = 1;
		} else {
			nr_free++;
		}
	}
	 
	deferred_free_range(pfn - nr_free, nr_free);
}

 
static unsigned long  __init deferred_init_pages(struct zone *zone,
						 unsigned long pfn,
						 unsigned long end_pfn)
{
	int nid = zone_to_nid(zone);
	unsigned long nr_pages = 0;
	int zid = zone_idx(zone);
	struct page *page = NULL;

	for (; pfn < end_pfn; pfn++) {
		if (!deferred_pfn_valid(pfn)) {
			page = NULL;
			continue;
		} else if (!page || IS_MAX_ORDER_ALIGNED(pfn)) {
			page = pfn_to_page(pfn);
		} else {
			page++;
		}
		__init_single_page(page, pfn, zid, nid);
		nr_pages++;
	}
	return (nr_pages);
}

 
static bool __init
deferred_init_mem_pfn_range_in_zone(u64 *i, struct zone *zone,
				    unsigned long *spfn, unsigned long *epfn,
				    unsigned long first_init_pfn)
{
	u64 j;

	 
	for_each_free_mem_pfn_range_in_zone(j, zone, spfn, epfn) {
		if (*epfn <= first_init_pfn)
			continue;
		if (*spfn < first_init_pfn)
			*spfn = first_init_pfn;
		*i = j;
		return true;
	}

	return false;
}

 
static unsigned long __init
deferred_init_maxorder(u64 *i, struct zone *zone, unsigned long *start_pfn,
		       unsigned long *end_pfn)
{
	unsigned long mo_pfn = ALIGN(*start_pfn + 1, MAX_ORDER_NR_PAGES);
	unsigned long spfn = *start_pfn, epfn = *end_pfn;
	unsigned long nr_pages = 0;
	u64 j = *i;

	 
	for_each_free_mem_pfn_range_in_zone_from(j, zone, start_pfn, end_pfn) {
		unsigned long t;

		if (mo_pfn <= *start_pfn)
			break;

		t = min(mo_pfn, *end_pfn);
		nr_pages += deferred_init_pages(zone, *start_pfn, t);

		if (mo_pfn < *end_pfn) {
			*start_pfn = mo_pfn;
			break;
		}
	}

	 
	swap(j, *i);

	for_each_free_mem_pfn_range_in_zone_from(j, zone, &spfn, &epfn) {
		unsigned long t;

		if (mo_pfn <= spfn)
			break;

		t = min(mo_pfn, epfn);
		deferred_free_pages(spfn, t);

		if (mo_pfn <= epfn)
			break;
	}

	return nr_pages;
}

static void __init
deferred_init_memmap_chunk(unsigned long start_pfn, unsigned long end_pfn,
			   void *arg)
{
	unsigned long spfn, epfn;
	struct zone *zone = arg;
	u64 i;

	deferred_init_mem_pfn_range_in_zone(&i, zone, &spfn, &epfn, start_pfn);

	 
	while (spfn < end_pfn) {
		deferred_init_maxorder(&i, zone, &spfn, &epfn);
		cond_resched();
	}
}

 
__weak int __init
deferred_page_init_max_threads(const struct cpumask *node_cpumask)
{
	return 1;
}

 
static int __init deferred_init_memmap(void *data)
{
	pg_data_t *pgdat = data;
	const struct cpumask *cpumask = cpumask_of_node(pgdat->node_id);
	unsigned long spfn = 0, epfn = 0;
	unsigned long first_init_pfn, flags;
	unsigned long start = jiffies;
	struct zone *zone;
	int zid, max_threads;
	u64 i;

	 
	if (!cpumask_empty(cpumask))
		set_cpus_allowed_ptr(current, cpumask);

	pgdat_resize_lock(pgdat, &flags);
	first_init_pfn = pgdat->first_deferred_pfn;
	if (first_init_pfn == ULONG_MAX) {
		pgdat_resize_unlock(pgdat, &flags);
		pgdat_init_report_one_done();
		return 0;
	}

	 
	BUG_ON(pgdat->first_deferred_pfn < pgdat->node_start_pfn);
	BUG_ON(pgdat->first_deferred_pfn > pgdat_end_pfn(pgdat));
	pgdat->first_deferred_pfn = ULONG_MAX;

	 
	pgdat_resize_unlock(pgdat, &flags);

	 
	for (zid = 0; zid < MAX_NR_ZONES; zid++) {
		zone = pgdat->node_zones + zid;
		if (first_init_pfn < zone_end_pfn(zone))
			break;
	}

	 
	if (!deferred_init_mem_pfn_range_in_zone(&i, zone, &spfn, &epfn,
						 first_init_pfn))
		goto zone_empty;

	max_threads = deferred_page_init_max_threads(cpumask);

	while (spfn < epfn) {
		unsigned long epfn_align = ALIGN(epfn, PAGES_PER_SECTION);
		struct padata_mt_job job = {
			.thread_fn   = deferred_init_memmap_chunk,
			.fn_arg      = zone,
			.start       = spfn,
			.size        = epfn_align - spfn,
			.align       = PAGES_PER_SECTION,
			.min_chunk   = PAGES_PER_SECTION,
			.max_threads = max_threads,
		};

		padata_do_multithreaded(&job);
		deferred_init_mem_pfn_range_in_zone(&i, zone, &spfn, &epfn,
						    epfn_align);
	}
zone_empty:
	 
	WARN_ON(++zid < MAX_NR_ZONES && populated_zone(++zone));

	pr_info("node %d deferred pages initialised in %ums\n",
		pgdat->node_id, jiffies_to_msecs(jiffies - start));

	pgdat_init_report_one_done();
	return 0;
}

 
bool __init deferred_grow_zone(struct zone *zone, unsigned int order)
{
	unsigned long nr_pages_needed = ALIGN(1 << order, PAGES_PER_SECTION);
	pg_data_t *pgdat = zone->zone_pgdat;
	unsigned long first_deferred_pfn = pgdat->first_deferred_pfn;
	unsigned long spfn, epfn, flags;
	unsigned long nr_pages = 0;
	u64 i;

	 
	if (zone_end_pfn(zone) != pgdat_end_pfn(pgdat))
		return false;

	pgdat_resize_lock(pgdat, &flags);

	 
	if (first_deferred_pfn != pgdat->first_deferred_pfn) {
		pgdat_resize_unlock(pgdat, &flags);
		return true;
	}

	 
	if (!deferred_init_mem_pfn_range_in_zone(&i, zone, &spfn, &epfn,
						 first_deferred_pfn)) {
		pgdat->first_deferred_pfn = ULONG_MAX;
		pgdat_resize_unlock(pgdat, &flags);
		 
		return first_deferred_pfn != ULONG_MAX;
	}

	 
	while (spfn < epfn) {
		 
		first_deferred_pfn = spfn;

		nr_pages += deferred_init_maxorder(&i, zone, &spfn, &epfn);
		touch_nmi_watchdog();

		 
		if ((first_deferred_pfn ^ spfn) < PAGES_PER_SECTION)
			continue;

		 
		if (nr_pages >= nr_pages_needed)
			break;
	}

	pgdat->first_deferred_pfn = spfn;
	pgdat_resize_unlock(pgdat, &flags);

	return nr_pages > 0;
}

#endif  

#ifdef CONFIG_CMA
void __init init_cma_reserved_pageblock(struct page *page)
{
	unsigned i = pageblock_nr_pages;
	struct page *p = page;

	do {
		__ClearPageReserved(p);
		set_page_count(p, 0);
	} while (++p, --i);

	set_pageblock_migratetype(page, MIGRATE_CMA);
	set_page_refcounted(page);
	__free_pages(page, pageblock_order);

	adjust_managed_page_count(page, pageblock_nr_pages);
	page_zone(page)->cma_pages += pageblock_nr_pages;
}
#endif

void set_zone_contiguous(struct zone *zone)
{
	unsigned long block_start_pfn = zone->zone_start_pfn;
	unsigned long block_end_pfn;

	block_end_pfn = pageblock_end_pfn(block_start_pfn);
	for (; block_start_pfn < zone_end_pfn(zone);
			block_start_pfn = block_end_pfn,
			 block_end_pfn += pageblock_nr_pages) {

		block_end_pfn = min(block_end_pfn, zone_end_pfn(zone));

		if (!__pageblock_pfn_to_page(block_start_pfn,
					     block_end_pfn, zone))
			return;
		cond_resched();
	}

	 
	zone->contiguous = true;
}

void __init page_alloc_init_late(void)
{
	struct zone *zone;
	int nid;

#ifdef CONFIG_DEFERRED_STRUCT_PAGE_INIT

	 
	atomic_set(&pgdat_init_n_undone, num_node_state(N_MEMORY));
	for_each_node_state(nid, N_MEMORY) {
		kthread_run(deferred_init_memmap, NODE_DATA(nid), "pgdatinit%d", nid);
	}

	 
	wait_for_completion(&pgdat_init_all_done_comp);

	 
	static_branch_disable(&deferred_pages);

	 
	files_maxfiles_init();
#endif

	buffer_init();

	 
	memblock_discard();

	for_each_node_state(nid, N_MEMORY)
		shuffle_free_memory(NODE_DATA(nid));

	for_each_populated_zone(zone)
		set_zone_contiguous(zone);

	 
	if (deferred_struct_pages)
		page_ext_init();

	page_alloc_sysctl_init();
}

#ifndef __HAVE_ARCH_RESERVED_KERNEL_PAGES
 
static unsigned long __init arch_reserved_kernel_pages(void)
{
	return 0;
}
#endif

 
#if __BITS_PER_LONG > 32
#define ADAPT_SCALE_BASE	(64ul << 30)
#define ADAPT_SCALE_SHIFT	2
#define ADAPT_SCALE_NPAGES	(ADAPT_SCALE_BASE >> PAGE_SHIFT)
#endif

 
void *__init alloc_large_system_hash(const char *tablename,
				     unsigned long bucketsize,
				     unsigned long numentries,
				     int scale,
				     int flags,
				     unsigned int *_hash_shift,
				     unsigned int *_hash_mask,
				     unsigned long low_limit,
				     unsigned long high_limit)
{
	unsigned long long max = high_limit;
	unsigned long log2qty, size;
	void *table;
	gfp_t gfp_flags;
	bool virt;
	bool huge;

	 
	if (!numentries) {
		 
		numentries = nr_kernel_pages;
		numentries -= arch_reserved_kernel_pages();

		 
		if (PAGE_SIZE < SZ_1M)
			numentries = round_up(numentries, SZ_1M / PAGE_SIZE);

#if __BITS_PER_LONG > 32
		if (!high_limit) {
			unsigned long adapt;

			for (adapt = ADAPT_SCALE_NPAGES; adapt < numentries;
			     adapt <<= ADAPT_SCALE_SHIFT)
				scale++;
		}
#endif

		 
		if (scale > PAGE_SHIFT)
			numentries >>= (scale - PAGE_SHIFT);
		else
			numentries <<= (PAGE_SHIFT - scale);

		if (unlikely((numentries * bucketsize) < PAGE_SIZE))
			numentries = PAGE_SIZE / bucketsize;
	}
	numentries = roundup_pow_of_two(numentries);

	 
	if (max == 0) {
		max = ((unsigned long long)nr_all_pages << PAGE_SHIFT) >> 4;
		do_div(max, bucketsize);
	}
	max = min(max, 0x80000000ULL);

	if (numentries < low_limit)
		numentries = low_limit;
	if (numentries > max)
		numentries = max;

	log2qty = ilog2(numentries);

	gfp_flags = (flags & HASH_ZERO) ? GFP_ATOMIC | __GFP_ZERO : GFP_ATOMIC;
	do {
		virt = false;
		size = bucketsize << log2qty;
		if (flags & HASH_EARLY) {
			if (flags & HASH_ZERO)
				table = memblock_alloc(size, SMP_CACHE_BYTES);
			else
				table = memblock_alloc_raw(size,
							   SMP_CACHE_BYTES);
		} else if (get_order(size) > MAX_ORDER || hashdist) {
			table = vmalloc_huge(size, gfp_flags);
			virt = true;
			if (table)
				huge = is_vm_area_hugepages(table);
		} else {
			 
			table = alloc_pages_exact(size, gfp_flags);
			kmemleak_alloc(table, size, 1, gfp_flags);
		}
	} while (!table && size > PAGE_SIZE && --log2qty);

	if (!table)
		panic("Failed to allocate %s hash table\n", tablename);

	pr_info("%s hash table entries: %ld (order: %d, %lu bytes, %s)\n",
		tablename, 1UL << log2qty, ilog2(size) - PAGE_SHIFT, size,
		virt ? (huge ? "vmalloc hugepage" : "vmalloc") : "linear");

	if (_hash_shift)
		*_hash_shift = log2qty;
	if (_hash_mask)
		*_hash_mask = (1 << log2qty) - 1;

	return table;
}

 
void __init set_dma_reserve(unsigned long new_dma_reserve)
{
	dma_reserve = new_dma_reserve;
}

void __init memblock_free_pages(struct page *page, unsigned long pfn,
							unsigned int order)
{

	if (IS_ENABLED(CONFIG_DEFERRED_STRUCT_PAGE_INIT)) {
		int nid = early_pfn_to_nid(pfn);

		if (!early_page_initialised(pfn, nid))
			return;
	}

	if (!kmsan_memblock_free_pages(page, order)) {
		 
		return;
	}
	__free_pages_core(page, order);
}

DEFINE_STATIC_KEY_MAYBE(CONFIG_INIT_ON_ALLOC_DEFAULT_ON, init_on_alloc);
EXPORT_SYMBOL(init_on_alloc);

DEFINE_STATIC_KEY_MAYBE(CONFIG_INIT_ON_FREE_DEFAULT_ON, init_on_free);
EXPORT_SYMBOL(init_on_free);

static bool _init_on_alloc_enabled_early __read_mostly
				= IS_ENABLED(CONFIG_INIT_ON_ALLOC_DEFAULT_ON);
static int __init early_init_on_alloc(char *buf)
{

	return kstrtobool(buf, &_init_on_alloc_enabled_early);
}
early_param("init_on_alloc", early_init_on_alloc);

static bool _init_on_free_enabled_early __read_mostly
				= IS_ENABLED(CONFIG_INIT_ON_FREE_DEFAULT_ON);
static int __init early_init_on_free(char *buf)
{
	return kstrtobool(buf, &_init_on_free_enabled_early);
}
early_param("init_on_free", early_init_on_free);

DEFINE_STATIC_KEY_MAYBE(CONFIG_DEBUG_VM, check_pages_enabled);

 
static void __init mem_debugging_and_hardening_init(void)
{
	bool page_poisoning_requested = false;
	bool want_check_pages = false;

#ifdef CONFIG_PAGE_POISONING
	 
	if (page_poisoning_enabled() ||
	     (!IS_ENABLED(CONFIG_ARCH_SUPPORTS_DEBUG_PAGEALLOC) &&
	      debug_pagealloc_enabled())) {
		static_branch_enable(&_page_poisoning_enabled);
		page_poisoning_requested = true;
		want_check_pages = true;
	}
#endif

	if ((_init_on_alloc_enabled_early || _init_on_free_enabled_early) &&
	    page_poisoning_requested) {
		pr_info("mem auto-init: CONFIG_PAGE_POISONING is on, "
			"will take precedence over init_on_alloc and init_on_free\n");
		_init_on_alloc_enabled_early = false;
		_init_on_free_enabled_early = false;
	}

	if (_init_on_alloc_enabled_early) {
		want_check_pages = true;
		static_branch_enable(&init_on_alloc);
	} else {
		static_branch_disable(&init_on_alloc);
	}

	if (_init_on_free_enabled_early) {
		want_check_pages = true;
		static_branch_enable(&init_on_free);
	} else {
		static_branch_disable(&init_on_free);
	}

	if (IS_ENABLED(CONFIG_KMSAN) &&
	    (_init_on_alloc_enabled_early || _init_on_free_enabled_early))
		pr_info("mem auto-init: please make sure init_on_alloc and init_on_free are disabled when running KMSAN\n");

#ifdef CONFIG_DEBUG_PAGEALLOC
	if (debug_pagealloc_enabled()) {
		want_check_pages = true;
		static_branch_enable(&_debug_pagealloc_enabled);

		if (debug_guardpage_minorder())
			static_branch_enable(&_debug_guardpage_enabled);
	}
#endif

	 
	if (!IS_ENABLED(CONFIG_DEBUG_VM) && want_check_pages)
		static_branch_enable(&check_pages_enabled);
}

 
static void __init report_meminit(void)
{
	const char *stack;

	if (IS_ENABLED(CONFIG_INIT_STACK_ALL_PATTERN))
		stack = "all(pattern)";
	else if (IS_ENABLED(CONFIG_INIT_STACK_ALL_ZERO))
		stack = "all(zero)";
	else if (IS_ENABLED(CONFIG_GCC_PLUGIN_STRUCTLEAK_BYREF_ALL))
		stack = "byref_all(zero)";
	else if (IS_ENABLED(CONFIG_GCC_PLUGIN_STRUCTLEAK_BYREF))
		stack = "byref(zero)";
	else if (IS_ENABLED(CONFIG_GCC_PLUGIN_STRUCTLEAK_USER))
		stack = "__user(zero)";
	else
		stack = "off";

	pr_info("mem auto-init: stack:%s, heap alloc:%s, heap free:%s\n",
		stack, want_init_on_alloc(GFP_KERNEL) ? "on" : "off",
		want_init_on_free() ? "on" : "off");
	if (want_init_on_free())
		pr_info("mem auto-init: clearing system memory may take some time...\n");
}

static void __init mem_init_print_info(void)
{
	unsigned long physpages, codesize, datasize, rosize, bss_size;
	unsigned long init_code_size, init_data_size;

	physpages = get_num_physpages();
	codesize = _etext - _stext;
	datasize = _edata - _sdata;
	rosize = __end_rodata - __start_rodata;
	bss_size = __bss_stop - __bss_start;
	init_data_size = __init_end - __init_begin;
	init_code_size = _einittext - _sinittext;

	 
#define adj_init_size(start, end, size, pos, adj) \
	do { \
		if (&start[0] <= &pos[0] && &pos[0] < &end[0] && size > adj) \
			size -= adj; \
	} while (0)

	adj_init_size(__init_begin, __init_end, init_data_size,
		     _sinittext, init_code_size);
	adj_init_size(_stext, _etext, codesize, _sinittext, init_code_size);
	adj_init_size(_sdata, _edata, datasize, __init_begin, init_data_size);
	adj_init_size(_stext, _etext, codesize, __start_rodata, rosize);
	adj_init_size(_sdata, _edata, datasize, __start_rodata, rosize);

#undef	adj_init_size

	pr_info("Memory: %luK/%luK available (%luK kernel code, %luK rwdata, %luK rodata, %luK init, %luK bss, %luK reserved, %luK cma-reserved"
#ifdef	CONFIG_HIGHMEM
		", %luK highmem"
#endif
		")\n",
		K(nr_free_pages()), K(physpages),
		codesize / SZ_1K, datasize / SZ_1K, rosize / SZ_1K,
		(init_data_size + init_code_size) / SZ_1K, bss_size / SZ_1K,
		K(physpages - totalram_pages() - totalcma_pages),
		K(totalcma_pages)
#ifdef	CONFIG_HIGHMEM
		, K(totalhigh_pages())
#endif
		);
}

 
void __init mm_core_init(void)
{
	 
	build_all_zonelists(NULL);
	page_alloc_init_cpuhp();

	 
	page_ext_init_flatmem();
	mem_debugging_and_hardening_init();
	kfence_alloc_pool_and_metadata();
	report_meminit();
	kmsan_init_shadow();
	stack_depot_early_init();
	mem_init();
	mem_init_print_info();
	kmem_cache_init();
	 
	page_ext_init_flatmem_late();
	kmemleak_init();
	ptlock_cache_init();
	pgtable_cache_init();
	debug_objects_mem_init();
	vmalloc_init();
	 
	if (!deferred_struct_pages)
		page_ext_init();
	 
	init_espfix_bsp();
	 
	pti_init();
	kmsan_init_runtime();
	mm_cache_init();
}
