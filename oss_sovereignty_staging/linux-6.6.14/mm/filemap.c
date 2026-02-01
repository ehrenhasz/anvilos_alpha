
 

 
#include <linux/export.h>
#include <linux/compiler.h>
#include <linux/dax.h>
#include <linux/fs.h>
#include <linux/sched/signal.h>
#include <linux/uaccess.h>
#include <linux/capability.h>
#include <linux/kernel_stat.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/syscalls.h>
#include <linux/mman.h>
#include <linux/pagemap.h>
#include <linux/file.h>
#include <linux/uio.h>
#include <linux/error-injection.h>
#include <linux/hash.h>
#include <linux/writeback.h>
#include <linux/backing-dev.h>
#include <linux/pagevec.h>
#include <linux/security.h>
#include <linux/cpuset.h>
#include <linux/hugetlb.h>
#include <linux/memcontrol.h>
#include <linux/shmem_fs.h>
#include <linux/rmap.h>
#include <linux/delayacct.h>
#include <linux/psi.h>
#include <linux/ramfs.h>
#include <linux/page_idle.h>
#include <linux/migrate.h>
#include <linux/pipe_fs_i.h>
#include <linux/splice.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>
#include "internal.h"

#define CREATE_TRACE_POINTS
#include <trace/events/filemap.h>

 
#include <linux/buffer_head.h>  

#include <asm/mman.h>

#include "swap.h"

 

 

static void page_cache_delete(struct address_space *mapping,
				   struct folio *folio, void *shadow)
{
	XA_STATE(xas, &mapping->i_pages, folio->index);
	long nr = 1;

	mapping_set_update(&xas, mapping);

	 
	if (!folio_test_hugetlb(folio)) {
		xas_set_order(&xas, folio->index, folio_order(folio));
		nr = folio_nr_pages(folio);
	}

	VM_BUG_ON_FOLIO(!folio_test_locked(folio), folio);

	xas_store(&xas, shadow);
	xas_init_marks(&xas);

	folio->mapping = NULL;
	 
	mapping->nrpages -= nr;
}

static void filemap_unaccount_folio(struct address_space *mapping,
		struct folio *folio)
{
	long nr;

	VM_BUG_ON_FOLIO(folio_mapped(folio), folio);
	if (!IS_ENABLED(CONFIG_DEBUG_VM) && unlikely(folio_mapped(folio))) {
		pr_alert("BUG: Bad page cache in process %s  pfn:%05lx\n",
			 current->comm, folio_pfn(folio));
		dump_page(&folio->page, "still mapped when deleted");
		dump_stack();
		add_taint(TAINT_BAD_PAGE, LOCKDEP_NOW_UNRELIABLE);

		if (mapping_exiting(mapping) && !folio_test_large(folio)) {
			int mapcount = page_mapcount(&folio->page);

			if (folio_ref_count(folio) >= mapcount + 2) {
				 
				page_mapcount_reset(&folio->page);
				folio_ref_sub(folio, mapcount);
			}
		}
	}

	 
	if (folio_test_hugetlb(folio))
		return;

	nr = folio_nr_pages(folio);

	__lruvec_stat_mod_folio(folio, NR_FILE_PAGES, -nr);
	if (folio_test_swapbacked(folio)) {
		__lruvec_stat_mod_folio(folio, NR_SHMEM, -nr);
		if (folio_test_pmd_mappable(folio))
			__lruvec_stat_mod_folio(folio, NR_SHMEM_THPS, -nr);
	} else if (folio_test_pmd_mappable(folio)) {
		__lruvec_stat_mod_folio(folio, NR_FILE_THPS, -nr);
		filemap_nr_thps_dec(mapping);
	}

	 
	if (WARN_ON_ONCE(folio_test_dirty(folio) &&
			 mapping_can_writeback(mapping)))
		folio_account_cleaned(folio, inode_to_wb(mapping->host));
}

 
void __filemap_remove_folio(struct folio *folio, void *shadow)
{
	struct address_space *mapping = folio->mapping;

	trace_mm_filemap_delete_from_page_cache(folio);
	filemap_unaccount_folio(mapping, folio);
	page_cache_delete(mapping, folio, shadow);
}

void filemap_free_folio(struct address_space *mapping, struct folio *folio)
{
	void (*free_folio)(struct folio *);
	int refs = 1;

	free_folio = mapping->a_ops->free_folio;
	if (free_folio)
		free_folio(folio);

	if (folio_test_large(folio) && !folio_test_hugetlb(folio))
		refs = folio_nr_pages(folio);
	folio_put_refs(folio, refs);
}

 
void filemap_remove_folio(struct folio *folio)
{
	struct address_space *mapping = folio->mapping;

	BUG_ON(!folio_test_locked(folio));
	spin_lock(&mapping->host->i_lock);
	xa_lock_irq(&mapping->i_pages);
	__filemap_remove_folio(folio, NULL);
	xa_unlock_irq(&mapping->i_pages);
	if (mapping_shrinkable(mapping))
		inode_add_lru(mapping->host);
	spin_unlock(&mapping->host->i_lock);

	filemap_free_folio(mapping, folio);
}

 
static void page_cache_delete_batch(struct address_space *mapping,
			     struct folio_batch *fbatch)
{
	XA_STATE(xas, &mapping->i_pages, fbatch->folios[0]->index);
	long total_pages = 0;
	int i = 0;
	struct folio *folio;

	mapping_set_update(&xas, mapping);
	xas_for_each(&xas, folio, ULONG_MAX) {
		if (i >= folio_batch_count(fbatch))
			break;

		 
		if (xa_is_value(folio))
			continue;
		 
		if (folio != fbatch->folios[i]) {
			VM_BUG_ON_FOLIO(folio->index >
					fbatch->folios[i]->index, folio);
			continue;
		}

		WARN_ON_ONCE(!folio_test_locked(folio));

		folio->mapping = NULL;
		 

		i++;
		xas_store(&xas, NULL);
		total_pages += folio_nr_pages(folio);
	}
	mapping->nrpages -= total_pages;
}

void delete_from_page_cache_batch(struct address_space *mapping,
				  struct folio_batch *fbatch)
{
	int i;

	if (!folio_batch_count(fbatch))
		return;

	spin_lock(&mapping->host->i_lock);
	xa_lock_irq(&mapping->i_pages);
	for (i = 0; i < folio_batch_count(fbatch); i++) {
		struct folio *folio = fbatch->folios[i];

		trace_mm_filemap_delete_from_page_cache(folio);
		filemap_unaccount_folio(mapping, folio);
	}
	page_cache_delete_batch(mapping, fbatch);
	xa_unlock_irq(&mapping->i_pages);
	if (mapping_shrinkable(mapping))
		inode_add_lru(mapping->host);
	spin_unlock(&mapping->host->i_lock);

	for (i = 0; i < folio_batch_count(fbatch); i++)
		filemap_free_folio(mapping, fbatch->folios[i]);
}

int filemap_check_errors(struct address_space *mapping)
{
	int ret = 0;
	 
	if (test_bit(AS_ENOSPC, &mapping->flags) &&
	    test_and_clear_bit(AS_ENOSPC, &mapping->flags))
		ret = -ENOSPC;
	if (test_bit(AS_EIO, &mapping->flags) &&
	    test_and_clear_bit(AS_EIO, &mapping->flags))
		ret = -EIO;
	return ret;
}
EXPORT_SYMBOL(filemap_check_errors);

static int filemap_check_and_keep_errors(struct address_space *mapping)
{
	 
	if (test_bit(AS_EIO, &mapping->flags))
		return -EIO;
	if (test_bit(AS_ENOSPC, &mapping->flags))
		return -ENOSPC;
	return 0;
}

 
int filemap_fdatawrite_wbc(struct address_space *mapping,
			   struct writeback_control *wbc)
{
	int ret;

	if (!mapping_can_writeback(mapping) ||
	    !mapping_tagged(mapping, PAGECACHE_TAG_DIRTY))
		return 0;

	wbc_attach_fdatawrite_inode(wbc, mapping->host);
	ret = do_writepages(mapping, wbc);
	wbc_detach_inode(wbc);
	return ret;
}
EXPORT_SYMBOL(filemap_fdatawrite_wbc);

 
int __filemap_fdatawrite_range(struct address_space *mapping, loff_t start,
				loff_t end, int sync_mode)
{
	struct writeback_control wbc = {
		.sync_mode = sync_mode,
		.nr_to_write = LONG_MAX,
		.range_start = start,
		.range_end = end,
	};

	return filemap_fdatawrite_wbc(mapping, &wbc);
}

static inline int __filemap_fdatawrite(struct address_space *mapping,
	int sync_mode)
{
	return __filemap_fdatawrite_range(mapping, 0, LLONG_MAX, sync_mode);
}

int filemap_fdatawrite(struct address_space *mapping)
{
	return __filemap_fdatawrite(mapping, WB_SYNC_ALL);
}
EXPORT_SYMBOL(filemap_fdatawrite);

int filemap_fdatawrite_range(struct address_space *mapping, loff_t start,
				loff_t end)
{
	return __filemap_fdatawrite_range(mapping, start, end, WB_SYNC_ALL);
}
EXPORT_SYMBOL(filemap_fdatawrite_range);

 
int filemap_flush(struct address_space *mapping)
{
	return __filemap_fdatawrite(mapping, WB_SYNC_NONE);
}
EXPORT_SYMBOL(filemap_flush);

 
bool filemap_range_has_page(struct address_space *mapping,
			   loff_t start_byte, loff_t end_byte)
{
	struct folio *folio;
	XA_STATE(xas, &mapping->i_pages, start_byte >> PAGE_SHIFT);
	pgoff_t max = end_byte >> PAGE_SHIFT;

	if (end_byte < start_byte)
		return false;

	rcu_read_lock();
	for (;;) {
		folio = xas_find(&xas, max);
		if (xas_retry(&xas, folio))
			continue;
		 
		if (xa_is_value(folio))
			continue;
		 
		break;
	}
	rcu_read_unlock();

	return folio != NULL;
}
EXPORT_SYMBOL(filemap_range_has_page);

static void __filemap_fdatawait_range(struct address_space *mapping,
				     loff_t start_byte, loff_t end_byte)
{
	pgoff_t index = start_byte >> PAGE_SHIFT;
	pgoff_t end = end_byte >> PAGE_SHIFT;
	struct folio_batch fbatch;
	unsigned nr_folios;

	folio_batch_init(&fbatch);

	while (index <= end) {
		unsigned i;

		nr_folios = filemap_get_folios_tag(mapping, &index, end,
				PAGECACHE_TAG_WRITEBACK, &fbatch);

		if (!nr_folios)
			break;

		for (i = 0; i < nr_folios; i++) {
			struct folio *folio = fbatch.folios[i];

			folio_wait_writeback(folio);
			folio_clear_error(folio);
		}
		folio_batch_release(&fbatch);
		cond_resched();
	}
}

 
int filemap_fdatawait_range(struct address_space *mapping, loff_t start_byte,
			    loff_t end_byte)
{
	__filemap_fdatawait_range(mapping, start_byte, end_byte);
	return filemap_check_errors(mapping);
}
EXPORT_SYMBOL(filemap_fdatawait_range);

 
int filemap_fdatawait_range_keep_errors(struct address_space *mapping,
		loff_t start_byte, loff_t end_byte)
{
	__filemap_fdatawait_range(mapping, start_byte, end_byte);
	return filemap_check_and_keep_errors(mapping);
}
EXPORT_SYMBOL(filemap_fdatawait_range_keep_errors);

 
int file_fdatawait_range(struct file *file, loff_t start_byte, loff_t end_byte)
{
	struct address_space *mapping = file->f_mapping;

	__filemap_fdatawait_range(mapping, start_byte, end_byte);
	return file_check_and_advance_wb_err(file);
}
EXPORT_SYMBOL(file_fdatawait_range);

 
int filemap_fdatawait_keep_errors(struct address_space *mapping)
{
	__filemap_fdatawait_range(mapping, 0, LLONG_MAX);
	return filemap_check_and_keep_errors(mapping);
}
EXPORT_SYMBOL(filemap_fdatawait_keep_errors);

 
static bool mapping_needs_writeback(struct address_space *mapping)
{
	return mapping->nrpages;
}

bool filemap_range_has_writeback(struct address_space *mapping,
				 loff_t start_byte, loff_t end_byte)
{
	XA_STATE(xas, &mapping->i_pages, start_byte >> PAGE_SHIFT);
	pgoff_t max = end_byte >> PAGE_SHIFT;
	struct folio *folio;

	if (end_byte < start_byte)
		return false;

	rcu_read_lock();
	xas_for_each(&xas, folio, max) {
		if (xas_retry(&xas, folio))
			continue;
		if (xa_is_value(folio))
			continue;
		if (folio_test_dirty(folio) || folio_test_locked(folio) ||
				folio_test_writeback(folio))
			break;
	}
	rcu_read_unlock();
	return folio != NULL;
}
EXPORT_SYMBOL_GPL(filemap_range_has_writeback);

 
int filemap_write_and_wait_range(struct address_space *mapping,
				 loff_t lstart, loff_t lend)
{
	int err = 0, err2;

	if (lend < lstart)
		return 0;

	if (mapping_needs_writeback(mapping)) {
		err = __filemap_fdatawrite_range(mapping, lstart, lend,
						 WB_SYNC_ALL);
		 
		if (err != -EIO)
			__filemap_fdatawait_range(mapping, lstart, lend);
	}
	err2 = filemap_check_errors(mapping);
	if (!err)
		err = err2;
	return err;
}
EXPORT_SYMBOL(filemap_write_and_wait_range);

void __filemap_set_wb_err(struct address_space *mapping, int err)
{
	errseq_t eseq = errseq_set(&mapping->wb_err, err);

	trace_filemap_set_wb_err(mapping, eseq);
}
EXPORT_SYMBOL(__filemap_set_wb_err);

 
int file_check_and_advance_wb_err(struct file *file)
{
	int err = 0;
	errseq_t old = READ_ONCE(file->f_wb_err);
	struct address_space *mapping = file->f_mapping;

	 
	if (errseq_check(&mapping->wb_err, old)) {
		 
		spin_lock(&file->f_lock);
		old = file->f_wb_err;
		err = errseq_check_and_advance(&mapping->wb_err,
						&file->f_wb_err);
		trace_file_check_and_advance_wb_err(file, old);
		spin_unlock(&file->f_lock);
	}

	 
	clear_bit(AS_EIO, &mapping->flags);
	clear_bit(AS_ENOSPC, &mapping->flags);
	return err;
}
EXPORT_SYMBOL(file_check_and_advance_wb_err);

 
int file_write_and_wait_range(struct file *file, loff_t lstart, loff_t lend)
{
	int err = 0, err2;
	struct address_space *mapping = file->f_mapping;

	if (lend < lstart)
		return 0;

	if (mapping_needs_writeback(mapping)) {
		err = __filemap_fdatawrite_range(mapping, lstart, lend,
						 WB_SYNC_ALL);
		 
		if (err != -EIO)
			__filemap_fdatawait_range(mapping, lstart, lend);
	}
	err2 = file_check_and_advance_wb_err(file);
	if (!err)
		err = err2;
	return err;
}
EXPORT_SYMBOL(file_write_and_wait_range);

 
void replace_page_cache_folio(struct folio *old, struct folio *new)
{
	struct address_space *mapping = old->mapping;
	void (*free_folio)(struct folio *) = mapping->a_ops->free_folio;
	pgoff_t offset = old->index;
	XA_STATE(xas, &mapping->i_pages, offset);

	VM_BUG_ON_FOLIO(!folio_test_locked(old), old);
	VM_BUG_ON_FOLIO(!folio_test_locked(new), new);
	VM_BUG_ON_FOLIO(new->mapping, new);

	folio_get(new);
	new->mapping = mapping;
	new->index = offset;

	mem_cgroup_migrate(old, new);

	xas_lock_irq(&xas);
	xas_store(&xas, new);

	old->mapping = NULL;
	 
	if (!folio_test_hugetlb(old))
		__lruvec_stat_sub_folio(old, NR_FILE_PAGES);
	if (!folio_test_hugetlb(new))
		__lruvec_stat_add_folio(new, NR_FILE_PAGES);
	if (folio_test_swapbacked(old))
		__lruvec_stat_sub_folio(old, NR_SHMEM);
	if (folio_test_swapbacked(new))
		__lruvec_stat_add_folio(new, NR_SHMEM);
	xas_unlock_irq(&xas);
	if (free_folio)
		free_folio(old);
	folio_put(old);
}
EXPORT_SYMBOL_GPL(replace_page_cache_folio);

noinline int __filemap_add_folio(struct address_space *mapping,
		struct folio *folio, pgoff_t index, gfp_t gfp, void **shadowp)
{
	XA_STATE(xas, &mapping->i_pages, index);
	int huge = folio_test_hugetlb(folio);
	bool charged = false;
	long nr = 1;

	VM_BUG_ON_FOLIO(!folio_test_locked(folio), folio);
	VM_BUG_ON_FOLIO(folio_test_swapbacked(folio), folio);
	mapping_set_update(&xas, mapping);

	if (!huge) {
		int error = mem_cgroup_charge(folio, NULL, gfp);
		VM_BUG_ON_FOLIO(index & (folio_nr_pages(folio) - 1), folio);
		if (error)
			return error;
		charged = true;
		xas_set_order(&xas, index, folio_order(folio));
		nr = folio_nr_pages(folio);
	}

	gfp &= GFP_RECLAIM_MASK;
	folio_ref_add(folio, nr);
	folio->mapping = mapping;
	folio->index = xas.xa_index;

	do {
		unsigned int order = xa_get_order(xas.xa, xas.xa_index);
		void *entry, *old = NULL;

		if (order > folio_order(folio))
			xas_split_alloc(&xas, xa_load(xas.xa, xas.xa_index),
					order, gfp);
		xas_lock_irq(&xas);
		xas_for_each_conflict(&xas, entry) {
			old = entry;
			if (!xa_is_value(entry)) {
				xas_set_err(&xas, -EEXIST);
				goto unlock;
			}
		}

		if (old) {
			if (shadowp)
				*shadowp = old;
			 
			order = xa_get_order(xas.xa, xas.xa_index);
			if (order > folio_order(folio)) {
				 
				BUG_ON(shmem_mapping(mapping));
				xas_split(&xas, old, order);
				xas_reset(&xas);
			}
		}

		xas_store(&xas, folio);
		if (xas_error(&xas))
			goto unlock;

		mapping->nrpages += nr;

		 
		if (!huge) {
			__lruvec_stat_mod_folio(folio, NR_FILE_PAGES, nr);
			if (folio_test_pmd_mappable(folio))
				__lruvec_stat_mod_folio(folio,
						NR_FILE_THPS, nr);
		}
unlock:
		xas_unlock_irq(&xas);
	} while (xas_nomem(&xas, gfp));

	if (xas_error(&xas))
		goto error;

	trace_mm_filemap_add_to_page_cache(folio);
	return 0;
error:
	if (charged)
		mem_cgroup_uncharge(folio);
	folio->mapping = NULL;
	 
	folio_put_refs(folio, nr);
	return xas_error(&xas);
}
ALLOW_ERROR_INJECTION(__filemap_add_folio, ERRNO);

int filemap_add_folio(struct address_space *mapping, struct folio *folio,
				pgoff_t index, gfp_t gfp)
{
	void *shadow = NULL;
	int ret;

	__folio_set_locked(folio);
	ret = __filemap_add_folio(mapping, folio, index, gfp, &shadow);
	if (unlikely(ret))
		__folio_clear_locked(folio);
	else {
		 
		WARN_ON_ONCE(folio_test_active(folio));
		if (!(gfp & __GFP_WRITE) && shadow)
			workingset_refault(folio, shadow);
		folio_add_lru(folio);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(filemap_add_folio);

#ifdef CONFIG_NUMA
struct folio *filemap_alloc_folio(gfp_t gfp, unsigned int order)
{
	int n;
	struct folio *folio;

	if (cpuset_do_page_mem_spread()) {
		unsigned int cpuset_mems_cookie;
		do {
			cpuset_mems_cookie = read_mems_allowed_begin();
			n = cpuset_mem_spread_node();
			folio = __folio_alloc_node(gfp, order, n);
		} while (!folio && read_mems_allowed_retry(cpuset_mems_cookie));

		return folio;
	}
	return folio_alloc(gfp, order);
}
EXPORT_SYMBOL(filemap_alloc_folio);
#endif

 
void filemap_invalidate_lock_two(struct address_space *mapping1,
				 struct address_space *mapping2)
{
	if (mapping1 > mapping2)
		swap(mapping1, mapping2);
	if (mapping1)
		down_write(&mapping1->invalidate_lock);
	if (mapping2 && mapping1 != mapping2)
		down_write_nested(&mapping2->invalidate_lock, 1);
}
EXPORT_SYMBOL(filemap_invalidate_lock_two);

 
void filemap_invalidate_unlock_two(struct address_space *mapping1,
				   struct address_space *mapping2)
{
	if (mapping1)
		up_write(&mapping1->invalidate_lock);
	if (mapping2 && mapping1 != mapping2)
		up_write(&mapping2->invalidate_lock);
}
EXPORT_SYMBOL(filemap_invalidate_unlock_two);

 
#define PAGE_WAIT_TABLE_BITS 8
#define PAGE_WAIT_TABLE_SIZE (1 << PAGE_WAIT_TABLE_BITS)
static wait_queue_head_t folio_wait_table[PAGE_WAIT_TABLE_SIZE] __cacheline_aligned;

static wait_queue_head_t *folio_waitqueue(struct folio *folio)
{
	return &folio_wait_table[hash_ptr(folio, PAGE_WAIT_TABLE_BITS)];
}

void __init pagecache_init(void)
{
	int i;

	for (i = 0; i < PAGE_WAIT_TABLE_SIZE; i++)
		init_waitqueue_head(&folio_wait_table[i]);

	page_writeback_init();
}

 
static int wake_page_function(wait_queue_entry_t *wait, unsigned mode, int sync, void *arg)
{
	unsigned int flags;
	struct wait_page_key *key = arg;
	struct wait_page_queue *wait_page
		= container_of(wait, struct wait_page_queue, wait);

	if (!wake_page_match(wait_page, key))
		return 0;

	 
	flags = wait->flags;
	if (flags & WQ_FLAG_EXCLUSIVE) {
		if (test_bit(key->bit_nr, &key->folio->flags))
			return -1;
		if (flags & WQ_FLAG_CUSTOM) {
			if (test_and_set_bit(key->bit_nr, &key->folio->flags))
				return -1;
			flags |= WQ_FLAG_DONE;
		}
	}

	 
	smp_store_release(&wait->flags, flags | WQ_FLAG_WOKEN);
	wake_up_state(wait->private, mode);

	 
	list_del_init_careful(&wait->entry);
	return (flags & WQ_FLAG_EXCLUSIVE) != 0;
}

static void folio_wake_bit(struct folio *folio, int bit_nr)
{
	wait_queue_head_t *q = folio_waitqueue(folio);
	struct wait_page_key key;
	unsigned long flags;
	wait_queue_entry_t bookmark;

	key.folio = folio;
	key.bit_nr = bit_nr;
	key.page_match = 0;

	bookmark.flags = 0;
	bookmark.private = NULL;
	bookmark.func = NULL;
	INIT_LIST_HEAD(&bookmark.entry);

	spin_lock_irqsave(&q->lock, flags);
	__wake_up_locked_key_bookmark(q, TASK_NORMAL, &key, &bookmark);

	while (bookmark.flags & WQ_FLAG_BOOKMARK) {
		 
		spin_unlock_irqrestore(&q->lock, flags);
		cpu_relax();
		spin_lock_irqsave(&q->lock, flags);
		__wake_up_locked_key_bookmark(q, TASK_NORMAL, &key, &bookmark);
	}

	 
	if (!waitqueue_active(q) || !key.page_match)
		folio_clear_waiters(folio);

	spin_unlock_irqrestore(&q->lock, flags);
}

static void folio_wake(struct folio *folio, int bit)
{
	if (!folio_test_waiters(folio))
		return;
	folio_wake_bit(folio, bit);
}

 
enum behavior {
	EXCLUSIVE,	 
	SHARED,		 
	DROP,		 
};

 
static inline bool folio_trylock_flag(struct folio *folio, int bit_nr,
					struct wait_queue_entry *wait)
{
	if (wait->flags & WQ_FLAG_EXCLUSIVE) {
		if (test_and_set_bit(bit_nr, &folio->flags))
			return false;
	} else if (test_bit(bit_nr, &folio->flags))
		return false;

	wait->flags |= WQ_FLAG_WOKEN | WQ_FLAG_DONE;
	return true;
}

 
int sysctl_page_lock_unfairness = 5;

static inline int folio_wait_bit_common(struct folio *folio, int bit_nr,
		int state, enum behavior behavior)
{
	wait_queue_head_t *q = folio_waitqueue(folio);
	int unfairness = sysctl_page_lock_unfairness;
	struct wait_page_queue wait_page;
	wait_queue_entry_t *wait = &wait_page.wait;
	bool thrashing = false;
	unsigned long pflags;
	bool in_thrashing;

	if (bit_nr == PG_locked &&
	    !folio_test_uptodate(folio) && folio_test_workingset(folio)) {
		delayacct_thrashing_start(&in_thrashing);
		psi_memstall_enter(&pflags);
		thrashing = true;
	}

	init_wait(wait);
	wait->func = wake_page_function;
	wait_page.folio = folio;
	wait_page.bit_nr = bit_nr;

repeat:
	wait->flags = 0;
	if (behavior == EXCLUSIVE) {
		wait->flags = WQ_FLAG_EXCLUSIVE;
		if (--unfairness < 0)
			wait->flags |= WQ_FLAG_CUSTOM;
	}

	 
	spin_lock_irq(&q->lock);
	folio_set_waiters(folio);
	if (!folio_trylock_flag(folio, bit_nr, wait))
		__add_wait_queue_entry_tail(q, wait);
	spin_unlock_irq(&q->lock);

	 
	if (behavior == DROP)
		folio_put(folio);

	 
	for (;;) {
		unsigned int flags;

		set_current_state(state);

		 
		flags = smp_load_acquire(&wait->flags);
		if (!(flags & WQ_FLAG_WOKEN)) {
			if (signal_pending_state(state, current))
				break;

			io_schedule();
			continue;
		}

		 
		if (behavior != EXCLUSIVE)
			break;

		 
		if (flags & WQ_FLAG_DONE)
			break;

		 
		if (unlikely(test_and_set_bit(bit_nr, folio_flags(folio, 0))))
			goto repeat;

		wait->flags |= WQ_FLAG_DONE;
		break;
	}

	 
	finish_wait(q, wait);

	if (thrashing) {
		delayacct_thrashing_end(&in_thrashing);
		psi_memstall_leave(&pflags);
	}

	 
	if (behavior == EXCLUSIVE)
		return wait->flags & WQ_FLAG_DONE ? 0 : -EINTR;

	return wait->flags & WQ_FLAG_WOKEN ? 0 : -EINTR;
}

#ifdef CONFIG_MIGRATION
 
void migration_entry_wait_on_locked(swp_entry_t entry, spinlock_t *ptl)
	__releases(ptl)
{
	struct wait_page_queue wait_page;
	wait_queue_entry_t *wait = &wait_page.wait;
	bool thrashing = false;
	unsigned long pflags;
	bool in_thrashing;
	wait_queue_head_t *q;
	struct folio *folio = page_folio(pfn_swap_entry_to_page(entry));

	q = folio_waitqueue(folio);
	if (!folio_test_uptodate(folio) && folio_test_workingset(folio)) {
		delayacct_thrashing_start(&in_thrashing);
		psi_memstall_enter(&pflags);
		thrashing = true;
	}

	init_wait(wait);
	wait->func = wake_page_function;
	wait_page.folio = folio;
	wait_page.bit_nr = PG_locked;
	wait->flags = 0;

	spin_lock_irq(&q->lock);
	folio_set_waiters(folio);
	if (!folio_trylock_flag(folio, PG_locked, wait))
		__add_wait_queue_entry_tail(q, wait);
	spin_unlock_irq(&q->lock);

	 
	spin_unlock(ptl);

	for (;;) {
		unsigned int flags;

		set_current_state(TASK_UNINTERRUPTIBLE);

		 
		flags = smp_load_acquire(&wait->flags);
		if (!(flags & WQ_FLAG_WOKEN)) {
			if (signal_pending_state(TASK_UNINTERRUPTIBLE, current))
				break;

			io_schedule();
			continue;
		}
		break;
	}

	finish_wait(q, wait);

	if (thrashing) {
		delayacct_thrashing_end(&in_thrashing);
		psi_memstall_leave(&pflags);
	}
}
#endif

void folio_wait_bit(struct folio *folio, int bit_nr)
{
	folio_wait_bit_common(folio, bit_nr, TASK_UNINTERRUPTIBLE, SHARED);
}
EXPORT_SYMBOL(folio_wait_bit);

int folio_wait_bit_killable(struct folio *folio, int bit_nr)
{
	return folio_wait_bit_common(folio, bit_nr, TASK_KILLABLE, SHARED);
}
EXPORT_SYMBOL(folio_wait_bit_killable);

 
static int folio_put_wait_locked(struct folio *folio, int state)
{
	return folio_wait_bit_common(folio, PG_locked, state, DROP);
}

 
void folio_add_wait_queue(struct folio *folio, wait_queue_entry_t *waiter)
{
	wait_queue_head_t *q = folio_waitqueue(folio);
	unsigned long flags;

	spin_lock_irqsave(&q->lock, flags);
	__add_wait_queue_entry_tail(q, waiter);
	folio_set_waiters(folio);
	spin_unlock_irqrestore(&q->lock, flags);
}
EXPORT_SYMBOL_GPL(folio_add_wait_queue);

#ifndef clear_bit_unlock_is_negative_byte

 
static inline bool clear_bit_unlock_is_negative_byte(long nr, volatile void *mem)
{
	clear_bit_unlock(nr, mem);
	 
	return test_bit(PG_waiters, mem);
}

#endif

 
void folio_unlock(struct folio *folio)
{
	 
	BUILD_BUG_ON(PG_waiters != 7);
	BUILD_BUG_ON(PG_locked > 7);
	VM_BUG_ON_FOLIO(!folio_test_locked(folio), folio);
	if (clear_bit_unlock_is_negative_byte(PG_locked, folio_flags(folio, 0)))
		folio_wake_bit(folio, PG_locked);
}
EXPORT_SYMBOL(folio_unlock);

 
void folio_end_private_2(struct folio *folio)
{
	VM_BUG_ON_FOLIO(!folio_test_private_2(folio), folio);
	clear_bit_unlock(PG_private_2, folio_flags(folio, 0));
	folio_wake_bit(folio, PG_private_2);
	folio_put(folio);
}
EXPORT_SYMBOL(folio_end_private_2);

 
void folio_wait_private_2(struct folio *folio)
{
	while (folio_test_private_2(folio))
		folio_wait_bit(folio, PG_private_2);
}
EXPORT_SYMBOL(folio_wait_private_2);

 
int folio_wait_private_2_killable(struct folio *folio)
{
	int ret = 0;

	while (folio_test_private_2(folio)) {
		ret = folio_wait_bit_killable(folio, PG_private_2);
		if (ret < 0)
			break;
	}

	return ret;
}
EXPORT_SYMBOL(folio_wait_private_2_killable);

 
void folio_end_writeback(struct folio *folio)
{
	 
	if (folio_test_reclaim(folio)) {
		folio_clear_reclaim(folio);
		folio_rotate_reclaimable(folio);
	}

	 
	folio_get(folio);
	if (!__folio_end_writeback(folio))
		BUG();

	smp_mb__after_atomic();
	folio_wake(folio, PG_writeback);
	acct_reclaim_writeback(folio);
	folio_put(folio);
}
EXPORT_SYMBOL(folio_end_writeback);

 
void __folio_lock(struct folio *folio)
{
	folio_wait_bit_common(folio, PG_locked, TASK_UNINTERRUPTIBLE,
				EXCLUSIVE);
}
EXPORT_SYMBOL(__folio_lock);

int __folio_lock_killable(struct folio *folio)
{
	return folio_wait_bit_common(folio, PG_locked, TASK_KILLABLE,
					EXCLUSIVE);
}
EXPORT_SYMBOL_GPL(__folio_lock_killable);

static int __folio_lock_async(struct folio *folio, struct wait_page_queue *wait)
{
	struct wait_queue_head *q = folio_waitqueue(folio);
	int ret = 0;

	wait->folio = folio;
	wait->bit_nr = PG_locked;

	spin_lock_irq(&q->lock);
	__add_wait_queue_entry_tail(q, &wait->wait);
	folio_set_waiters(folio);
	ret = !folio_trylock(folio);
	 
	if (!ret)
		__remove_wait_queue(q, &wait->wait);
	else
		ret = -EIOCBQUEUED;
	spin_unlock_irq(&q->lock);
	return ret;
}

 
vm_fault_t __folio_lock_or_retry(struct folio *folio, struct vm_fault *vmf)
{
	unsigned int flags = vmf->flags;

	if (fault_flag_allow_retry_first(flags)) {
		 
		if (flags & FAULT_FLAG_RETRY_NOWAIT)
			return VM_FAULT_RETRY;

		release_fault_lock(vmf);
		if (flags & FAULT_FLAG_KILLABLE)
			folio_wait_locked_killable(folio);
		else
			folio_wait_locked(folio);
		return VM_FAULT_RETRY;
	}
	if (flags & FAULT_FLAG_KILLABLE) {
		bool ret;

		ret = __folio_lock_killable(folio);
		if (ret) {
			release_fault_lock(vmf);
			return VM_FAULT_RETRY;
		}
	} else {
		__folio_lock(folio);
	}

	return 0;
}

 
pgoff_t page_cache_next_miss(struct address_space *mapping,
			     pgoff_t index, unsigned long max_scan)
{
	XA_STATE(xas, &mapping->i_pages, index);

	while (max_scan--) {
		void *entry = xas_next(&xas);
		if (!entry || xa_is_value(entry))
			break;
		if (xas.xa_index == 0)
			break;
	}

	return xas.xa_index;
}
EXPORT_SYMBOL(page_cache_next_miss);

 
pgoff_t page_cache_prev_miss(struct address_space *mapping,
			     pgoff_t index, unsigned long max_scan)
{
	XA_STATE(xas, &mapping->i_pages, index);

	while (max_scan--) {
		void *entry = xas_prev(&xas);
		if (!entry || xa_is_value(entry))
			break;
		if (xas.xa_index == ULONG_MAX)
			break;
	}

	return xas.xa_index;
}
EXPORT_SYMBOL(page_cache_prev_miss);

 

 
void *filemap_get_entry(struct address_space *mapping, pgoff_t index)
{
	XA_STATE(xas, &mapping->i_pages, index);
	struct folio *folio;

	rcu_read_lock();
repeat:
	xas_reset(&xas);
	folio = xas_load(&xas);
	if (xas_retry(&xas, folio))
		goto repeat;
	 
	if (!folio || xa_is_value(folio))
		goto out;

	if (!folio_try_get_rcu(folio))
		goto repeat;

	if (unlikely(folio != xas_reload(&xas))) {
		folio_put(folio);
		goto repeat;
	}
out:
	rcu_read_unlock();

	return folio;
}

 
struct folio *__filemap_get_folio(struct address_space *mapping, pgoff_t index,
		fgf_t fgp_flags, gfp_t gfp)
{
	struct folio *folio;

repeat:
	folio = filemap_get_entry(mapping, index);
	if (xa_is_value(folio))
		folio = NULL;
	if (!folio)
		goto no_page;

	if (fgp_flags & FGP_LOCK) {
		if (fgp_flags & FGP_NOWAIT) {
			if (!folio_trylock(folio)) {
				folio_put(folio);
				return ERR_PTR(-EAGAIN);
			}
		} else {
			folio_lock(folio);
		}

		 
		if (unlikely(folio->mapping != mapping)) {
			folio_unlock(folio);
			folio_put(folio);
			goto repeat;
		}
		VM_BUG_ON_FOLIO(!folio_contains(folio, index), folio);
	}

	if (fgp_flags & FGP_ACCESSED)
		folio_mark_accessed(folio);
	else if (fgp_flags & FGP_WRITE) {
		 
		if (folio_test_idle(folio))
			folio_clear_idle(folio);
	}

	if (fgp_flags & FGP_STABLE)
		folio_wait_stable(folio);
no_page:
	if (!folio && (fgp_flags & FGP_CREAT)) {
		unsigned order = FGF_GET_ORDER(fgp_flags);
		int err;

		if ((fgp_flags & FGP_WRITE) && mapping_can_writeback(mapping))
			gfp |= __GFP_WRITE;
		if (fgp_flags & FGP_NOFS)
			gfp &= ~__GFP_FS;
		if (fgp_flags & FGP_NOWAIT) {
			gfp &= ~GFP_KERNEL;
			gfp |= GFP_NOWAIT | __GFP_NOWARN;
		}
		if (WARN_ON_ONCE(!(fgp_flags & (FGP_LOCK | FGP_FOR_MMAP))))
			fgp_flags |= FGP_LOCK;

		if (!mapping_large_folio_support(mapping))
			order = 0;
		if (order > MAX_PAGECACHE_ORDER)
			order = MAX_PAGECACHE_ORDER;
		 
		if (index & ((1UL << order) - 1))
			order = __ffs(index);

		do {
			gfp_t alloc_gfp = gfp;

			err = -ENOMEM;
			if (order == 1)
				order = 0;
			if (order > 0)
				alloc_gfp |= __GFP_NORETRY | __GFP_NOWARN;
			folio = filemap_alloc_folio(alloc_gfp, order);
			if (!folio)
				continue;

			 
			if (fgp_flags & FGP_ACCESSED)
				__folio_set_referenced(folio);

			err = filemap_add_folio(mapping, folio, index, gfp);
			if (!err)
				break;
			folio_put(folio);
			folio = NULL;
		} while (order-- > 0);

		if (err == -EEXIST)
			goto repeat;
		if (err)
			return ERR_PTR(err);
		 
		if (folio && (fgp_flags & FGP_FOR_MMAP))
			folio_unlock(folio);
	}

	if (!folio)
		return ERR_PTR(-ENOENT);
	return folio;
}
EXPORT_SYMBOL(__filemap_get_folio);

static inline struct folio *find_get_entry(struct xa_state *xas, pgoff_t max,
		xa_mark_t mark)
{
	struct folio *folio;

retry:
	if (mark == XA_PRESENT)
		folio = xas_find(xas, max);
	else
		folio = xas_find_marked(xas, max, mark);

	if (xas_retry(xas, folio))
		goto retry;
	 
	if (!folio || xa_is_value(folio))
		return folio;

	if (!folio_try_get_rcu(folio))
		goto reset;

	if (unlikely(folio != xas_reload(xas))) {
		folio_put(folio);
		goto reset;
	}

	return folio;
reset:
	xas_reset(xas);
	goto retry;
}

 
unsigned find_get_entries(struct address_space *mapping, pgoff_t *start,
		pgoff_t end, struct folio_batch *fbatch, pgoff_t *indices)
{
	XA_STATE(xas, &mapping->i_pages, *start);
	struct folio *folio;

	rcu_read_lock();
	while ((folio = find_get_entry(&xas, end, XA_PRESENT)) != NULL) {
		indices[fbatch->nr] = xas.xa_index;
		if (!folio_batch_add(fbatch, folio))
			break;
	}
	rcu_read_unlock();

	if (folio_batch_count(fbatch)) {
		unsigned long nr = 1;
		int idx = folio_batch_count(fbatch) - 1;

		folio = fbatch->folios[idx];
		if (!xa_is_value(folio) && !folio_test_hugetlb(folio))
			nr = folio_nr_pages(folio);
		*start = indices[idx] + nr;
	}
	return folio_batch_count(fbatch);
}

 
unsigned find_lock_entries(struct address_space *mapping, pgoff_t *start,
		pgoff_t end, struct folio_batch *fbatch, pgoff_t *indices)
{
	XA_STATE(xas, &mapping->i_pages, *start);
	struct folio *folio;

	rcu_read_lock();
	while ((folio = find_get_entry(&xas, end, XA_PRESENT))) {
		if (!xa_is_value(folio)) {
			if (folio->index < *start)
				goto put;
			if (folio_next_index(folio) - 1 > end)
				goto put;
			if (!folio_trylock(folio))
				goto put;
			if (folio->mapping != mapping ||
			    folio_test_writeback(folio))
				goto unlock;
			VM_BUG_ON_FOLIO(!folio_contains(folio, xas.xa_index),
					folio);
		}
		indices[fbatch->nr] = xas.xa_index;
		if (!folio_batch_add(fbatch, folio))
			break;
		continue;
unlock:
		folio_unlock(folio);
put:
		folio_put(folio);
	}
	rcu_read_unlock();

	if (folio_batch_count(fbatch)) {
		unsigned long nr = 1;
		int idx = folio_batch_count(fbatch) - 1;

		folio = fbatch->folios[idx];
		if (!xa_is_value(folio) && !folio_test_hugetlb(folio))
			nr = folio_nr_pages(folio);
		*start = indices[idx] + nr;
	}
	return folio_batch_count(fbatch);
}

 
unsigned filemap_get_folios(struct address_space *mapping, pgoff_t *start,
		pgoff_t end, struct folio_batch *fbatch)
{
	XA_STATE(xas, &mapping->i_pages, *start);
	struct folio *folio;

	rcu_read_lock();
	while ((folio = find_get_entry(&xas, end, XA_PRESENT)) != NULL) {
		 
		if (xa_is_value(folio))
			continue;
		if (!folio_batch_add(fbatch, folio)) {
			unsigned long nr = folio_nr_pages(folio);

			if (folio_test_hugetlb(folio))
				nr = 1;
			*start = folio->index + nr;
			goto out;
		}
	}

	 
	if (end == (pgoff_t)-1)
		*start = (pgoff_t)-1;
	else
		*start = end + 1;
out:
	rcu_read_unlock();

	return folio_batch_count(fbatch);
}
EXPORT_SYMBOL(filemap_get_folios);

 

unsigned filemap_get_folios_contig(struct address_space *mapping,
		pgoff_t *start, pgoff_t end, struct folio_batch *fbatch)
{
	XA_STATE(xas, &mapping->i_pages, *start);
	unsigned long nr;
	struct folio *folio;

	rcu_read_lock();

	for (folio = xas_load(&xas); folio && xas.xa_index <= end;
			folio = xas_next(&xas)) {
		if (xas_retry(&xas, folio))
			continue;
		 
		if (xa_is_value(folio))
			goto update_start;

		if (!folio_try_get_rcu(folio))
			goto retry;

		if (unlikely(folio != xas_reload(&xas)))
			goto put_folio;

		if (!folio_batch_add(fbatch, folio)) {
			nr = folio_nr_pages(folio);

			if (folio_test_hugetlb(folio))
				nr = 1;
			*start = folio->index + nr;
			goto out;
		}
		continue;
put_folio:
		folio_put(folio);

retry:
		xas_reset(&xas);
	}

update_start:
	nr = folio_batch_count(fbatch);

	if (nr) {
		folio = fbatch->folios[nr - 1];
		if (folio_test_hugetlb(folio))
			*start = folio->index + 1;
		else
			*start = folio_next_index(folio);
	}
out:
	rcu_read_unlock();
	return folio_batch_count(fbatch);
}
EXPORT_SYMBOL(filemap_get_folios_contig);

 
unsigned filemap_get_folios_tag(struct address_space *mapping, pgoff_t *start,
			pgoff_t end, xa_mark_t tag, struct folio_batch *fbatch)
{
	XA_STATE(xas, &mapping->i_pages, *start);
	struct folio *folio;

	rcu_read_lock();
	while ((folio = find_get_entry(&xas, end, tag)) != NULL) {
		 
		if (xa_is_value(folio))
			continue;
		if (!folio_batch_add(fbatch, folio)) {
			unsigned long nr = folio_nr_pages(folio);

			if (folio_test_hugetlb(folio))
				nr = 1;
			*start = folio->index + nr;
			goto out;
		}
	}
	 
	if (end == (pgoff_t)-1)
		*start = (pgoff_t)-1;
	else
		*start = end + 1;
out:
	rcu_read_unlock();

	return folio_batch_count(fbatch);
}
EXPORT_SYMBOL(filemap_get_folios_tag);

 
static void shrink_readahead_size_eio(struct file_ra_state *ra)
{
	ra->ra_pages /= 4;
}

 
static void filemap_get_read_batch(struct address_space *mapping,
		pgoff_t index, pgoff_t max, struct folio_batch *fbatch)
{
	XA_STATE(xas, &mapping->i_pages, index);
	struct folio *folio;

	rcu_read_lock();
	for (folio = xas_load(&xas); folio; folio = xas_next(&xas)) {
		if (xas_retry(&xas, folio))
			continue;
		if (xas.xa_index > max || xa_is_value(folio))
			break;
		if (xa_is_sibling(folio))
			break;
		if (!folio_try_get_rcu(folio))
			goto retry;

		if (unlikely(folio != xas_reload(&xas)))
			goto put_folio;

		if (!folio_batch_add(fbatch, folio))
			break;
		if (!folio_test_uptodate(folio))
			break;
		if (folio_test_readahead(folio))
			break;
		xas_advance(&xas, folio_next_index(folio) - 1);
		continue;
put_folio:
		folio_put(folio);
retry:
		xas_reset(&xas);
	}
	rcu_read_unlock();
}

static int filemap_read_folio(struct file *file, filler_t filler,
		struct folio *folio)
{
	bool workingset = folio_test_workingset(folio);
	unsigned long pflags;
	int error;

	 
	folio_clear_error(folio);

	 
	if (unlikely(workingset))
		psi_memstall_enter(&pflags);
	error = filler(file, folio);
	if (unlikely(workingset))
		psi_memstall_leave(&pflags);
	if (error)
		return error;

	error = folio_wait_locked_killable(folio);
	if (error)
		return error;
	if (folio_test_uptodate(folio))
		return 0;
	if (file)
		shrink_readahead_size_eio(&file->f_ra);
	return -EIO;
}

static bool filemap_range_uptodate(struct address_space *mapping,
		loff_t pos, size_t count, struct folio *folio,
		bool need_uptodate)
{
	if (folio_test_uptodate(folio))
		return true;
	 
	if (need_uptodate)
		return false;
	if (!mapping->a_ops->is_partially_uptodate)
		return false;
	if (mapping->host->i_blkbits >= folio_shift(folio))
		return false;

	if (folio_pos(folio) > pos) {
		count -= folio_pos(folio) - pos;
		pos = 0;
	} else {
		pos -= folio_pos(folio);
	}

	return mapping->a_ops->is_partially_uptodate(folio, pos, count);
}

static int filemap_update_page(struct kiocb *iocb,
		struct address_space *mapping, size_t count,
		struct folio *folio, bool need_uptodate)
{
	int error;

	if (iocb->ki_flags & IOCB_NOWAIT) {
		if (!filemap_invalidate_trylock_shared(mapping))
			return -EAGAIN;
	} else {
		filemap_invalidate_lock_shared(mapping);
	}

	if (!folio_trylock(folio)) {
		error = -EAGAIN;
		if (iocb->ki_flags & (IOCB_NOWAIT | IOCB_NOIO))
			goto unlock_mapping;
		if (!(iocb->ki_flags & IOCB_WAITQ)) {
			filemap_invalidate_unlock_shared(mapping);
			 
			folio_put_wait_locked(folio, TASK_KILLABLE);
			return AOP_TRUNCATED_PAGE;
		}
		error = __folio_lock_async(folio, iocb->ki_waitq);
		if (error)
			goto unlock_mapping;
	}

	error = AOP_TRUNCATED_PAGE;
	if (!folio->mapping)
		goto unlock;

	error = 0;
	if (filemap_range_uptodate(mapping, iocb->ki_pos, count, folio,
				   need_uptodate))
		goto unlock;

	error = -EAGAIN;
	if (iocb->ki_flags & (IOCB_NOIO | IOCB_NOWAIT | IOCB_WAITQ))
		goto unlock;

	error = filemap_read_folio(iocb->ki_filp, mapping->a_ops->read_folio,
			folio);
	goto unlock_mapping;
unlock:
	folio_unlock(folio);
unlock_mapping:
	filemap_invalidate_unlock_shared(mapping);
	if (error == AOP_TRUNCATED_PAGE)
		folio_put(folio);
	return error;
}

static int filemap_create_folio(struct file *file,
		struct address_space *mapping, pgoff_t index,
		struct folio_batch *fbatch)
{
	struct folio *folio;
	int error;

	folio = filemap_alloc_folio(mapping_gfp_mask(mapping), 0);
	if (!folio)
		return -ENOMEM;

	 
	filemap_invalidate_lock_shared(mapping);
	error = filemap_add_folio(mapping, folio, index,
			mapping_gfp_constraint(mapping, GFP_KERNEL));
	if (error == -EEXIST)
		error = AOP_TRUNCATED_PAGE;
	if (error)
		goto error;

	error = filemap_read_folio(file, mapping->a_ops->read_folio, folio);
	if (error)
		goto error;

	filemap_invalidate_unlock_shared(mapping);
	folio_batch_add(fbatch, folio);
	return 0;
error:
	filemap_invalidate_unlock_shared(mapping);
	folio_put(folio);
	return error;
}

static int filemap_readahead(struct kiocb *iocb, struct file *file,
		struct address_space *mapping, struct folio *folio,
		pgoff_t last_index)
{
	DEFINE_READAHEAD(ractl, file, &file->f_ra, mapping, folio->index);

	if (iocb->ki_flags & IOCB_NOIO)
		return -EAGAIN;
	page_cache_async_ra(&ractl, folio, last_index - folio->index);
	return 0;
}

static int filemap_get_pages(struct kiocb *iocb, size_t count,
		struct folio_batch *fbatch, bool need_uptodate)
{
	struct file *filp = iocb->ki_filp;
	struct address_space *mapping = filp->f_mapping;
	struct file_ra_state *ra = &filp->f_ra;
	pgoff_t index = iocb->ki_pos >> PAGE_SHIFT;
	pgoff_t last_index;
	struct folio *folio;
	int err = 0;

	 
	last_index = DIV_ROUND_UP(iocb->ki_pos + count, PAGE_SIZE);
retry:
	if (fatal_signal_pending(current))
		return -EINTR;

	filemap_get_read_batch(mapping, index, last_index - 1, fbatch);
	if (!folio_batch_count(fbatch)) {
		if (iocb->ki_flags & IOCB_NOIO)
			return -EAGAIN;
		page_cache_sync_readahead(mapping, ra, filp, index,
				last_index - index);
		filemap_get_read_batch(mapping, index, last_index - 1, fbatch);
	}
	if (!folio_batch_count(fbatch)) {
		if (iocb->ki_flags & (IOCB_NOWAIT | IOCB_WAITQ))
			return -EAGAIN;
		err = filemap_create_folio(filp, mapping,
				iocb->ki_pos >> PAGE_SHIFT, fbatch);
		if (err == AOP_TRUNCATED_PAGE)
			goto retry;
		return err;
	}

	folio = fbatch->folios[folio_batch_count(fbatch) - 1];
	if (folio_test_readahead(folio)) {
		err = filemap_readahead(iocb, filp, mapping, folio, last_index);
		if (err)
			goto err;
	}
	if (!folio_test_uptodate(folio)) {
		if ((iocb->ki_flags & IOCB_WAITQ) &&
		    folio_batch_count(fbatch) > 1)
			iocb->ki_flags |= IOCB_NOWAIT;
		err = filemap_update_page(iocb, mapping, count, folio,
					  need_uptodate);
		if (err)
			goto err;
	}

	return 0;
err:
	if (err < 0)
		folio_put(folio);
	if (likely(--fbatch->nr))
		return 0;
	if (err == AOP_TRUNCATED_PAGE)
		goto retry;
	return err;
}

static inline bool pos_same_folio(loff_t pos1, loff_t pos2, struct folio *folio)
{
	unsigned int shift = folio_shift(folio);

	return (pos1 >> shift == pos2 >> shift);
}

 
ssize_t filemap_read(struct kiocb *iocb, struct iov_iter *iter,
		ssize_t already_read)
{
	struct file *filp = iocb->ki_filp;
	struct file_ra_state *ra = &filp->f_ra;
	struct address_space *mapping = filp->f_mapping;
	struct inode *inode = mapping->host;
	struct folio_batch fbatch;
	int i, error = 0;
	bool writably_mapped;
	loff_t isize, end_offset;
	loff_t last_pos = ra->prev_pos;

	if (unlikely(iocb->ki_pos >= inode->i_sb->s_maxbytes))
		return 0;
	if (unlikely(!iov_iter_count(iter)))
		return 0;

	iov_iter_truncate(iter, inode->i_sb->s_maxbytes);
	folio_batch_init(&fbatch);

	do {
		cond_resched();

		 
		if ((iocb->ki_flags & IOCB_WAITQ) && already_read)
			iocb->ki_flags |= IOCB_NOWAIT;

		if (unlikely(iocb->ki_pos >= i_size_read(inode)))
			break;

		error = filemap_get_pages(iocb, iter->count, &fbatch, false);
		if (error < 0)
			break;

		 
		isize = i_size_read(inode);
		if (unlikely(iocb->ki_pos >= isize))
			goto put_folios;
		end_offset = min_t(loff_t, isize, iocb->ki_pos + iter->count);

		 
		smp_rmb();

		 
		writably_mapped = mapping_writably_mapped(mapping);

		 
		if (!pos_same_folio(iocb->ki_pos, last_pos - 1,
				    fbatch.folios[0]))
			folio_mark_accessed(fbatch.folios[0]);

		for (i = 0; i < folio_batch_count(&fbatch); i++) {
			struct folio *folio = fbatch.folios[i];
			size_t fsize = folio_size(folio);
			size_t offset = iocb->ki_pos & (fsize - 1);
			size_t bytes = min_t(loff_t, end_offset - iocb->ki_pos,
					     fsize - offset);
			size_t copied;

			if (end_offset < folio_pos(folio))
				break;
			if (i > 0)
				folio_mark_accessed(folio);
			 
			if (writably_mapped)
				flush_dcache_folio(folio);

			copied = copy_folio_to_iter(folio, offset, bytes, iter);

			already_read += copied;
			iocb->ki_pos += copied;
			last_pos = iocb->ki_pos;

			if (copied < bytes) {
				error = -EFAULT;
				break;
			}
		}
put_folios:
		for (i = 0; i < folio_batch_count(&fbatch); i++)
			folio_put(fbatch.folios[i]);
		folio_batch_init(&fbatch);
	} while (iov_iter_count(iter) && iocb->ki_pos < isize && !error);

	file_accessed(filp);
	ra->prev_pos = last_pos;
	return already_read ? already_read : error;
}
EXPORT_SYMBOL_GPL(filemap_read);

int kiocb_write_and_wait(struct kiocb *iocb, size_t count)
{
	struct address_space *mapping = iocb->ki_filp->f_mapping;
	loff_t pos = iocb->ki_pos;
	loff_t end = pos + count - 1;

	if (iocb->ki_flags & IOCB_NOWAIT) {
		if (filemap_range_needs_writeback(mapping, pos, end))
			return -EAGAIN;
		return 0;
	}

	return filemap_write_and_wait_range(mapping, pos, end);
}

int kiocb_invalidate_pages(struct kiocb *iocb, size_t count)
{
	struct address_space *mapping = iocb->ki_filp->f_mapping;
	loff_t pos = iocb->ki_pos;
	loff_t end = pos + count - 1;
	int ret;

	if (iocb->ki_flags & IOCB_NOWAIT) {
		 
		if (filemap_range_has_page(mapping, pos, end))
			return -EAGAIN;
	} else {
		ret = filemap_write_and_wait_range(mapping, pos, end);
		if (ret)
			return ret;
	}

	 
	return invalidate_inode_pages2_range(mapping, pos >> PAGE_SHIFT,
					     end >> PAGE_SHIFT);
}

 
ssize_t
generic_file_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	size_t count = iov_iter_count(iter);
	ssize_t retval = 0;

	if (!count)
		return 0;  

	if (iocb->ki_flags & IOCB_DIRECT) {
		struct file *file = iocb->ki_filp;
		struct address_space *mapping = file->f_mapping;
		struct inode *inode = mapping->host;

		retval = kiocb_write_and_wait(iocb, count);
		if (retval < 0)
			return retval;
		file_accessed(file);

		retval = mapping->a_ops->direct_IO(iocb, iter);
		if (retval >= 0) {
			iocb->ki_pos += retval;
			count -= retval;
		}
		if (retval != -EIOCBQUEUED)
			iov_iter_revert(iter, count - iov_iter_count(iter));

		 
		if (retval < 0 || !count || IS_DAX(inode))
			return retval;
		if (iocb->ki_pos >= i_size_read(inode))
			return retval;
	}

	return filemap_read(iocb, iter, retval);
}
EXPORT_SYMBOL(generic_file_read_iter);

 
size_t splice_folio_into_pipe(struct pipe_inode_info *pipe,
			      struct folio *folio, loff_t fpos, size_t size)
{
	struct page *page;
	size_t spliced = 0, offset = offset_in_folio(folio, fpos);

	page = folio_page(folio, offset / PAGE_SIZE);
	size = min(size, folio_size(folio) - offset);
	offset %= PAGE_SIZE;

	while (spliced < size &&
	       !pipe_full(pipe->head, pipe->tail, pipe->max_usage)) {
		struct pipe_buffer *buf = pipe_head_buf(pipe);
		size_t part = min_t(size_t, PAGE_SIZE - offset, size - spliced);

		*buf = (struct pipe_buffer) {
			.ops	= &page_cache_pipe_buf_ops,
			.page	= page,
			.offset	= offset,
			.len	= part,
		};
		folio_get(folio);
		pipe->head++;
		page++;
		spliced += part;
		offset = 0;
	}

	return spliced;
}

 
ssize_t filemap_splice_read(struct file *in, loff_t *ppos,
			    struct pipe_inode_info *pipe,
			    size_t len, unsigned int flags)
{
	struct folio_batch fbatch;
	struct kiocb iocb;
	size_t total_spliced = 0, used, npages;
	loff_t isize, end_offset;
	bool writably_mapped;
	int i, error = 0;

	if (unlikely(*ppos >= in->f_mapping->host->i_sb->s_maxbytes))
		return 0;

	init_sync_kiocb(&iocb, in);
	iocb.ki_pos = *ppos;

	 
	used = pipe_occupancy(pipe->head, pipe->tail);
	npages = max_t(ssize_t, pipe->max_usage - used, 0);
	len = min_t(size_t, len, npages * PAGE_SIZE);

	folio_batch_init(&fbatch);

	do {
		cond_resched();

		if (*ppos >= i_size_read(in->f_mapping->host))
			break;

		iocb.ki_pos = *ppos;
		error = filemap_get_pages(&iocb, len, &fbatch, true);
		if (error < 0)
			break;

		 
		isize = i_size_read(in->f_mapping->host);
		if (unlikely(*ppos >= isize))
			break;
		end_offset = min_t(loff_t, isize, *ppos + len);

		 
		writably_mapped = mapping_writably_mapped(in->f_mapping);

		for (i = 0; i < folio_batch_count(&fbatch); i++) {
			struct folio *folio = fbatch.folios[i];
			size_t n;

			if (folio_pos(folio) >= end_offset)
				goto out;
			folio_mark_accessed(folio);

			 
			if (writably_mapped)
				flush_dcache_folio(folio);

			n = min_t(loff_t, len, isize - *ppos);
			n = splice_folio_into_pipe(pipe, folio, *ppos, n);
			if (!n)
				goto out;
			len -= n;
			total_spliced += n;
			*ppos += n;
			in->f_ra.prev_pos = *ppos;
			if (pipe_full(pipe->head, pipe->tail, pipe->max_usage))
				goto out;
		}

		folio_batch_release(&fbatch);
	} while (len);

out:
	folio_batch_release(&fbatch);
	file_accessed(in);

	return total_spliced ? total_spliced : error;
}
EXPORT_SYMBOL(filemap_splice_read);

static inline loff_t folio_seek_hole_data(struct xa_state *xas,
		struct address_space *mapping, struct folio *folio,
		loff_t start, loff_t end, bool seek_data)
{
	const struct address_space_operations *ops = mapping->a_ops;
	size_t offset, bsz = i_blocksize(mapping->host);

	if (xa_is_value(folio) || folio_test_uptodate(folio))
		return seek_data ? start : end;
	if (!ops->is_partially_uptodate)
		return seek_data ? end : start;

	xas_pause(xas);
	rcu_read_unlock();
	folio_lock(folio);
	if (unlikely(folio->mapping != mapping))
		goto unlock;

	offset = offset_in_folio(folio, start) & ~(bsz - 1);

	do {
		if (ops->is_partially_uptodate(folio, offset, bsz) ==
							seek_data)
			break;
		start = (start + bsz) & ~(bsz - 1);
		offset += bsz;
	} while (offset < folio_size(folio));
unlock:
	folio_unlock(folio);
	rcu_read_lock();
	return start;
}

static inline size_t seek_folio_size(struct xa_state *xas, struct folio *folio)
{
	if (xa_is_value(folio))
		return PAGE_SIZE << xa_get_order(xas->xa, xas->xa_index);
	return folio_size(folio);
}

 
loff_t mapping_seek_hole_data(struct address_space *mapping, loff_t start,
		loff_t end, int whence)
{
	XA_STATE(xas, &mapping->i_pages, start >> PAGE_SHIFT);
	pgoff_t max = (end - 1) >> PAGE_SHIFT;
	bool seek_data = (whence == SEEK_DATA);
	struct folio *folio;

	if (end <= start)
		return -ENXIO;

	rcu_read_lock();
	while ((folio = find_get_entry(&xas, max, XA_PRESENT))) {
		loff_t pos = (u64)xas.xa_index << PAGE_SHIFT;
		size_t seek_size;

		if (start < pos) {
			if (!seek_data)
				goto unlock;
			start = pos;
		}

		seek_size = seek_folio_size(&xas, folio);
		pos = round_up((u64)pos + 1, seek_size);
		start = folio_seek_hole_data(&xas, mapping, folio, start, pos,
				seek_data);
		if (start < pos)
			goto unlock;
		if (start >= end)
			break;
		if (seek_size > PAGE_SIZE)
			xas_set(&xas, pos >> PAGE_SHIFT);
		if (!xa_is_value(folio))
			folio_put(folio);
	}
	if (seek_data)
		start = -ENXIO;
unlock:
	rcu_read_unlock();
	if (folio && !xa_is_value(folio))
		folio_put(folio);
	if (start > end)
		return end;
	return start;
}

#ifdef CONFIG_MMU
#define MMAP_LOTSAMISS  (100)
 
static int lock_folio_maybe_drop_mmap(struct vm_fault *vmf, struct folio *folio,
				     struct file **fpin)
{
	if (folio_trylock(folio))
		return 1;

	 
	if (vmf->flags & FAULT_FLAG_RETRY_NOWAIT)
		return 0;

	*fpin = maybe_unlock_mmap_for_io(vmf, *fpin);
	if (vmf->flags & FAULT_FLAG_KILLABLE) {
		if (__folio_lock_killable(folio)) {
			 
			if (*fpin == NULL)
				mmap_read_unlock(vmf->vma->vm_mm);
			return 0;
		}
	} else
		__folio_lock(folio);

	return 1;
}

 
static struct file *do_sync_mmap_readahead(struct vm_fault *vmf)
{
	struct file *file = vmf->vma->vm_file;
	struct file_ra_state *ra = &file->f_ra;
	struct address_space *mapping = file->f_mapping;
	DEFINE_READAHEAD(ractl, file, ra, mapping, vmf->pgoff);
	struct file *fpin = NULL;
	unsigned long vm_flags = vmf->vma->vm_flags;
	unsigned int mmap_miss;

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	 
	if (vm_flags & VM_HUGEPAGE) {
		fpin = maybe_unlock_mmap_for_io(vmf, fpin);
		ractl._index &= ~((unsigned long)HPAGE_PMD_NR - 1);
		ra->size = HPAGE_PMD_NR;
		 
		if (!(vm_flags & VM_RAND_READ))
			ra->size *= 2;
		ra->async_size = HPAGE_PMD_NR;
		page_cache_ra_order(&ractl, ra, HPAGE_PMD_ORDER);
		return fpin;
	}
#endif

	 
	if (vm_flags & VM_RAND_READ)
		return fpin;
	if (!ra->ra_pages)
		return fpin;

	if (vm_flags & VM_SEQ_READ) {
		fpin = maybe_unlock_mmap_for_io(vmf, fpin);
		page_cache_sync_ra(&ractl, ra->ra_pages);
		return fpin;
	}

	 
	mmap_miss = READ_ONCE(ra->mmap_miss);
	if (mmap_miss < MMAP_LOTSAMISS * 10)
		WRITE_ONCE(ra->mmap_miss, ++mmap_miss);

	 
	if (mmap_miss > MMAP_LOTSAMISS)
		return fpin;

	 
	fpin = maybe_unlock_mmap_for_io(vmf, fpin);
	ra->start = max_t(long, 0, vmf->pgoff - ra->ra_pages / 2);
	ra->size = ra->ra_pages;
	ra->async_size = ra->ra_pages / 4;
	ractl._index = ra->start;
	page_cache_ra_order(&ractl, ra, 0);
	return fpin;
}

 
static struct file *do_async_mmap_readahead(struct vm_fault *vmf,
					    struct folio *folio)
{
	struct file *file = vmf->vma->vm_file;
	struct file_ra_state *ra = &file->f_ra;
	DEFINE_READAHEAD(ractl, file, ra, file->f_mapping, vmf->pgoff);
	struct file *fpin = NULL;
	unsigned int mmap_miss;

	 
	if (vmf->vma->vm_flags & VM_RAND_READ || !ra->ra_pages)
		return fpin;

	mmap_miss = READ_ONCE(ra->mmap_miss);
	if (mmap_miss)
		WRITE_ONCE(ra->mmap_miss, --mmap_miss);

	if (folio_test_readahead(folio)) {
		fpin = maybe_unlock_mmap_for_io(vmf, fpin);
		page_cache_async_ra(&ractl, folio, ra->ra_pages);
	}
	return fpin;
}

 
vm_fault_t filemap_fault(struct vm_fault *vmf)
{
	int error;
	struct file *file = vmf->vma->vm_file;
	struct file *fpin = NULL;
	struct address_space *mapping = file->f_mapping;
	struct inode *inode = mapping->host;
	pgoff_t max_idx, index = vmf->pgoff;
	struct folio *folio;
	vm_fault_t ret = 0;
	bool mapping_locked = false;

	max_idx = DIV_ROUND_UP(i_size_read(inode), PAGE_SIZE);
	if (unlikely(index >= max_idx))
		return VM_FAULT_SIGBUS;

	 
	folio = filemap_get_folio(mapping, index);
	if (likely(!IS_ERR(folio))) {
		 
		if (!(vmf->flags & FAULT_FLAG_TRIED))
			fpin = do_async_mmap_readahead(vmf, folio);
		if (unlikely(!folio_test_uptodate(folio))) {
			filemap_invalidate_lock_shared(mapping);
			mapping_locked = true;
		}
	} else {
		 
		count_vm_event(PGMAJFAULT);
		count_memcg_event_mm(vmf->vma->vm_mm, PGMAJFAULT);
		ret = VM_FAULT_MAJOR;
		fpin = do_sync_mmap_readahead(vmf);
retry_find:
		 
		if (!mapping_locked) {
			filemap_invalidate_lock_shared(mapping);
			mapping_locked = true;
		}
		folio = __filemap_get_folio(mapping, index,
					  FGP_CREAT|FGP_FOR_MMAP,
					  vmf->gfp_mask);
		if (IS_ERR(folio)) {
			if (fpin)
				goto out_retry;
			filemap_invalidate_unlock_shared(mapping);
			return VM_FAULT_OOM;
		}
	}

	if (!lock_folio_maybe_drop_mmap(vmf, folio, &fpin))
		goto out_retry;

	 
	if (unlikely(folio->mapping != mapping)) {
		folio_unlock(folio);
		folio_put(folio);
		goto retry_find;
	}
	VM_BUG_ON_FOLIO(!folio_contains(folio, index), folio);

	 
	if (unlikely(!folio_test_uptodate(folio))) {
		 
		if (!mapping_locked) {
			folio_unlock(folio);
			folio_put(folio);
			goto retry_find;
		}
		goto page_not_uptodate;
	}

	 
	if (fpin) {
		folio_unlock(folio);
		goto out_retry;
	}
	if (mapping_locked)
		filemap_invalidate_unlock_shared(mapping);

	 
	max_idx = DIV_ROUND_UP(i_size_read(inode), PAGE_SIZE);
	if (unlikely(index >= max_idx)) {
		folio_unlock(folio);
		folio_put(folio);
		return VM_FAULT_SIGBUS;
	}

	vmf->page = folio_file_page(folio, index);
	return ret | VM_FAULT_LOCKED;

page_not_uptodate:
	 
	fpin = maybe_unlock_mmap_for_io(vmf, fpin);
	error = filemap_read_folio(file, mapping->a_ops->read_folio, folio);
	if (fpin)
		goto out_retry;
	folio_put(folio);

	if (!error || error == AOP_TRUNCATED_PAGE)
		goto retry_find;
	filemap_invalidate_unlock_shared(mapping);

	return VM_FAULT_SIGBUS;

out_retry:
	 
	if (!IS_ERR(folio))
		folio_put(folio);
	if (mapping_locked)
		filemap_invalidate_unlock_shared(mapping);
	if (fpin)
		fput(fpin);
	return ret | VM_FAULT_RETRY;
}
EXPORT_SYMBOL(filemap_fault);

static bool filemap_map_pmd(struct vm_fault *vmf, struct folio *folio,
		pgoff_t start)
{
	struct mm_struct *mm = vmf->vma->vm_mm;

	 
	if (pmd_trans_huge(*vmf->pmd)) {
		folio_unlock(folio);
		folio_put(folio);
		return true;
	}

	if (pmd_none(*vmf->pmd) && folio_test_pmd_mappable(folio)) {
		struct page *page = folio_file_page(folio, start);
		vm_fault_t ret = do_set_pmd(vmf, page);
		if (!ret) {
			 
			folio_unlock(folio);
			return true;
		}
	}

	if (pmd_none(*vmf->pmd) && vmf->prealloc_pte)
		pmd_install(mm, vmf->pmd, &vmf->prealloc_pte);

	return false;
}

static struct folio *next_uptodate_folio(struct xa_state *xas,
		struct address_space *mapping, pgoff_t end_pgoff)
{
	struct folio *folio = xas_next_entry(xas, end_pgoff);
	unsigned long max_idx;

	do {
		if (!folio)
			return NULL;
		if (xas_retry(xas, folio))
			continue;
		if (xa_is_value(folio))
			continue;
		if (folio_test_locked(folio))
			continue;
		if (!folio_try_get_rcu(folio))
			continue;
		 
		if (unlikely(folio != xas_reload(xas)))
			goto skip;
		if (!folio_test_uptodate(folio) || folio_test_readahead(folio))
			goto skip;
		if (!folio_trylock(folio))
			goto skip;
		if (folio->mapping != mapping)
			goto unlock;
		if (!folio_test_uptodate(folio))
			goto unlock;
		max_idx = DIV_ROUND_UP(i_size_read(mapping->host), PAGE_SIZE);
		if (xas->xa_index >= max_idx)
			goto unlock;
		return folio;
unlock:
		folio_unlock(folio);
skip:
		folio_put(folio);
	} while ((folio = xas_next_entry(xas, end_pgoff)) != NULL);

	return NULL;
}

 
static vm_fault_t filemap_map_folio_range(struct vm_fault *vmf,
			struct folio *folio, unsigned long start,
			unsigned long addr, unsigned int nr_pages,
			unsigned int *mmap_miss)
{
	vm_fault_t ret = 0;
	struct page *page = folio_page(folio, start);
	unsigned int count = 0;
	pte_t *old_ptep = vmf->pte;

	do {
		if (PageHWPoison(page + count))
			goto skip;

		(*mmap_miss)++;

		 
		if (!pte_none(vmf->pte[count]))
			goto skip;

		count++;
		continue;
skip:
		if (count) {
			set_pte_range(vmf, folio, page, count, addr);
			folio_ref_add(folio, count);
			if (in_range(vmf->address, addr, count * PAGE_SIZE))
				ret = VM_FAULT_NOPAGE;
		}

		count++;
		page += count;
		vmf->pte += count;
		addr += count * PAGE_SIZE;
		count = 0;
	} while (--nr_pages > 0);

	if (count) {
		set_pte_range(vmf, folio, page, count, addr);
		folio_ref_add(folio, count);
		if (in_range(vmf->address, addr, count * PAGE_SIZE))
			ret = VM_FAULT_NOPAGE;
	}

	vmf->pte = old_ptep;

	return ret;
}

static vm_fault_t filemap_map_order0_folio(struct vm_fault *vmf,
		struct folio *folio, unsigned long addr,
		unsigned int *mmap_miss)
{
	vm_fault_t ret = 0;
	struct page *page = &folio->page;

	if (PageHWPoison(page))
		return ret;

	(*mmap_miss)++;

	 
	if (!pte_none(ptep_get(vmf->pte)))
		return ret;

	if (vmf->address == addr)
		ret = VM_FAULT_NOPAGE;

	set_pte_range(vmf, folio, page, 1, addr);
	folio_ref_inc(folio);

	return ret;
}

vm_fault_t filemap_map_pages(struct vm_fault *vmf,
			     pgoff_t start_pgoff, pgoff_t end_pgoff)
{
	struct vm_area_struct *vma = vmf->vma;
	struct file *file = vma->vm_file;
	struct address_space *mapping = file->f_mapping;
	pgoff_t last_pgoff = start_pgoff;
	unsigned long addr;
	XA_STATE(xas, &mapping->i_pages, start_pgoff);
	struct folio *folio;
	vm_fault_t ret = 0;
	unsigned int nr_pages = 0, mmap_miss = 0, mmap_miss_saved;

	rcu_read_lock();
	folio = next_uptodate_folio(&xas, mapping, end_pgoff);
	if (!folio)
		goto out;

	if (filemap_map_pmd(vmf, folio, start_pgoff)) {
		ret = VM_FAULT_NOPAGE;
		goto out;
	}

	addr = vma->vm_start + ((start_pgoff - vma->vm_pgoff) << PAGE_SHIFT);
	vmf->pte = pte_offset_map_lock(vma->vm_mm, vmf->pmd, addr, &vmf->ptl);
	if (!vmf->pte) {
		folio_unlock(folio);
		folio_put(folio);
		goto out;
	}
	do {
		unsigned long end;

		addr += (xas.xa_index - last_pgoff) << PAGE_SHIFT;
		vmf->pte += xas.xa_index - last_pgoff;
		last_pgoff = xas.xa_index;
		end = folio->index + folio_nr_pages(folio) - 1;
		nr_pages = min(end, end_pgoff) - xas.xa_index + 1;

		if (!folio_test_large(folio))
			ret |= filemap_map_order0_folio(vmf,
					folio, addr, &mmap_miss);
		else
			ret |= filemap_map_folio_range(vmf, folio,
					xas.xa_index - folio->index, addr,
					nr_pages, &mmap_miss);

		folio_unlock(folio);
		folio_put(folio);
	} while ((folio = next_uptodate_folio(&xas, mapping, end_pgoff)) != NULL);
	pte_unmap_unlock(vmf->pte, vmf->ptl);
out:
	rcu_read_unlock();

	mmap_miss_saved = READ_ONCE(file->f_ra.mmap_miss);
	if (mmap_miss >= mmap_miss_saved)
		WRITE_ONCE(file->f_ra.mmap_miss, 0);
	else
		WRITE_ONCE(file->f_ra.mmap_miss, mmap_miss_saved - mmap_miss);

	return ret;
}
EXPORT_SYMBOL(filemap_map_pages);

vm_fault_t filemap_page_mkwrite(struct vm_fault *vmf)
{
	struct address_space *mapping = vmf->vma->vm_file->f_mapping;
	struct folio *folio = page_folio(vmf->page);
	vm_fault_t ret = VM_FAULT_LOCKED;

	sb_start_pagefault(mapping->host->i_sb);
	file_update_time(vmf->vma->vm_file);
	folio_lock(folio);
	if (folio->mapping != mapping) {
		folio_unlock(folio);
		ret = VM_FAULT_NOPAGE;
		goto out;
	}
	 
	folio_mark_dirty(folio);
	folio_wait_stable(folio);
out:
	sb_end_pagefault(mapping->host->i_sb);
	return ret;
}

const struct vm_operations_struct generic_file_vm_ops = {
	.fault		= filemap_fault,
	.map_pages	= filemap_map_pages,
	.page_mkwrite	= filemap_page_mkwrite,
};

 

int generic_file_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct address_space *mapping = file->f_mapping;

	if (!mapping->a_ops->read_folio)
		return -ENOEXEC;
	file_accessed(file);
	vma->vm_ops = &generic_file_vm_ops;
	return 0;
}

 
int generic_file_readonly_mmap(struct file *file, struct vm_area_struct *vma)
{
	if ((vma->vm_flags & VM_SHARED) && (vma->vm_flags & VM_MAYWRITE))
		return -EINVAL;
	return generic_file_mmap(file, vma);
}
#else
vm_fault_t filemap_page_mkwrite(struct vm_fault *vmf)
{
	return VM_FAULT_SIGBUS;
}
int generic_file_mmap(struct file *file, struct vm_area_struct *vma)
{
	return -ENOSYS;
}
int generic_file_readonly_mmap(struct file *file, struct vm_area_struct *vma)
{
	return -ENOSYS;
}
#endif  

EXPORT_SYMBOL(filemap_page_mkwrite);
EXPORT_SYMBOL(generic_file_mmap);
EXPORT_SYMBOL(generic_file_readonly_mmap);

static struct folio *do_read_cache_folio(struct address_space *mapping,
		pgoff_t index, filler_t filler, struct file *file, gfp_t gfp)
{
	struct folio *folio;
	int err;

	if (!filler)
		filler = mapping->a_ops->read_folio;
repeat:
	folio = filemap_get_folio(mapping, index);
	if (IS_ERR(folio)) {
		folio = filemap_alloc_folio(gfp, 0);
		if (!folio)
			return ERR_PTR(-ENOMEM);
		err = filemap_add_folio(mapping, folio, index, gfp);
		if (unlikely(err)) {
			folio_put(folio);
			if (err == -EEXIST)
				goto repeat;
			 
			return ERR_PTR(err);
		}

		goto filler;
	}
	if (folio_test_uptodate(folio))
		goto out;

	if (!folio_trylock(folio)) {
		folio_put_wait_locked(folio, TASK_UNINTERRUPTIBLE);
		goto repeat;
	}

	 
	if (!folio->mapping) {
		folio_unlock(folio);
		folio_put(folio);
		goto repeat;
	}

	 
	if (folio_test_uptodate(folio)) {
		folio_unlock(folio);
		goto out;
	}

filler:
	err = filemap_read_folio(file, filler, folio);
	if (err) {
		folio_put(folio);
		if (err == AOP_TRUNCATED_PAGE)
			goto repeat;
		return ERR_PTR(err);
	}

out:
	folio_mark_accessed(folio);
	return folio;
}

 
struct folio *read_cache_folio(struct address_space *mapping, pgoff_t index,
		filler_t filler, struct file *file)
{
	return do_read_cache_folio(mapping, index, filler, file,
			mapping_gfp_mask(mapping));
}
EXPORT_SYMBOL(read_cache_folio);

 
struct folio *mapping_read_folio_gfp(struct address_space *mapping,
		pgoff_t index, gfp_t gfp)
{
	return do_read_cache_folio(mapping, index, NULL, NULL, gfp);
}
EXPORT_SYMBOL(mapping_read_folio_gfp);

static struct page *do_read_cache_page(struct address_space *mapping,
		pgoff_t index, filler_t *filler, struct file *file, gfp_t gfp)
{
	struct folio *folio;

	folio = do_read_cache_folio(mapping, index, filler, file, gfp);
	if (IS_ERR(folio))
		return &folio->page;
	return folio_file_page(folio, index);
}

struct page *read_cache_page(struct address_space *mapping,
			pgoff_t index, filler_t *filler, struct file *file)
{
	return do_read_cache_page(mapping, index, filler, file,
			mapping_gfp_mask(mapping));
}
EXPORT_SYMBOL(read_cache_page);

 
struct page *read_cache_page_gfp(struct address_space *mapping,
				pgoff_t index,
				gfp_t gfp)
{
	return do_read_cache_page(mapping, index, NULL, NULL, gfp);
}
EXPORT_SYMBOL(read_cache_page_gfp);

 
static void dio_warn_stale_pagecache(struct file *filp)
{
	static DEFINE_RATELIMIT_STATE(_rs, 86400 * HZ, DEFAULT_RATELIMIT_BURST);
	char pathname[128];
	char *path;

	errseq_set(&filp->f_mapping->wb_err, -EIO);
	if (__ratelimit(&_rs)) {
		path = file_path(filp, pathname, sizeof(pathname));
		if (IS_ERR(path))
			path = "(unknown)";
		pr_crit("Page cache invalidation failure on direct I/O.  Possible data corruption due to collision with buffered I/O!\n");
		pr_crit("File: %s PID: %d Comm: %.20s\n", path, current->pid,
			current->comm);
	}
}

void kiocb_invalidate_post_direct_write(struct kiocb *iocb, size_t count)
{
	struct address_space *mapping = iocb->ki_filp->f_mapping;

	if (mapping->nrpages &&
	    invalidate_inode_pages2_range(mapping,
			iocb->ki_pos >> PAGE_SHIFT,
			(iocb->ki_pos + count - 1) >> PAGE_SHIFT))
		dio_warn_stale_pagecache(iocb->ki_filp);
}

ssize_t
generic_file_direct_write(struct kiocb *iocb, struct iov_iter *from)
{
	struct address_space *mapping = iocb->ki_filp->f_mapping;
	size_t write_len = iov_iter_count(from);
	ssize_t written;

	 
	written = kiocb_invalidate_pages(iocb, write_len);
	if (written) {
		if (written == -EBUSY)
			return 0;
		return written;
	}

	written = mapping->a_ops->direct_IO(iocb, from);

	 
	if (written > 0) {
		struct inode *inode = mapping->host;
		loff_t pos = iocb->ki_pos;

		kiocb_invalidate_post_direct_write(iocb, written);
		pos += written;
		write_len -= written;
		if (pos > i_size_read(inode) && !S_ISBLK(inode->i_mode)) {
			i_size_write(inode, pos);
			mark_inode_dirty(inode);
		}
		iocb->ki_pos = pos;
	}
	if (written != -EIOCBQUEUED)
		iov_iter_revert(from, write_len - iov_iter_count(from));
	return written;
}
EXPORT_SYMBOL(generic_file_direct_write);

ssize_t generic_perform_write(struct kiocb *iocb, struct iov_iter *i)
{
	struct file *file = iocb->ki_filp;
	loff_t pos = iocb->ki_pos;
	struct address_space *mapping = file->f_mapping;
	const struct address_space_operations *a_ops = mapping->a_ops;
	long status = 0;
	ssize_t written = 0;

	do {
		struct page *page;
		unsigned long offset;	 
		unsigned long bytes;	 
		size_t copied;		 
		void *fsdata = NULL;

		offset = (pos & (PAGE_SIZE - 1));
		bytes = min_t(unsigned long, PAGE_SIZE - offset,
						iov_iter_count(i));

again:
		 
		if (unlikely(fault_in_iov_iter_readable(i, bytes) == bytes)) {
			status = -EFAULT;
			break;
		}

		if (fatal_signal_pending(current)) {
			status = -EINTR;
			break;
		}

		status = a_ops->write_begin(file, mapping, pos, bytes,
						&page, &fsdata);
		if (unlikely(status < 0))
			break;

		if (mapping_writably_mapped(mapping))
			flush_dcache_page(page);

		copied = copy_page_from_iter_atomic(page, offset, bytes, i);
		flush_dcache_page(page);

		status = a_ops->write_end(file, mapping, pos, bytes, copied,
						page, fsdata);
		if (unlikely(status != copied)) {
			iov_iter_revert(i, copied - max(status, 0L));
			if (unlikely(status < 0))
				break;
		}
		cond_resched();

		if (unlikely(status == 0)) {
			 
			if (copied)
				bytes = copied;
			goto again;
		}
		pos += status;
		written += status;

		balance_dirty_pages_ratelimited(mapping);
	} while (iov_iter_count(i));

	if (!written)
		return status;
	iocb->ki_pos += written;
	return written;
}
EXPORT_SYMBOL(generic_perform_write);

 
ssize_t __generic_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct file *file = iocb->ki_filp;
	struct address_space *mapping = file->f_mapping;
	struct inode *inode = mapping->host;
	ssize_t ret;

	ret = file_remove_privs(file);
	if (ret)
		return ret;

	ret = file_update_time(file);
	if (ret)
		return ret;

	if (iocb->ki_flags & IOCB_DIRECT) {
		ret = generic_file_direct_write(iocb, from);
		 
		if (ret < 0 || !iov_iter_count(from) || IS_DAX(inode))
			return ret;
		return direct_write_fallback(iocb, from, ret,
				generic_perform_write(iocb, from));
	}

	return generic_perform_write(iocb, from);
}
EXPORT_SYMBOL(__generic_file_write_iter);

 
ssize_t generic_file_write_iter(struct kiocb *iocb, struct iov_iter *from)
{
	struct file *file = iocb->ki_filp;
	struct inode *inode = file->f_mapping->host;
	ssize_t ret;

	inode_lock(inode);
	ret = generic_write_checks(iocb, from);
	if (ret > 0)
		ret = __generic_file_write_iter(iocb, from);
	inode_unlock(inode);

	if (ret > 0)
		ret = generic_write_sync(iocb, ret);
	return ret;
}
EXPORT_SYMBOL(generic_file_write_iter);

 
bool filemap_release_folio(struct folio *folio, gfp_t gfp)
{
	struct address_space * const mapping = folio->mapping;

	BUG_ON(!folio_test_locked(folio));
	if (!folio_needs_release(folio))
		return true;
	if (folio_test_writeback(folio))
		return false;

	if (mapping && mapping->a_ops->release_folio)
		return mapping->a_ops->release_folio(folio, gfp);
	return try_to_free_buffers(folio);
}
EXPORT_SYMBOL(filemap_release_folio);

#ifdef CONFIG_CACHESTAT_SYSCALL
 
static void filemap_cachestat(struct address_space *mapping,
		pgoff_t first_index, pgoff_t last_index, struct cachestat *cs)
{
	XA_STATE(xas, &mapping->i_pages, first_index);
	struct folio *folio;

	rcu_read_lock();
	xas_for_each(&xas, folio, last_index) {
		unsigned long nr_pages;
		pgoff_t folio_first_index, folio_last_index;

		if (xas_retry(&xas, folio))
			continue;

		if (xa_is_value(folio)) {
			 
			void *shadow = (void *)folio;
			bool workingset;  
			int order = xa_get_order(xas.xa, xas.xa_index);

			nr_pages = 1 << order;
			folio_first_index = round_down(xas.xa_index, 1 << order);
			folio_last_index = folio_first_index + nr_pages - 1;

			 
			if (folio_first_index < first_index)
				nr_pages -= first_index - folio_first_index;

			if (folio_last_index > last_index)
				nr_pages -= folio_last_index - last_index;

			cs->nr_evicted += nr_pages;

#ifdef CONFIG_SWAP  
			if (shmem_mapping(mapping)) {
				 
				swp_entry_t swp = radix_to_swp_entry(folio);

				shadow = get_shadow_from_swap_cache(swp);
			}
#endif
			if (workingset_test_recent(shadow, true, &workingset))
				cs->nr_recently_evicted += nr_pages;

			goto resched;
		}

		nr_pages = folio_nr_pages(folio);
		folio_first_index = folio_pgoff(folio);
		folio_last_index = folio_first_index + nr_pages - 1;

		 
		if (folio_first_index < first_index)
			nr_pages -= first_index - folio_first_index;

		if (folio_last_index > last_index)
			nr_pages -= folio_last_index - last_index;

		 
		cs->nr_cache += nr_pages;

		if (folio_test_dirty(folio))
			cs->nr_dirty += nr_pages;

		if (folio_test_writeback(folio))
			cs->nr_writeback += nr_pages;

resched:
		if (need_resched()) {
			xas_pause(&xas);
			cond_resched_rcu();
		}
	}
	rcu_read_unlock();
}

 
SYSCALL_DEFINE4(cachestat, unsigned int, fd,
		struct cachestat_range __user *, cstat_range,
		struct cachestat __user *, cstat, unsigned int, flags)
{
	struct fd f = fdget(fd);
	struct address_space *mapping;
	struct cachestat_range csr;
	struct cachestat cs;
	pgoff_t first_index, last_index;

	if (!f.file)
		return -EBADF;

	if (copy_from_user(&csr, cstat_range,
			sizeof(struct cachestat_range))) {
		fdput(f);
		return -EFAULT;
	}

	 
	if (is_file_hugepages(f.file)) {
		fdput(f);
		return -EOPNOTSUPP;
	}

	if (flags != 0) {
		fdput(f);
		return -EINVAL;
	}

	first_index = csr.off >> PAGE_SHIFT;
	last_index =
		csr.len == 0 ? ULONG_MAX : (csr.off + csr.len - 1) >> PAGE_SHIFT;
	memset(&cs, 0, sizeof(struct cachestat));
	mapping = f.file->f_mapping;
	filemap_cachestat(mapping, first_index, last_index, &cs);
	fdput(f);

	if (copy_to_user(cstat, &cs, sizeof(struct cachestat)))
		return -EFAULT;

	return 0;
}
#endif  
