
 

#include <linux/stddef.h>
#include <linux/mm.h>
#include <linux/sched/signal.h>
#include <linux/swap.h>
#include <linux/interrupt.h>
#include <linux/pagemap.h>
#include <linux/compiler.h>
#include <linux/export.h>
#include <linux/writeback.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/cpu.h>
#include <linux/memory.h>
#include <linux/memremap.h>
#include <linux/memory_hotplug.h>
#include <linux/vmalloc.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/migrate.h>
#include <linux/page-isolation.h>
#include <linux/pfn.h>
#include <linux/suspend.h>
#include <linux/mm_inline.h>
#include <linux/firmware-map.h>
#include <linux/stop_machine.h>
#include <linux/hugetlb.h>
#include <linux/memblock.h>
#include <linux/compaction.h>
#include <linux/rmap.h>
#include <linux/module.h>

#include <asm/tlbflush.h>

#include "internal.h"
#include "shuffle.h"

enum {
	MEMMAP_ON_MEMORY_DISABLE = 0,
	MEMMAP_ON_MEMORY_ENABLE,
	MEMMAP_ON_MEMORY_FORCE,
};

static int memmap_mode __read_mostly = MEMMAP_ON_MEMORY_DISABLE;

static inline unsigned long memory_block_memmap_size(void)
{
	return PHYS_PFN(memory_block_size_bytes()) * sizeof(struct page);
}

static inline unsigned long memory_block_memmap_on_memory_pages(void)
{
	unsigned long nr_pages = PFN_UP(memory_block_memmap_size());

	 
	if (memmap_mode == MEMMAP_ON_MEMORY_FORCE)
		return pageblock_align(nr_pages);
	return nr_pages;
}

#ifdef CONFIG_MHP_MEMMAP_ON_MEMORY
 
static int set_memmap_mode(const char *val, const struct kernel_param *kp)
{
	int ret, mode;
	bool enabled;

	if (sysfs_streq(val, "force") ||  sysfs_streq(val, "FORCE")) {
		mode = MEMMAP_ON_MEMORY_FORCE;
	} else {
		ret = kstrtobool(val, &enabled);
		if (ret < 0)
			return ret;
		if (enabled)
			mode = MEMMAP_ON_MEMORY_ENABLE;
		else
			mode = MEMMAP_ON_MEMORY_DISABLE;
	}
	*((int *)kp->arg) = mode;
	if (mode == MEMMAP_ON_MEMORY_FORCE) {
		unsigned long memmap_pages = memory_block_memmap_on_memory_pages();

		pr_info_once("Memory hotplug will waste %ld pages in each memory block\n",
			     memmap_pages - PFN_UP(memory_block_memmap_size()));
	}
	return 0;
}

static int get_memmap_mode(char *buffer, const struct kernel_param *kp)
{
	int mode = *((int *)kp->arg);

	if (mode == MEMMAP_ON_MEMORY_FORCE)
		return sprintf(buffer, "force\n");
	return sprintf(buffer, "%c\n", mode ? 'Y' : 'N');
}

static const struct kernel_param_ops memmap_mode_ops = {
	.set = set_memmap_mode,
	.get = get_memmap_mode,
};
module_param_cb(memmap_on_memory, &memmap_mode_ops, &memmap_mode, 0444);
MODULE_PARM_DESC(memmap_on_memory, "Enable memmap on memory for memory hotplug\n"
		 "With value \"force\" it could result in memory wastage due "
		 "to memmap size limitations (Y/N/force)");

static inline bool mhp_memmap_on_memory(void)
{
	return memmap_mode != MEMMAP_ON_MEMORY_DISABLE;
}
#else
static inline bool mhp_memmap_on_memory(void)
{
	return false;
}
#endif

enum {
	ONLINE_POLICY_CONTIG_ZONES = 0,
	ONLINE_POLICY_AUTO_MOVABLE,
};

static const char * const online_policy_to_str[] = {
	[ONLINE_POLICY_CONTIG_ZONES] = "contig-zones",
	[ONLINE_POLICY_AUTO_MOVABLE] = "auto-movable",
};

static int set_online_policy(const char *val, const struct kernel_param *kp)
{
	int ret = sysfs_match_string(online_policy_to_str, val);

	if (ret < 0)
		return ret;
	*((int *)kp->arg) = ret;
	return 0;
}

static int get_online_policy(char *buffer, const struct kernel_param *kp)
{
	return sprintf(buffer, "%s\n", online_policy_to_str[*((int *)kp->arg)]);
}

 
static int online_policy __read_mostly = ONLINE_POLICY_CONTIG_ZONES;
static const struct kernel_param_ops online_policy_ops = {
	.set = set_online_policy,
	.get = get_online_policy,
};
module_param_cb(online_policy, &online_policy_ops, &online_policy, 0644);
MODULE_PARM_DESC(online_policy,
		"Set the online policy (\"contig-zones\", \"auto-movable\") "
		"Default: \"contig-zones\"");

 
static unsigned int auto_movable_ratio __read_mostly = 301;
module_param(auto_movable_ratio, uint, 0644);
MODULE_PARM_DESC(auto_movable_ratio,
		"Set the maximum ratio of MOVABLE:KERNEL memory in the system "
		"in percent for \"auto-movable\" online policy. Default: 301");

 
#ifdef CONFIG_NUMA
static bool auto_movable_numa_aware __read_mostly = true;
module_param(auto_movable_numa_aware, bool, 0644);
MODULE_PARM_DESC(auto_movable_numa_aware,
		"Consider numa node stats in addition to global stats in "
		"\"auto-movable\" online policy. Default: true");
#endif  

 

static online_page_callback_t online_page_callback = generic_online_page;
static DEFINE_MUTEX(online_page_callback_lock);

DEFINE_STATIC_PERCPU_RWSEM(mem_hotplug_lock);

void get_online_mems(void)
{
	percpu_down_read(&mem_hotplug_lock);
}

void put_online_mems(void)
{
	percpu_up_read(&mem_hotplug_lock);
}

bool movable_node_enabled = false;

#ifndef CONFIG_MEMORY_HOTPLUG_DEFAULT_ONLINE
int mhp_default_online_type = MMOP_OFFLINE;
#else
int mhp_default_online_type = MMOP_ONLINE;
#endif

static int __init setup_memhp_default_state(char *str)
{
	const int online_type = mhp_online_type_from_str(str);

	if (online_type >= 0)
		mhp_default_online_type = online_type;

	return 1;
}
__setup("memhp_default_state=", setup_memhp_default_state);

void mem_hotplug_begin(void)
{
	cpus_read_lock();
	percpu_down_write(&mem_hotplug_lock);
}

void mem_hotplug_done(void)
{
	percpu_up_write(&mem_hotplug_lock);
	cpus_read_unlock();
}

u64 max_mem_size = U64_MAX;

 
static struct resource *register_memory_resource(u64 start, u64 size,
						 const char *resource_name)
{
	struct resource *res;
	unsigned long flags =  IORESOURCE_SYSTEM_RAM | IORESOURCE_BUSY;

	if (strcmp(resource_name, "System RAM"))
		flags |= IORESOURCE_SYSRAM_DRIVER_MANAGED;

	if (!mhp_range_allowed(start, size, true))
		return ERR_PTR(-E2BIG);

	 
	if (start + size > max_mem_size && system_state < SYSTEM_RUNNING)
		return ERR_PTR(-E2BIG);

	 
	res = __request_region(&iomem_resource, start, size,
			       resource_name, flags);

	if (!res) {
		pr_debug("Unable to reserve System RAM region: %016llx->%016llx\n",
				start, start + size);
		return ERR_PTR(-EEXIST);
	}
	return res;
}

static void release_memory_resource(struct resource *res)
{
	if (!res)
		return;
	release_resource(res);
	kfree(res);
}

static int check_pfn_span(unsigned long pfn, unsigned long nr_pages)
{
	 
	unsigned long min_align;

	if (IS_ENABLED(CONFIG_SPARSEMEM_VMEMMAP))
		min_align = PAGES_PER_SUBSECTION;
	else
		min_align = PAGES_PER_SECTION;
	if (!IS_ALIGNED(pfn | nr_pages, min_align))
		return -EINVAL;
	return 0;
}

 
struct page *pfn_to_online_page(unsigned long pfn)
{
	unsigned long nr = pfn_to_section_nr(pfn);
	struct dev_pagemap *pgmap;
	struct mem_section *ms;

	if (nr >= NR_MEM_SECTIONS)
		return NULL;

	ms = __nr_to_section(nr);
	if (!online_section(ms))
		return NULL;

	 
	if (IS_ENABLED(CONFIG_HAVE_ARCH_PFN_VALID) && !pfn_valid(pfn))
		return NULL;

	if (!pfn_section_valid(ms, pfn))
		return NULL;

	if (!online_device_section(ms))
		return pfn_to_page(pfn);

	 
	pgmap = get_dev_pagemap(pfn, NULL);
	put_dev_pagemap(pgmap);

	 
	if (pgmap)
		return NULL;

	return pfn_to_page(pfn);
}
EXPORT_SYMBOL_GPL(pfn_to_online_page);

int __ref __add_pages(int nid, unsigned long pfn, unsigned long nr_pages,
		struct mhp_params *params)
{
	const unsigned long end_pfn = pfn + nr_pages;
	unsigned long cur_nr_pages;
	int err;
	struct vmem_altmap *altmap = params->altmap;

	if (WARN_ON_ONCE(!pgprot_val(params->pgprot)))
		return -EINVAL;

	VM_BUG_ON(!mhp_range_allowed(PFN_PHYS(pfn), nr_pages * PAGE_SIZE, false));

	if (altmap) {
		 
		if (altmap->base_pfn != pfn
				|| vmem_altmap_offset(altmap) > nr_pages) {
			pr_warn_once("memory add fail, invalid altmap\n");
			return -EINVAL;
		}
		altmap->alloc = 0;
	}

	if (check_pfn_span(pfn, nr_pages)) {
		WARN(1, "Misaligned %s start: %#lx end: %#lx\n", __func__, pfn, pfn + nr_pages - 1);
		return -EINVAL;
	}

	for (; pfn < end_pfn; pfn += cur_nr_pages) {
		 
		cur_nr_pages = min(end_pfn - pfn,
				   SECTION_ALIGN_UP(pfn + 1) - pfn);
		err = sparse_add_section(nid, pfn, cur_nr_pages, altmap,
					 params->pgmap);
		if (err)
			break;
		cond_resched();
	}
	vmemmap_populate_print_last();
	return err;
}

 
static unsigned long find_smallest_section_pfn(int nid, struct zone *zone,
				     unsigned long start_pfn,
				     unsigned long end_pfn)
{
	for (; start_pfn < end_pfn; start_pfn += PAGES_PER_SUBSECTION) {
		if (unlikely(!pfn_to_online_page(start_pfn)))
			continue;

		if (unlikely(pfn_to_nid(start_pfn) != nid))
			continue;

		if (zone != page_zone(pfn_to_page(start_pfn)))
			continue;

		return start_pfn;
	}

	return 0;
}

 
static unsigned long find_biggest_section_pfn(int nid, struct zone *zone,
				    unsigned long start_pfn,
				    unsigned long end_pfn)
{
	unsigned long pfn;

	 
	pfn = end_pfn - 1;
	for (; pfn >= start_pfn; pfn -= PAGES_PER_SUBSECTION) {
		if (unlikely(!pfn_to_online_page(pfn)))
			continue;

		if (unlikely(pfn_to_nid(pfn) != nid))
			continue;

		if (zone != page_zone(pfn_to_page(pfn)))
			continue;

		return pfn;
	}

	return 0;
}

static void shrink_zone_span(struct zone *zone, unsigned long start_pfn,
			     unsigned long end_pfn)
{
	unsigned long pfn;
	int nid = zone_to_nid(zone);

	if (zone->zone_start_pfn == start_pfn) {
		 
		pfn = find_smallest_section_pfn(nid, zone, end_pfn,
						zone_end_pfn(zone));
		if (pfn) {
			zone->spanned_pages = zone_end_pfn(zone) - pfn;
			zone->zone_start_pfn = pfn;
		} else {
			zone->zone_start_pfn = 0;
			zone->spanned_pages = 0;
		}
	} else if (zone_end_pfn(zone) == end_pfn) {
		 
		pfn = find_biggest_section_pfn(nid, zone, zone->zone_start_pfn,
					       start_pfn);
		if (pfn)
			zone->spanned_pages = pfn - zone->zone_start_pfn + 1;
		else {
			zone->zone_start_pfn = 0;
			zone->spanned_pages = 0;
		}
	}
}

static void update_pgdat_span(struct pglist_data *pgdat)
{
	unsigned long node_start_pfn = 0, node_end_pfn = 0;
	struct zone *zone;

	for (zone = pgdat->node_zones;
	     zone < pgdat->node_zones + MAX_NR_ZONES; zone++) {
		unsigned long end_pfn = zone_end_pfn(zone);

		 
		if (!zone->spanned_pages)
			continue;
		if (!node_end_pfn) {
			node_start_pfn = zone->zone_start_pfn;
			node_end_pfn = end_pfn;
			continue;
		}

		if (end_pfn > node_end_pfn)
			node_end_pfn = end_pfn;
		if (zone->zone_start_pfn < node_start_pfn)
			node_start_pfn = zone->zone_start_pfn;
	}

	pgdat->node_start_pfn = node_start_pfn;
	pgdat->node_spanned_pages = node_end_pfn - node_start_pfn;
}

void __ref remove_pfn_range_from_zone(struct zone *zone,
				      unsigned long start_pfn,
				      unsigned long nr_pages)
{
	const unsigned long end_pfn = start_pfn + nr_pages;
	struct pglist_data *pgdat = zone->zone_pgdat;
	unsigned long pfn, cur_nr_pages;

	 
	for (pfn = start_pfn; pfn < end_pfn; pfn += cur_nr_pages) {
		cond_resched();

		 
		cur_nr_pages =
			min(end_pfn - pfn, SECTION_ALIGN_UP(pfn + 1) - pfn);
		page_init_poison(pfn_to_page(pfn),
				 sizeof(struct page) * cur_nr_pages);
	}

	 
	if (zone_is_zone_device(zone))
		return;

	clear_zone_contiguous(zone);

	shrink_zone_span(zone, start_pfn, start_pfn + nr_pages);
	update_pgdat_span(pgdat);

	set_zone_contiguous(zone);
}

 
void __remove_pages(unsigned long pfn, unsigned long nr_pages,
		    struct vmem_altmap *altmap)
{
	const unsigned long end_pfn = pfn + nr_pages;
	unsigned long cur_nr_pages;

	if (check_pfn_span(pfn, nr_pages)) {
		WARN(1, "Misaligned %s start: %#lx end: %#lx\n", __func__, pfn, pfn + nr_pages - 1);
		return;
	}

	for (; pfn < end_pfn; pfn += cur_nr_pages) {
		cond_resched();
		 
		cur_nr_pages = min(end_pfn - pfn,
				   SECTION_ALIGN_UP(pfn + 1) - pfn);
		sparse_remove_section(pfn, cur_nr_pages, altmap);
	}
}

int set_online_page_callback(online_page_callback_t callback)
{
	int rc = -EINVAL;

	get_online_mems();
	mutex_lock(&online_page_callback_lock);

	if (online_page_callback == generic_online_page) {
		online_page_callback = callback;
		rc = 0;
	}

	mutex_unlock(&online_page_callback_lock);
	put_online_mems();

	return rc;
}
EXPORT_SYMBOL_GPL(set_online_page_callback);

int restore_online_page_callback(online_page_callback_t callback)
{
	int rc = -EINVAL;

	get_online_mems();
	mutex_lock(&online_page_callback_lock);

	if (online_page_callback == callback) {
		online_page_callback = generic_online_page;
		rc = 0;
	}

	mutex_unlock(&online_page_callback_lock);
	put_online_mems();

	return rc;
}
EXPORT_SYMBOL_GPL(restore_online_page_callback);

void generic_online_page(struct page *page, unsigned int order)
{
	 
	debug_pagealloc_map_pages(page, 1 << order);
	__free_pages_core(page, order);
	totalram_pages_add(1UL << order);
}
EXPORT_SYMBOL_GPL(generic_online_page);

static void online_pages_range(unsigned long start_pfn, unsigned long nr_pages)
{
	const unsigned long end_pfn = start_pfn + nr_pages;
	unsigned long pfn;

	 
	for (pfn = start_pfn; pfn < end_pfn;) {
		int order;

		 
		if (pfn)
			order = min_t(int, MAX_ORDER, __ffs(pfn));
		else
			order = MAX_ORDER;

		(*online_page_callback)(pfn_to_page(pfn), order);
		pfn += (1UL << order);
	}

	 
	online_mem_sections(start_pfn, end_pfn);
}

 
static void node_states_check_changes_online(unsigned long nr_pages,
	struct zone *zone, struct memory_notify *arg)
{
	int nid = zone_to_nid(zone);

	arg->status_change_nid = NUMA_NO_NODE;
	arg->status_change_nid_normal = NUMA_NO_NODE;

	if (!node_state(nid, N_MEMORY))
		arg->status_change_nid = nid;
	if (zone_idx(zone) <= ZONE_NORMAL && !node_state(nid, N_NORMAL_MEMORY))
		arg->status_change_nid_normal = nid;
}

static void node_states_set_node(int node, struct memory_notify *arg)
{
	if (arg->status_change_nid_normal >= 0)
		node_set_state(node, N_NORMAL_MEMORY);

	if (arg->status_change_nid >= 0)
		node_set_state(node, N_MEMORY);
}

static void __meminit resize_zone_range(struct zone *zone, unsigned long start_pfn,
		unsigned long nr_pages)
{
	unsigned long old_end_pfn = zone_end_pfn(zone);

	if (zone_is_empty(zone) || start_pfn < zone->zone_start_pfn)
		zone->zone_start_pfn = start_pfn;

	zone->spanned_pages = max(start_pfn + nr_pages, old_end_pfn) - zone->zone_start_pfn;
}

static void __meminit resize_pgdat_range(struct pglist_data *pgdat, unsigned long start_pfn,
                                     unsigned long nr_pages)
{
	unsigned long old_end_pfn = pgdat_end_pfn(pgdat);

	if (!pgdat->node_spanned_pages || start_pfn < pgdat->node_start_pfn)
		pgdat->node_start_pfn = start_pfn;

	pgdat->node_spanned_pages = max(start_pfn + nr_pages, old_end_pfn) - pgdat->node_start_pfn;

}

#ifdef CONFIG_ZONE_DEVICE
static void section_taint_zone_device(unsigned long pfn)
{
	struct mem_section *ms = __pfn_to_section(pfn);

	ms->section_mem_map |= SECTION_TAINT_ZONE_DEVICE;
}
#else
static inline void section_taint_zone_device(unsigned long pfn)
{
}
#endif

 
void __ref move_pfn_range_to_zone(struct zone *zone, unsigned long start_pfn,
				  unsigned long nr_pages,
				  struct vmem_altmap *altmap, int migratetype)
{
	struct pglist_data *pgdat = zone->zone_pgdat;
	int nid = pgdat->node_id;

	clear_zone_contiguous(zone);

	if (zone_is_empty(zone))
		init_currently_empty_zone(zone, start_pfn, nr_pages);
	resize_zone_range(zone, start_pfn, nr_pages);
	resize_pgdat_range(pgdat, start_pfn, nr_pages);

	 
	if (zone_is_zone_device(zone)) {
		if (!IS_ALIGNED(start_pfn, PAGES_PER_SECTION))
			section_taint_zone_device(start_pfn);
		if (!IS_ALIGNED(start_pfn + nr_pages, PAGES_PER_SECTION))
			section_taint_zone_device(start_pfn + nr_pages);
	}

	 
	memmap_init_range(nr_pages, nid, zone_idx(zone), start_pfn, 0,
			 MEMINIT_HOTPLUG, altmap, migratetype);

	set_zone_contiguous(zone);
}

struct auto_movable_stats {
	unsigned long kernel_early_pages;
	unsigned long movable_pages;
};

static void auto_movable_stats_account_zone(struct auto_movable_stats *stats,
					    struct zone *zone)
{
	if (zone_idx(zone) == ZONE_MOVABLE) {
		stats->movable_pages += zone->present_pages;
	} else {
		stats->kernel_early_pages += zone->present_early_pages;
#ifdef CONFIG_CMA
		 
		stats->movable_pages += zone->cma_pages;
		stats->kernel_early_pages -= zone->cma_pages;
#endif  
	}
}
struct auto_movable_group_stats {
	unsigned long movable_pages;
	unsigned long req_kernel_early_pages;
};

static int auto_movable_stats_account_group(struct memory_group *group,
					   void *arg)
{
	const int ratio = READ_ONCE(auto_movable_ratio);
	struct auto_movable_group_stats *stats = arg;
	long pages;

	 
	if (!ratio)
		return 0;

	 
	pages = group->present_movable_pages * 100 / ratio;
	pages -= group->present_kernel_pages;

	if (pages > 0)
		stats->req_kernel_early_pages += pages;
	stats->movable_pages += group->present_movable_pages;
	return 0;
}

static bool auto_movable_can_online_movable(int nid, struct memory_group *group,
					    unsigned long nr_pages)
{
	unsigned long kernel_early_pages, movable_pages;
	struct auto_movable_group_stats group_stats = {};
	struct auto_movable_stats stats = {};
	pg_data_t *pgdat = NODE_DATA(nid);
	struct zone *zone;
	int i;

	 
	if (nid == NUMA_NO_NODE) {
		 
		for_each_populated_zone(zone)
			auto_movable_stats_account_zone(&stats, zone);
	} else {
		for (i = 0; i < MAX_NR_ZONES; i++) {
			zone = pgdat->node_zones + i;
			if (populated_zone(zone))
				auto_movable_stats_account_zone(&stats, zone);
		}
	}

	kernel_early_pages = stats.kernel_early_pages;
	movable_pages = stats.movable_pages;

	 
	walk_dynamic_memory_groups(nid, auto_movable_stats_account_group,
				   group, &group_stats);
	if (kernel_early_pages <= group_stats.req_kernel_early_pages)
		return false;
	kernel_early_pages -= group_stats.req_kernel_early_pages;
	movable_pages -= group_stats.movable_pages;

	if (group && group->is_dynamic)
		kernel_early_pages += group->present_kernel_pages;

	 
	movable_pages += nr_pages;
	return movable_pages <= (auto_movable_ratio * kernel_early_pages) / 100;
}

 
static struct zone *default_kernel_zone_for_pfn(int nid, unsigned long start_pfn,
		unsigned long nr_pages)
{
	struct pglist_data *pgdat = NODE_DATA(nid);
	int zid;

	for (zid = 0; zid < ZONE_NORMAL; zid++) {
		struct zone *zone = &pgdat->node_zones[zid];

		if (zone_intersects(zone, start_pfn, nr_pages))
			return zone;
	}

	return &pgdat->node_zones[ZONE_NORMAL];
}

 
static struct zone *auto_movable_zone_for_pfn(int nid,
					      struct memory_group *group,
					      unsigned long pfn,
					      unsigned long nr_pages)
{
	unsigned long online_pages = 0, max_pages, end_pfn;
	struct page *page;

	if (!auto_movable_ratio)
		goto kernel_zone;

	if (group && !group->is_dynamic) {
		max_pages = group->s.max_pages;
		online_pages = group->present_movable_pages;

		 
		if (group->present_kernel_pages)
			goto kernel_zone;
	} else if (!group || group->d.unit_pages == nr_pages) {
		max_pages = nr_pages;
	} else {
		max_pages = group->d.unit_pages;
		 
		pfn = ALIGN_DOWN(pfn, group->d.unit_pages);
		end_pfn = pfn + group->d.unit_pages;
		for (; pfn < end_pfn; pfn += PAGES_PER_SECTION) {
			page = pfn_to_online_page(pfn);
			if (!page)
				continue;
			 
			if (!is_zone_movable_page(page))
				goto kernel_zone;
			online_pages += PAGES_PER_SECTION;
		}
	}

	 
	nr_pages = max_pages - online_pages;
	if (!auto_movable_can_online_movable(NUMA_NO_NODE, group, nr_pages))
		goto kernel_zone;

#ifdef CONFIG_NUMA
	if (auto_movable_numa_aware &&
	    !auto_movable_can_online_movable(nid, group, nr_pages))
		goto kernel_zone;
#endif  

	return &NODE_DATA(nid)->node_zones[ZONE_MOVABLE];
kernel_zone:
	return default_kernel_zone_for_pfn(nid, pfn, nr_pages);
}

static inline struct zone *default_zone_for_pfn(int nid, unsigned long start_pfn,
		unsigned long nr_pages)
{
	struct zone *kernel_zone = default_kernel_zone_for_pfn(nid, start_pfn,
			nr_pages);
	struct zone *movable_zone = &NODE_DATA(nid)->node_zones[ZONE_MOVABLE];
	bool in_kernel = zone_intersects(kernel_zone, start_pfn, nr_pages);
	bool in_movable = zone_intersects(movable_zone, start_pfn, nr_pages);

	 
	if (in_kernel ^ in_movable)
		return (in_kernel) ? kernel_zone : movable_zone;

	 
	return movable_node_enabled ? movable_zone : kernel_zone;
}

struct zone *zone_for_pfn_range(int online_type, int nid,
		struct memory_group *group, unsigned long start_pfn,
		unsigned long nr_pages)
{
	if (online_type == MMOP_ONLINE_KERNEL)
		return default_kernel_zone_for_pfn(nid, start_pfn, nr_pages);

	if (online_type == MMOP_ONLINE_MOVABLE)
		return &NODE_DATA(nid)->node_zones[ZONE_MOVABLE];

	if (online_policy == ONLINE_POLICY_AUTO_MOVABLE)
		return auto_movable_zone_for_pfn(nid, group, start_pfn, nr_pages);

	return default_zone_for_pfn(nid, start_pfn, nr_pages);
}

 
void adjust_present_page_count(struct page *page, struct memory_group *group,
			       long nr_pages)
{
	struct zone *zone = page_zone(page);
	const bool movable = zone_idx(zone) == ZONE_MOVABLE;

	 
	if (early_section(__pfn_to_section(page_to_pfn(page))))
		zone->present_early_pages += nr_pages;
	zone->present_pages += nr_pages;
	zone->zone_pgdat->node_present_pages += nr_pages;

	if (group && movable)
		group->present_movable_pages += nr_pages;
	else if (group && !movable)
		group->present_kernel_pages += nr_pages;
}

int mhp_init_memmap_on_memory(unsigned long pfn, unsigned long nr_pages,
			      struct zone *zone)
{
	unsigned long end_pfn = pfn + nr_pages;
	int ret, i;

	ret = kasan_add_zero_shadow(__va(PFN_PHYS(pfn)), PFN_PHYS(nr_pages));
	if (ret)
		return ret;

	move_pfn_range_to_zone(zone, pfn, nr_pages, NULL, MIGRATE_UNMOVABLE);

	for (i = 0; i < nr_pages; i++)
		SetPageVmemmapSelfHosted(pfn_to_page(pfn + i));

	 
	if (nr_pages >= PAGES_PER_SECTION)
	        online_mem_sections(pfn, ALIGN_DOWN(end_pfn, PAGES_PER_SECTION));

	return ret;
}

void mhp_deinit_memmap_on_memory(unsigned long pfn, unsigned long nr_pages)
{
	unsigned long end_pfn = pfn + nr_pages;

	 
	if (nr_pages >= PAGES_PER_SECTION)
		offline_mem_sections(pfn, ALIGN_DOWN(end_pfn, PAGES_PER_SECTION));

         
	remove_pfn_range_from_zone(page_zone(pfn_to_page(pfn)), pfn, nr_pages);
	kasan_remove_zero_shadow(__va(PFN_PHYS(pfn)), PFN_PHYS(nr_pages));
}

 
int __ref online_pages(unsigned long pfn, unsigned long nr_pages,
		       struct zone *zone, struct memory_group *group)
{
	unsigned long flags;
	int need_zonelists_rebuild = 0;
	const int nid = zone_to_nid(zone);
	int ret;
	struct memory_notify arg;

	 
	if (WARN_ON_ONCE(!nr_pages || !pageblock_aligned(pfn) ||
			 !IS_ALIGNED(pfn + nr_pages, PAGES_PER_SECTION)))
		return -EINVAL;


	 
	move_pfn_range_to_zone(zone, pfn, nr_pages, NULL, MIGRATE_ISOLATE);

	arg.start_pfn = pfn;
	arg.nr_pages = nr_pages;
	node_states_check_changes_online(nr_pages, zone, &arg);

	ret = memory_notify(MEM_GOING_ONLINE, &arg);
	ret = notifier_to_errno(ret);
	if (ret)
		goto failed_addition;

	 
	spin_lock_irqsave(&zone->lock, flags);
	zone->nr_isolate_pageblock += nr_pages / pageblock_nr_pages;
	spin_unlock_irqrestore(&zone->lock, flags);

	 
	if (!populated_zone(zone)) {
		need_zonelists_rebuild = 1;
		setup_zone_pageset(zone);
	}

	online_pages_range(pfn, nr_pages);
	adjust_present_page_count(pfn_to_page(pfn), group, nr_pages);

	node_states_set_node(nid, &arg);
	if (need_zonelists_rebuild)
		build_all_zonelists(NULL);

	 
	undo_isolate_page_range(pfn, pfn + nr_pages, MIGRATE_MOVABLE);

	 
	shuffle_zone(zone);

	 
	init_per_zone_wmark_min();

	kswapd_run(nid);
	kcompactd_run(nid);

	writeback_set_ratelimit();

	memory_notify(MEM_ONLINE, &arg);
	return 0;

failed_addition:
	pr_debug("online_pages [mem %#010llx-%#010llx] failed\n",
		 (unsigned long long) pfn << PAGE_SHIFT,
		 (((unsigned long long) pfn + nr_pages) << PAGE_SHIFT) - 1);
	memory_notify(MEM_CANCEL_ONLINE, &arg);
	remove_pfn_range_from_zone(zone, pfn, nr_pages);
	return ret;
}

 
static pg_data_t __ref *hotadd_init_pgdat(int nid)
{
	struct pglist_data *pgdat;

	 
	pgdat = NODE_DATA(nid);

	 
	free_area_init_core_hotplug(pgdat);

	 
	build_all_zonelists(pgdat);

	return pgdat;
}

 
static int __try_online_node(int nid, bool set_node_online)
{
	pg_data_t *pgdat;
	int ret = 1;

	if (node_online(nid))
		return 0;

	pgdat = hotadd_init_pgdat(nid);
	if (!pgdat) {
		pr_err("Cannot online node %d due to NULL pgdat\n", nid);
		ret = -ENOMEM;
		goto out;
	}

	if (set_node_online) {
		node_set_online(nid);
		ret = register_one_node(nid);
		BUG_ON(ret);
	}
out:
	return ret;
}

 
int try_online_node(int nid)
{
	int ret;

	mem_hotplug_begin();
	ret =  __try_online_node(nid, true);
	mem_hotplug_done();
	return ret;
}

static int check_hotplug_memory_range(u64 start, u64 size)
{
	 
	if (!size || !IS_ALIGNED(start, memory_block_size_bytes()) ||
	    !IS_ALIGNED(size, memory_block_size_bytes())) {
		pr_err("Block size [%#lx] unaligned hotplug range: start %#llx, size %#llx",
		       memory_block_size_bytes(), start, size);
		return -EINVAL;
	}

	return 0;
}

static int online_memory_block(struct memory_block *mem, void *arg)
{
	mem->online_type = mhp_default_online_type;
	return device_online(&mem->dev);
}

#ifndef arch_supports_memmap_on_memory
static inline bool arch_supports_memmap_on_memory(unsigned long vmemmap_size)
{
	 
	return IS_ALIGNED(vmemmap_size, PMD_SIZE);
}
#endif

static bool mhp_supports_memmap_on_memory(unsigned long size)
{
	unsigned long vmemmap_size = memory_block_memmap_size();
	unsigned long memmap_pages = memory_block_memmap_on_memory_pages();

	 
	if (!mhp_memmap_on_memory() || size != memory_block_size_bytes())
		return false;

	 
	if (!IS_ALIGNED(vmemmap_size, PAGE_SIZE))
		return false;

	 
	if (!pageblock_aligned(memmap_pages))
		return false;

	if (memmap_pages == PHYS_PFN(memory_block_size_bytes()))
		 
		return false;

	return arch_supports_memmap_on_memory(vmemmap_size);
}

 
int __ref add_memory_resource(int nid, struct resource *res, mhp_t mhp_flags)
{
	struct mhp_params params = { .pgprot = pgprot_mhp(PAGE_KERNEL) };
	enum memblock_flags memblock_flags = MEMBLOCK_NONE;
	struct vmem_altmap mhp_altmap = {
		.base_pfn =  PHYS_PFN(res->start),
		.end_pfn  =  PHYS_PFN(res->end),
	};
	struct memory_group *group = NULL;
	u64 start, size;
	bool new_node = false;
	int ret;

	start = res->start;
	size = resource_size(res);

	ret = check_hotplug_memory_range(start, size);
	if (ret)
		return ret;

	if (mhp_flags & MHP_NID_IS_MGID) {
		group = memory_group_find_by_id(nid);
		if (!group)
			return -EINVAL;
		nid = group->nid;
	}

	if (!node_possible(nid)) {
		WARN(1, "node %d was absent from the node_possible_map\n", nid);
		return -EINVAL;
	}

	mem_hotplug_begin();

	if (IS_ENABLED(CONFIG_ARCH_KEEP_MEMBLOCK)) {
		if (res->flags & IORESOURCE_SYSRAM_DRIVER_MANAGED)
			memblock_flags = MEMBLOCK_DRIVER_MANAGED;
		ret = memblock_add_node(start, size, nid, memblock_flags);
		if (ret)
			goto error_mem_hotplug_end;
	}

	ret = __try_online_node(nid, false);
	if (ret < 0)
		goto error;
	new_node = ret;

	 
	if (mhp_flags & MHP_MEMMAP_ON_MEMORY) {
		if (mhp_supports_memmap_on_memory(size)) {
			mhp_altmap.free = memory_block_memmap_on_memory_pages();
			params.altmap = kmalloc(sizeof(struct vmem_altmap), GFP_KERNEL);
			if (!params.altmap) {
				ret = -ENOMEM;
				goto error;
			}

			memcpy(params.altmap, &mhp_altmap, sizeof(mhp_altmap));
		}
		 
	}

	 
	ret = arch_add_memory(nid, start, size, &params);
	if (ret < 0)
		goto error_free;

	 
	ret = create_memory_block_devices(start, size, params.altmap, group);
	if (ret) {
		arch_remove_memory(start, size, params.altmap);
		goto error_free;
	}

	if (new_node) {
		 
		node_set_online(nid);
		ret = __register_one_node(nid);
		BUG_ON(ret);
	}

	register_memory_blocks_under_node(nid, PFN_DOWN(start),
					  PFN_UP(start + size - 1),
					  MEMINIT_HOTPLUG);

	 
	if (!strcmp(res->name, "System RAM"))
		firmware_map_add_hotplug(start, start + size, "System RAM");

	 
	mem_hotplug_done();

	 
	if (mhp_flags & MHP_MERGE_RESOURCE)
		merge_system_ram_resource(res);

	 
	if (mhp_default_online_type != MMOP_OFFLINE)
		walk_memory_blocks(start, size, NULL, online_memory_block);

	return ret;
error_free:
	kfree(params.altmap);
error:
	if (IS_ENABLED(CONFIG_ARCH_KEEP_MEMBLOCK))
		memblock_remove(start, size);
error_mem_hotplug_end:
	mem_hotplug_done();
	return ret;
}

 
int __ref __add_memory(int nid, u64 start, u64 size, mhp_t mhp_flags)
{
	struct resource *res;
	int ret;

	res = register_memory_resource(start, size, "System RAM");
	if (IS_ERR(res))
		return PTR_ERR(res);

	ret = add_memory_resource(nid, res, mhp_flags);
	if (ret < 0)
		release_memory_resource(res);
	return ret;
}

int add_memory(int nid, u64 start, u64 size, mhp_t mhp_flags)
{
	int rc;

	lock_device_hotplug();
	rc = __add_memory(nid, start, size, mhp_flags);
	unlock_device_hotplug();

	return rc;
}
EXPORT_SYMBOL_GPL(add_memory);

 
int add_memory_driver_managed(int nid, u64 start, u64 size,
			      const char *resource_name, mhp_t mhp_flags)
{
	struct resource *res;
	int rc;

	if (!resource_name ||
	    strstr(resource_name, "System RAM (") != resource_name ||
	    resource_name[strlen(resource_name) - 1] != ')')
		return -EINVAL;

	lock_device_hotplug();

	res = register_memory_resource(start, size, resource_name);
	if (IS_ERR(res)) {
		rc = PTR_ERR(res);
		goto out_unlock;
	}

	rc = add_memory_resource(nid, res, mhp_flags);
	if (rc < 0)
		release_memory_resource(res);

out_unlock:
	unlock_device_hotplug();
	return rc;
}
EXPORT_SYMBOL_GPL(add_memory_driver_managed);

 
struct range __weak arch_get_mappable_range(void)
{
	struct range mhp_range = {
		.start = 0UL,
		.end = -1ULL,
	};
	return mhp_range;
}

struct range mhp_get_pluggable_range(bool need_mapping)
{
	const u64 max_phys = (1ULL << MAX_PHYSMEM_BITS) - 1;
	struct range mhp_range;

	if (need_mapping) {
		mhp_range = arch_get_mappable_range();
		if (mhp_range.start > max_phys) {
			mhp_range.start = 0;
			mhp_range.end = 0;
		}
		mhp_range.end = min_t(u64, mhp_range.end, max_phys);
	} else {
		mhp_range.start = 0;
		mhp_range.end = max_phys;
	}
	return mhp_range;
}
EXPORT_SYMBOL_GPL(mhp_get_pluggable_range);

bool mhp_range_allowed(u64 start, u64 size, bool need_mapping)
{
	struct range mhp_range = mhp_get_pluggable_range(need_mapping);
	u64 end = start + size;

	if (start < end && start >= mhp_range.start && (end - 1) <= mhp_range.end)
		return true;

	pr_warn("Hotplug memory [%#llx-%#llx] exceeds maximum addressable range [%#llx-%#llx]\n",
		start, end, mhp_range.start, mhp_range.end);
	return false;
}

#ifdef CONFIG_MEMORY_HOTREMOVE
 
static int scan_movable_pages(unsigned long start, unsigned long end,
			      unsigned long *movable_pfn)
{
	unsigned long pfn;

	for (pfn = start; pfn < end; pfn++) {
		struct page *page, *head;
		unsigned long skip;

		if (!pfn_valid(pfn))
			continue;
		page = pfn_to_page(pfn);
		if (PageLRU(page))
			goto found;
		if (__PageMovable(page))
			goto found;

		 
		if (PageOffline(page) && page_count(page))
			return -EBUSY;

		if (!PageHuge(page))
			continue;
		head = compound_head(page);
		 
		if (HPageMigratable(head))
			goto found;
		skip = compound_nr(head) - (pfn - page_to_pfn(head));
		pfn += skip - 1;
	}
	return -ENOENT;
found:
	*movable_pfn = pfn;
	return 0;
}

static void do_migrate_range(unsigned long start_pfn, unsigned long end_pfn)
{
	unsigned long pfn;
	struct page *page, *head;
	LIST_HEAD(source);
	static DEFINE_RATELIMIT_STATE(migrate_rs, DEFAULT_RATELIMIT_INTERVAL,
				      DEFAULT_RATELIMIT_BURST);

	for (pfn = start_pfn; pfn < end_pfn; pfn++) {
		struct folio *folio;
		bool isolated;

		if (!pfn_valid(pfn))
			continue;
		page = pfn_to_page(pfn);
		folio = page_folio(page);
		head = &folio->page;

		if (PageHuge(page)) {
			pfn = page_to_pfn(head) + compound_nr(head) - 1;
			isolate_hugetlb(folio, &source);
			continue;
		} else if (PageTransHuge(page))
			pfn = page_to_pfn(head) + thp_nr_pages(page) - 1;

		 
		if (PageHWPoison(page)) {
			if (WARN_ON(folio_test_lru(folio)))
				folio_isolate_lru(folio);
			if (folio_mapped(folio))
				try_to_unmap(folio, TTU_IGNORE_MLOCK);
			continue;
		}

		if (!get_page_unless_zero(page))
			continue;
		 
		if (PageLRU(page))
			isolated = isolate_lru_page(page);
		else
			isolated = isolate_movable_page(page, ISOLATE_UNEVICTABLE);
		if (isolated) {
			list_add_tail(&page->lru, &source);
			if (!__PageMovable(page))
				inc_node_page_state(page, NR_ISOLATED_ANON +
						    page_is_file_lru(page));

		} else {
			if (__ratelimit(&migrate_rs)) {
				pr_warn("failed to isolate pfn %lx\n", pfn);
				dump_page(page, "isolation failed");
			}
		}
		put_page(page);
	}
	if (!list_empty(&source)) {
		nodemask_t nmask = node_states[N_MEMORY];
		struct migration_target_control mtc = {
			.nmask = &nmask,
			.gfp_mask = GFP_USER | __GFP_MOVABLE | __GFP_RETRY_MAYFAIL,
		};
		int ret;

		 
		mtc.nid = page_to_nid(list_first_entry(&source, struct page, lru));

		 
		node_clear(mtc.nid, nmask);
		if (nodes_empty(nmask))
			node_set(mtc.nid, nmask);
		ret = migrate_pages(&source, alloc_migration_target, NULL,
			(unsigned long)&mtc, MIGRATE_SYNC, MR_MEMORY_HOTPLUG, NULL);
		if (ret) {
			list_for_each_entry(page, &source, lru) {
				if (__ratelimit(&migrate_rs)) {
					pr_warn("migrating pfn %lx failed ret:%d\n",
						page_to_pfn(page), ret);
					dump_page(page, "migration failure");
				}
			}
			putback_movable_pages(&source);
		}
	}
}

static int __init cmdline_parse_movable_node(char *p)
{
	movable_node_enabled = true;
	return 0;
}
early_param("movable_node", cmdline_parse_movable_node);

 
static void node_states_check_changes_offline(unsigned long nr_pages,
		struct zone *zone, struct memory_notify *arg)
{
	struct pglist_data *pgdat = zone->zone_pgdat;
	unsigned long present_pages = 0;
	enum zone_type zt;

	arg->status_change_nid = NUMA_NO_NODE;
	arg->status_change_nid_normal = NUMA_NO_NODE;

	 
	for (zt = 0; zt <= ZONE_NORMAL; zt++)
		present_pages += pgdat->node_zones[zt].present_pages;
	if (zone_idx(zone) <= ZONE_NORMAL && nr_pages >= present_pages)
		arg->status_change_nid_normal = zone_to_nid(zone);

	 
	present_pages += pgdat->node_zones[ZONE_MOVABLE].present_pages;

	if (nr_pages >= present_pages)
		arg->status_change_nid = zone_to_nid(zone);
}

static void node_states_clear_node(int node, struct memory_notify *arg)
{
	if (arg->status_change_nid_normal >= 0)
		node_clear_state(node, N_NORMAL_MEMORY);

	if (arg->status_change_nid >= 0)
		node_clear_state(node, N_MEMORY);
}

static int count_system_ram_pages_cb(unsigned long start_pfn,
				     unsigned long nr_pages, void *data)
{
	unsigned long *nr_system_ram_pages = data;

	*nr_system_ram_pages += nr_pages;
	return 0;
}

 
int __ref offline_pages(unsigned long start_pfn, unsigned long nr_pages,
			struct zone *zone, struct memory_group *group)
{
	const unsigned long end_pfn = start_pfn + nr_pages;
	unsigned long pfn, system_ram_pages = 0;
	const int node = zone_to_nid(zone);
	unsigned long flags;
	struct memory_notify arg;
	char *reason;
	int ret;

	 
	if (WARN_ON_ONCE(!nr_pages || !pageblock_aligned(start_pfn) ||
			 !IS_ALIGNED(start_pfn + nr_pages, PAGES_PER_SECTION)))
		return -EINVAL;

	 
	walk_system_ram_range(start_pfn, nr_pages, &system_ram_pages,
			      count_system_ram_pages_cb);
	if (system_ram_pages != nr_pages) {
		ret = -EINVAL;
		reason = "memory holes";
		goto failed_removal;
	}

	 
	if (WARN_ON_ONCE(page_zone(pfn_to_page(start_pfn)) != zone ||
			 page_zone(pfn_to_page(end_pfn - 1)) != zone)) {
		ret = -EINVAL;
		reason = "multizone range";
		goto failed_removal;
	}

	 
	zone_pcp_disable(zone);
	lru_cache_disable();

	 
	ret = start_isolate_page_range(start_pfn, end_pfn,
				       MIGRATE_MOVABLE,
				       MEMORY_OFFLINE | REPORT_FAILURE,
				       GFP_USER | __GFP_MOVABLE | __GFP_RETRY_MAYFAIL);
	if (ret) {
		reason = "failure to isolate range";
		goto failed_removal_pcplists_disabled;
	}

	arg.start_pfn = start_pfn;
	arg.nr_pages = nr_pages;
	node_states_check_changes_offline(nr_pages, zone, &arg);

	ret = memory_notify(MEM_GOING_OFFLINE, &arg);
	ret = notifier_to_errno(ret);
	if (ret) {
		reason = "notifier failure";
		goto failed_removal_isolated;
	}

	do {
		pfn = start_pfn;
		do {
			 
			if (signal_pending(current)) {
				ret = -EINTR;
				reason = "signal backoff";
				goto failed_removal_isolated;
			}

			cond_resched();

			ret = scan_movable_pages(pfn, end_pfn, &pfn);
			if (!ret) {
				 
				do_migrate_range(pfn, end_pfn);
			}
		} while (!ret);

		if (ret != -ENOENT) {
			reason = "unmovable page";
			goto failed_removal_isolated;
		}

		 
		ret = dissolve_free_huge_pages(start_pfn, end_pfn);
		if (ret) {
			reason = "failure to dissolve huge pages";
			goto failed_removal_isolated;
		}

		ret = test_pages_isolated(start_pfn, end_pfn, MEMORY_OFFLINE);

	} while (ret);

	 
	__offline_isolated_pages(start_pfn, end_pfn);
	pr_debug("Offlined Pages %ld\n", nr_pages);

	 
	spin_lock_irqsave(&zone->lock, flags);
	zone->nr_isolate_pageblock -= nr_pages / pageblock_nr_pages;
	spin_unlock_irqrestore(&zone->lock, flags);

	lru_cache_enable();
	zone_pcp_enable(zone);

	 
	adjust_managed_page_count(pfn_to_page(start_pfn), -nr_pages);
	adjust_present_page_count(pfn_to_page(start_pfn), group, -nr_pages);

	 
	init_per_zone_wmark_min();

	if (!populated_zone(zone)) {
		zone_pcp_reset(zone);
		build_all_zonelists(NULL);
	}

	node_states_clear_node(node, &arg);
	if (arg.status_change_nid >= 0) {
		kcompactd_stop(node);
		kswapd_stop(node);
	}

	writeback_set_ratelimit();

	memory_notify(MEM_OFFLINE, &arg);
	remove_pfn_range_from_zone(zone, start_pfn, nr_pages);
	return 0;

failed_removal_isolated:
	 
	undo_isolate_page_range(start_pfn, end_pfn, MIGRATE_MOVABLE);
	memory_notify(MEM_CANCEL_OFFLINE, &arg);
failed_removal_pcplists_disabled:
	lru_cache_enable();
	zone_pcp_enable(zone);
failed_removal:
	pr_debug("memory offlining [mem %#010llx-%#010llx] failed due to %s\n",
		 (unsigned long long) start_pfn << PAGE_SHIFT,
		 ((unsigned long long) end_pfn << PAGE_SHIFT) - 1,
		 reason);
	return ret;
}

static int check_memblock_offlined_cb(struct memory_block *mem, void *arg)
{
	int *nid = arg;

	*nid = mem->nid;
	if (unlikely(mem->state != MEM_OFFLINE)) {
		phys_addr_t beginpa, endpa;

		beginpa = PFN_PHYS(section_nr_to_pfn(mem->start_section_nr));
		endpa = beginpa + memory_block_size_bytes() - 1;
		pr_warn("removing memory fails, because memory [%pa-%pa] is onlined\n",
			&beginpa, &endpa);

		return -EBUSY;
	}
	return 0;
}

static int test_has_altmap_cb(struct memory_block *mem, void *arg)
{
	struct memory_block **mem_ptr = (struct memory_block **)arg;
	 
	if (mem->altmap) {
		*mem_ptr = mem;
		return 1;
	}
	return 0;
}

static int check_cpu_on_node(int nid)
{
	int cpu;

	for_each_present_cpu(cpu) {
		if (cpu_to_node(cpu) == nid)
			 
			return -EBUSY;
	}

	return 0;
}

static int check_no_memblock_for_node_cb(struct memory_block *mem, void *arg)
{
	int nid = *(int *)arg;

	 
	return mem->nid == nid ? -EEXIST : 0;
}

 
void try_offline_node(int nid)
{
	int rc;

	 
	if (node_spanned_pages(nid))
		return;

	 
	rc = for_each_memory_block(&nid, check_no_memblock_for_node_cb);
	if (rc)
		return;

	if (check_cpu_on_node(nid))
		return;

	 
	node_set_offline(nid);
	unregister_one_node(nid);
}
EXPORT_SYMBOL(try_offline_node);

static int __ref try_remove_memory(u64 start, u64 size)
{
	struct memory_block *mem;
	int rc = 0, nid = NUMA_NO_NODE;
	struct vmem_altmap *altmap = NULL;

	BUG_ON(check_hotplug_memory_range(start, size));

	 
	rc = walk_memory_blocks(start, size, &nid, check_memblock_offlined_cb);
	if (rc)
		return rc;

	 
	if (mhp_memmap_on_memory()) {
		rc = walk_memory_blocks(start, size, &mem, test_has_altmap_cb);
		if (rc) {
			if (size != memory_block_size_bytes()) {
				pr_warn("Refuse to remove %#llx - %#llx,"
					"wrong granularity\n",
					start, start + size);
				return -EINVAL;
			}
			altmap = mem->altmap;
			 
			mem->altmap = NULL;
		}
	}

	 
	firmware_map_remove(start, start + size, "System RAM");

	 
	remove_memory_block_devices(start, size);

	mem_hotplug_begin();

	arch_remove_memory(start, size, altmap);

	 
	if (altmap) {
		WARN(altmap->alloc, "Altmap not fully unmapped");
		kfree(altmap);
	}

	if (IS_ENABLED(CONFIG_ARCH_KEEP_MEMBLOCK)) {
		memblock_phys_free(start, size);
		memblock_remove(start, size);
	}

	release_mem_region_adjustable(start, size);

	if (nid != NUMA_NO_NODE)
		try_offline_node(nid);

	mem_hotplug_done();
	return 0;
}

 
void __remove_memory(u64 start, u64 size)
{

	 
	if (try_remove_memory(start, size))
		BUG();
}

 
int remove_memory(u64 start, u64 size)
{
	int rc;

	lock_device_hotplug();
	rc = try_remove_memory(start, size);
	unlock_device_hotplug();

	return rc;
}
EXPORT_SYMBOL_GPL(remove_memory);

static int try_offline_memory_block(struct memory_block *mem, void *arg)
{
	uint8_t online_type = MMOP_ONLINE_KERNEL;
	uint8_t **online_types = arg;
	struct page *page;
	int rc;

	 
	page = pfn_to_online_page(section_nr_to_pfn(mem->start_section_nr));
	if (page && zone_idx(page_zone(page)) == ZONE_MOVABLE)
		online_type = MMOP_ONLINE_MOVABLE;

	rc = device_offline(&mem->dev);
	 
	if (!rc)
		**online_types = online_type;

	(*online_types)++;
	 
	return rc < 0 ? rc : 0;
}

static int try_reonline_memory_block(struct memory_block *mem, void *arg)
{
	uint8_t **online_types = arg;
	int rc;

	if (**online_types != MMOP_OFFLINE) {
		mem->online_type = **online_types;
		rc = device_online(&mem->dev);
		if (rc < 0)
			pr_warn("%s: Failed to re-online memory: %d",
				__func__, rc);
	}

	 
	(*online_types)++;
	return 0;
}

 
int offline_and_remove_memory(u64 start, u64 size)
{
	const unsigned long mb_count = size / memory_block_size_bytes();
	uint8_t *online_types, *tmp;
	int rc;

	if (!IS_ALIGNED(start, memory_block_size_bytes()) ||
	    !IS_ALIGNED(size, memory_block_size_bytes()) || !size)
		return -EINVAL;

	 
	online_types = kmalloc_array(mb_count, sizeof(*online_types),
				     GFP_KERNEL);
	if (!online_types)
		return -ENOMEM;
	 
	memset(online_types, MMOP_OFFLINE, mb_count);

	lock_device_hotplug();

	tmp = online_types;
	rc = walk_memory_blocks(start, size, &tmp, try_offline_memory_block);

	 
	if (!rc) {
		rc = try_remove_memory(start, size);
		if (rc)
			pr_err("%s: Failed to remove memory: %d", __func__, rc);
	}

	 
	if (rc) {
		tmp = online_types;
		walk_memory_blocks(start, size, &tmp,
				   try_reonline_memory_block);
	}
	unlock_device_hotplug();

	kfree(online_types);
	return rc;
}
EXPORT_SYMBOL_GPL(offline_and_remove_memory);
#endif  
