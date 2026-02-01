
 
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/balloon_compaction.h>

static void balloon_page_enqueue_one(struct balloon_dev_info *b_dev_info,
				     struct page *page)
{
	 
	BUG_ON(!trylock_page(page));
	balloon_page_insert(b_dev_info, page);
	unlock_page(page);
	__count_vm_event(BALLOON_INFLATE);
}

 
size_t balloon_page_list_enqueue(struct balloon_dev_info *b_dev_info,
				 struct list_head *pages)
{
	struct page *page, *tmp;
	unsigned long flags;
	size_t n_pages = 0;

	spin_lock_irqsave(&b_dev_info->pages_lock, flags);
	list_for_each_entry_safe(page, tmp, pages, lru) {
		list_del(&page->lru);
		balloon_page_enqueue_one(b_dev_info, page);
		n_pages++;
	}
	spin_unlock_irqrestore(&b_dev_info->pages_lock, flags);
	return n_pages;
}
EXPORT_SYMBOL_GPL(balloon_page_list_enqueue);

 
size_t balloon_page_list_dequeue(struct balloon_dev_info *b_dev_info,
				 struct list_head *pages, size_t n_req_pages)
{
	struct page *page, *tmp;
	unsigned long flags;
	size_t n_pages = 0;

	spin_lock_irqsave(&b_dev_info->pages_lock, flags);
	list_for_each_entry_safe(page, tmp, &b_dev_info->pages, lru) {
		if (n_pages == n_req_pages)
			break;

		 
		if (!trylock_page(page))
			continue;

		if (IS_ENABLED(CONFIG_BALLOON_COMPACTION) &&
		    PageIsolated(page)) {
			 
			unlock_page(page);
			continue;
		}
		balloon_page_delete(page);
		__count_vm_event(BALLOON_DEFLATE);
		list_add(&page->lru, pages);
		unlock_page(page);
		n_pages++;
	}
	spin_unlock_irqrestore(&b_dev_info->pages_lock, flags);

	return n_pages;
}
EXPORT_SYMBOL_GPL(balloon_page_list_dequeue);

 
struct page *balloon_page_alloc(void)
{
	struct page *page = alloc_page(balloon_mapping_gfp_mask() |
				       __GFP_NOMEMALLOC | __GFP_NORETRY |
				       __GFP_NOWARN);
	return page;
}
EXPORT_SYMBOL_GPL(balloon_page_alloc);

 
void balloon_page_enqueue(struct balloon_dev_info *b_dev_info,
			  struct page *page)
{
	unsigned long flags;

	spin_lock_irqsave(&b_dev_info->pages_lock, flags);
	balloon_page_enqueue_one(b_dev_info, page);
	spin_unlock_irqrestore(&b_dev_info->pages_lock, flags);
}
EXPORT_SYMBOL_GPL(balloon_page_enqueue);

 
struct page *balloon_page_dequeue(struct balloon_dev_info *b_dev_info)
{
	unsigned long flags;
	LIST_HEAD(pages);
	int n_pages;

	n_pages = balloon_page_list_dequeue(b_dev_info, &pages, 1);

	if (n_pages != 1) {
		 
		spin_lock_irqsave(&b_dev_info->pages_lock, flags);
		if (unlikely(list_empty(&b_dev_info->pages) &&
			     !b_dev_info->isolated_pages))
			BUG();
		spin_unlock_irqrestore(&b_dev_info->pages_lock, flags);
		return NULL;
	}
	return list_first_entry(&pages, struct page, lru);
}
EXPORT_SYMBOL_GPL(balloon_page_dequeue);

#ifdef CONFIG_BALLOON_COMPACTION

static bool balloon_page_isolate(struct page *page, isolate_mode_t mode)

{
	struct balloon_dev_info *b_dev_info = balloon_page_device(page);
	unsigned long flags;

	spin_lock_irqsave(&b_dev_info->pages_lock, flags);
	list_del(&page->lru);
	b_dev_info->isolated_pages++;
	spin_unlock_irqrestore(&b_dev_info->pages_lock, flags);

	return true;
}

static void balloon_page_putback(struct page *page)
{
	struct balloon_dev_info *b_dev_info = balloon_page_device(page);
	unsigned long flags;

	spin_lock_irqsave(&b_dev_info->pages_lock, flags);
	list_add(&page->lru, &b_dev_info->pages);
	b_dev_info->isolated_pages--;
	spin_unlock_irqrestore(&b_dev_info->pages_lock, flags);
}

 
static int balloon_page_migrate(struct page *newpage, struct page *page,
		enum migrate_mode mode)
{
	struct balloon_dev_info *balloon = balloon_page_device(page);

	 
	if (mode == MIGRATE_SYNC_NO_COPY)
		return -EINVAL;

	VM_BUG_ON_PAGE(!PageLocked(page), page);
	VM_BUG_ON_PAGE(!PageLocked(newpage), newpage);

	return balloon->migratepage(balloon, newpage, page, mode);
}

const struct movable_operations balloon_mops = {
	.migrate_page = balloon_page_migrate,
	.isolate_page = balloon_page_isolate,
	.putback_page = balloon_page_putback,
};
EXPORT_SYMBOL_GPL(balloon_mops);

#endif  
