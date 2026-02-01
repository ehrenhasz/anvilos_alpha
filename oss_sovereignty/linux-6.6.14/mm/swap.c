
 

 

#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/mman.h>
#include <linux/pagemap.h>
#include <linux/pagevec.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/mm_inline.h>
#include <linux/percpu_counter.h>
#include <linux/memremap.h>
#include <linux/percpu.h>
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <linux/backing-dev.h>
#include <linux/memcontrol.h>
#include <linux/gfp.h>
#include <linux/uio.h>
#include <linux/hugetlb.h>
#include <linux/page_idle.h>
#include <linux/local_lock.h>
#include <linux/buffer_head.h>

#include "internal.h"

#define CREATE_TRACE_POINTS
#include <trace/events/pagemap.h>

 
int page_cluster;
const int page_cluster_max = 31;

 
struct lru_rotate {
	local_lock_t lock;
	struct folio_batch fbatch;
};
static DEFINE_PER_CPU(struct lru_rotate, lru_rotate) = {
	.lock = INIT_LOCAL_LOCK(lock),
};

 
struct cpu_fbatches {
	local_lock_t lock;
	struct folio_batch lru_add;
	struct folio_batch lru_deactivate_file;
	struct folio_batch lru_deactivate;
	struct folio_batch lru_lazyfree;
#ifdef CONFIG_SMP
	struct folio_batch activate;
#endif
};
static DEFINE_PER_CPU(struct cpu_fbatches, cpu_fbatches) = {
	.lock = INIT_LOCAL_LOCK(lock),
};

 
static void __page_cache_release(struct folio *folio)
{
	if (folio_test_lru(folio)) {
		struct lruvec *lruvec;
		unsigned long flags;

		lruvec = folio_lruvec_lock_irqsave(folio, &flags);
		lruvec_del_folio(lruvec, folio);
		__folio_clear_lru_flags(folio);
		unlock_page_lruvec_irqrestore(lruvec, flags);
	}
	 
	if (unlikely(folio_test_mlocked(folio))) {
		long nr_pages = folio_nr_pages(folio);

		__folio_clear_mlocked(folio);
		zone_stat_mod_folio(folio, NR_MLOCK, -nr_pages);
		count_vm_events(UNEVICTABLE_PGCLEARED, nr_pages);
	}
}

static void __folio_put_small(struct folio *folio)
{
	__page_cache_release(folio);
	mem_cgroup_uncharge(folio);
	free_unref_page(&folio->page, 0);
}

static void __folio_put_large(struct folio *folio)
{
	 
	if (!folio_test_hugetlb(folio))
		__page_cache_release(folio);
	destroy_large_folio(folio);
}

void __folio_put(struct folio *folio)
{
	if (unlikely(folio_is_zone_device(folio)))
		free_zone_device_page(&folio->page);
	else if (unlikely(folio_test_large(folio)))
		__folio_put_large(folio);
	else
		__folio_put_small(folio);
}
EXPORT_SYMBOL(__folio_put);

 
void put_pages_list(struct list_head *pages)
{
	struct folio *folio, *next;

	list_for_each_entry_safe(folio, next, pages, lru) {
		if (!folio_put_testzero(folio)) {
			list_del(&folio->lru);
			continue;
		}
		if (folio_test_large(folio)) {
			list_del(&folio->lru);
			__folio_put_large(folio);
			continue;
		}
		 
	}

	free_unref_page_list(pages);
	INIT_LIST_HEAD(pages);
}
EXPORT_SYMBOL(put_pages_list);

typedef void (*move_fn_t)(struct lruvec *lruvec, struct folio *folio);

static void lru_add_fn(struct lruvec *lruvec, struct folio *folio)
{
	int was_unevictable = folio_test_clear_unevictable(folio);
	long nr_pages = folio_nr_pages(folio);

	VM_BUG_ON_FOLIO(folio_test_lru(folio), folio);

	 
	if (folio_evictable(folio)) {
		if (was_unevictable)
			__count_vm_events(UNEVICTABLE_PGRESCUED, nr_pages);
	} else {
		folio_clear_active(folio);
		folio_set_unevictable(folio);
		 
		folio->mlock_count = 0;
		if (!was_unevictable)
			__count_vm_events(UNEVICTABLE_PGCULLED, nr_pages);
	}

	lruvec_add_folio(lruvec, folio);
	trace_mm_lru_insertion(folio);
}

static void folio_batch_move_lru(struct folio_batch *fbatch, move_fn_t move_fn)
{
	int i;
	struct lruvec *lruvec = NULL;
	unsigned long flags = 0;

	for (i = 0; i < folio_batch_count(fbatch); i++) {
		struct folio *folio = fbatch->folios[i];

		 
		if (move_fn != lru_add_fn && !folio_test_clear_lru(folio))
			continue;

		lruvec = folio_lruvec_relock_irqsave(folio, lruvec, &flags);
		move_fn(lruvec, folio);

		folio_set_lru(folio);
	}

	if (lruvec)
		unlock_page_lruvec_irqrestore(lruvec, flags);
	folios_put(fbatch->folios, folio_batch_count(fbatch));
	folio_batch_reinit(fbatch);
}

static void folio_batch_add_and_move(struct folio_batch *fbatch,
		struct folio *folio, move_fn_t move_fn)
{
	if (folio_batch_add(fbatch, folio) && !folio_test_large(folio) &&
	    !lru_cache_disabled())
		return;
	folio_batch_move_lru(fbatch, move_fn);
}

static void lru_move_tail_fn(struct lruvec *lruvec, struct folio *folio)
{
	if (!folio_test_unevictable(folio)) {
		lruvec_del_folio(lruvec, folio);
		folio_clear_active(folio);
		lruvec_add_folio_tail(lruvec, folio);
		__count_vm_events(PGROTATED, folio_nr_pages(folio));
	}
}

 
void folio_rotate_reclaimable(struct folio *folio)
{
	if (!folio_test_locked(folio) && !folio_test_dirty(folio) &&
	    !folio_test_unevictable(folio) && folio_test_lru(folio)) {
		struct folio_batch *fbatch;
		unsigned long flags;

		folio_get(folio);
		local_lock_irqsave(&lru_rotate.lock, flags);
		fbatch = this_cpu_ptr(&lru_rotate.fbatch);
		folio_batch_add_and_move(fbatch, folio, lru_move_tail_fn);
		local_unlock_irqrestore(&lru_rotate.lock, flags);
	}
}

void lru_note_cost(struct lruvec *lruvec, bool file,
		   unsigned int nr_io, unsigned int nr_rotated)
{
	unsigned long cost;

	 
	cost = nr_io * SWAP_CLUSTER_MAX + nr_rotated;

	do {
		unsigned long lrusize;

		 
		spin_lock_irq(&lruvec->lru_lock);
		 
		if (file)
			lruvec->file_cost += cost;
		else
			lruvec->anon_cost += cost;

		 
		lrusize = lruvec_page_state(lruvec, NR_INACTIVE_ANON) +
			  lruvec_page_state(lruvec, NR_ACTIVE_ANON) +
			  lruvec_page_state(lruvec, NR_INACTIVE_FILE) +
			  lruvec_page_state(lruvec, NR_ACTIVE_FILE);

		if (lruvec->file_cost + lruvec->anon_cost > lrusize / 4) {
			lruvec->file_cost /= 2;
			lruvec->anon_cost /= 2;
		}
		spin_unlock_irq(&lruvec->lru_lock);
	} while ((lruvec = parent_lruvec(lruvec)));
}

void lru_note_cost_refault(struct folio *folio)
{
	lru_note_cost(folio_lruvec(folio), folio_is_file_lru(folio),
		      folio_nr_pages(folio), 0);
}

static void folio_activate_fn(struct lruvec *lruvec, struct folio *folio)
{
	if (!folio_test_active(folio) && !folio_test_unevictable(folio)) {
		long nr_pages = folio_nr_pages(folio);

		lruvec_del_folio(lruvec, folio);
		folio_set_active(folio);
		lruvec_add_folio(lruvec, folio);
		trace_mm_lru_activate(folio);

		__count_vm_events(PGACTIVATE, nr_pages);
		__count_memcg_events(lruvec_memcg(lruvec), PGACTIVATE,
				     nr_pages);
	}
}

#ifdef CONFIG_SMP
static void folio_activate_drain(int cpu)
{
	struct folio_batch *fbatch = &per_cpu(cpu_fbatches.activate, cpu);

	if (folio_batch_count(fbatch))
		folio_batch_move_lru(fbatch, folio_activate_fn);
}

void folio_activate(struct folio *folio)
{
	if (folio_test_lru(folio) && !folio_test_active(folio) &&
	    !folio_test_unevictable(folio)) {
		struct folio_batch *fbatch;

		folio_get(folio);
		local_lock(&cpu_fbatches.lock);
		fbatch = this_cpu_ptr(&cpu_fbatches.activate);
		folio_batch_add_and_move(fbatch, folio, folio_activate_fn);
		local_unlock(&cpu_fbatches.lock);
	}
}

#else
static inline void folio_activate_drain(int cpu)
{
}

void folio_activate(struct folio *folio)
{
	struct lruvec *lruvec;

	if (folio_test_clear_lru(folio)) {
		lruvec = folio_lruvec_lock_irq(folio);
		folio_activate_fn(lruvec, folio);
		unlock_page_lruvec_irq(lruvec);
		folio_set_lru(folio);
	}
}
#endif

static void __lru_cache_activate_folio(struct folio *folio)
{
	struct folio_batch *fbatch;
	int i;

	local_lock(&cpu_fbatches.lock);
	fbatch = this_cpu_ptr(&cpu_fbatches.lru_add);

	 
	for (i = folio_batch_count(fbatch) - 1; i >= 0; i--) {
		struct folio *batch_folio = fbatch->folios[i];

		if (batch_folio == folio) {
			folio_set_active(folio);
			break;
		}
	}

	local_unlock(&cpu_fbatches.lock);
}

#ifdef CONFIG_LRU_GEN
static void folio_inc_refs(struct folio *folio)
{
	unsigned long new_flags, old_flags = READ_ONCE(folio->flags);

	if (folio_test_unevictable(folio))
		return;

	if (!folio_test_referenced(folio)) {
		folio_set_referenced(folio);
		return;
	}

	if (!folio_test_workingset(folio)) {
		folio_set_workingset(folio);
		return;
	}

	 
	do {
		new_flags = old_flags & LRU_REFS_MASK;
		if (new_flags == LRU_REFS_MASK)
			break;

		new_flags += BIT(LRU_REFS_PGOFF);
		new_flags |= old_flags & ~LRU_REFS_MASK;
	} while (!try_cmpxchg(&folio->flags, &old_flags, new_flags));
}
#else
static void folio_inc_refs(struct folio *folio)
{
}
#endif  

 
void folio_mark_accessed(struct folio *folio)
{
	if (lru_gen_enabled()) {
		folio_inc_refs(folio);
		return;
	}

	if (!folio_test_referenced(folio)) {
		folio_set_referenced(folio);
	} else if (folio_test_unevictable(folio)) {
		 
	} else if (!folio_test_active(folio)) {
		 
		if (folio_test_lru(folio))
			folio_activate(folio);
		else
			__lru_cache_activate_folio(folio);
		folio_clear_referenced(folio);
		workingset_activation(folio);
	}
	if (folio_test_idle(folio))
		folio_clear_idle(folio);
}
EXPORT_SYMBOL(folio_mark_accessed);

 
void folio_add_lru(struct folio *folio)
{
	struct folio_batch *fbatch;

	VM_BUG_ON_FOLIO(folio_test_active(folio) &&
			folio_test_unevictable(folio), folio);
	VM_BUG_ON_FOLIO(folio_test_lru(folio), folio);

	 
	if (lru_gen_enabled() && !folio_test_unevictable(folio) &&
	    lru_gen_in_fault() && !(current->flags & PF_MEMALLOC))
		folio_set_active(folio);

	folio_get(folio);
	local_lock(&cpu_fbatches.lock);
	fbatch = this_cpu_ptr(&cpu_fbatches.lru_add);
	folio_batch_add_and_move(fbatch, folio, lru_add_fn);
	local_unlock(&cpu_fbatches.lock);
}
EXPORT_SYMBOL(folio_add_lru);

 
void folio_add_lru_vma(struct folio *folio, struct vm_area_struct *vma)
{
	VM_BUG_ON_FOLIO(folio_test_lru(folio), folio);

	if (unlikely((vma->vm_flags & (VM_LOCKED | VM_SPECIAL)) == VM_LOCKED))
		mlock_new_folio(folio);
	else
		folio_add_lru(folio);
}

 
static void lru_deactivate_file_fn(struct lruvec *lruvec, struct folio *folio)
{
	bool active = folio_test_active(folio);
	long nr_pages = folio_nr_pages(folio);

	if (folio_test_unevictable(folio))
		return;

	 
	if (folio_mapped(folio))
		return;

	lruvec_del_folio(lruvec, folio);
	folio_clear_active(folio);
	folio_clear_referenced(folio);

	if (folio_test_writeback(folio) || folio_test_dirty(folio)) {
		 
		lruvec_add_folio(lruvec, folio);
		folio_set_reclaim(folio);
	} else {
		 
		lruvec_add_folio_tail(lruvec, folio);
		__count_vm_events(PGROTATED, nr_pages);
	}

	if (active) {
		__count_vm_events(PGDEACTIVATE, nr_pages);
		__count_memcg_events(lruvec_memcg(lruvec), PGDEACTIVATE,
				     nr_pages);
	}
}

static void lru_deactivate_fn(struct lruvec *lruvec, struct folio *folio)
{
	if (!folio_test_unevictable(folio) && (folio_test_active(folio) || lru_gen_enabled())) {
		long nr_pages = folio_nr_pages(folio);

		lruvec_del_folio(lruvec, folio);
		folio_clear_active(folio);
		folio_clear_referenced(folio);
		lruvec_add_folio(lruvec, folio);

		__count_vm_events(PGDEACTIVATE, nr_pages);
		__count_memcg_events(lruvec_memcg(lruvec), PGDEACTIVATE,
				     nr_pages);
	}
}

static void lru_lazyfree_fn(struct lruvec *lruvec, struct folio *folio)
{
	if (folio_test_anon(folio) && folio_test_swapbacked(folio) &&
	    !folio_test_swapcache(folio) && !folio_test_unevictable(folio)) {
		long nr_pages = folio_nr_pages(folio);

		lruvec_del_folio(lruvec, folio);
		folio_clear_active(folio);
		folio_clear_referenced(folio);
		 
		folio_clear_swapbacked(folio);
		lruvec_add_folio(lruvec, folio);

		__count_vm_events(PGLAZYFREE, nr_pages);
		__count_memcg_events(lruvec_memcg(lruvec), PGLAZYFREE,
				     nr_pages);
	}
}

 
void lru_add_drain_cpu(int cpu)
{
	struct cpu_fbatches *fbatches = &per_cpu(cpu_fbatches, cpu);
	struct folio_batch *fbatch = &fbatches->lru_add;

	if (folio_batch_count(fbatch))
		folio_batch_move_lru(fbatch, lru_add_fn);

	fbatch = &per_cpu(lru_rotate.fbatch, cpu);
	 
	if (data_race(folio_batch_count(fbatch))) {
		unsigned long flags;

		 
		local_lock_irqsave(&lru_rotate.lock, flags);
		folio_batch_move_lru(fbatch, lru_move_tail_fn);
		local_unlock_irqrestore(&lru_rotate.lock, flags);
	}

	fbatch = &fbatches->lru_deactivate_file;
	if (folio_batch_count(fbatch))
		folio_batch_move_lru(fbatch, lru_deactivate_file_fn);

	fbatch = &fbatches->lru_deactivate;
	if (folio_batch_count(fbatch))
		folio_batch_move_lru(fbatch, lru_deactivate_fn);

	fbatch = &fbatches->lru_lazyfree;
	if (folio_batch_count(fbatch))
		folio_batch_move_lru(fbatch, lru_lazyfree_fn);

	folio_activate_drain(cpu);
}

 
void deactivate_file_folio(struct folio *folio)
{
	struct folio_batch *fbatch;

	 
	if (folio_test_unevictable(folio))
		return;

	folio_get(folio);
	local_lock(&cpu_fbatches.lock);
	fbatch = this_cpu_ptr(&cpu_fbatches.lru_deactivate_file);
	folio_batch_add_and_move(fbatch, folio, lru_deactivate_file_fn);
	local_unlock(&cpu_fbatches.lock);
}

 
void folio_deactivate(struct folio *folio)
{
	if (folio_test_lru(folio) && !folio_test_unevictable(folio) &&
	    (folio_test_active(folio) || lru_gen_enabled())) {
		struct folio_batch *fbatch;

		folio_get(folio);
		local_lock(&cpu_fbatches.lock);
		fbatch = this_cpu_ptr(&cpu_fbatches.lru_deactivate);
		folio_batch_add_and_move(fbatch, folio, lru_deactivate_fn);
		local_unlock(&cpu_fbatches.lock);
	}
}

 
void folio_mark_lazyfree(struct folio *folio)
{
	if (folio_test_lru(folio) && folio_test_anon(folio) &&
	    folio_test_swapbacked(folio) && !folio_test_swapcache(folio) &&
	    !folio_test_unevictable(folio)) {
		struct folio_batch *fbatch;

		folio_get(folio);
		local_lock(&cpu_fbatches.lock);
		fbatch = this_cpu_ptr(&cpu_fbatches.lru_lazyfree);
		folio_batch_add_and_move(fbatch, folio, lru_lazyfree_fn);
		local_unlock(&cpu_fbatches.lock);
	}
}

void lru_add_drain(void)
{
	local_lock(&cpu_fbatches.lock);
	lru_add_drain_cpu(smp_processor_id());
	local_unlock(&cpu_fbatches.lock);
	mlock_drain_local();
}

 
static void lru_add_and_bh_lrus_drain(void)
{
	local_lock(&cpu_fbatches.lock);
	lru_add_drain_cpu(smp_processor_id());
	local_unlock(&cpu_fbatches.lock);
	invalidate_bh_lrus_cpu();
	mlock_drain_local();
}

void lru_add_drain_cpu_zone(struct zone *zone)
{
	local_lock(&cpu_fbatches.lock);
	lru_add_drain_cpu(smp_processor_id());
	drain_local_pages(zone);
	local_unlock(&cpu_fbatches.lock);
	mlock_drain_local();
}

#ifdef CONFIG_SMP

static DEFINE_PER_CPU(struct work_struct, lru_add_drain_work);

static void lru_add_drain_per_cpu(struct work_struct *dummy)
{
	lru_add_and_bh_lrus_drain();
}

static bool cpu_needs_drain(unsigned int cpu)
{
	struct cpu_fbatches *fbatches = &per_cpu(cpu_fbatches, cpu);

	 
	return folio_batch_count(&fbatches->lru_add) ||
		data_race(folio_batch_count(&per_cpu(lru_rotate.fbatch, cpu))) ||
		folio_batch_count(&fbatches->lru_deactivate_file) ||
		folio_batch_count(&fbatches->lru_deactivate) ||
		folio_batch_count(&fbatches->lru_lazyfree) ||
		folio_batch_count(&fbatches->activate) ||
		need_mlock_drain(cpu) ||
		has_bh_in_lru(cpu, NULL);
}

 
static inline void __lru_add_drain_all(bool force_all_cpus)
{
	 
	static unsigned int lru_drain_gen;
	static struct cpumask has_work;
	static DEFINE_MUTEX(lock);
	unsigned cpu, this_gen;

	 
	if (WARN_ON(!mm_percpu_wq))
		return;

	 
	smp_mb();

	 
	this_gen = smp_load_acquire(&lru_drain_gen);

	mutex_lock(&lock);

	 
	if (unlikely(this_gen != lru_drain_gen && !force_all_cpus))
		goto done;

	 
	WRITE_ONCE(lru_drain_gen, lru_drain_gen + 1);
	smp_mb();

	cpumask_clear(&has_work);
	for_each_online_cpu(cpu) {
		struct work_struct *work = &per_cpu(lru_add_drain_work, cpu);

		if (cpu_needs_drain(cpu)) {
			INIT_WORK(work, lru_add_drain_per_cpu);
			queue_work_on(cpu, mm_percpu_wq, work);
			__cpumask_set_cpu(cpu, &has_work);
		}
	}

	for_each_cpu(cpu, &has_work)
		flush_work(&per_cpu(lru_add_drain_work, cpu));

done:
	mutex_unlock(&lock);
}

void lru_add_drain_all(void)
{
	__lru_add_drain_all(false);
}
#else
void lru_add_drain_all(void)
{
	lru_add_drain();
}
#endif  

atomic_t lru_disable_count = ATOMIC_INIT(0);

 
void lru_cache_disable(void)
{
	atomic_inc(&lru_disable_count);
	 
	synchronize_rcu_expedited();
#ifdef CONFIG_SMP
	__lru_add_drain_all(true);
#else
	lru_add_and_bh_lrus_drain();
#endif
}

 
void release_pages(release_pages_arg arg, int nr)
{
	int i;
	struct encoded_page **encoded = arg.encoded_pages;
	LIST_HEAD(pages_to_free);
	struct lruvec *lruvec = NULL;
	unsigned long flags = 0;
	unsigned int lock_batch;

	for (i = 0; i < nr; i++) {
		struct folio *folio;

		 
		folio = page_folio(encoded_page_ptr(encoded[i]));

		 
		if (lruvec && ++lock_batch == SWAP_CLUSTER_MAX) {
			unlock_page_lruvec_irqrestore(lruvec, flags);
			lruvec = NULL;
		}

		if (is_huge_zero_page(&folio->page))
			continue;

		if (folio_is_zone_device(folio)) {
			if (lruvec) {
				unlock_page_lruvec_irqrestore(lruvec, flags);
				lruvec = NULL;
			}
			if (put_devmap_managed_page(&folio->page))
				continue;
			if (folio_put_testzero(folio))
				free_zone_device_page(&folio->page);
			continue;
		}

		if (!folio_put_testzero(folio))
			continue;

		if (folio_test_large(folio)) {
			if (lruvec) {
				unlock_page_lruvec_irqrestore(lruvec, flags);
				lruvec = NULL;
			}
			__folio_put_large(folio);
			continue;
		}

		if (folio_test_lru(folio)) {
			struct lruvec *prev_lruvec = lruvec;

			lruvec = folio_lruvec_relock_irqsave(folio, lruvec,
									&flags);
			if (prev_lruvec != lruvec)
				lock_batch = 0;

			lruvec_del_folio(lruvec, folio);
			__folio_clear_lru_flags(folio);
		}

		 
		if (unlikely(folio_test_mlocked(folio))) {
			__folio_clear_mlocked(folio);
			zone_stat_sub_folio(folio, NR_MLOCK);
			count_vm_event(UNEVICTABLE_PGCLEARED);
		}

		list_add(&folio->lru, &pages_to_free);
	}
	if (lruvec)
		unlock_page_lruvec_irqrestore(lruvec, flags);

	mem_cgroup_uncharge_list(&pages_to_free);
	free_unref_page_list(&pages_to_free);
}
EXPORT_SYMBOL(release_pages);

 
void __folio_batch_release(struct folio_batch *fbatch)
{
	if (!fbatch->percpu_pvec_drained) {
		lru_add_drain();
		fbatch->percpu_pvec_drained = true;
	}
	release_pages(fbatch->folios, folio_batch_count(fbatch));
	folio_batch_reinit(fbatch);
}
EXPORT_SYMBOL(__folio_batch_release);

 
void folio_batch_remove_exceptionals(struct folio_batch *fbatch)
{
	unsigned int i, j;

	for (i = 0, j = 0; i < folio_batch_count(fbatch); i++) {
		struct folio *folio = fbatch->folios[i];
		if (!xa_is_value(folio))
			fbatch->folios[j++] = folio;
	}
	fbatch->nr = j;
}

 
void __init swap_setup(void)
{
	unsigned long megs = totalram_pages() >> (20 - PAGE_SHIFT);

	 
	if (megs < 16)
		page_cluster = 2;
	else
		page_cluster = 3;
	 
}
