
 

#include <linux/kernel.h>
#include <linux/backing-dev.h>
#include <linux/dax.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/export.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/pagevec.h>
#include <linux/task_io_accounting_ops.h>
#include <linux/shmem_fs.h>
#include <linux/rmap.h>
#include "internal.h"

 
static inline void __clear_shadow_entry(struct address_space *mapping,
				pgoff_t index, void *entry)
{
	XA_STATE(xas, &mapping->i_pages, index);

	xas_set_update(&xas, workingset_update_node);
	if (xas_load(&xas) != entry)
		return;
	xas_store(&xas, NULL);
}

static void clear_shadow_entry(struct address_space *mapping, pgoff_t index,
			       void *entry)
{
	spin_lock(&mapping->host->i_lock);
	xa_lock_irq(&mapping->i_pages);
	__clear_shadow_entry(mapping, index, entry);
	xa_unlock_irq(&mapping->i_pages);
	if (mapping_shrinkable(mapping))
		inode_add_lru(mapping->host);
	spin_unlock(&mapping->host->i_lock);
}

 
static void truncate_folio_batch_exceptionals(struct address_space *mapping,
				struct folio_batch *fbatch, pgoff_t *indices)
{
	int i, j;
	bool dax;

	 
	if (shmem_mapping(mapping))
		return;

	for (j = 0; j < folio_batch_count(fbatch); j++)
		if (xa_is_value(fbatch->folios[j]))
			break;

	if (j == folio_batch_count(fbatch))
		return;

	dax = dax_mapping(mapping);
	if (!dax) {
		spin_lock(&mapping->host->i_lock);
		xa_lock_irq(&mapping->i_pages);
	}

	for (i = j; i < folio_batch_count(fbatch); i++) {
		struct folio *folio = fbatch->folios[i];
		pgoff_t index = indices[i];

		if (!xa_is_value(folio)) {
			fbatch->folios[j++] = folio;
			continue;
		}

		if (unlikely(dax)) {
			dax_delete_mapping_entry(mapping, index);
			continue;
		}

		__clear_shadow_entry(mapping, index, folio);
	}

	if (!dax) {
		xa_unlock_irq(&mapping->i_pages);
		if (mapping_shrinkable(mapping))
			inode_add_lru(mapping->host);
		spin_unlock(&mapping->host->i_lock);
	}
	fbatch->nr = j;
}

 
static int invalidate_exceptional_entry(struct address_space *mapping,
					pgoff_t index, void *entry)
{
	 
	if (shmem_mapping(mapping) || dax_mapping(mapping))
		return 1;
	clear_shadow_entry(mapping, index, entry);
	return 1;
}

 
static int invalidate_exceptional_entry2(struct address_space *mapping,
					 pgoff_t index, void *entry)
{
	 
	if (shmem_mapping(mapping))
		return 1;
	if (dax_mapping(mapping))
		return dax_invalidate_mapping_entry_sync(mapping, index);
	clear_shadow_entry(mapping, index, entry);
	return 1;
}

 
void folio_invalidate(struct folio *folio, size_t offset, size_t length)
{
	const struct address_space_operations *aops = folio->mapping->a_ops;

	if (aops->invalidate_folio)
		aops->invalidate_folio(folio, offset, length);
}
EXPORT_SYMBOL_GPL(folio_invalidate);

 
static void truncate_cleanup_folio(struct folio *folio)
{
	if (folio_mapped(folio))
		unmap_mapping_folio(folio);

	if (folio_has_private(folio))
		folio_invalidate(folio, 0, folio_size(folio));

	 
	folio_cancel_dirty(folio);
	folio_clear_mappedtodisk(folio);
}

int truncate_inode_folio(struct address_space *mapping, struct folio *folio)
{
	if (folio->mapping != mapping)
		return -EIO;

	truncate_cleanup_folio(folio);
	filemap_remove_folio(folio);
	return 0;
}

 
bool truncate_inode_partial_folio(struct folio *folio, loff_t start, loff_t end)
{
	loff_t pos = folio_pos(folio);
	unsigned int offset, length;

	if (pos < start)
		offset = start - pos;
	else
		offset = 0;
	length = folio_size(folio);
	if (pos + length <= (u64)end)
		length = length - offset;
	else
		length = end + 1 - pos - offset;

	folio_wait_writeback(folio);
	if (length == folio_size(folio)) {
		truncate_inode_folio(folio->mapping, folio);
		return true;
	}

	 
	folio_zero_range(folio, offset, length);

	if (folio_has_private(folio))
		folio_invalidate(folio, offset, length);
	if (!folio_test_large(folio))
		return true;
	if (split_folio(folio) == 0)
		return true;
	if (folio_test_dirty(folio))
		return false;
	truncate_inode_folio(folio->mapping, folio);
	return true;
}

 
int generic_error_remove_page(struct address_space *mapping, struct page *page)
{
	VM_BUG_ON_PAGE(PageTail(page), page);

	if (!mapping)
		return -EINVAL;
	 
	if (!S_ISREG(mapping->host->i_mode))
		return -EIO;
	return truncate_inode_folio(mapping, page_folio(page));
}
EXPORT_SYMBOL(generic_error_remove_page);

static long mapping_evict_folio(struct address_space *mapping,
		struct folio *folio)
{
	if (folio_test_dirty(folio) || folio_test_writeback(folio))
		return 0;
	 
	if (folio_ref_count(folio) >
			folio_nr_pages(folio) + folio_has_private(folio) + 1)
		return 0;
	if (!filemap_release_folio(folio, 0))
		return 0;

	return remove_mapping(mapping, folio);
}

 
long invalidate_inode_page(struct page *page)
{
	struct folio *folio = page_folio(page);
	struct address_space *mapping = folio_mapping(folio);

	 
	if (!mapping)
		return 0;
	return mapping_evict_folio(mapping, folio);
}

 
void truncate_inode_pages_range(struct address_space *mapping,
				loff_t lstart, loff_t lend)
{
	pgoff_t		start;		 
	pgoff_t		end;		 
	struct folio_batch fbatch;
	pgoff_t		indices[PAGEVEC_SIZE];
	pgoff_t		index;
	int		i;
	struct folio	*folio;
	bool		same_folio;

	if (mapping_empty(mapping))
		return;

	 
	start = (lstart + PAGE_SIZE - 1) >> PAGE_SHIFT;
	if (lend == -1)
		 
		end = -1;
	else
		end = (lend + 1) >> PAGE_SHIFT;

	folio_batch_init(&fbatch);
	index = start;
	while (index < end && find_lock_entries(mapping, &index, end - 1,
			&fbatch, indices)) {
		truncate_folio_batch_exceptionals(mapping, &fbatch, indices);
		for (i = 0; i < folio_batch_count(&fbatch); i++)
			truncate_cleanup_folio(fbatch.folios[i]);
		delete_from_page_cache_batch(mapping, &fbatch);
		for (i = 0; i < folio_batch_count(&fbatch); i++)
			folio_unlock(fbatch.folios[i]);
		folio_batch_release(&fbatch);
		cond_resched();
	}

	same_folio = (lstart >> PAGE_SHIFT) == (lend >> PAGE_SHIFT);
	folio = __filemap_get_folio(mapping, lstart >> PAGE_SHIFT, FGP_LOCK, 0);
	if (!IS_ERR(folio)) {
		same_folio = lend < folio_pos(folio) + folio_size(folio);
		if (!truncate_inode_partial_folio(folio, lstart, lend)) {
			start = folio_next_index(folio);
			if (same_folio)
				end = folio->index;
		}
		folio_unlock(folio);
		folio_put(folio);
		folio = NULL;
	}

	if (!same_folio) {
		folio = __filemap_get_folio(mapping, lend >> PAGE_SHIFT,
						FGP_LOCK, 0);
		if (!IS_ERR(folio)) {
			if (!truncate_inode_partial_folio(folio, lstart, lend))
				end = folio->index;
			folio_unlock(folio);
			folio_put(folio);
		}
	}

	index = start;
	while (index < end) {
		cond_resched();
		if (!find_get_entries(mapping, &index, end - 1, &fbatch,
				indices)) {
			 
			if (index == start)
				break;
			 
			index = start;
			continue;
		}

		for (i = 0; i < folio_batch_count(&fbatch); i++) {
			struct folio *folio = fbatch.folios[i];

			 

			if (xa_is_value(folio))
				continue;

			folio_lock(folio);
			VM_BUG_ON_FOLIO(!folio_contains(folio, indices[i]), folio);
			folio_wait_writeback(folio);
			truncate_inode_folio(mapping, folio);
			folio_unlock(folio);
		}
		truncate_folio_batch_exceptionals(mapping, &fbatch, indices);
		folio_batch_release(&fbatch);
	}
}
EXPORT_SYMBOL(truncate_inode_pages_range);

 
void truncate_inode_pages(struct address_space *mapping, loff_t lstart)
{
	truncate_inode_pages_range(mapping, lstart, (loff_t)-1);
}
EXPORT_SYMBOL(truncate_inode_pages);

 
void truncate_inode_pages_final(struct address_space *mapping)
{
	 
	mapping_set_exiting(mapping);

	if (!mapping_empty(mapping)) {
		 
		xa_lock_irq(&mapping->i_pages);
		xa_unlock_irq(&mapping->i_pages);
	}

	truncate_inode_pages(mapping, 0);
}
EXPORT_SYMBOL(truncate_inode_pages_final);

 
unsigned long mapping_try_invalidate(struct address_space *mapping,
		pgoff_t start, pgoff_t end, unsigned long *nr_failed)
{
	pgoff_t indices[PAGEVEC_SIZE];
	struct folio_batch fbatch;
	pgoff_t index = start;
	unsigned long ret;
	unsigned long count = 0;
	int i;

	folio_batch_init(&fbatch);
	while (find_lock_entries(mapping, &index, end, &fbatch, indices)) {
		for (i = 0; i < folio_batch_count(&fbatch); i++) {
			struct folio *folio = fbatch.folios[i];

			 

			if (xa_is_value(folio)) {
				count += invalidate_exceptional_entry(mapping,
							     indices[i], folio);
				continue;
			}

			ret = mapping_evict_folio(mapping, folio);
			folio_unlock(folio);
			 
			if (!ret) {
				deactivate_file_folio(folio);
				 
				if (nr_failed)
					(*nr_failed)++;
			}
			count += ret;
		}
		folio_batch_remove_exceptionals(&fbatch);
		folio_batch_release(&fbatch);
		cond_resched();
	}
	return count;
}

 
unsigned long invalidate_mapping_pages(struct address_space *mapping,
		pgoff_t start, pgoff_t end)
{
	return mapping_try_invalidate(mapping, start, end, NULL);
}
EXPORT_SYMBOL(invalidate_mapping_pages);

 
static int invalidate_complete_folio2(struct address_space *mapping,
					struct folio *folio)
{
	if (folio->mapping != mapping)
		return 0;

	if (!filemap_release_folio(folio, GFP_KERNEL))
		return 0;

	spin_lock(&mapping->host->i_lock);
	xa_lock_irq(&mapping->i_pages);
	if (folio_test_dirty(folio))
		goto failed;

	BUG_ON(folio_has_private(folio));
	__filemap_remove_folio(folio, NULL);
	xa_unlock_irq(&mapping->i_pages);
	if (mapping_shrinkable(mapping))
		inode_add_lru(mapping->host);
	spin_unlock(&mapping->host->i_lock);

	filemap_free_folio(mapping, folio);
	return 1;
failed:
	xa_unlock_irq(&mapping->i_pages);
	spin_unlock(&mapping->host->i_lock);
	return 0;
}

static int folio_launder(struct address_space *mapping, struct folio *folio)
{
	if (!folio_test_dirty(folio))
		return 0;
	if (folio->mapping != mapping || mapping->a_ops->launder_folio == NULL)
		return 0;
	return mapping->a_ops->launder_folio(folio);
}

 
int invalidate_inode_pages2_range(struct address_space *mapping,
				  pgoff_t start, pgoff_t end)
{
	pgoff_t indices[PAGEVEC_SIZE];
	struct folio_batch fbatch;
	pgoff_t index;
	int i;
	int ret = 0;
	int ret2 = 0;
	int did_range_unmap = 0;

	if (mapping_empty(mapping))
		return 0;

	folio_batch_init(&fbatch);
	index = start;
	while (find_get_entries(mapping, &index, end, &fbatch, indices)) {
		for (i = 0; i < folio_batch_count(&fbatch); i++) {
			struct folio *folio = fbatch.folios[i];

			 

			if (xa_is_value(folio)) {
				if (!invalidate_exceptional_entry2(mapping,
						indices[i], folio))
					ret = -EBUSY;
				continue;
			}

			if (!did_range_unmap && folio_mapped(folio)) {
				 
				unmap_mapping_pages(mapping, indices[i],
						(1 + end - indices[i]), false);
				did_range_unmap = 1;
			}

			folio_lock(folio);
			if (unlikely(folio->mapping != mapping)) {
				folio_unlock(folio);
				continue;
			}
			VM_BUG_ON_FOLIO(!folio_contains(folio, indices[i]), folio);
			folio_wait_writeback(folio);

			if (folio_mapped(folio))
				unmap_mapping_folio(folio);
			BUG_ON(folio_mapped(folio));

			ret2 = folio_launder(mapping, folio);
			if (ret2 == 0) {
				if (!invalidate_complete_folio2(mapping, folio))
					ret2 = -EBUSY;
			}
			if (ret2 < 0)
				ret = ret2;
			folio_unlock(folio);
		}
		folio_batch_remove_exceptionals(&fbatch);
		folio_batch_release(&fbatch);
		cond_resched();
	}
	 
	if (dax_mapping(mapping)) {
		unmap_mapping_pages(mapping, start, end - start + 1, false);
	}
	return ret;
}
EXPORT_SYMBOL_GPL(invalidate_inode_pages2_range);

 
int invalidate_inode_pages2(struct address_space *mapping)
{
	return invalidate_inode_pages2_range(mapping, 0, -1);
}
EXPORT_SYMBOL_GPL(invalidate_inode_pages2);

 
void truncate_pagecache(struct inode *inode, loff_t newsize)
{
	struct address_space *mapping = inode->i_mapping;
	loff_t holebegin = round_up(newsize, PAGE_SIZE);

	 
	unmap_mapping_range(mapping, holebegin, 0, 1);
	truncate_inode_pages(mapping, newsize);
	unmap_mapping_range(mapping, holebegin, 0, 1);
}
EXPORT_SYMBOL(truncate_pagecache);

 
void truncate_setsize(struct inode *inode, loff_t newsize)
{
	loff_t oldsize = inode->i_size;

	i_size_write(inode, newsize);
	if (newsize > oldsize)
		pagecache_isize_extended(inode, oldsize, newsize);
	truncate_pagecache(inode, newsize);
}
EXPORT_SYMBOL(truncate_setsize);

 
void pagecache_isize_extended(struct inode *inode, loff_t from, loff_t to)
{
	int bsize = i_blocksize(inode);
	loff_t rounded_from;
	struct page *page;
	pgoff_t index;

	WARN_ON(to > inode->i_size);

	if (from >= to || bsize == PAGE_SIZE)
		return;
	 
	rounded_from = round_up(from, bsize);
	if (to <= rounded_from || !(rounded_from & (PAGE_SIZE - 1)))
		return;

	index = from >> PAGE_SHIFT;
	page = find_lock_page(inode->i_mapping, index);
	 
	if (!page)
		return;
	 
	if (page_mkclean(page))
		set_page_dirty(page);
	unlock_page(page);
	put_page(page);
}
EXPORT_SYMBOL(pagecache_isize_extended);

 
void truncate_pagecache_range(struct inode *inode, loff_t lstart, loff_t lend)
{
	struct address_space *mapping = inode->i_mapping;
	loff_t unmap_start = round_up(lstart, PAGE_SIZE);
	loff_t unmap_end = round_down(1 + lend, PAGE_SIZE) - 1;
	 

	 
	if ((u64)unmap_end > (u64)unmap_start)
		unmap_mapping_range(mapping, unmap_start,
				    1 + unmap_end - unmap_start, 0);
	truncate_inode_pages_range(mapping, lstart, lend);
}
EXPORT_SYMBOL(truncate_pagecache_range);
