
#include <linux/mm.h>
#include <linux/mmzone.h>
#include <linux/page_reporting.h>
#include <linux/gfp.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/scatterlist.h>

#include "page_reporting.h"
#include "internal.h"

 
unsigned int page_reporting_order = -1;

static int page_order_update_notify(const char *val, const struct kernel_param *kp)
{
	 
	return  param_set_uint_minmax(val, kp, 0, MAX_ORDER);
}

static const struct kernel_param_ops page_reporting_param_ops = {
	.set = &page_order_update_notify,
	 
	.get = &param_get_int,
};

module_param_cb(page_reporting_order, &page_reporting_param_ops,
			&page_reporting_order, 0644);
MODULE_PARM_DESC(page_reporting_order, "Set page reporting order");

 
EXPORT_SYMBOL_GPL(page_reporting_order);

#define PAGE_REPORTING_DELAY	(2 * HZ)
static struct page_reporting_dev_info __rcu *pr_dev_info __read_mostly;

enum {
	PAGE_REPORTING_IDLE = 0,
	PAGE_REPORTING_REQUESTED,
	PAGE_REPORTING_ACTIVE
};

 
static void
__page_reporting_request(struct page_reporting_dev_info *prdev)
{
	unsigned int state;

	 
	state = atomic_read(&prdev->state);
	if (state == PAGE_REPORTING_REQUESTED)
		return;

	 
	state = atomic_xchg(&prdev->state, PAGE_REPORTING_REQUESTED);
	if (state != PAGE_REPORTING_IDLE)
		return;

	 
	schedule_delayed_work(&prdev->work, PAGE_REPORTING_DELAY);
}

 
void __page_reporting_notify(void)
{
	struct page_reporting_dev_info *prdev;

	 
	rcu_read_lock();
	prdev = rcu_dereference(pr_dev_info);
	if (likely(prdev))
		__page_reporting_request(prdev);

	rcu_read_unlock();
}

static void
page_reporting_drain(struct page_reporting_dev_info *prdev,
		     struct scatterlist *sgl, unsigned int nents, bool reported)
{
	struct scatterlist *sg = sgl;

	 
	do {
		struct page *page = sg_page(sg);
		int mt = get_pageblock_migratetype(page);
		unsigned int order = get_order(sg->length);

		__putback_isolated_page(page, order, mt);

		 
		if (!reported)
			continue;

		 
		if (PageBuddy(page) && buddy_order(page) == order)
			__SetPageReported(page);
	} while ((sg = sg_next(sg)));

	 
	sg_init_table(sgl, nents);
}

 
static int
page_reporting_cycle(struct page_reporting_dev_info *prdev, struct zone *zone,
		     unsigned int order, unsigned int mt,
		     struct scatterlist *sgl, unsigned int *offset)
{
	struct free_area *area = &zone->free_area[order];
	struct list_head *list = &area->free_list[mt];
	unsigned int page_len = PAGE_SIZE << order;
	struct page *page, *next;
	long budget;
	int err = 0;

	 
	if (list_empty(list))
		return err;

	spin_lock_irq(&zone->lock);

	 
	budget = DIV_ROUND_UP(area->nr_free, PAGE_REPORTING_CAPACITY * 16);

	 
	list_for_each_entry_safe(page, next, list, lru) {
		 
		if (PageReported(page))
			continue;

		 
		if (budget < 0) {
			atomic_set(&prdev->state, PAGE_REPORTING_REQUESTED);
			next = page;
			break;
		}

		 
		if (*offset) {
			if (!__isolate_free_page(page, order)) {
				next = page;
				break;
			}

			 
			--(*offset);
			sg_set_page(&sgl[*offset], page, page_len, 0);

			continue;
		}

		 
		if (!list_is_first(&page->lru, list))
			list_rotate_to_front(&page->lru, list);

		 
		spin_unlock_irq(&zone->lock);

		 
		err = prdev->report(prdev, sgl, PAGE_REPORTING_CAPACITY);

		 
		*offset = PAGE_REPORTING_CAPACITY;

		 
		budget--;

		 
		spin_lock_irq(&zone->lock);

		 
		page_reporting_drain(prdev, sgl, PAGE_REPORTING_CAPACITY, !err);

		 
		next = list_first_entry(list, struct page, lru);

		 
		if (err)
			break;
	}

	 
	if (!list_entry_is_head(next, list, lru) && !list_is_first(&next->lru, list))
		list_rotate_to_front(&next->lru, list);

	spin_unlock_irq(&zone->lock);

	return err;
}

static int
page_reporting_process_zone(struct page_reporting_dev_info *prdev,
			    struct scatterlist *sgl, struct zone *zone)
{
	unsigned int order, mt, leftover, offset = PAGE_REPORTING_CAPACITY;
	unsigned long watermark;
	int err = 0;

	 
	watermark = low_wmark_pages(zone) +
		    (PAGE_REPORTING_CAPACITY << page_reporting_order);

	 
	if (!zone_watermark_ok(zone, 0, watermark, 0, ALLOC_CMA))
		return err;

	 
	for (order = page_reporting_order; order <= MAX_ORDER; order++) {
		for (mt = 0; mt < MIGRATE_TYPES; mt++) {
			 
			if (is_migrate_isolate(mt))
				continue;

			err = page_reporting_cycle(prdev, zone, order, mt,
						   sgl, &offset);
			if (err)
				return err;
		}
	}

	 
	leftover = PAGE_REPORTING_CAPACITY - offset;
	if (leftover) {
		sgl = &sgl[offset];
		err = prdev->report(prdev, sgl, leftover);

		 
		spin_lock_irq(&zone->lock);
		page_reporting_drain(prdev, sgl, leftover, !err);
		spin_unlock_irq(&zone->lock);
	}

	return err;
}

static void page_reporting_process(struct work_struct *work)
{
	struct delayed_work *d_work = to_delayed_work(work);
	struct page_reporting_dev_info *prdev =
		container_of(d_work, struct page_reporting_dev_info, work);
	int err = 0, state = PAGE_REPORTING_ACTIVE;
	struct scatterlist *sgl;
	struct zone *zone;

	 
	atomic_set(&prdev->state, state);

	 
	sgl = kmalloc_array(PAGE_REPORTING_CAPACITY, sizeof(*sgl), GFP_KERNEL);
	if (!sgl)
		goto err_out;

	sg_init_table(sgl, PAGE_REPORTING_CAPACITY);

	for_each_zone(zone) {
		err = page_reporting_process_zone(prdev, sgl, zone);
		if (err)
			break;
	}

	kfree(sgl);
err_out:
	 
	state = atomic_cmpxchg(&prdev->state, state, PAGE_REPORTING_IDLE);
	if (state == PAGE_REPORTING_REQUESTED)
		schedule_delayed_work(&prdev->work, PAGE_REPORTING_DELAY);
}

static DEFINE_MUTEX(page_reporting_mutex);
DEFINE_STATIC_KEY_FALSE(page_reporting_enabled);

int page_reporting_register(struct page_reporting_dev_info *prdev)
{
	int err = 0;

	mutex_lock(&page_reporting_mutex);

	 
	if (rcu_dereference_protected(pr_dev_info,
				lockdep_is_held(&page_reporting_mutex))) {
		err = -EBUSY;
		goto err_out;
	}

	 

	if (page_reporting_order == -1) {
		if (prdev->order > 0 && prdev->order <= MAX_ORDER)
			page_reporting_order = prdev->order;
		else
			page_reporting_order = pageblock_order;
	}

	 
	atomic_set(&prdev->state, PAGE_REPORTING_IDLE);
	INIT_DELAYED_WORK(&prdev->work, &page_reporting_process);

	 
	__page_reporting_request(prdev);

	 
	rcu_assign_pointer(pr_dev_info, prdev);

	 
	if (!static_key_enabled(&page_reporting_enabled)) {
		static_branch_enable(&page_reporting_enabled);
		pr_info("Free page reporting enabled\n");
	}
err_out:
	mutex_unlock(&page_reporting_mutex);

	return err;
}
EXPORT_SYMBOL_GPL(page_reporting_register);

void page_reporting_unregister(struct page_reporting_dev_info *prdev)
{
	mutex_lock(&page_reporting_mutex);

	if (prdev == rcu_dereference_protected(pr_dev_info,
				lockdep_is_held(&page_reporting_mutex))) {
		 
		RCU_INIT_POINTER(pr_dev_info, NULL);
		synchronize_rcu();

		 
		cancel_delayed_work_sync(&prdev->work);
	}

	mutex_unlock(&page_reporting_mutex);
}
EXPORT_SYMBOL_GPL(page_reporting_unregister);
