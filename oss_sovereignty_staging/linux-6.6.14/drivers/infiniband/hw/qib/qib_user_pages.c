 

#include <linux/mm.h>
#include <linux/sched/signal.h>
#include <linux/device.h>

#include "qib.h"

static void __qib_release_user_pages(struct page **p, size_t num_pages,
				     int dirty)
{
	unpin_user_pages_dirty_lock(p, num_pages, dirty);
}

 
int qib_map_page(struct pci_dev *hwdev, struct page *page, dma_addr_t *daddr)
{
	dma_addr_t phys;

	phys = dma_map_page(&hwdev->dev, page, 0, PAGE_SIZE, DMA_FROM_DEVICE);
	if (dma_mapping_error(&hwdev->dev, phys))
		return -ENOMEM;

	if (!phys) {
		dma_unmap_page(&hwdev->dev, phys, PAGE_SIZE, DMA_FROM_DEVICE);
		phys = dma_map_page(&hwdev->dev, page, 0, PAGE_SIZE,
				    DMA_FROM_DEVICE);
		if (dma_mapping_error(&hwdev->dev, phys))
			return -ENOMEM;
		 
	}
	*daddr = phys;
	return 0;
}

 
int qib_get_user_pages(unsigned long start_page, size_t num_pages,
		       struct page **p)
{
	unsigned long locked, lock_limit;
	size_t got;
	int ret;

	lock_limit = rlimit(RLIMIT_MEMLOCK) >> PAGE_SHIFT;
	locked = atomic64_add_return(num_pages, &current->mm->pinned_vm);

	if (locked > lock_limit && !capable(CAP_IPC_LOCK)) {
		ret = -ENOMEM;
		goto bail;
	}

	mmap_read_lock(current->mm);
	for (got = 0; got < num_pages; got += ret) {
		ret = pin_user_pages(start_page + got * PAGE_SIZE,
				     num_pages - got,
				     FOLL_LONGTERM | FOLL_WRITE,
				     p + got);
		if (ret < 0) {
			mmap_read_unlock(current->mm);
			goto bail_release;
		}
	}
	mmap_read_unlock(current->mm);

	return 0;
bail_release:
	__qib_release_user_pages(p, got, 0);
bail:
	atomic64_sub(num_pages, &current->mm->pinned_vm);
	return ret;
}

void qib_release_user_pages(struct page **p, size_t num_pages)
{
	__qib_release_user_pages(p, num_pages, 1);

	 
	if (current->mm)
		atomic64_sub(num_pages, &current->mm->pinned_vm);
}
