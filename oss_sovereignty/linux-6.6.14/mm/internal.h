 
 
#ifndef __MM_INTERNAL_H
#define __MM_INTERNAL_H

#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/rmap.h>
#include <linux/tracepoint-defs.h>

struct folio_batch;

 
#define GFP_RECLAIM_MASK (__GFP_RECLAIM|__GFP_HIGH|__GFP_IO|__GFP_FS|\
			__GFP_NOWARN|__GFP_RETRY_MAYFAIL|__GFP_NOFAIL|\
			__GFP_NORETRY|__GFP_MEMALLOC|__GFP_NOMEMALLOC|\
			__GFP_NOLOCKDEP)

 
#define GFP_BOOT_MASK (__GFP_BITS_MASK & ~(__GFP_RECLAIM|__GFP_IO|__GFP_FS))

 
#define GFP_CONSTRAINT_MASK (__GFP_HARDWALL|__GFP_THISNODE)

 
#define GFP_SLAB_BUG_MASK (__GFP_DMA32|__GFP_HIGHMEM|~__GFP_BITS_MASK)

 
#define WARN_ON_ONCE_GFP(cond, gfp)	({				\
	static bool __section(".data.once") __warned;			\
	int __ret_warn_once = !!(cond);					\
									\
	if (unlikely(!(gfp & __GFP_NOWARN) && __ret_warn_once && !__warned)) { \
		__warned = true;					\
		WARN_ON(1);						\
	}								\
	unlikely(__ret_warn_once);					\
})

void page_writeback_init(void);

 
#define COMPOUND_MAPPED		0x800000
#define FOLIO_PAGES_MAPPED	(COMPOUND_MAPPED - 1)

 
#define SHOW_MEM_FILTER_NODES		(0x0001u)	 

 
static inline int folio_nr_pages_mapped(struct folio *folio)
{
	return atomic_read(&folio->_nr_pages_mapped) & FOLIO_PAGES_MAPPED;
}

static inline void *folio_raw_mapping(struct folio *folio)
{
	unsigned long mapping = (unsigned long)folio->mapping;

	return (void *)(mapping & ~PAGE_MAPPING_FLAGS);
}

void __acct_reclaim_writeback(pg_data_t *pgdat, struct folio *folio,
						int nr_throttled);
static inline void acct_reclaim_writeback(struct folio *folio)
{
	pg_data_t *pgdat = folio_pgdat(folio);
	int nr_throttled = atomic_read(&pgdat->nr_writeback_throttled);

	if (nr_throttled)
		__acct_reclaim_writeback(pgdat, folio, nr_throttled);
}

static inline void wake_throttle_isolated(pg_data_t *pgdat)
{
	wait_queue_head_t *wqh;

	wqh = &pgdat->reclaim_wait[VMSCAN_THROTTLE_ISOLATED];
	if (waitqueue_active(wqh))
		wake_up(wqh);
}

vm_fault_t do_swap_page(struct vm_fault *vmf);
void folio_rotate_reclaimable(struct folio *folio);
bool __folio_end_writeback(struct folio *folio);
void deactivate_file_folio(struct folio *folio);
void folio_activate(struct folio *folio);

void free_pgtables(struct mmu_gather *tlb, struct ma_state *mas,
		   struct vm_area_struct *start_vma, unsigned long floor,
		   unsigned long ceiling, bool mm_wr_locked);
void pmd_install(struct mm_struct *mm, pmd_t *pmd, pgtable_t *pte);

struct zap_details;
void unmap_page_range(struct mmu_gather *tlb,
			     struct vm_area_struct *vma,
			     unsigned long addr, unsigned long end,
			     struct zap_details *details);

void page_cache_ra_order(struct readahead_control *, struct file_ra_state *,
		unsigned int order);
void force_page_cache_ra(struct readahead_control *, unsigned long nr);
static inline void force_page_cache_readahead(struct address_space *mapping,
		struct file *file, pgoff_t index, unsigned long nr_to_read)
{
	DEFINE_READAHEAD(ractl, file, &file->f_ra, mapping, index);
	force_page_cache_ra(&ractl, nr_to_read);
}

unsigned find_lock_entries(struct address_space *mapping, pgoff_t *start,
		pgoff_t end, struct folio_batch *fbatch, pgoff_t *indices);
unsigned find_get_entries(struct address_space *mapping, pgoff_t *start,
		pgoff_t end, struct folio_batch *fbatch, pgoff_t *indices);
void filemap_free_folio(struct address_space *mapping, struct folio *folio);
int truncate_inode_folio(struct address_space *mapping, struct folio *folio);
bool truncate_inode_partial_folio(struct folio *folio, loff_t start,
		loff_t end);
long invalidate_inode_page(struct page *page);
unsigned long mapping_try_invalidate(struct address_space *mapping,
		pgoff_t start, pgoff_t end, unsigned long *nr_failed);

 
static inline bool folio_evictable(struct folio *folio)
{
	bool ret;

	 
	rcu_read_lock();
	ret = !mapping_unevictable(folio_mapping(folio)) &&
			!folio_test_mlocked(folio);
	rcu_read_unlock();
	return ret;
}

 
static inline void set_page_refcounted(struct page *page)
{
	VM_BUG_ON_PAGE(PageTail(page), page);
	VM_BUG_ON_PAGE(page_ref_count(page), page);
	set_page_count(page, 1);
}

 
static inline bool folio_needs_release(struct folio *folio)
{
	struct address_space *mapping = folio_mapping(folio);

	return folio_has_private(folio) ||
		(mapping && mapping_release_always(mapping));
}

extern unsigned long highest_memmap_pfn;

 
#define MAX_RECLAIM_RETRIES 16

 
bool isolate_lru_page(struct page *page);
bool folio_isolate_lru(struct folio *folio);
void putback_lru_page(struct page *page);
void folio_putback_lru(struct folio *folio);
extern void reclaim_throttle(pg_data_t *pgdat, enum vmscan_throttle_state reason);

 
pmd_t *mm_find_pmd(struct mm_struct *mm, unsigned long address);

 
#define K(x) ((x) << (PAGE_SHIFT-10))

extern char * const zone_names[MAX_NR_ZONES];

 
DECLARE_STATIC_KEY_MAYBE(CONFIG_DEBUG_VM, check_pages_enabled);

extern int min_free_kbytes;

void setup_per_zone_wmarks(void);
void calculate_min_free_kbytes(void);
int __meminit init_per_zone_wmark_min(void);
void page_alloc_sysctl_init(void);

 
struct alloc_context {
	struct zonelist *zonelist;
	nodemask_t *nodemask;
	struct zoneref *preferred_zoneref;
	int migratetype;

	 
	enum zone_type highest_zoneidx;
	bool spread_dirty_pages;
};

 
static inline unsigned int buddy_order(struct page *page)
{
	 
	return page_private(page);
}

 
#define buddy_order_unsafe(page)	READ_ONCE(page_private(page))

 
static inline bool page_is_buddy(struct page *page, struct page *buddy,
				 unsigned int order)
{
	if (!page_is_guard(buddy) && !PageBuddy(buddy))
		return false;

	if (buddy_order(buddy) != order)
		return false;

	 
	if (page_zone_id(page) != page_zone_id(buddy))
		return false;

	VM_BUG_ON_PAGE(page_count(buddy) != 0, buddy);

	return true;
}

 
static inline unsigned long
__find_buddy_pfn(unsigned long page_pfn, unsigned int order)
{
	return page_pfn ^ (1 << order);
}

 
static inline struct page *find_buddy_page_pfn(struct page *page,
			unsigned long pfn, unsigned int order, unsigned long *buddy_pfn)
{
	unsigned long __buddy_pfn = __find_buddy_pfn(pfn, order);
	struct page *buddy;

	buddy = page + (__buddy_pfn - pfn);
	if (buddy_pfn)
		*buddy_pfn = __buddy_pfn;

	if (page_is_buddy(page, buddy, order))
		return buddy;
	return NULL;
}

extern struct page *__pageblock_pfn_to_page(unsigned long start_pfn,
				unsigned long end_pfn, struct zone *zone);

static inline struct page *pageblock_pfn_to_page(unsigned long start_pfn,
				unsigned long end_pfn, struct zone *zone)
{
	if (zone->contiguous)
		return pfn_to_page(start_pfn);

	return __pageblock_pfn_to_page(start_pfn, end_pfn, zone);
}

void set_zone_contiguous(struct zone *zone);

static inline void clear_zone_contiguous(struct zone *zone)
{
	zone->contiguous = false;
}

extern int __isolate_free_page(struct page *page, unsigned int order);
extern void __putback_isolated_page(struct page *page, unsigned int order,
				    int mt);
extern void memblock_free_pages(struct page *page, unsigned long pfn,
					unsigned int order);
extern void __free_pages_core(struct page *page, unsigned int order);

 
static inline void folio_set_order(struct folio *folio, unsigned int order)
{
	if (WARN_ON_ONCE(!order || !folio_test_large(folio)))
		return;

	folio->_flags_1 = (folio->_flags_1 & ~0xffUL) | order;
#ifdef CONFIG_64BIT
	folio->_folio_nr_pages = 1U << order;
#endif
}

void folio_undo_large_rmappable(struct folio *folio);

static inline void prep_compound_head(struct page *page, unsigned int order)
{
	struct folio *folio = (struct folio *)page;

	folio_set_order(folio, order);
	atomic_set(&folio->_entire_mapcount, -1);
	atomic_set(&folio->_nr_pages_mapped, 0);
	atomic_set(&folio->_pincount, 0);
}

static inline void prep_compound_tail(struct page *head, int tail_idx)
{
	struct page *p = head + tail_idx;

	p->mapping = TAIL_MAPPING;
	set_compound_head(p, head);
	set_page_private(p, 0);
}

extern void prep_compound_page(struct page *page, unsigned int order);

extern void post_alloc_hook(struct page *page, unsigned int order,
					gfp_t gfp_flags);
extern int user_min_free_kbytes;

extern void free_unref_page(struct page *page, unsigned int order);
extern void free_unref_page_list(struct list_head *list);

extern void zone_pcp_reset(struct zone *zone);
extern void zone_pcp_disable(struct zone *zone);
extern void zone_pcp_enable(struct zone *zone);
extern void zone_pcp_init(struct zone *zone);

extern void *memmap_alloc(phys_addr_t size, phys_addr_t align,
			  phys_addr_t min_addr,
			  int nid, bool exact_nid);

void memmap_init_range(unsigned long, int, unsigned long, unsigned long,
		unsigned long, enum meminit_context, struct vmem_altmap *, int);


int split_free_page(struct page *free_page,
			unsigned int order, unsigned long split_pfn_offset);

#if defined CONFIG_COMPACTION || defined CONFIG_CMA

 
 
struct compact_control {
	struct list_head freepages;	 
	struct list_head migratepages;	 
	unsigned int nr_freepages;	 
	unsigned int nr_migratepages;	 
	unsigned long free_pfn;		 
	 
	unsigned long migrate_pfn;
	unsigned long fast_start_pfn;	 
	struct zone *zone;
	unsigned long total_migrate_scanned;
	unsigned long total_free_scanned;
	unsigned short fast_search_fail; 
	short search_order;		 
	const gfp_t gfp_mask;		 
	int order;			 
	int migratetype;		 
	const unsigned int alloc_flags;	 
	const int highest_zoneidx;	 
	enum migrate_mode mode;		 
	bool ignore_skip_hint;		 
	bool no_set_skip_hint;		 
	bool ignore_block_suitable;	 
	bool direct_compaction;		 
	bool proactive_compaction;	 
	bool whole_zone;		 
	bool contended;			 
	bool finish_pageblock;		 
	bool alloc_contig;		 
};

 
struct capture_control {
	struct compact_control *cc;
	struct page *page;
};

unsigned long
isolate_freepages_range(struct compact_control *cc,
			unsigned long start_pfn, unsigned long end_pfn);
int
isolate_migratepages_range(struct compact_control *cc,
			   unsigned long low_pfn, unsigned long end_pfn);

int __alloc_contig_migrate_range(struct compact_control *cc,
					unsigned long start, unsigned long end);

 
void init_cma_reserved_pageblock(struct page *page);

#endif  

int find_suitable_fallback(struct free_area *area, unsigned int order,
			int migratetype, bool only_stealable, bool *can_steal);

static inline bool free_area_empty(struct free_area *area, int migratetype)
{
	return list_empty(&area->free_list[migratetype]);
}

 

 
static inline bool is_exec_mapping(vm_flags_t flags)
{
	return (flags & (VM_EXEC | VM_WRITE | VM_STACK)) == VM_EXEC;
}

 
static inline bool is_stack_mapping(vm_flags_t flags)
{
	return ((flags & VM_STACK) == VM_STACK) || (flags & VM_SHADOW_STACK);
}

 
static inline bool is_data_mapping(vm_flags_t flags)
{
	return (flags & (VM_WRITE | VM_SHARED | VM_STACK)) == VM_WRITE;
}

 
struct anon_vma *folio_anon_vma(struct folio *folio);

#ifdef CONFIG_MMU
void unmap_mapping_folio(struct folio *folio);
extern long populate_vma_page_range(struct vm_area_struct *vma,
		unsigned long start, unsigned long end, int *locked);
extern long faultin_vma_page_range(struct vm_area_struct *vma,
				   unsigned long start, unsigned long end,
				   bool write, int *locked);
extern bool mlock_future_ok(struct mm_struct *mm, unsigned long flags,
			       unsigned long bytes);
 
void mlock_folio(struct folio *folio);
static inline void mlock_vma_folio(struct folio *folio,
			struct vm_area_struct *vma, bool compound)
{
	 
	if (unlikely((vma->vm_flags & (VM_LOCKED|VM_SPECIAL)) == VM_LOCKED) &&
	    (compound || !folio_test_large(folio)))
		mlock_folio(folio);
}

void munlock_folio(struct folio *folio);
static inline void munlock_vma_folio(struct folio *folio,
			struct vm_area_struct *vma, bool compound)
{
	if (unlikely(vma->vm_flags & VM_LOCKED) &&
	    (compound || !folio_test_large(folio)))
		munlock_folio(folio);
}

void mlock_new_folio(struct folio *folio);
bool need_mlock_drain(int cpu);
void mlock_drain_local(void);
void mlock_drain_remote(int cpu);

extern pmd_t maybe_pmd_mkwrite(pmd_t pmd, struct vm_area_struct *vma);

 
static inline unsigned long
vma_pgoff_address(pgoff_t pgoff, unsigned long nr_pages,
		  struct vm_area_struct *vma)
{
	unsigned long address;

	if (pgoff >= vma->vm_pgoff) {
		address = vma->vm_start +
			((pgoff - vma->vm_pgoff) << PAGE_SHIFT);
		 
		if (address < vma->vm_start || address >= vma->vm_end)
			address = -EFAULT;
	} else if (pgoff + nr_pages - 1 >= vma->vm_pgoff) {
		 
		address = vma->vm_start;
	} else {
		address = -EFAULT;
	}
	return address;
}

 
static inline unsigned long
vma_address(struct page *page, struct vm_area_struct *vma)
{
	VM_BUG_ON_PAGE(PageKsm(page), page);	 
	return vma_pgoff_address(page_to_pgoff(page), compound_nr(page), vma);
}

 
static inline unsigned long vma_address_end(struct page_vma_mapped_walk *pvmw)
{
	struct vm_area_struct *vma = pvmw->vma;
	pgoff_t pgoff;
	unsigned long address;

	 
	if (pvmw->nr_pages == 1)
		return pvmw->address + PAGE_SIZE;

	pgoff = pvmw->pgoff + pvmw->nr_pages;
	address = vma->vm_start + ((pgoff - vma->vm_pgoff) << PAGE_SHIFT);
	 
	if (address < vma->vm_start || address > vma->vm_end)
		address = vma->vm_end;
	return address;
}

static inline struct file *maybe_unlock_mmap_for_io(struct vm_fault *vmf,
						    struct file *fpin)
{
	int flags = vmf->flags;

	if (fpin)
		return fpin;

	 
	if (fault_flag_allow_retry_first(flags) &&
	    !(flags & FAULT_FLAG_RETRY_NOWAIT)) {
		fpin = get_file(vmf->vma->vm_file);
		release_fault_lock(vmf);
	}
	return fpin;
}
#else  
static inline void unmap_mapping_folio(struct folio *folio) { }
static inline void mlock_new_folio(struct folio *folio) { }
static inline bool need_mlock_drain(int cpu) { return false; }
static inline void mlock_drain_local(void) { }
static inline void mlock_drain_remote(int cpu) { }
static inline void vunmap_range_noflush(unsigned long start, unsigned long end)
{
}
#endif  

 
#ifdef CONFIG_DEFERRED_STRUCT_PAGE_INIT
DECLARE_STATIC_KEY_TRUE(deferred_pages);

bool __init deferred_grow_zone(struct zone *zone, unsigned int order);
#endif  

enum mminit_level {
	MMINIT_WARNING,
	MMINIT_VERIFY,
	MMINIT_TRACE
};

#ifdef CONFIG_DEBUG_MEMORY_INIT

extern int mminit_loglevel;

#define mminit_dprintk(level, prefix, fmt, arg...) \
do { \
	if (level < mminit_loglevel) { \
		if (level <= MMINIT_WARNING) \
			pr_warn("mminit::" prefix " " fmt, ##arg);	\
		else \
			printk(KERN_DEBUG "mminit::" prefix " " fmt, ##arg); \
	} \
} while (0)

extern void mminit_verify_pageflags_layout(void);
extern void mminit_verify_zonelist(void);
#else

static inline void mminit_dprintk(enum mminit_level level,
				const char *prefix, const char *fmt, ...)
{
}

static inline void mminit_verify_pageflags_layout(void)
{
}

static inline void mminit_verify_zonelist(void)
{
}
#endif  

#define NODE_RECLAIM_NOSCAN	-2
#define NODE_RECLAIM_FULL	-1
#define NODE_RECLAIM_SOME	0
#define NODE_RECLAIM_SUCCESS	1

#ifdef CONFIG_NUMA
extern int node_reclaim(struct pglist_data *, gfp_t, unsigned int);
extern int find_next_best_node(int node, nodemask_t *used_node_mask);
#else
static inline int node_reclaim(struct pglist_data *pgdat, gfp_t mask,
				unsigned int order)
{
	return NODE_RECLAIM_NOSCAN;
}
static inline int find_next_best_node(int node, nodemask_t *used_node_mask)
{
	return NUMA_NO_NODE;
}
#endif

 
extern int hwpoison_filter(struct page *p);

extern u32 hwpoison_filter_dev_major;
extern u32 hwpoison_filter_dev_minor;
extern u64 hwpoison_filter_flags_mask;
extern u64 hwpoison_filter_flags_value;
extern u64 hwpoison_filter_memcg;
extern u32 hwpoison_filter_enable;

extern unsigned long  __must_check vm_mmap_pgoff(struct file *, unsigned long,
        unsigned long, unsigned long,
        unsigned long, unsigned long);

extern void set_pageblock_order(void);
unsigned long reclaim_pages(struct list_head *folio_list);
unsigned int reclaim_clean_pages_from_list(struct zone *zone,
					    struct list_head *folio_list);
 
#define ALLOC_WMARK_MIN		WMARK_MIN
#define ALLOC_WMARK_LOW		WMARK_LOW
#define ALLOC_WMARK_HIGH	WMARK_HIGH
#define ALLOC_NO_WATERMARKS	0x04  

 
#define ALLOC_WMARK_MASK	(ALLOC_NO_WATERMARKS-1)

 
#ifdef CONFIG_MMU
#define ALLOC_OOM		0x08
#else
#define ALLOC_OOM		ALLOC_NO_WATERMARKS
#endif

#define ALLOC_NON_BLOCK		 0x10  
#define ALLOC_MIN_RESERVE	 0x20  
#define ALLOC_CPUSET		 0x40  
#define ALLOC_CMA		 0x80  
#ifdef CONFIG_ZONE_DMA32
#define ALLOC_NOFRAGMENT	0x100  
#else
#define ALLOC_NOFRAGMENT	  0x0
#endif
#define ALLOC_HIGHATOMIC	0x200  
#define ALLOC_KSWAPD		0x800  

 
#define ALLOC_RESERVES (ALLOC_NON_BLOCK|ALLOC_MIN_RESERVE|ALLOC_HIGHATOMIC|ALLOC_OOM)

enum ttu_flags;
struct tlbflush_unmap_batch;


 
extern struct workqueue_struct *mm_percpu_wq;

#ifdef CONFIG_ARCH_WANT_BATCHED_UNMAP_TLB_FLUSH
void try_to_unmap_flush(void);
void try_to_unmap_flush_dirty(void);
void flush_tlb_batched_pending(struct mm_struct *mm);
#else
static inline void try_to_unmap_flush(void)
{
}
static inline void try_to_unmap_flush_dirty(void)
{
}
static inline void flush_tlb_batched_pending(struct mm_struct *mm)
{
}
#endif  

extern const struct trace_print_flags pageflag_names[];
extern const struct trace_print_flags pagetype_names[];
extern const struct trace_print_flags vmaflag_names[];
extern const struct trace_print_flags gfpflag_names[];

static inline bool is_migrate_highatomic(enum migratetype migratetype)
{
	return migratetype == MIGRATE_HIGHATOMIC;
}

static inline bool is_migrate_highatomic_page(struct page *page)
{
	return get_pageblock_migratetype(page) == MIGRATE_HIGHATOMIC;
}

void setup_zone_pageset(struct zone *zone);

struct migration_target_control {
	int nid;		 
	nodemask_t *nmask;
	gfp_t gfp_mask;
};

 
size_t splice_folio_into_pipe(struct pipe_inode_info *pipe,
			      struct folio *folio, loff_t fpos, size_t size);

 
#ifdef CONFIG_MMU
void __init vmalloc_init(void);
int __must_check vmap_pages_range_noflush(unsigned long addr, unsigned long end,
                pgprot_t prot, struct page **pages, unsigned int page_shift);
#else
static inline void vmalloc_init(void)
{
}

static inline
int __must_check vmap_pages_range_noflush(unsigned long addr, unsigned long end,
                pgprot_t prot, struct page **pages, unsigned int page_shift)
{
	return -EINVAL;
}
#endif

int __must_check __vmap_pages_range_noflush(unsigned long addr,
			       unsigned long end, pgprot_t prot,
			       struct page **pages, unsigned int page_shift);

void vunmap_range_noflush(unsigned long start, unsigned long end);

void __vunmap_range_noflush(unsigned long start, unsigned long end);

int numa_migrate_prep(struct page *page, struct vm_area_struct *vma,
		      unsigned long addr, int page_nid, int *flags);

void free_zone_device_page(struct page *page);
int migrate_device_coherent_page(struct page *page);

 
struct folio *try_grab_folio(struct page *page, int refs, unsigned int flags);
int __must_check try_grab_page(struct page *page, unsigned int flags);

 
struct page *follow_trans_huge_pmd(struct vm_area_struct *vma,
				   unsigned long addr, pmd_t *pmd,
				   unsigned int flags);

enum {
	 
	FOLL_TOUCH = 1 << 16,
	 
	FOLL_TRIED = 1 << 17,
	 
	FOLL_REMOTE = 1 << 18,
	 
	FOLL_PIN = 1 << 19,
	 
	FOLL_FAST_ONLY = 1 << 20,
	 
	FOLL_UNLOCKABLE = 1 << 21,
};

 
static inline bool gup_must_unshare(struct vm_area_struct *vma,
				    unsigned int flags, struct page *page)
{
	 
	if ((flags & (FOLL_WRITE | FOLL_PIN)) != FOLL_PIN)
		return false;
	 
	if (!PageAnon(page)) {
		 
		if (!(flags & FOLL_LONGTERM))
			return false;

		 
		if (!vma)
			return true;

		 
		return is_cow_mapping(vma->vm_flags);
	}

	 
	if (IS_ENABLED(CONFIG_HAVE_FAST_GUP))
		smp_rmb();

	 
	if (unlikely(!PageHead(page) && PageHuge(page)))
		page = compound_head(page);

	 
	return !PageAnonExclusive(page);
}

extern bool mirrored_kernelcore;
extern bool memblock_has_mirror(void);

static inline bool vma_soft_dirty_enabled(struct vm_area_struct *vma)
{
	 
	if (!IS_ENABLED(CONFIG_MEM_SOFT_DIRTY))
		return false;

	 
	return !(vma->vm_flags & VM_SOFTDIRTY);
}

static inline void vma_iter_config(struct vma_iterator *vmi,
		unsigned long index, unsigned long last)
{
	MAS_BUG_ON(&vmi->mas, vmi->mas.node != MAS_START &&
		   (vmi->mas.index > index || vmi->mas.last < index));
	__mas_set_range(&vmi->mas, index, last - 1);
}

 
static inline int vma_iter_prealloc(struct vma_iterator *vmi,
		struct vm_area_struct *vma)
{
	return mas_preallocate(&vmi->mas, vma, GFP_KERNEL);
}

static inline void vma_iter_clear(struct vma_iterator *vmi)
{
	mas_store_prealloc(&vmi->mas, NULL);
}

static inline int vma_iter_clear_gfp(struct vma_iterator *vmi,
			unsigned long start, unsigned long end, gfp_t gfp)
{
	__mas_set_range(&vmi->mas, start, end - 1);
	mas_store_gfp(&vmi->mas, NULL, gfp);
	if (unlikely(mas_is_err(&vmi->mas)))
		return -ENOMEM;

	return 0;
}

static inline struct vm_area_struct *vma_iter_load(struct vma_iterator *vmi)
{
	return mas_walk(&vmi->mas);
}

 
static inline void vma_iter_store(struct vma_iterator *vmi,
				  struct vm_area_struct *vma)
{

#if defined(CONFIG_DEBUG_VM_MAPLE_TREE)
	if (MAS_WARN_ON(&vmi->mas, vmi->mas.node != MAS_START &&
			vmi->mas.index > vma->vm_start)) {
		pr_warn("%lx > %lx\n store vma %lx-%lx\n into slot %lx-%lx\n",
			vmi->mas.index, vma->vm_start, vma->vm_start,
			vma->vm_end, vmi->mas.index, vmi->mas.last);
	}
	if (MAS_WARN_ON(&vmi->mas, vmi->mas.node != MAS_START &&
			vmi->mas.last <  vma->vm_start)) {
		pr_warn("%lx < %lx\nstore vma %lx-%lx\ninto slot %lx-%lx\n",
		       vmi->mas.last, vma->vm_start, vma->vm_start, vma->vm_end,
		       vmi->mas.index, vmi->mas.last);
	}
#endif

	if (vmi->mas.node != MAS_START &&
	    ((vmi->mas.index > vma->vm_start) || (vmi->mas.last < vma->vm_start)))
		vma_iter_invalidate(vmi);

	__mas_set_range(&vmi->mas, vma->vm_start, vma->vm_end - 1);
	mas_store_prealloc(&vmi->mas, vma);
}

static inline int vma_iter_store_gfp(struct vma_iterator *vmi,
			struct vm_area_struct *vma, gfp_t gfp)
{
	if (vmi->mas.node != MAS_START &&
	    ((vmi->mas.index > vma->vm_start) || (vmi->mas.last < vma->vm_start)))
		vma_iter_invalidate(vmi);

	__mas_set_range(&vmi->mas, vma->vm_start, vma->vm_end - 1);
	mas_store_gfp(&vmi->mas, vma, gfp);
	if (unlikely(mas_is_err(&vmi->mas)))
		return -ENOMEM;

	return 0;
}

 
struct vma_prepare {
	struct vm_area_struct *vma;
	struct vm_area_struct *adj_next;
	struct file *file;
	struct address_space *mapping;
	struct anon_vma *anon_vma;
	struct vm_area_struct *insert;
	struct vm_area_struct *remove;
	struct vm_area_struct *remove2;
};
#endif	 
