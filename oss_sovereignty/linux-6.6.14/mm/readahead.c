
 

 

#include <linux/blkdev.h>
#include <linux/kernel.h>
#include <linux/dax.h>
#include <linux/gfp.h>
#include <linux/export.h>
#include <linux/backing-dev.h>
#include <linux/task_io_accounting_ops.h>
#include <linux/pagemap.h>
#include <linux/psi.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/mm_inline.h>
#include <linux/blk-cgroup.h>
#include <linux/fadvise.h>
#include <linux/sched/mm.h>

#include "internal.h"

 
void
file_ra_state_init(struct file_ra_state *ra, struct address_space *mapping)
{
	ra->ra_pages = inode_to_bdi(mapping->host)->ra_pages;
	ra->prev_pos = -1;
}
EXPORT_SYMBOL_GPL(file_ra_state_init);

static void read_pages(struct readahead_control *rac)
{
	const struct address_space_operations *aops = rac->mapping->a_ops;
	struct folio *folio;
	struct blk_plug plug;

	if (!readahead_count(rac))
		return;

	if (unlikely(rac->_workingset))
		psi_memstall_enter(&rac->_pflags);
	blk_start_plug(&plug);

	if (aops->readahead) {
		aops->readahead(rac);
		 
		while ((folio = readahead_folio(rac)) != NULL) {
			unsigned long nr = folio_nr_pages(folio);

			folio_get(folio);
			rac->ra->size -= nr;
			if (rac->ra->async_size >= nr) {
				rac->ra->async_size -= nr;
				filemap_remove_folio(folio);
			}
			folio_unlock(folio);
			folio_put(folio);
		}
	} else {
		while ((folio = readahead_folio(rac)) != NULL)
			aops->read_folio(rac->file, folio);
	}

	blk_finish_plug(&plug);
	if (unlikely(rac->_workingset))
		psi_memstall_leave(&rac->_pflags);
	rac->_workingset = false;

	BUG_ON(readahead_count(rac));
}

 
void page_cache_ra_unbounded(struct readahead_control *ractl,
		unsigned long nr_to_read, unsigned long lookahead_size)
{
	struct address_space *mapping = ractl->mapping;
	unsigned long index = readahead_index(ractl);
	gfp_t gfp_mask = readahead_gfp_mask(mapping);
	unsigned long i;

	 
	unsigned int nofs = memalloc_nofs_save();

	filemap_invalidate_lock_shared(mapping);
	 
	for (i = 0; i < nr_to_read; i++) {
		struct folio *folio = xa_load(&mapping->i_pages, index + i);

		if (folio && !xa_is_value(folio)) {
			 
			read_pages(ractl);
			ractl->_index++;
			i = ractl->_index + ractl->_nr_pages - index - 1;
			continue;
		}

		folio = filemap_alloc_folio(gfp_mask, 0);
		if (!folio)
			break;
		if (filemap_add_folio(mapping, folio, index + i,
					gfp_mask) < 0) {
			folio_put(folio);
			read_pages(ractl);
			ractl->_index++;
			i = ractl->_index + ractl->_nr_pages - index - 1;
			continue;
		}
		if (i == nr_to_read - lookahead_size)
			folio_set_readahead(folio);
		ractl->_workingset |= folio_test_workingset(folio);
		ractl->_nr_pages++;
	}

	 
	read_pages(ractl);
	filemap_invalidate_unlock_shared(mapping);
	memalloc_nofs_restore(nofs);
}
EXPORT_SYMBOL_GPL(page_cache_ra_unbounded);

 
static void do_page_cache_ra(struct readahead_control *ractl,
		unsigned long nr_to_read, unsigned long lookahead_size)
{
	struct inode *inode = ractl->mapping->host;
	unsigned long index = readahead_index(ractl);
	loff_t isize = i_size_read(inode);
	pgoff_t end_index;	 

	if (isize == 0)
		return;

	end_index = (isize - 1) >> PAGE_SHIFT;
	if (index > end_index)
		return;
	 
	if (nr_to_read > end_index - index)
		nr_to_read = end_index - index + 1;

	page_cache_ra_unbounded(ractl, nr_to_read, lookahead_size);
}

 
void force_page_cache_ra(struct readahead_control *ractl,
		unsigned long nr_to_read)
{
	struct address_space *mapping = ractl->mapping;
	struct file_ra_state *ra = ractl->ra;
	struct backing_dev_info *bdi = inode_to_bdi(mapping->host);
	unsigned long max_pages, index;

	if (unlikely(!mapping->a_ops->read_folio && !mapping->a_ops->readahead))
		return;

	 
	index = readahead_index(ractl);
	max_pages = max_t(unsigned long, bdi->io_pages, ra->ra_pages);
	nr_to_read = min_t(unsigned long, nr_to_read, max_pages);
	while (nr_to_read) {
		unsigned long this_chunk = (2 * 1024 * 1024) / PAGE_SIZE;

		if (this_chunk > nr_to_read)
			this_chunk = nr_to_read;
		ractl->_index = index;
		do_page_cache_ra(ractl, this_chunk, 0);

		index += this_chunk;
		nr_to_read -= this_chunk;
	}
}

 
static unsigned long get_init_ra_size(unsigned long size, unsigned long max)
{
	unsigned long newsize = roundup_pow_of_two(size);

	if (newsize <= max / 32)
		newsize = newsize * 4;
	else if (newsize <= max / 4)
		newsize = newsize * 2;
	else
		newsize = max;

	return newsize;
}

 
static unsigned long get_next_ra_size(struct file_ra_state *ra,
				      unsigned long max)
{
	unsigned long cur = ra->size;

	if (cur < max / 16)
		return 4 * cur;
	if (cur <= max / 2)
		return 2 * cur;
	return max;
}

 

 
static pgoff_t count_history_pages(struct address_space *mapping,
				   pgoff_t index, unsigned long max)
{
	pgoff_t head;

	rcu_read_lock();
	head = page_cache_prev_miss(mapping, index - 1, max);
	rcu_read_unlock();

	return index - 1 - head;
}

 
static int try_context_readahead(struct address_space *mapping,
				 struct file_ra_state *ra,
				 pgoff_t index,
				 unsigned long req_size,
				 unsigned long max)
{
	pgoff_t size;

	size = count_history_pages(mapping, index, max);

	 
	if (size <= req_size)
		return 0;

	 
	if (size >= index)
		size *= 2;

	ra->start = index;
	ra->size = min(size + req_size, max);
	ra->async_size = 1;

	return 1;
}

static inline int ra_alloc_folio(struct readahead_control *ractl, pgoff_t index,
		pgoff_t mark, unsigned int order, gfp_t gfp)
{
	int err;
	struct folio *folio = filemap_alloc_folio(gfp, order);

	if (!folio)
		return -ENOMEM;
	mark = round_up(mark, 1UL << order);
	if (index == mark)
		folio_set_readahead(folio);
	err = filemap_add_folio(ractl->mapping, folio, index, gfp);
	if (err) {
		folio_put(folio);
		return err;
	}

	ractl->_nr_pages += 1UL << order;
	ractl->_workingset |= folio_test_workingset(folio);
	return 0;
}

void page_cache_ra_order(struct readahead_control *ractl,
		struct file_ra_state *ra, unsigned int new_order)
{
	struct address_space *mapping = ractl->mapping;
	pgoff_t index = readahead_index(ractl);
	pgoff_t limit = (i_size_read(mapping->host) - 1) >> PAGE_SHIFT;
	pgoff_t mark = index + ra->size - ra->async_size;
	int err = 0;
	gfp_t gfp = readahead_gfp_mask(mapping);

	if (!mapping_large_folio_support(mapping) || ra->size < 4)
		goto fallback;

	limit = min(limit, index + ra->size - 1);

	if (new_order < MAX_PAGECACHE_ORDER) {
		new_order += 2;
		if (new_order > MAX_PAGECACHE_ORDER)
			new_order = MAX_PAGECACHE_ORDER;
		while ((1 << new_order) > ra->size)
			new_order--;
	}

	filemap_invalidate_lock_shared(mapping);
	while (index <= limit) {
		unsigned int order = new_order;

		 
		if (index & ((1UL << order) - 1)) {
			order = __ffs(index);
			if (order == 1)
				order = 0;
		}
		 
		while (index + (1UL << order) - 1 > limit) {
			if (--order == 1)
				order = 0;
		}
		err = ra_alloc_folio(ractl, index, mark, order, gfp);
		if (err)
			break;
		index += 1UL << order;
	}

	if (index > limit) {
		ra->size += index - limit - 1;
		ra->async_size += index - limit - 1;
	}

	read_pages(ractl);
	filemap_invalidate_unlock_shared(mapping);

	 
	if (!err)
		return;
fallback:
	do_page_cache_ra(ractl, ra->size, ra->async_size);
}

 
static void ondemand_readahead(struct readahead_control *ractl,
		struct folio *folio, unsigned long req_size)
{
	struct backing_dev_info *bdi = inode_to_bdi(ractl->mapping->host);
	struct file_ra_state *ra = ractl->ra;
	unsigned long max_pages = ra->ra_pages;
	unsigned long add_pages;
	pgoff_t index = readahead_index(ractl);
	pgoff_t expected, prev_index;
	unsigned int order = folio ? folio_order(folio) : 0;

	 
	if (req_size > max_pages && bdi->io_pages > max_pages)
		max_pages = min(req_size, bdi->io_pages);

	 
	if (!index)
		goto initial_readahead;

	 
	expected = round_up(ra->start + ra->size - ra->async_size,
			1UL << order);
	if (index == expected || index == (ra->start + ra->size)) {
		ra->start += ra->size;
		ra->size = get_next_ra_size(ra, max_pages);
		ra->async_size = ra->size;
		goto readit;
	}

	 
	if (folio) {
		pgoff_t start;

		rcu_read_lock();
		start = page_cache_next_miss(ractl->mapping, index + 1,
				max_pages);
		rcu_read_unlock();

		if (!start || start - index > max_pages)
			return;

		ra->start = start;
		ra->size = start - index;	 
		ra->size += req_size;
		ra->size = get_next_ra_size(ra, max_pages);
		ra->async_size = ra->size;
		goto readit;
	}

	 
	if (req_size > max_pages)
		goto initial_readahead;

	 
	prev_index = (unsigned long long)ra->prev_pos >> PAGE_SHIFT;
	if (index - prev_index <= 1UL)
		goto initial_readahead;

	 
	if (try_context_readahead(ractl->mapping, ra, index, req_size,
			max_pages))
		goto readit;

	 
	do_page_cache_ra(ractl, req_size, 0);
	return;

initial_readahead:
	ra->start = index;
	ra->size = get_init_ra_size(req_size, max_pages);
	ra->async_size = ra->size > req_size ? ra->size - req_size : ra->size;

readit:
	 
	if (index == ra->start && ra->size == ra->async_size) {
		add_pages = get_next_ra_size(ra, max_pages);
		if (ra->size + add_pages <= max_pages) {
			ra->async_size = add_pages;
			ra->size += add_pages;
		} else {
			ra->size = max_pages;
			ra->async_size = max_pages >> 1;
		}
	}

	ractl->_index = ra->start;
	page_cache_ra_order(ractl, ra, order);
}

void page_cache_sync_ra(struct readahead_control *ractl,
		unsigned long req_count)
{
	bool do_forced_ra = ractl->file && (ractl->file->f_mode & FMODE_RANDOM);

	 
	if (!ractl->ra->ra_pages || blk_cgroup_congested()) {
		if (!ractl->file)
			return;
		req_count = 1;
		do_forced_ra = true;
	}

	 
	if (do_forced_ra) {
		force_page_cache_ra(ractl, req_count);
		return;
	}

	ondemand_readahead(ractl, NULL, req_count);
}
EXPORT_SYMBOL_GPL(page_cache_sync_ra);

void page_cache_async_ra(struct readahead_control *ractl,
		struct folio *folio, unsigned long req_count)
{
	 
	if (!ractl->ra->ra_pages)
		return;

	 
	if (folio_test_writeback(folio))
		return;

	folio_clear_readahead(folio);

	if (blk_cgroup_congested())
		return;

	ondemand_readahead(ractl, folio, req_count);
}
EXPORT_SYMBOL_GPL(page_cache_async_ra);

ssize_t ksys_readahead(int fd, loff_t offset, size_t count)
{
	ssize_t ret;
	struct fd f;

	ret = -EBADF;
	f = fdget(fd);
	if (!f.file || !(f.file->f_mode & FMODE_READ))
		goto out;

	 
	ret = -EINVAL;
	if (!f.file->f_mapping || !f.file->f_mapping->a_ops ||
	    (!S_ISREG(file_inode(f.file)->i_mode) &&
	    !S_ISBLK(file_inode(f.file)->i_mode)))
		goto out;

	ret = vfs_fadvise(f.file, offset, count, POSIX_FADV_WILLNEED);
out:
	fdput(f);
	return ret;
}

SYSCALL_DEFINE3(readahead, int, fd, loff_t, offset, size_t, count)
{
	return ksys_readahead(fd, offset, count);
}

#if defined(CONFIG_COMPAT) && defined(__ARCH_WANT_COMPAT_READAHEAD)
COMPAT_SYSCALL_DEFINE4(readahead, int, fd, compat_arg_u64_dual(offset), size_t, count)
{
	return ksys_readahead(fd, compat_arg_u64_glue(offset), count);
}
#endif

 
void readahead_expand(struct readahead_control *ractl,
		      loff_t new_start, size_t new_len)
{
	struct address_space *mapping = ractl->mapping;
	struct file_ra_state *ra = ractl->ra;
	pgoff_t new_index, new_nr_pages;
	gfp_t gfp_mask = readahead_gfp_mask(mapping);

	new_index = new_start / PAGE_SIZE;

	 
	while (ractl->_index > new_index) {
		unsigned long index = ractl->_index - 1;
		struct folio *folio = xa_load(&mapping->i_pages, index);

		if (folio && !xa_is_value(folio))
			return;  

		folio = filemap_alloc_folio(gfp_mask, 0);
		if (!folio)
			return;
		if (filemap_add_folio(mapping, folio, index, gfp_mask) < 0) {
			folio_put(folio);
			return;
		}
		if (unlikely(folio_test_workingset(folio)) &&
				!ractl->_workingset) {
			ractl->_workingset = true;
			psi_memstall_enter(&ractl->_pflags);
		}
		ractl->_nr_pages++;
		ractl->_index = folio->index;
	}

	new_len += new_start - readahead_pos(ractl);
	new_nr_pages = DIV_ROUND_UP(new_len, PAGE_SIZE);

	 
	while (ractl->_nr_pages < new_nr_pages) {
		unsigned long index = ractl->_index + ractl->_nr_pages;
		struct folio *folio = xa_load(&mapping->i_pages, index);

		if (folio && !xa_is_value(folio))
			return;  

		folio = filemap_alloc_folio(gfp_mask, 0);
		if (!folio)
			return;
		if (filemap_add_folio(mapping, folio, index, gfp_mask) < 0) {
			folio_put(folio);
			return;
		}
		if (unlikely(folio_test_workingset(folio)) &&
				!ractl->_workingset) {
			ractl->_workingset = true;
			psi_memstall_enter(&ractl->_pflags);
		}
		ractl->_nr_pages++;
		if (ra) {
			ra->size++;
			ra->async_size++;
		}
	}
}
EXPORT_SYMBOL(readahead_expand);
