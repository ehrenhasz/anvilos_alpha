
 

#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/mm_inline.h>
#include <linux/fs.h>
#include <linux/mman.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/sched/coredump.h>
#include <linux/rwsem.h>
#include <linux/pagemap.h>
#include <linux/rmap.h>
#include <linux/spinlock.h>
#include <linux/xxhash.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/rbtree.h>
#include <linux/memory.h>
#include <linux/mmu_notifier.h>
#include <linux/swap.h>
#include <linux/ksm.h>
#include <linux/hashtable.h>
#include <linux/freezer.h>
#include <linux/oom.h>
#include <linux/numa.h>
#include <linux/pagewalk.h>

#include <asm/tlbflush.h>
#include "internal.h"
#include "mm_slot.h"

#define CREATE_TRACE_POINTS
#include <trace/events/ksm.h>

#ifdef CONFIG_NUMA
#define NUMA(x)		(x)
#define DO_NUMA(x)	do { (x); } while (0)
#else
#define NUMA(x)		(0)
#define DO_NUMA(x)	do { } while (0)
#endif

 

 
struct ksm_mm_slot {
	struct mm_slot slot;
	struct ksm_rmap_item *rmap_list;
};

 
struct ksm_scan {
	struct ksm_mm_slot *mm_slot;
	unsigned long address;
	struct ksm_rmap_item **rmap_list;
	unsigned long seqnr;
};

 
struct ksm_stable_node {
	union {
		struct rb_node node;	 
		struct {		 
			struct list_head *head;
			struct {
				struct hlist_node hlist_dup;
				struct list_head list;
			};
		};
	};
	struct hlist_head hlist;
	union {
		unsigned long kpfn;
		unsigned long chain_prune_time;
	};
	 
#define STABLE_NODE_CHAIN -1024
	int rmap_hlist_len;
#ifdef CONFIG_NUMA
	int nid;
#endif
};

 
struct ksm_rmap_item {
	struct ksm_rmap_item *rmap_list;
	union {
		struct anon_vma *anon_vma;	 
#ifdef CONFIG_NUMA
		int nid;		 
#endif
	};
	struct mm_struct *mm;
	unsigned long address;		 
	unsigned int oldchecksum;	 
	union {
		struct rb_node node;	 
		struct {		 
			struct ksm_stable_node *head;
			struct hlist_node hlist;
		};
	};
};

#define SEQNR_MASK	0x0ff	 
#define UNSTABLE_FLAG	0x100	 
#define STABLE_FLAG	0x200	 

 
static struct rb_root one_stable_tree[1] = { RB_ROOT };
static struct rb_root one_unstable_tree[1] = { RB_ROOT };
static struct rb_root *root_stable_tree = one_stable_tree;
static struct rb_root *root_unstable_tree = one_unstable_tree;

 
static LIST_HEAD(migrate_nodes);
#define STABLE_NODE_DUP_HEAD ((struct list_head *)&migrate_nodes.prev)

#define MM_SLOTS_HASH_BITS 10
static DEFINE_HASHTABLE(mm_slots_hash, MM_SLOTS_HASH_BITS);

static struct ksm_mm_slot ksm_mm_head = {
	.slot.mm_node = LIST_HEAD_INIT(ksm_mm_head.slot.mm_node),
};
static struct ksm_scan ksm_scan = {
	.mm_slot = &ksm_mm_head,
};

static struct kmem_cache *rmap_item_cache;
static struct kmem_cache *stable_node_cache;
static struct kmem_cache *mm_slot_cache;

 
static unsigned long ksm_pages_scanned;

 
static unsigned long ksm_pages_shared;

 
static unsigned long ksm_pages_sharing;

 
static unsigned long ksm_pages_unshared;

 
static unsigned long ksm_rmap_items;

 
static unsigned long ksm_stable_node_chains;

 
static unsigned long ksm_stable_node_dups;

 
static unsigned int ksm_stable_node_chains_prune_millisecs = 2000;

 
static int ksm_max_page_sharing = 256;

 
static unsigned int ksm_thread_pages_to_scan = 100;

 
static unsigned int ksm_thread_sleep_millisecs = 20;

 
static unsigned int zero_checksum __read_mostly;

 
static bool ksm_use_zero_pages __read_mostly;

 
unsigned long ksm_zero_pages;

#ifdef CONFIG_NUMA
 
static unsigned int ksm_merge_across_nodes = 1;
static int ksm_nr_node_ids = 1;
#else
#define ksm_merge_across_nodes	1U
#define ksm_nr_node_ids		1
#endif

#define KSM_RUN_STOP	0
#define KSM_RUN_MERGE	1
#define KSM_RUN_UNMERGE	2
#define KSM_RUN_OFFLINE	4
static unsigned long ksm_run = KSM_RUN_STOP;
static void wait_while_offlining(void);

static DECLARE_WAIT_QUEUE_HEAD(ksm_thread_wait);
static DECLARE_WAIT_QUEUE_HEAD(ksm_iter_wait);
static DEFINE_MUTEX(ksm_thread_mutex);
static DEFINE_SPINLOCK(ksm_mmlist_lock);

#define KSM_KMEM_CACHE(__struct, __flags) kmem_cache_create(#__struct,\
		sizeof(struct __struct), __alignof__(struct __struct),\
		(__flags), NULL)

static int __init ksm_slab_init(void)
{
	rmap_item_cache = KSM_KMEM_CACHE(ksm_rmap_item, 0);
	if (!rmap_item_cache)
		goto out;

	stable_node_cache = KSM_KMEM_CACHE(ksm_stable_node, 0);
	if (!stable_node_cache)
		goto out_free1;

	mm_slot_cache = KSM_KMEM_CACHE(ksm_mm_slot, 0);
	if (!mm_slot_cache)
		goto out_free2;

	return 0;

out_free2:
	kmem_cache_destroy(stable_node_cache);
out_free1:
	kmem_cache_destroy(rmap_item_cache);
out:
	return -ENOMEM;
}

static void __init ksm_slab_free(void)
{
	kmem_cache_destroy(mm_slot_cache);
	kmem_cache_destroy(stable_node_cache);
	kmem_cache_destroy(rmap_item_cache);
	mm_slot_cache = NULL;
}

static __always_inline bool is_stable_node_chain(struct ksm_stable_node *chain)
{
	return chain->rmap_hlist_len == STABLE_NODE_CHAIN;
}

static __always_inline bool is_stable_node_dup(struct ksm_stable_node *dup)
{
	return dup->head == STABLE_NODE_DUP_HEAD;
}

static inline void stable_node_chain_add_dup(struct ksm_stable_node *dup,
					     struct ksm_stable_node *chain)
{
	VM_BUG_ON(is_stable_node_dup(dup));
	dup->head = STABLE_NODE_DUP_HEAD;
	VM_BUG_ON(!is_stable_node_chain(chain));
	hlist_add_head(&dup->hlist_dup, &chain->hlist);
	ksm_stable_node_dups++;
}

static inline void __stable_node_dup_del(struct ksm_stable_node *dup)
{
	VM_BUG_ON(!is_stable_node_dup(dup));
	hlist_del(&dup->hlist_dup);
	ksm_stable_node_dups--;
}

static inline void stable_node_dup_del(struct ksm_stable_node *dup)
{
	VM_BUG_ON(is_stable_node_chain(dup));
	if (is_stable_node_dup(dup))
		__stable_node_dup_del(dup);
	else
		rb_erase(&dup->node, root_stable_tree + NUMA(dup->nid));
#ifdef CONFIG_DEBUG_VM
	dup->head = NULL;
#endif
}

static inline struct ksm_rmap_item *alloc_rmap_item(void)
{
	struct ksm_rmap_item *rmap_item;

	rmap_item = kmem_cache_zalloc(rmap_item_cache, GFP_KERNEL |
						__GFP_NORETRY | __GFP_NOWARN);
	if (rmap_item)
		ksm_rmap_items++;
	return rmap_item;
}

static inline void free_rmap_item(struct ksm_rmap_item *rmap_item)
{
	ksm_rmap_items--;
	rmap_item->mm->ksm_rmap_items--;
	rmap_item->mm = NULL;	 
	kmem_cache_free(rmap_item_cache, rmap_item);
}

static inline struct ksm_stable_node *alloc_stable_node(void)
{
	 
	return kmem_cache_alloc(stable_node_cache, GFP_KERNEL | __GFP_HIGH);
}

static inline void free_stable_node(struct ksm_stable_node *stable_node)
{
	VM_BUG_ON(stable_node->rmap_hlist_len &&
		  !is_stable_node_chain(stable_node));
	kmem_cache_free(stable_node_cache, stable_node);
}

 
static inline bool ksm_test_exit(struct mm_struct *mm)
{
	return atomic_read(&mm->mm_users) == 0;
}

static int break_ksm_pmd_entry(pmd_t *pmd, unsigned long addr, unsigned long next,
			struct mm_walk *walk)
{
	struct page *page = NULL;
	spinlock_t *ptl;
	pte_t *pte;
	pte_t ptent;
	int ret;

	pte = pte_offset_map_lock(walk->mm, pmd, addr, &ptl);
	if (!pte)
		return 0;
	ptent = ptep_get(pte);
	if (pte_present(ptent)) {
		page = vm_normal_page(walk->vma, addr, ptent);
	} else if (!pte_none(ptent)) {
		swp_entry_t entry = pte_to_swp_entry(ptent);

		 
		if (is_migration_entry(entry))
			page = pfn_swap_entry_to_page(entry);
	}
	 
	ret = (page && PageKsm(page)) || is_ksm_zero_pte(*pte);
	pte_unmap_unlock(pte, ptl);
	return ret;
}

static const struct mm_walk_ops break_ksm_ops = {
	.pmd_entry = break_ksm_pmd_entry,
	.walk_lock = PGWALK_RDLOCK,
};

static const struct mm_walk_ops break_ksm_lock_vma_ops = {
	.pmd_entry = break_ksm_pmd_entry,
	.walk_lock = PGWALK_WRLOCK,
};

 
static int break_ksm(struct vm_area_struct *vma, unsigned long addr, bool lock_vma)
{
	vm_fault_t ret = 0;
	const struct mm_walk_ops *ops = lock_vma ?
				&break_ksm_lock_vma_ops : &break_ksm_ops;

	do {
		int ksm_page;

		cond_resched();
		ksm_page = walk_page_range_vma(vma, addr, addr + 1, ops, NULL);
		if (WARN_ON_ONCE(ksm_page < 0))
			return ksm_page;
		if (!ksm_page)
			return 0;
		ret = handle_mm_fault(vma, addr,
				      FAULT_FLAG_UNSHARE | FAULT_FLAG_REMOTE,
				      NULL);
	} while (!(ret & (VM_FAULT_SIGBUS | VM_FAULT_SIGSEGV | VM_FAULT_OOM)));
	 
	return (ret & VM_FAULT_OOM) ? -ENOMEM : 0;
}

static bool vma_ksm_compatible(struct vm_area_struct *vma)
{
	if (vma->vm_flags & (VM_SHARED  | VM_MAYSHARE   | VM_PFNMAP  |
			     VM_IO      | VM_DONTEXPAND | VM_HUGETLB |
			     VM_MIXEDMAP))
		return false;		 

	if (vma_is_dax(vma))
		return false;

#ifdef VM_SAO
	if (vma->vm_flags & VM_SAO)
		return false;
#endif
#ifdef VM_SPARC_ADI
	if (vma->vm_flags & VM_SPARC_ADI)
		return false;
#endif

	return true;
}

static struct vm_area_struct *find_mergeable_vma(struct mm_struct *mm,
		unsigned long addr)
{
	struct vm_area_struct *vma;
	if (ksm_test_exit(mm))
		return NULL;
	vma = vma_lookup(mm, addr);
	if (!vma || !(vma->vm_flags & VM_MERGEABLE) || !vma->anon_vma)
		return NULL;
	return vma;
}

static void break_cow(struct ksm_rmap_item *rmap_item)
{
	struct mm_struct *mm = rmap_item->mm;
	unsigned long addr = rmap_item->address;
	struct vm_area_struct *vma;

	 
	put_anon_vma(rmap_item->anon_vma);

	mmap_read_lock(mm);
	vma = find_mergeable_vma(mm, addr);
	if (vma)
		break_ksm(vma, addr, false);
	mmap_read_unlock(mm);
}

static struct page *get_mergeable_page(struct ksm_rmap_item *rmap_item)
{
	struct mm_struct *mm = rmap_item->mm;
	unsigned long addr = rmap_item->address;
	struct vm_area_struct *vma;
	struct page *page;

	mmap_read_lock(mm);
	vma = find_mergeable_vma(mm, addr);
	if (!vma)
		goto out;

	page = follow_page(vma, addr, FOLL_GET);
	if (IS_ERR_OR_NULL(page))
		goto out;
	if (is_zone_device_page(page))
		goto out_putpage;
	if (PageAnon(page)) {
		flush_anon_page(vma, page, addr);
		flush_dcache_page(page);
	} else {
out_putpage:
		put_page(page);
out:
		page = NULL;
	}
	mmap_read_unlock(mm);
	return page;
}

 
static inline int get_kpfn_nid(unsigned long kpfn)
{
	return ksm_merge_across_nodes ? 0 : NUMA(pfn_to_nid(kpfn));
}

static struct ksm_stable_node *alloc_stable_node_chain(struct ksm_stable_node *dup,
						   struct rb_root *root)
{
	struct ksm_stable_node *chain = alloc_stable_node();
	VM_BUG_ON(is_stable_node_chain(dup));
	if (likely(chain)) {
		INIT_HLIST_HEAD(&chain->hlist);
		chain->chain_prune_time = jiffies;
		chain->rmap_hlist_len = STABLE_NODE_CHAIN;
#if defined (CONFIG_DEBUG_VM) && defined(CONFIG_NUMA)
		chain->nid = NUMA_NO_NODE;  
#endif
		ksm_stable_node_chains++;

		 
		rb_replace_node(&dup->node, &chain->node, root);

		 
		stable_node_chain_add_dup(dup, chain);
	}
	return chain;
}

static inline void free_stable_node_chain(struct ksm_stable_node *chain,
					  struct rb_root *root)
{
	rb_erase(&chain->node, root);
	free_stable_node(chain);
	ksm_stable_node_chains--;
}

static void remove_node_from_stable_tree(struct ksm_stable_node *stable_node)
{
	struct ksm_rmap_item *rmap_item;

	 
	BUG_ON(stable_node->rmap_hlist_len < 0);

	hlist_for_each_entry(rmap_item, &stable_node->hlist, hlist) {
		if (rmap_item->hlist.next) {
			ksm_pages_sharing--;
			trace_ksm_remove_rmap_item(stable_node->kpfn, rmap_item, rmap_item->mm);
		} else {
			ksm_pages_shared--;
		}

		rmap_item->mm->ksm_merging_pages--;

		VM_BUG_ON(stable_node->rmap_hlist_len <= 0);
		stable_node->rmap_hlist_len--;
		put_anon_vma(rmap_item->anon_vma);
		rmap_item->address &= PAGE_MASK;
		cond_resched();
	}

	 
	BUILD_BUG_ON(STABLE_NODE_DUP_HEAD <= &migrate_nodes);
	BUILD_BUG_ON(STABLE_NODE_DUP_HEAD >= &migrate_nodes + 1);

	trace_ksm_remove_ksm_page(stable_node->kpfn);
	if (stable_node->head == &migrate_nodes)
		list_del(&stable_node->list);
	else
		stable_node_dup_del(stable_node);
	free_stable_node(stable_node);
}

enum get_ksm_page_flags {
	GET_KSM_PAGE_NOLOCK,
	GET_KSM_PAGE_LOCK,
	GET_KSM_PAGE_TRYLOCK
};

 
static struct page *get_ksm_page(struct ksm_stable_node *stable_node,
				 enum get_ksm_page_flags flags)
{
	struct page *page;
	void *expected_mapping;
	unsigned long kpfn;

	expected_mapping = (void *)((unsigned long)stable_node |
					PAGE_MAPPING_KSM);
again:
	kpfn = READ_ONCE(stable_node->kpfn);  
	page = pfn_to_page(kpfn);
	if (READ_ONCE(page->mapping) != expected_mapping)
		goto stale;

	 
	while (!get_page_unless_zero(page)) {
		 
		if (!PageSwapCache(page))
			goto stale;
		cpu_relax();
	}

	if (READ_ONCE(page->mapping) != expected_mapping) {
		put_page(page);
		goto stale;
	}

	if (flags == GET_KSM_PAGE_TRYLOCK) {
		if (!trylock_page(page)) {
			put_page(page);
			return ERR_PTR(-EBUSY);
		}
	} else if (flags == GET_KSM_PAGE_LOCK)
		lock_page(page);

	if (flags != GET_KSM_PAGE_NOLOCK) {
		if (READ_ONCE(page->mapping) != expected_mapping) {
			unlock_page(page);
			put_page(page);
			goto stale;
		}
	}
	return page;

stale:
	 
	smp_rmb();
	if (READ_ONCE(stable_node->kpfn) != kpfn)
		goto again;
	remove_node_from_stable_tree(stable_node);
	return NULL;
}

 
static void remove_rmap_item_from_tree(struct ksm_rmap_item *rmap_item)
{
	if (rmap_item->address & STABLE_FLAG) {
		struct ksm_stable_node *stable_node;
		struct page *page;

		stable_node = rmap_item->head;
		page = get_ksm_page(stable_node, GET_KSM_PAGE_LOCK);
		if (!page)
			goto out;

		hlist_del(&rmap_item->hlist);
		unlock_page(page);
		put_page(page);

		if (!hlist_empty(&stable_node->hlist))
			ksm_pages_sharing--;
		else
			ksm_pages_shared--;

		rmap_item->mm->ksm_merging_pages--;

		VM_BUG_ON(stable_node->rmap_hlist_len <= 0);
		stable_node->rmap_hlist_len--;

		put_anon_vma(rmap_item->anon_vma);
		rmap_item->head = NULL;
		rmap_item->address &= PAGE_MASK;

	} else if (rmap_item->address & UNSTABLE_FLAG) {
		unsigned char age;
		 
		age = (unsigned char)(ksm_scan.seqnr - rmap_item->address);
		BUG_ON(age > 1);
		if (!age)
			rb_erase(&rmap_item->node,
				 root_unstable_tree + NUMA(rmap_item->nid));
		ksm_pages_unshared--;
		rmap_item->address &= PAGE_MASK;
	}
out:
	cond_resched();		 
}

static void remove_trailing_rmap_items(struct ksm_rmap_item **rmap_list)
{
	while (*rmap_list) {
		struct ksm_rmap_item *rmap_item = *rmap_list;
		*rmap_list = rmap_item->rmap_list;
		remove_rmap_item_from_tree(rmap_item);
		free_rmap_item(rmap_item);
	}
}

 
static int unmerge_ksm_pages(struct vm_area_struct *vma,
			     unsigned long start, unsigned long end, bool lock_vma)
{
	unsigned long addr;
	int err = 0;

	for (addr = start; addr < end && !err; addr += PAGE_SIZE) {
		if (ksm_test_exit(vma->vm_mm))
			break;
		if (signal_pending(current))
			err = -ERESTARTSYS;
		else
			err = break_ksm(vma, addr, lock_vma);
	}
	return err;
}

static inline struct ksm_stable_node *folio_stable_node(struct folio *folio)
{
	return folio_test_ksm(folio) ? folio_raw_mapping(folio) : NULL;
}

static inline struct ksm_stable_node *page_stable_node(struct page *page)
{
	return folio_stable_node(page_folio(page));
}

static inline void set_page_stable_node(struct page *page,
					struct ksm_stable_node *stable_node)
{
	VM_BUG_ON_PAGE(PageAnon(page) && PageAnonExclusive(page), page);
	page->mapping = (void *)((unsigned long)stable_node | PAGE_MAPPING_KSM);
}

#ifdef CONFIG_SYSFS
 
static int remove_stable_node(struct ksm_stable_node *stable_node)
{
	struct page *page;
	int err;

	page = get_ksm_page(stable_node, GET_KSM_PAGE_LOCK);
	if (!page) {
		 
		return 0;
	}

	 
	err = -EBUSY;
	if (!page_mapped(page)) {
		 
		set_page_stable_node(page, NULL);
		remove_node_from_stable_tree(stable_node);
		err = 0;
	}

	unlock_page(page);
	put_page(page);
	return err;
}

static int remove_stable_node_chain(struct ksm_stable_node *stable_node,
				    struct rb_root *root)
{
	struct ksm_stable_node *dup;
	struct hlist_node *hlist_safe;

	if (!is_stable_node_chain(stable_node)) {
		VM_BUG_ON(is_stable_node_dup(stable_node));
		if (remove_stable_node(stable_node))
			return true;
		else
			return false;
	}

	hlist_for_each_entry_safe(dup, hlist_safe,
				  &stable_node->hlist, hlist_dup) {
		VM_BUG_ON(!is_stable_node_dup(dup));
		if (remove_stable_node(dup))
			return true;
	}
	BUG_ON(!hlist_empty(&stable_node->hlist));
	free_stable_node_chain(stable_node, root);
	return false;
}

static int remove_all_stable_nodes(void)
{
	struct ksm_stable_node *stable_node, *next;
	int nid;
	int err = 0;

	for (nid = 0; nid < ksm_nr_node_ids; nid++) {
		while (root_stable_tree[nid].rb_node) {
			stable_node = rb_entry(root_stable_tree[nid].rb_node,
						struct ksm_stable_node, node);
			if (remove_stable_node_chain(stable_node,
						     root_stable_tree + nid)) {
				err = -EBUSY;
				break;	 
			}
			cond_resched();
		}
	}
	list_for_each_entry_safe(stable_node, next, &migrate_nodes, list) {
		if (remove_stable_node(stable_node))
			err = -EBUSY;
		cond_resched();
	}
	return err;
}

static int unmerge_and_remove_all_rmap_items(void)
{
	struct ksm_mm_slot *mm_slot;
	struct mm_slot *slot;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	int err = 0;

	spin_lock(&ksm_mmlist_lock);
	slot = list_entry(ksm_mm_head.slot.mm_node.next,
			  struct mm_slot, mm_node);
	ksm_scan.mm_slot = mm_slot_entry(slot, struct ksm_mm_slot, slot);
	spin_unlock(&ksm_mmlist_lock);

	for (mm_slot = ksm_scan.mm_slot; mm_slot != &ksm_mm_head;
	     mm_slot = ksm_scan.mm_slot) {
		VMA_ITERATOR(vmi, mm_slot->slot.mm, 0);

		mm = mm_slot->slot.mm;
		mmap_read_lock(mm);

		 
		if (ksm_test_exit(mm))
			goto mm_exiting;

		for_each_vma(vmi, vma) {
			if (!(vma->vm_flags & VM_MERGEABLE) || !vma->anon_vma)
				continue;
			err = unmerge_ksm_pages(vma,
						vma->vm_start, vma->vm_end, false);
			if (err)
				goto error;
		}

mm_exiting:
		remove_trailing_rmap_items(&mm_slot->rmap_list);
		mmap_read_unlock(mm);

		spin_lock(&ksm_mmlist_lock);
		slot = list_entry(mm_slot->slot.mm_node.next,
				  struct mm_slot, mm_node);
		ksm_scan.mm_slot = mm_slot_entry(slot, struct ksm_mm_slot, slot);
		if (ksm_test_exit(mm)) {
			hash_del(&mm_slot->slot.hash);
			list_del(&mm_slot->slot.mm_node);
			spin_unlock(&ksm_mmlist_lock);

			mm_slot_free(mm_slot_cache, mm_slot);
			clear_bit(MMF_VM_MERGEABLE, &mm->flags);
			clear_bit(MMF_VM_MERGE_ANY, &mm->flags);
			mmdrop(mm);
		} else
			spin_unlock(&ksm_mmlist_lock);
	}

	 
	remove_all_stable_nodes();
	ksm_scan.seqnr = 0;
	return 0;

error:
	mmap_read_unlock(mm);
	spin_lock(&ksm_mmlist_lock);
	ksm_scan.mm_slot = &ksm_mm_head;
	spin_unlock(&ksm_mmlist_lock);
	return err;
}
#endif  

static u32 calc_checksum(struct page *page)
{
	u32 checksum;
	void *addr = kmap_atomic(page);
	checksum = xxhash(addr, PAGE_SIZE, 0);
	kunmap_atomic(addr);
	return checksum;
}

static int write_protect_page(struct vm_area_struct *vma, struct page *page,
			      pte_t *orig_pte)
{
	struct mm_struct *mm = vma->vm_mm;
	DEFINE_PAGE_VMA_WALK(pvmw, page, vma, 0, 0);
	int swapped;
	int err = -EFAULT;
	struct mmu_notifier_range range;
	bool anon_exclusive;
	pte_t entry;

	pvmw.address = page_address_in_vma(page, vma);
	if (pvmw.address == -EFAULT)
		goto out;

	BUG_ON(PageTransCompound(page));

	mmu_notifier_range_init(&range, MMU_NOTIFY_CLEAR, 0, mm, pvmw.address,
				pvmw.address + PAGE_SIZE);
	mmu_notifier_invalidate_range_start(&range);

	if (!page_vma_mapped_walk(&pvmw))
		goto out_mn;
	if (WARN_ONCE(!pvmw.pte, "Unexpected PMD mapping?"))
		goto out_unlock;

	anon_exclusive = PageAnonExclusive(page);
	entry = ptep_get(pvmw.pte);
	if (pte_write(entry) || pte_dirty(entry) ||
	    anon_exclusive || mm_tlb_flush_pending(mm)) {
		swapped = PageSwapCache(page);
		flush_cache_page(vma, pvmw.address, page_to_pfn(page));
		 
		entry = ptep_clear_flush(vma, pvmw.address, pvmw.pte);
		 
		if (page_mapcount(page) + 1 + swapped != page_count(page)) {
			set_pte_at(mm, pvmw.address, pvmw.pte, entry);
			goto out_unlock;
		}

		 
		if (anon_exclusive && page_try_share_anon_rmap(page)) {
			set_pte_at(mm, pvmw.address, pvmw.pte, entry);
			goto out_unlock;
		}

		if (pte_dirty(entry))
			set_page_dirty(page);
		entry = pte_mkclean(entry);

		if (pte_write(entry))
			entry = pte_wrprotect(entry);

		set_pte_at_notify(mm, pvmw.address, pvmw.pte, entry);
	}
	*orig_pte = entry;
	err = 0;

out_unlock:
	page_vma_mapped_walk_done(&pvmw);
out_mn:
	mmu_notifier_invalidate_range_end(&range);
out:
	return err;
}

 
static int replace_page(struct vm_area_struct *vma, struct page *page,
			struct page *kpage, pte_t orig_pte)
{
	struct mm_struct *mm = vma->vm_mm;
	struct folio *folio;
	pmd_t *pmd;
	pmd_t pmde;
	pte_t *ptep;
	pte_t newpte;
	spinlock_t *ptl;
	unsigned long addr;
	int err = -EFAULT;
	struct mmu_notifier_range range;

	addr = page_address_in_vma(page, vma);
	if (addr == -EFAULT)
		goto out;

	pmd = mm_find_pmd(mm, addr);
	if (!pmd)
		goto out;
	 
	pmde = pmdp_get_lockless(pmd);
	if (!pmd_present(pmde) || pmd_trans_huge(pmde))
		goto out;

	mmu_notifier_range_init(&range, MMU_NOTIFY_CLEAR, 0, mm, addr,
				addr + PAGE_SIZE);
	mmu_notifier_invalidate_range_start(&range);

	ptep = pte_offset_map_lock(mm, pmd, addr, &ptl);
	if (!ptep)
		goto out_mn;
	if (!pte_same(ptep_get(ptep), orig_pte)) {
		pte_unmap_unlock(ptep, ptl);
		goto out_mn;
	}
	VM_BUG_ON_PAGE(PageAnonExclusive(page), page);
	VM_BUG_ON_PAGE(PageAnon(kpage) && PageAnonExclusive(kpage), kpage);

	 
	if (!is_zero_pfn(page_to_pfn(kpage))) {
		get_page(kpage);
		page_add_anon_rmap(kpage, vma, addr, RMAP_NONE);
		newpte = mk_pte(kpage, vma->vm_page_prot);
	} else {
		 
		newpte = pte_mkdirty(pte_mkspecial(pfn_pte(page_to_pfn(kpage), vma->vm_page_prot)));
		ksm_zero_pages++;
		mm->ksm_zero_pages++;
		 
		dec_mm_counter(mm, MM_ANONPAGES);
	}

	flush_cache_page(vma, addr, pte_pfn(ptep_get(ptep)));
	 
	ptep_clear_flush(vma, addr, ptep);
	set_pte_at_notify(mm, addr, ptep, newpte);

	folio = page_folio(page);
	page_remove_rmap(page, vma, false);
	if (!folio_mapped(folio))
		folio_free_swap(folio);
	folio_put(folio);

	pte_unmap_unlock(ptep, ptl);
	err = 0;
out_mn:
	mmu_notifier_invalidate_range_end(&range);
out:
	return err;
}

 
static int try_to_merge_one_page(struct vm_area_struct *vma,
				 struct page *page, struct page *kpage)
{
	pte_t orig_pte = __pte(0);
	int err = -EFAULT;

	if (page == kpage)			 
		return 0;

	if (!PageAnon(page))
		goto out;

	 
	if (!trylock_page(page))
		goto out;

	if (PageTransCompound(page)) {
		if (split_huge_page(page))
			goto out_unlock;
	}

	 
	if (write_protect_page(vma, page, &orig_pte) == 0) {
		if (!kpage) {
			 
			set_page_stable_node(page, NULL);
			mark_page_accessed(page);
			 
			if (!PageDirty(page))
				SetPageDirty(page);
			err = 0;
		} else if (pages_identical(page, kpage))
			err = replace_page(vma, page, kpage, orig_pte);
	}

out_unlock:
	unlock_page(page);
out:
	return err;
}

 
static int try_to_merge_with_ksm_page(struct ksm_rmap_item *rmap_item,
				      struct page *page, struct page *kpage)
{
	struct mm_struct *mm = rmap_item->mm;
	struct vm_area_struct *vma;
	int err = -EFAULT;

	mmap_read_lock(mm);
	vma = find_mergeable_vma(mm, rmap_item->address);
	if (!vma)
		goto out;

	err = try_to_merge_one_page(vma, page, kpage);
	if (err)
		goto out;

	 
	remove_rmap_item_from_tree(rmap_item);

	 
	rmap_item->anon_vma = vma->anon_vma;
	get_anon_vma(vma->anon_vma);
out:
	mmap_read_unlock(mm);
	trace_ksm_merge_with_ksm_page(kpage, page_to_pfn(kpage ? kpage : page),
				rmap_item, mm, err);
	return err;
}

 
static struct page *try_to_merge_two_pages(struct ksm_rmap_item *rmap_item,
					   struct page *page,
					   struct ksm_rmap_item *tree_rmap_item,
					   struct page *tree_page)
{
	int err;

	err = try_to_merge_with_ksm_page(rmap_item, page, NULL);
	if (!err) {
		err = try_to_merge_with_ksm_page(tree_rmap_item,
							tree_page, page);
		 
		if (err)
			break_cow(rmap_item);
	}
	return err ? NULL : page;
}

static __always_inline
bool __is_page_sharing_candidate(struct ksm_stable_node *stable_node, int offset)
{
	VM_BUG_ON(stable_node->rmap_hlist_len < 0);
	 
	return stable_node->rmap_hlist_len &&
		stable_node->rmap_hlist_len + offset < ksm_max_page_sharing;
}

static __always_inline
bool is_page_sharing_candidate(struct ksm_stable_node *stable_node)
{
	return __is_page_sharing_candidate(stable_node, 0);
}

static struct page *stable_node_dup(struct ksm_stable_node **_stable_node_dup,
				    struct ksm_stable_node **_stable_node,
				    struct rb_root *root,
				    bool prune_stale_stable_nodes)
{
	struct ksm_stable_node *dup, *found = NULL, *stable_node = *_stable_node;
	struct hlist_node *hlist_safe;
	struct page *_tree_page, *tree_page = NULL;
	int nr = 0;
	int found_rmap_hlist_len;

	if (!prune_stale_stable_nodes ||
	    time_before(jiffies, stable_node->chain_prune_time +
			msecs_to_jiffies(
				ksm_stable_node_chains_prune_millisecs)))
		prune_stale_stable_nodes = false;
	else
		stable_node->chain_prune_time = jiffies;

	hlist_for_each_entry_safe(dup, hlist_safe,
				  &stable_node->hlist, hlist_dup) {
		cond_resched();
		 
		_tree_page = get_ksm_page(dup, GET_KSM_PAGE_NOLOCK);
		if (!_tree_page)
			continue;
		nr += 1;
		if (is_page_sharing_candidate(dup)) {
			if (!found ||
			    dup->rmap_hlist_len > found_rmap_hlist_len) {
				if (found)
					put_page(tree_page);
				found = dup;
				found_rmap_hlist_len = found->rmap_hlist_len;
				tree_page = _tree_page;

				 
				if (!prune_stale_stable_nodes)
					break;
				continue;
			}
		}
		put_page(_tree_page);
	}

	if (found) {
		 
		if (prune_stale_stable_nodes && nr == 1) {
			 
			BUG_ON(stable_node->hlist.first->next);

			 
			rb_replace_node(&stable_node->node, &found->node,
					root);
			free_stable_node(stable_node);
			ksm_stable_node_chains--;
			ksm_stable_node_dups--;
			 
			*_stable_node = found;
			 
			stable_node = NULL;
		} else if (stable_node->hlist.first != &found->hlist_dup &&
			   __is_page_sharing_candidate(found, 1)) {
			 
			hlist_del(&found->hlist_dup);
			hlist_add_head(&found->hlist_dup,
				       &stable_node->hlist);
		}
	}

	*_stable_node_dup = found;
	return tree_page;
}

static struct ksm_stable_node *stable_node_dup_any(struct ksm_stable_node *stable_node,
					       struct rb_root *root)
{
	if (!is_stable_node_chain(stable_node))
		return stable_node;
	if (hlist_empty(&stable_node->hlist)) {
		free_stable_node_chain(stable_node, root);
		return NULL;
	}
	return hlist_entry(stable_node->hlist.first,
			   typeof(*stable_node), hlist_dup);
}

 
static struct page *__stable_node_chain(struct ksm_stable_node **_stable_node_dup,
					struct ksm_stable_node **_stable_node,
					struct rb_root *root,
					bool prune_stale_stable_nodes)
{
	struct ksm_stable_node *stable_node = *_stable_node;
	if (!is_stable_node_chain(stable_node)) {
		if (is_page_sharing_candidate(stable_node)) {
			*_stable_node_dup = stable_node;
			return get_ksm_page(stable_node, GET_KSM_PAGE_NOLOCK);
		}
		 
		*_stable_node_dup = NULL;
		return NULL;
	}
	return stable_node_dup(_stable_node_dup, _stable_node, root,
			       prune_stale_stable_nodes);
}

static __always_inline struct page *chain_prune(struct ksm_stable_node **s_n_d,
						struct ksm_stable_node **s_n,
						struct rb_root *root)
{
	return __stable_node_chain(s_n_d, s_n, root, true);
}

static __always_inline struct page *chain(struct ksm_stable_node **s_n_d,
					  struct ksm_stable_node *s_n,
					  struct rb_root *root)
{
	struct ksm_stable_node *old_stable_node = s_n;
	struct page *tree_page;

	tree_page = __stable_node_chain(s_n_d, &s_n, root, false);
	 
	VM_BUG_ON(s_n != old_stable_node);
	return tree_page;
}

 
static struct page *stable_tree_search(struct page *page)
{
	int nid;
	struct rb_root *root;
	struct rb_node **new;
	struct rb_node *parent;
	struct ksm_stable_node *stable_node, *stable_node_dup, *stable_node_any;
	struct ksm_stable_node *page_node;

	page_node = page_stable_node(page);
	if (page_node && page_node->head != &migrate_nodes) {
		 
		get_page(page);
		return page;
	}

	nid = get_kpfn_nid(page_to_pfn(page));
	root = root_stable_tree + nid;
again:
	new = &root->rb_node;
	parent = NULL;

	while (*new) {
		struct page *tree_page;
		int ret;

		cond_resched();
		stable_node = rb_entry(*new, struct ksm_stable_node, node);
		stable_node_any = NULL;
		tree_page = chain_prune(&stable_node_dup, &stable_node,	root);
		 
		if (!stable_node_dup) {
			 
			stable_node_any = stable_node_dup_any(stable_node,
							      root);
			if (!stable_node_any) {
				 
				goto again;
			}
			 
			tree_page = get_ksm_page(stable_node_any,
						 GET_KSM_PAGE_NOLOCK);
		}
		VM_BUG_ON(!stable_node_dup ^ !!stable_node_any);
		if (!tree_page) {
			 
			goto again;
		}

		ret = memcmp_pages(page, tree_page);
		put_page(tree_page);

		parent = *new;
		if (ret < 0)
			new = &parent->rb_left;
		else if (ret > 0)
			new = &parent->rb_right;
		else {
			if (page_node) {
				VM_BUG_ON(page_node->head != &migrate_nodes);
				 
				if (page_mapcount(page) > 1)
					goto chain_append;
			}

			if (!stable_node_dup) {
				 
				return NULL;
			}

			 
			tree_page = get_ksm_page(stable_node_dup,
						 GET_KSM_PAGE_TRYLOCK);

			if (PTR_ERR(tree_page) == -EBUSY)
				return ERR_PTR(-EBUSY);

			if (unlikely(!tree_page))
				 
				goto again;
			unlock_page(tree_page);

			if (get_kpfn_nid(stable_node_dup->kpfn) !=
			    NUMA(stable_node_dup->nid)) {
				put_page(tree_page);
				goto replace;
			}
			return tree_page;
		}
	}

	if (!page_node)
		return NULL;

	list_del(&page_node->list);
	DO_NUMA(page_node->nid = nid);
	rb_link_node(&page_node->node, parent, new);
	rb_insert_color(&page_node->node, root);
out:
	if (is_page_sharing_candidate(page_node)) {
		get_page(page);
		return page;
	} else
		return NULL;

replace:
	 
	if (stable_node_dup == stable_node) {
		VM_BUG_ON(is_stable_node_chain(stable_node_dup));
		VM_BUG_ON(is_stable_node_dup(stable_node_dup));
		 
		if (page_node) {
			VM_BUG_ON(page_node->head != &migrate_nodes);
			list_del(&page_node->list);
			DO_NUMA(page_node->nid = nid);
			rb_replace_node(&stable_node_dup->node,
					&page_node->node,
					root);
			if (is_page_sharing_candidate(page_node))
				get_page(page);
			else
				page = NULL;
		} else {
			rb_erase(&stable_node_dup->node, root);
			page = NULL;
		}
	} else {
		VM_BUG_ON(!is_stable_node_chain(stable_node));
		__stable_node_dup_del(stable_node_dup);
		if (page_node) {
			VM_BUG_ON(page_node->head != &migrate_nodes);
			list_del(&page_node->list);
			DO_NUMA(page_node->nid = nid);
			stable_node_chain_add_dup(page_node, stable_node);
			if (is_page_sharing_candidate(page_node))
				get_page(page);
			else
				page = NULL;
		} else {
			page = NULL;
		}
	}
	stable_node_dup->head = &migrate_nodes;
	list_add(&stable_node_dup->list, stable_node_dup->head);
	return page;

chain_append:
	 
	if (!stable_node_dup)
		stable_node_dup = stable_node_any;
	 
	if (stable_node_dup == stable_node) {
		VM_BUG_ON(is_stable_node_dup(stable_node_dup));
		 
		stable_node = alloc_stable_node_chain(stable_node_dup,
						      root);
		if (!stable_node)
			return NULL;
	}
	 
	VM_BUG_ON(!is_stable_node_dup(stable_node_dup));
	VM_BUG_ON(page_node->head != &migrate_nodes);
	list_del(&page_node->list);
	DO_NUMA(page_node->nid = nid);
	stable_node_chain_add_dup(page_node, stable_node);
	goto out;
}

 
static struct ksm_stable_node *stable_tree_insert(struct page *kpage)
{
	int nid;
	unsigned long kpfn;
	struct rb_root *root;
	struct rb_node **new;
	struct rb_node *parent;
	struct ksm_stable_node *stable_node, *stable_node_dup, *stable_node_any;
	bool need_chain = false;

	kpfn = page_to_pfn(kpage);
	nid = get_kpfn_nid(kpfn);
	root = root_stable_tree + nid;
again:
	parent = NULL;
	new = &root->rb_node;

	while (*new) {
		struct page *tree_page;
		int ret;

		cond_resched();
		stable_node = rb_entry(*new, struct ksm_stable_node, node);
		stable_node_any = NULL;
		tree_page = chain(&stable_node_dup, stable_node, root);
		if (!stable_node_dup) {
			 
			stable_node_any = stable_node_dup_any(stable_node,
							      root);
			if (!stable_node_any) {
				 
				goto again;
			}
			 
			tree_page = get_ksm_page(stable_node_any,
						 GET_KSM_PAGE_NOLOCK);
		}
		VM_BUG_ON(!stable_node_dup ^ !!stable_node_any);
		if (!tree_page) {
			 
			goto again;
		}

		ret = memcmp_pages(kpage, tree_page);
		put_page(tree_page);

		parent = *new;
		if (ret < 0)
			new = &parent->rb_left;
		else if (ret > 0)
			new = &parent->rb_right;
		else {
			need_chain = true;
			break;
		}
	}

	stable_node_dup = alloc_stable_node();
	if (!stable_node_dup)
		return NULL;

	INIT_HLIST_HEAD(&stable_node_dup->hlist);
	stable_node_dup->kpfn = kpfn;
	set_page_stable_node(kpage, stable_node_dup);
	stable_node_dup->rmap_hlist_len = 0;
	DO_NUMA(stable_node_dup->nid = nid);
	if (!need_chain) {
		rb_link_node(&stable_node_dup->node, parent, new);
		rb_insert_color(&stable_node_dup->node, root);
	} else {
		if (!is_stable_node_chain(stable_node)) {
			struct ksm_stable_node *orig = stable_node;
			 
			stable_node = alloc_stable_node_chain(orig, root);
			if (!stable_node) {
				free_stable_node(stable_node_dup);
				return NULL;
			}
		}
		stable_node_chain_add_dup(stable_node_dup, stable_node);
	}

	return stable_node_dup;
}

 
static
struct ksm_rmap_item *unstable_tree_search_insert(struct ksm_rmap_item *rmap_item,
					      struct page *page,
					      struct page **tree_pagep)
{
	struct rb_node **new;
	struct rb_root *root;
	struct rb_node *parent = NULL;
	int nid;

	nid = get_kpfn_nid(page_to_pfn(page));
	root = root_unstable_tree + nid;
	new = &root->rb_node;

	while (*new) {
		struct ksm_rmap_item *tree_rmap_item;
		struct page *tree_page;
		int ret;

		cond_resched();
		tree_rmap_item = rb_entry(*new, struct ksm_rmap_item, node);
		tree_page = get_mergeable_page(tree_rmap_item);
		if (!tree_page)
			return NULL;

		 
		if (page == tree_page) {
			put_page(tree_page);
			return NULL;
		}

		ret = memcmp_pages(page, tree_page);

		parent = *new;
		if (ret < 0) {
			put_page(tree_page);
			new = &parent->rb_left;
		} else if (ret > 0) {
			put_page(tree_page);
			new = &parent->rb_right;
		} else if (!ksm_merge_across_nodes &&
			   page_to_nid(tree_page) != nid) {
			 
			put_page(tree_page);
			return NULL;
		} else {
			*tree_pagep = tree_page;
			return tree_rmap_item;
		}
	}

	rmap_item->address |= UNSTABLE_FLAG;
	rmap_item->address |= (ksm_scan.seqnr & SEQNR_MASK);
	DO_NUMA(rmap_item->nid = nid);
	rb_link_node(&rmap_item->node, parent, new);
	rb_insert_color(&rmap_item->node, root);

	ksm_pages_unshared++;
	return NULL;
}

 
static void stable_tree_append(struct ksm_rmap_item *rmap_item,
			       struct ksm_stable_node *stable_node,
			       bool max_page_sharing_bypass)
{
	 
	BUG_ON(stable_node->rmap_hlist_len < 0);

	stable_node->rmap_hlist_len++;
	if (!max_page_sharing_bypass)
		 
		WARN_ON_ONCE(stable_node->rmap_hlist_len >
			     ksm_max_page_sharing);

	rmap_item->head = stable_node;
	rmap_item->address |= STABLE_FLAG;
	hlist_add_head(&rmap_item->hlist, &stable_node->hlist);

	if (rmap_item->hlist.next)
		ksm_pages_sharing++;
	else
		ksm_pages_shared++;

	rmap_item->mm->ksm_merging_pages++;
}

 
static void cmp_and_merge_page(struct page *page, struct ksm_rmap_item *rmap_item)
{
	struct mm_struct *mm = rmap_item->mm;
	struct ksm_rmap_item *tree_rmap_item;
	struct page *tree_page = NULL;
	struct ksm_stable_node *stable_node;
	struct page *kpage;
	unsigned int checksum;
	int err;
	bool max_page_sharing_bypass = false;

	stable_node = page_stable_node(page);
	if (stable_node) {
		if (stable_node->head != &migrate_nodes &&
		    get_kpfn_nid(READ_ONCE(stable_node->kpfn)) !=
		    NUMA(stable_node->nid)) {
			stable_node_dup_del(stable_node);
			stable_node->head = &migrate_nodes;
			list_add(&stable_node->list, stable_node->head);
		}
		if (stable_node->head != &migrate_nodes &&
		    rmap_item->head == stable_node)
			return;
		 
		if (!is_page_sharing_candidate(stable_node))
			max_page_sharing_bypass = true;
	}

	 
	kpage = stable_tree_search(page);
	if (kpage == page && rmap_item->head == stable_node) {
		put_page(kpage);
		return;
	}

	remove_rmap_item_from_tree(rmap_item);

	if (kpage) {
		if (PTR_ERR(kpage) == -EBUSY)
			return;

		err = try_to_merge_with_ksm_page(rmap_item, page, kpage);
		if (!err) {
			 
			lock_page(kpage);
			stable_tree_append(rmap_item, page_stable_node(kpage),
					   max_page_sharing_bypass);
			unlock_page(kpage);
		}
		put_page(kpage);
		return;
	}

	 
	checksum = calc_checksum(page);
	if (rmap_item->oldchecksum != checksum) {
		rmap_item->oldchecksum = checksum;
		return;
	}

	 
	if (ksm_use_zero_pages && (checksum == zero_checksum)) {
		struct vm_area_struct *vma;

		mmap_read_lock(mm);
		vma = find_mergeable_vma(mm, rmap_item->address);
		if (vma) {
			err = try_to_merge_one_page(vma, page,
					ZERO_PAGE(rmap_item->address));
			trace_ksm_merge_one_page(
				page_to_pfn(ZERO_PAGE(rmap_item->address)),
				rmap_item, mm, err);
		} else {
			 
			err = 0;
		}
		mmap_read_unlock(mm);
		 
		if (!err)
			return;
	}
	tree_rmap_item =
		unstable_tree_search_insert(rmap_item, page, &tree_page);
	if (tree_rmap_item) {
		bool split;

		kpage = try_to_merge_two_pages(rmap_item, page,
						tree_rmap_item, tree_page);
		 
		split = PageTransCompound(page)
			&& compound_head(page) == compound_head(tree_page);
		put_page(tree_page);
		if (kpage) {
			 
			lock_page(kpage);
			stable_node = stable_tree_insert(kpage);
			if (stable_node) {
				stable_tree_append(tree_rmap_item, stable_node,
						   false);
				stable_tree_append(rmap_item, stable_node,
						   false);
			}
			unlock_page(kpage);

			 
			if (!stable_node) {
				break_cow(tree_rmap_item);
				break_cow(rmap_item);
			}
		} else if (split) {
			 
			if (!trylock_page(page))
				return;
			split_huge_page(page);
			unlock_page(page);
		}
	}
}

static struct ksm_rmap_item *get_next_rmap_item(struct ksm_mm_slot *mm_slot,
					    struct ksm_rmap_item **rmap_list,
					    unsigned long addr)
{
	struct ksm_rmap_item *rmap_item;

	while (*rmap_list) {
		rmap_item = *rmap_list;
		if ((rmap_item->address & PAGE_MASK) == addr)
			return rmap_item;
		if (rmap_item->address > addr)
			break;
		*rmap_list = rmap_item->rmap_list;
		remove_rmap_item_from_tree(rmap_item);
		free_rmap_item(rmap_item);
	}

	rmap_item = alloc_rmap_item();
	if (rmap_item) {
		 
		rmap_item->mm = mm_slot->slot.mm;
		rmap_item->mm->ksm_rmap_items++;
		rmap_item->address = addr;
		rmap_item->rmap_list = *rmap_list;
		*rmap_list = rmap_item;
	}
	return rmap_item;
}

static struct ksm_rmap_item *scan_get_next_rmap_item(struct page **page)
{
	struct mm_struct *mm;
	struct ksm_mm_slot *mm_slot;
	struct mm_slot *slot;
	struct vm_area_struct *vma;
	struct ksm_rmap_item *rmap_item;
	struct vma_iterator vmi;
	int nid;

	if (list_empty(&ksm_mm_head.slot.mm_node))
		return NULL;

	mm_slot = ksm_scan.mm_slot;
	if (mm_slot == &ksm_mm_head) {
		trace_ksm_start_scan(ksm_scan.seqnr, ksm_rmap_items);

		 
		lru_add_drain_all();

		 
		if (!ksm_merge_across_nodes) {
			struct ksm_stable_node *stable_node, *next;
			struct page *page;

			list_for_each_entry_safe(stable_node, next,
						 &migrate_nodes, list) {
				page = get_ksm_page(stable_node,
						    GET_KSM_PAGE_NOLOCK);
				if (page)
					put_page(page);
				cond_resched();
			}
		}

		for (nid = 0; nid < ksm_nr_node_ids; nid++)
			root_unstable_tree[nid] = RB_ROOT;

		spin_lock(&ksm_mmlist_lock);
		slot = list_entry(mm_slot->slot.mm_node.next,
				  struct mm_slot, mm_node);
		mm_slot = mm_slot_entry(slot, struct ksm_mm_slot, slot);
		ksm_scan.mm_slot = mm_slot;
		spin_unlock(&ksm_mmlist_lock);
		 
		if (mm_slot == &ksm_mm_head)
			return NULL;
next_mm:
		ksm_scan.address = 0;
		ksm_scan.rmap_list = &mm_slot->rmap_list;
	}

	slot = &mm_slot->slot;
	mm = slot->mm;
	vma_iter_init(&vmi, mm, ksm_scan.address);

	mmap_read_lock(mm);
	if (ksm_test_exit(mm))
		goto no_vmas;

	for_each_vma(vmi, vma) {
		if (!(vma->vm_flags & VM_MERGEABLE))
			continue;
		if (ksm_scan.address < vma->vm_start)
			ksm_scan.address = vma->vm_start;
		if (!vma->anon_vma)
			ksm_scan.address = vma->vm_end;

		while (ksm_scan.address < vma->vm_end) {
			if (ksm_test_exit(mm))
				break;
			*page = follow_page(vma, ksm_scan.address, FOLL_GET);
			if (IS_ERR_OR_NULL(*page)) {
				ksm_scan.address += PAGE_SIZE;
				cond_resched();
				continue;
			}
			if (is_zone_device_page(*page))
				goto next_page;
			if (PageAnon(*page)) {
				flush_anon_page(vma, *page, ksm_scan.address);
				flush_dcache_page(*page);
				rmap_item = get_next_rmap_item(mm_slot,
					ksm_scan.rmap_list, ksm_scan.address);
				if (rmap_item) {
					ksm_scan.rmap_list =
							&rmap_item->rmap_list;
					ksm_scan.address += PAGE_SIZE;
				} else
					put_page(*page);
				mmap_read_unlock(mm);
				return rmap_item;
			}
next_page:
			put_page(*page);
			ksm_scan.address += PAGE_SIZE;
			cond_resched();
		}
	}

	if (ksm_test_exit(mm)) {
no_vmas:
		ksm_scan.address = 0;
		ksm_scan.rmap_list = &mm_slot->rmap_list;
	}
	 
	remove_trailing_rmap_items(ksm_scan.rmap_list);

	spin_lock(&ksm_mmlist_lock);
	slot = list_entry(mm_slot->slot.mm_node.next,
			  struct mm_slot, mm_node);
	ksm_scan.mm_slot = mm_slot_entry(slot, struct ksm_mm_slot, slot);
	if (ksm_scan.address == 0) {
		 
		hash_del(&mm_slot->slot.hash);
		list_del(&mm_slot->slot.mm_node);
		spin_unlock(&ksm_mmlist_lock);

		mm_slot_free(mm_slot_cache, mm_slot);
		clear_bit(MMF_VM_MERGEABLE, &mm->flags);
		clear_bit(MMF_VM_MERGE_ANY, &mm->flags);
		mmap_read_unlock(mm);
		mmdrop(mm);
	} else {
		mmap_read_unlock(mm);
		 
		spin_unlock(&ksm_mmlist_lock);
	}

	 
	mm_slot = ksm_scan.mm_slot;
	if (mm_slot != &ksm_mm_head)
		goto next_mm;

	trace_ksm_stop_scan(ksm_scan.seqnr, ksm_rmap_items);
	ksm_scan.seqnr++;
	return NULL;
}

 
static void ksm_do_scan(unsigned int scan_npages)
{
	struct ksm_rmap_item *rmap_item;
	struct page *page;
	unsigned int npages = scan_npages;

	while (npages-- && likely(!freezing(current))) {
		cond_resched();
		rmap_item = scan_get_next_rmap_item(&page);
		if (!rmap_item)
			return;
		cmp_and_merge_page(page, rmap_item);
		put_page(page);
	}

	ksm_pages_scanned += scan_npages - npages;
}

static int ksmd_should_run(void)
{
	return (ksm_run & KSM_RUN_MERGE) && !list_empty(&ksm_mm_head.slot.mm_node);
}

static int ksm_scan_thread(void *nothing)
{
	unsigned int sleep_ms;

	set_freezable();
	set_user_nice(current, 5);

	while (!kthread_should_stop()) {
		mutex_lock(&ksm_thread_mutex);
		wait_while_offlining();
		if (ksmd_should_run())
			ksm_do_scan(ksm_thread_pages_to_scan);
		mutex_unlock(&ksm_thread_mutex);

		try_to_freeze();

		if (ksmd_should_run()) {
			sleep_ms = READ_ONCE(ksm_thread_sleep_millisecs);
			wait_event_interruptible_timeout(ksm_iter_wait,
				sleep_ms != READ_ONCE(ksm_thread_sleep_millisecs),
				msecs_to_jiffies(sleep_ms));
		} else {
			wait_event_freezable(ksm_thread_wait,
				ksmd_should_run() || kthread_should_stop());
		}
	}
	return 0;
}

static void __ksm_add_vma(struct vm_area_struct *vma)
{
	unsigned long vm_flags = vma->vm_flags;

	if (vm_flags & VM_MERGEABLE)
		return;

	if (vma_ksm_compatible(vma))
		vm_flags_set(vma, VM_MERGEABLE);
}

static int __ksm_del_vma(struct vm_area_struct *vma)
{
	int err;

	if (!(vma->vm_flags & VM_MERGEABLE))
		return 0;

	if (vma->anon_vma) {
		err = unmerge_ksm_pages(vma, vma->vm_start, vma->vm_end, true);
		if (err)
			return err;
	}

	vm_flags_clear(vma, VM_MERGEABLE);
	return 0;
}
 
void ksm_add_vma(struct vm_area_struct *vma)
{
	struct mm_struct *mm = vma->vm_mm;

	if (test_bit(MMF_VM_MERGE_ANY, &mm->flags))
		__ksm_add_vma(vma);
}

static void ksm_add_vmas(struct mm_struct *mm)
{
	struct vm_area_struct *vma;

	VMA_ITERATOR(vmi, mm, 0);
	for_each_vma(vmi, vma)
		__ksm_add_vma(vma);
}

static int ksm_del_vmas(struct mm_struct *mm)
{
	struct vm_area_struct *vma;
	int err;

	VMA_ITERATOR(vmi, mm, 0);
	for_each_vma(vmi, vma) {
		err = __ksm_del_vma(vma);
		if (err)
			return err;
	}
	return 0;
}

 
int ksm_enable_merge_any(struct mm_struct *mm)
{
	int err;

	if (test_bit(MMF_VM_MERGE_ANY, &mm->flags))
		return 0;

	if (!test_bit(MMF_VM_MERGEABLE, &mm->flags)) {
		err = __ksm_enter(mm);
		if (err)
			return err;
	}

	set_bit(MMF_VM_MERGE_ANY, &mm->flags);
	ksm_add_vmas(mm);

	return 0;
}

 
int ksm_disable_merge_any(struct mm_struct *mm)
{
	int err;

	if (!test_bit(MMF_VM_MERGE_ANY, &mm->flags))
		return 0;

	err = ksm_del_vmas(mm);
	if (err) {
		ksm_add_vmas(mm);
		return err;
	}

	clear_bit(MMF_VM_MERGE_ANY, &mm->flags);
	return 0;
}

int ksm_disable(struct mm_struct *mm)
{
	mmap_assert_write_locked(mm);

	if (!test_bit(MMF_VM_MERGEABLE, &mm->flags))
		return 0;
	if (test_bit(MMF_VM_MERGE_ANY, &mm->flags))
		return ksm_disable_merge_any(mm);
	return ksm_del_vmas(mm);
}

int ksm_madvise(struct vm_area_struct *vma, unsigned long start,
		unsigned long end, int advice, unsigned long *vm_flags)
{
	struct mm_struct *mm = vma->vm_mm;
	int err;

	switch (advice) {
	case MADV_MERGEABLE:
		if (vma->vm_flags & VM_MERGEABLE)
			return 0;
		if (!vma_ksm_compatible(vma))
			return 0;

		if (!test_bit(MMF_VM_MERGEABLE, &mm->flags)) {
			err = __ksm_enter(mm);
			if (err)
				return err;
		}

		*vm_flags |= VM_MERGEABLE;
		break;

	case MADV_UNMERGEABLE:
		if (!(*vm_flags & VM_MERGEABLE))
			return 0;		 

		if (vma->anon_vma) {
			err = unmerge_ksm_pages(vma, start, end, true);
			if (err)
				return err;
		}

		*vm_flags &= ~VM_MERGEABLE;
		break;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(ksm_madvise);

int __ksm_enter(struct mm_struct *mm)
{
	struct ksm_mm_slot *mm_slot;
	struct mm_slot *slot;
	int needs_wakeup;

	mm_slot = mm_slot_alloc(mm_slot_cache);
	if (!mm_slot)
		return -ENOMEM;

	slot = &mm_slot->slot;

	 
	needs_wakeup = list_empty(&ksm_mm_head.slot.mm_node);

	spin_lock(&ksm_mmlist_lock);
	mm_slot_insert(mm_slots_hash, mm, slot);
	 
	if (ksm_run & KSM_RUN_UNMERGE)
		list_add_tail(&slot->mm_node, &ksm_mm_head.slot.mm_node);
	else
		list_add_tail(&slot->mm_node, &ksm_scan.mm_slot->slot.mm_node);
	spin_unlock(&ksm_mmlist_lock);

	set_bit(MMF_VM_MERGEABLE, &mm->flags);
	mmgrab(mm);

	if (needs_wakeup)
		wake_up_interruptible(&ksm_thread_wait);

	trace_ksm_enter(mm);
	return 0;
}

void __ksm_exit(struct mm_struct *mm)
{
	struct ksm_mm_slot *mm_slot;
	struct mm_slot *slot;
	int easy_to_free = 0;

	 

	spin_lock(&ksm_mmlist_lock);
	slot = mm_slot_lookup(mm_slots_hash, mm);
	mm_slot = mm_slot_entry(slot, struct ksm_mm_slot, slot);
	if (mm_slot && ksm_scan.mm_slot != mm_slot) {
		if (!mm_slot->rmap_list) {
			hash_del(&slot->hash);
			list_del(&slot->mm_node);
			easy_to_free = 1;
		} else {
			list_move(&slot->mm_node,
				  &ksm_scan.mm_slot->slot.mm_node);
		}
	}
	spin_unlock(&ksm_mmlist_lock);

	if (easy_to_free) {
		mm_slot_free(mm_slot_cache, mm_slot);
		clear_bit(MMF_VM_MERGE_ANY, &mm->flags);
		clear_bit(MMF_VM_MERGEABLE, &mm->flags);
		mmdrop(mm);
	} else if (mm_slot) {
		mmap_write_lock(mm);
		mmap_write_unlock(mm);
	}

	trace_ksm_exit(mm);
}

struct page *ksm_might_need_to_copy(struct page *page,
			struct vm_area_struct *vma, unsigned long address)
{
	struct folio *folio = page_folio(page);
	struct anon_vma *anon_vma = folio_anon_vma(folio);
	struct page *new_page;

	if (PageKsm(page)) {
		if (page_stable_node(page) &&
		    !(ksm_run & KSM_RUN_UNMERGE))
			return page;	 
	} else if (!anon_vma) {
		return page;		 
	} else if (page->index == linear_page_index(vma, address) &&
			anon_vma->root == vma->anon_vma->root) {
		return page;		 
	}
	if (PageHWPoison(page))
		return ERR_PTR(-EHWPOISON);
	if (!PageUptodate(page))
		return page;		 

	new_page = alloc_page_vma(GFP_HIGHUSER_MOVABLE, vma, address);
	if (new_page &&
	    mem_cgroup_charge(page_folio(new_page), vma->vm_mm, GFP_KERNEL)) {
		put_page(new_page);
		new_page = NULL;
	}
	if (new_page) {
		if (copy_mc_user_highpage(new_page, page, address, vma)) {
			put_page(new_page);
			memory_failure_queue(page_to_pfn(page), 0);
			return ERR_PTR(-EHWPOISON);
		}
		SetPageDirty(new_page);
		__SetPageUptodate(new_page);
		__SetPageLocked(new_page);
#ifdef CONFIG_SWAP
		count_vm_event(KSM_SWPIN_COPY);
#endif
	}

	return new_page;
}

void rmap_walk_ksm(struct folio *folio, struct rmap_walk_control *rwc)
{
	struct ksm_stable_node *stable_node;
	struct ksm_rmap_item *rmap_item;
	int search_new_forks = 0;

	VM_BUG_ON_FOLIO(!folio_test_ksm(folio), folio);

	 
	VM_BUG_ON_FOLIO(!folio_test_locked(folio), folio);

	stable_node = folio_stable_node(folio);
	if (!stable_node)
		return;
again:
	hlist_for_each_entry(rmap_item, &stable_node->hlist, hlist) {
		struct anon_vma *anon_vma = rmap_item->anon_vma;
		struct anon_vma_chain *vmac;
		struct vm_area_struct *vma;

		cond_resched();
		if (!anon_vma_trylock_read(anon_vma)) {
			if (rwc->try_lock) {
				rwc->contended = true;
				return;
			}
			anon_vma_lock_read(anon_vma);
		}
		anon_vma_interval_tree_foreach(vmac, &anon_vma->rb_root,
					       0, ULONG_MAX) {
			unsigned long addr;

			cond_resched();
			vma = vmac->vma;

			 
			addr = rmap_item->address & PAGE_MASK;

			if (addr < vma->vm_start || addr >= vma->vm_end)
				continue;
			 
			if ((rmap_item->mm == vma->vm_mm) == search_new_forks)
				continue;

			if (rwc->invalid_vma && rwc->invalid_vma(vma, rwc->arg))
				continue;

			if (!rwc->rmap_one(folio, vma, addr, rwc->arg)) {
				anon_vma_unlock_read(anon_vma);
				return;
			}
			if (rwc->done && rwc->done(folio)) {
				anon_vma_unlock_read(anon_vma);
				return;
			}
		}
		anon_vma_unlock_read(anon_vma);
	}
	if (!search_new_forks++)
		goto again;
}

#ifdef CONFIG_MEMORY_FAILURE
 
void collect_procs_ksm(struct page *page, struct list_head *to_kill,
		       int force_early)
{
	struct ksm_stable_node *stable_node;
	struct ksm_rmap_item *rmap_item;
	struct folio *folio = page_folio(page);
	struct vm_area_struct *vma;
	struct task_struct *tsk;

	stable_node = folio_stable_node(folio);
	if (!stable_node)
		return;
	hlist_for_each_entry(rmap_item, &stable_node->hlist, hlist) {
		struct anon_vma *av = rmap_item->anon_vma;

		anon_vma_lock_read(av);
		rcu_read_lock();
		for_each_process(tsk) {
			struct anon_vma_chain *vmac;
			unsigned long addr;
			struct task_struct *t =
				task_early_kill(tsk, force_early);
			if (!t)
				continue;
			anon_vma_interval_tree_foreach(vmac, &av->rb_root, 0,
						       ULONG_MAX)
			{
				vma = vmac->vma;
				if (vma->vm_mm == t->mm) {
					addr = rmap_item->address & PAGE_MASK;
					add_to_kill_ksm(t, page, vma, to_kill,
							addr);
				}
			}
		}
		rcu_read_unlock();
		anon_vma_unlock_read(av);
	}
}
#endif

#ifdef CONFIG_MIGRATION
void folio_migrate_ksm(struct folio *newfolio, struct folio *folio)
{
	struct ksm_stable_node *stable_node;

	VM_BUG_ON_FOLIO(!folio_test_locked(folio), folio);
	VM_BUG_ON_FOLIO(!folio_test_locked(newfolio), newfolio);
	VM_BUG_ON_FOLIO(newfolio->mapping != folio->mapping, newfolio);

	stable_node = folio_stable_node(folio);
	if (stable_node) {
		VM_BUG_ON_FOLIO(stable_node->kpfn != folio_pfn(folio), folio);
		stable_node->kpfn = folio_pfn(newfolio);
		 
		smp_wmb();
		set_page_stable_node(&folio->page, NULL);
	}
}
#endif  

#ifdef CONFIG_MEMORY_HOTREMOVE
static void wait_while_offlining(void)
{
	while (ksm_run & KSM_RUN_OFFLINE) {
		mutex_unlock(&ksm_thread_mutex);
		wait_on_bit(&ksm_run, ilog2(KSM_RUN_OFFLINE),
			    TASK_UNINTERRUPTIBLE);
		mutex_lock(&ksm_thread_mutex);
	}
}

static bool stable_node_dup_remove_range(struct ksm_stable_node *stable_node,
					 unsigned long start_pfn,
					 unsigned long end_pfn)
{
	if (stable_node->kpfn >= start_pfn &&
	    stable_node->kpfn < end_pfn) {
		 
		remove_node_from_stable_tree(stable_node);
		return true;
	}
	return false;
}

static bool stable_node_chain_remove_range(struct ksm_stable_node *stable_node,
					   unsigned long start_pfn,
					   unsigned long end_pfn,
					   struct rb_root *root)
{
	struct ksm_stable_node *dup;
	struct hlist_node *hlist_safe;

	if (!is_stable_node_chain(stable_node)) {
		VM_BUG_ON(is_stable_node_dup(stable_node));
		return stable_node_dup_remove_range(stable_node, start_pfn,
						    end_pfn);
	}

	hlist_for_each_entry_safe(dup, hlist_safe,
				  &stable_node->hlist, hlist_dup) {
		VM_BUG_ON(!is_stable_node_dup(dup));
		stable_node_dup_remove_range(dup, start_pfn, end_pfn);
	}
	if (hlist_empty(&stable_node->hlist)) {
		free_stable_node_chain(stable_node, root);
		return true;  
	} else
		return false;
}

static void ksm_check_stable_tree(unsigned long start_pfn,
				  unsigned long end_pfn)
{
	struct ksm_stable_node *stable_node, *next;
	struct rb_node *node;
	int nid;

	for (nid = 0; nid < ksm_nr_node_ids; nid++) {
		node = rb_first(root_stable_tree + nid);
		while (node) {
			stable_node = rb_entry(node, struct ksm_stable_node, node);
			if (stable_node_chain_remove_range(stable_node,
							   start_pfn, end_pfn,
							   root_stable_tree +
							   nid))
				node = rb_first(root_stable_tree + nid);
			else
				node = rb_next(node);
			cond_resched();
		}
	}
	list_for_each_entry_safe(stable_node, next, &migrate_nodes, list) {
		if (stable_node->kpfn >= start_pfn &&
		    stable_node->kpfn < end_pfn)
			remove_node_from_stable_tree(stable_node);
		cond_resched();
	}
}

static int ksm_memory_callback(struct notifier_block *self,
			       unsigned long action, void *arg)
{
	struct memory_notify *mn = arg;

	switch (action) {
	case MEM_GOING_OFFLINE:
		 
		mutex_lock(&ksm_thread_mutex);
		ksm_run |= KSM_RUN_OFFLINE;
		mutex_unlock(&ksm_thread_mutex);
		break;

	case MEM_OFFLINE:
		 
		ksm_check_stable_tree(mn->start_pfn,
				      mn->start_pfn + mn->nr_pages);
		fallthrough;
	case MEM_CANCEL_OFFLINE:
		mutex_lock(&ksm_thread_mutex);
		ksm_run &= ~KSM_RUN_OFFLINE;
		mutex_unlock(&ksm_thread_mutex);

		smp_mb();	 
		wake_up_bit(&ksm_run, ilog2(KSM_RUN_OFFLINE));
		break;
	}
	return NOTIFY_OK;
}
#else
static void wait_while_offlining(void)
{
}
#endif  

#ifdef CONFIG_PROC_FS
long ksm_process_profit(struct mm_struct *mm)
{
	return (long)(mm->ksm_merging_pages + mm->ksm_zero_pages) * PAGE_SIZE -
		mm->ksm_rmap_items * sizeof(struct ksm_rmap_item);
}
#endif  

#ifdef CONFIG_SYSFS
 

#define KSM_ATTR_RO(_name) \
	static struct kobj_attribute _name##_attr = __ATTR_RO(_name)
#define KSM_ATTR(_name) \
	static struct kobj_attribute _name##_attr = __ATTR_RW(_name)

static ssize_t sleep_millisecs_show(struct kobject *kobj,
				    struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%u\n", ksm_thread_sleep_millisecs);
}

static ssize_t sleep_millisecs_store(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     const char *buf, size_t count)
{
	unsigned int msecs;
	int err;

	err = kstrtouint(buf, 10, &msecs);
	if (err)
		return -EINVAL;

	ksm_thread_sleep_millisecs = msecs;
	wake_up_interruptible(&ksm_iter_wait);

	return count;
}
KSM_ATTR(sleep_millisecs);

static ssize_t pages_to_scan_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%u\n", ksm_thread_pages_to_scan);
}

static ssize_t pages_to_scan_store(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   const char *buf, size_t count)
{
	unsigned int nr_pages;
	int err;

	err = kstrtouint(buf, 10, &nr_pages);
	if (err)
		return -EINVAL;

	ksm_thread_pages_to_scan = nr_pages;

	return count;
}
KSM_ATTR(pages_to_scan);

static ssize_t run_show(struct kobject *kobj, struct kobj_attribute *attr,
			char *buf)
{
	return sysfs_emit(buf, "%lu\n", ksm_run);
}

static ssize_t run_store(struct kobject *kobj, struct kobj_attribute *attr,
			 const char *buf, size_t count)
{
	unsigned int flags;
	int err;

	err = kstrtouint(buf, 10, &flags);
	if (err)
		return -EINVAL;
	if (flags > KSM_RUN_UNMERGE)
		return -EINVAL;

	 

	mutex_lock(&ksm_thread_mutex);
	wait_while_offlining();
	if (ksm_run != flags) {
		ksm_run = flags;
		if (flags & KSM_RUN_UNMERGE) {
			set_current_oom_origin();
			err = unmerge_and_remove_all_rmap_items();
			clear_current_oom_origin();
			if (err) {
				ksm_run = KSM_RUN_STOP;
				count = err;
			}
		}
	}
	mutex_unlock(&ksm_thread_mutex);

	if (flags & KSM_RUN_MERGE)
		wake_up_interruptible(&ksm_thread_wait);

	return count;
}
KSM_ATTR(run);

#ifdef CONFIG_NUMA
static ssize_t merge_across_nodes_show(struct kobject *kobj,
				       struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%u\n", ksm_merge_across_nodes);
}

static ssize_t merge_across_nodes_store(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   const char *buf, size_t count)
{
	int err;
	unsigned long knob;

	err = kstrtoul(buf, 10, &knob);
	if (err)
		return err;
	if (knob > 1)
		return -EINVAL;

	mutex_lock(&ksm_thread_mutex);
	wait_while_offlining();
	if (ksm_merge_across_nodes != knob) {
		if (ksm_pages_shared || remove_all_stable_nodes())
			err = -EBUSY;
		else if (root_stable_tree == one_stable_tree) {
			struct rb_root *buf;
			 
			buf = kcalloc(nr_node_ids + nr_node_ids, sizeof(*buf),
				      GFP_KERNEL);
			 
			if (!buf)
				err = -ENOMEM;
			else {
				root_stable_tree = buf;
				root_unstable_tree = buf + nr_node_ids;
				 
				root_unstable_tree[0] = one_unstable_tree[0];
			}
		}
		if (!err) {
			ksm_merge_across_nodes = knob;
			ksm_nr_node_ids = knob ? 1 : nr_node_ids;
		}
	}
	mutex_unlock(&ksm_thread_mutex);

	return err ? err : count;
}
KSM_ATTR(merge_across_nodes);
#endif

static ssize_t use_zero_pages_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%u\n", ksm_use_zero_pages);
}
static ssize_t use_zero_pages_store(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   const char *buf, size_t count)
{
	int err;
	bool value;

	err = kstrtobool(buf, &value);
	if (err)
		return -EINVAL;

	ksm_use_zero_pages = value;

	return count;
}
KSM_ATTR(use_zero_pages);

static ssize_t max_page_sharing_show(struct kobject *kobj,
				     struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%u\n", ksm_max_page_sharing);
}

static ssize_t max_page_sharing_store(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	int err;
	int knob;

	err = kstrtoint(buf, 10, &knob);
	if (err)
		return err;
	 
	if (knob < 2)
		return -EINVAL;

	if (READ_ONCE(ksm_max_page_sharing) == knob)
		return count;

	mutex_lock(&ksm_thread_mutex);
	wait_while_offlining();
	if (ksm_max_page_sharing != knob) {
		if (ksm_pages_shared || remove_all_stable_nodes())
			err = -EBUSY;
		else
			ksm_max_page_sharing = knob;
	}
	mutex_unlock(&ksm_thread_mutex);

	return err ? err : count;
}
KSM_ATTR(max_page_sharing);

static ssize_t pages_scanned_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%lu\n", ksm_pages_scanned);
}
KSM_ATTR_RO(pages_scanned);

static ssize_t pages_shared_show(struct kobject *kobj,
				 struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%lu\n", ksm_pages_shared);
}
KSM_ATTR_RO(pages_shared);

static ssize_t pages_sharing_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%lu\n", ksm_pages_sharing);
}
KSM_ATTR_RO(pages_sharing);

static ssize_t pages_unshared_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%lu\n", ksm_pages_unshared);
}
KSM_ATTR_RO(pages_unshared);

static ssize_t pages_volatile_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	long ksm_pages_volatile;

	ksm_pages_volatile = ksm_rmap_items - ksm_pages_shared
				- ksm_pages_sharing - ksm_pages_unshared;
	 
	if (ksm_pages_volatile < 0)
		ksm_pages_volatile = 0;
	return sysfs_emit(buf, "%ld\n", ksm_pages_volatile);
}
KSM_ATTR_RO(pages_volatile);

static ssize_t ksm_zero_pages_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%ld\n", ksm_zero_pages);
}
KSM_ATTR_RO(ksm_zero_pages);

static ssize_t general_profit_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	long general_profit;

	general_profit = (ksm_pages_sharing + ksm_zero_pages) * PAGE_SIZE -
				ksm_rmap_items * sizeof(struct ksm_rmap_item);

	return sysfs_emit(buf, "%ld\n", general_profit);
}
KSM_ATTR_RO(general_profit);

static ssize_t stable_node_dups_show(struct kobject *kobj,
				     struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%lu\n", ksm_stable_node_dups);
}
KSM_ATTR_RO(stable_node_dups);

static ssize_t stable_node_chains_show(struct kobject *kobj,
				       struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%lu\n", ksm_stable_node_chains);
}
KSM_ATTR_RO(stable_node_chains);

static ssize_t
stable_node_chains_prune_millisecs_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	return sysfs_emit(buf, "%u\n", ksm_stable_node_chains_prune_millisecs);
}

static ssize_t
stable_node_chains_prune_millisecs_store(struct kobject *kobj,
					 struct kobj_attribute *attr,
					 const char *buf, size_t count)
{
	unsigned int msecs;
	int err;

	err = kstrtouint(buf, 10, &msecs);
	if (err)
		return -EINVAL;

	ksm_stable_node_chains_prune_millisecs = msecs;

	return count;
}
KSM_ATTR(stable_node_chains_prune_millisecs);

static ssize_t full_scans_show(struct kobject *kobj,
			       struct kobj_attribute *attr, char *buf)
{
	return sysfs_emit(buf, "%lu\n", ksm_scan.seqnr);
}
KSM_ATTR_RO(full_scans);

static struct attribute *ksm_attrs[] = {
	&sleep_millisecs_attr.attr,
	&pages_to_scan_attr.attr,
	&run_attr.attr,
	&pages_scanned_attr.attr,
	&pages_shared_attr.attr,
	&pages_sharing_attr.attr,
	&pages_unshared_attr.attr,
	&pages_volatile_attr.attr,
	&ksm_zero_pages_attr.attr,
	&full_scans_attr.attr,
#ifdef CONFIG_NUMA
	&merge_across_nodes_attr.attr,
#endif
	&max_page_sharing_attr.attr,
	&stable_node_chains_attr.attr,
	&stable_node_dups_attr.attr,
	&stable_node_chains_prune_millisecs_attr.attr,
	&use_zero_pages_attr.attr,
	&general_profit_attr.attr,
	NULL,
};

static const struct attribute_group ksm_attr_group = {
	.attrs = ksm_attrs,
	.name = "ksm",
};
#endif  

static int __init ksm_init(void)
{
	struct task_struct *ksm_thread;
	int err;

	 
	zero_checksum = calc_checksum(ZERO_PAGE(0));
	 
	ksm_use_zero_pages = false;

	err = ksm_slab_init();
	if (err)
		goto out;

	ksm_thread = kthread_run(ksm_scan_thread, NULL, "ksmd");
	if (IS_ERR(ksm_thread)) {
		pr_err("ksm: creating kthread failed\n");
		err = PTR_ERR(ksm_thread);
		goto out_free;
	}

#ifdef CONFIG_SYSFS
	err = sysfs_create_group(mm_kobj, &ksm_attr_group);
	if (err) {
		pr_err("ksm: register sysfs failed\n");
		kthread_stop(ksm_thread);
		goto out_free;
	}
#else
	ksm_run = KSM_RUN_MERGE;	 

#endif  

#ifdef CONFIG_MEMORY_HOTREMOVE
	 
	hotplug_memory_notifier(ksm_memory_callback, KSM_CALLBACK_PRI);
#endif
	return 0;

out_free:
	ksm_slab_free();
out:
	return err;
}
subsys_initcall(ksm_init);
