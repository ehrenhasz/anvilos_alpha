
 

#include <linux/mm.h>
#include <linux/sched/signal.h>
#include <linux/device.h>
#include <linux/module.h>

#include "hfi.h"

static unsigned long cache_size = 256;
module_param(cache_size, ulong, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(cache_size, "Send and receive side cache size limit (in MB)");

 
bool hfi1_can_pin_pages(struct hfi1_devdata *dd, struct mm_struct *mm,
			u32 nlocked, u32 npages)
{
	unsigned long ulimit_pages;
	unsigned long cache_limit_pages;
	unsigned int usr_ctxts;

	 
	if (!capable(CAP_IPC_LOCK)) {
		ulimit_pages =
			DIV_ROUND_DOWN_ULL(rlimit(RLIMIT_MEMLOCK), PAGE_SIZE);

		 
		if (atomic64_read(&mm->pinned_vm) + npages > ulimit_pages)
			return false;

		 
		usr_ctxts = dd->num_rcv_contexts - dd->first_dyn_alloc_ctxt;
		if (nlocked + npages > (ulimit_pages / usr_ctxts / 4))
			return false;
	}

	 
	cache_limit_pages = cache_size * (1024 * 1024) / PAGE_SIZE;
	if (nlocked + npages > cache_limit_pages)
		return false;

	return true;
}

int hfi1_acquire_user_pages(struct mm_struct *mm, unsigned long vaddr, size_t npages,
			    bool writable, struct page **pages)
{
	int ret;
	unsigned int gup_flags = FOLL_LONGTERM | (writable ? FOLL_WRITE : 0);

	ret = pin_user_pages_fast(vaddr, npages, gup_flags, pages);
	if (ret < 0)
		return ret;

	atomic64_add(ret, &mm->pinned_vm);

	return ret;
}

void hfi1_release_user_pages(struct mm_struct *mm, struct page **p,
			     size_t npages, bool dirty)
{
	unpin_user_pages_dirty_lock(p, npages, dirty);

	if (mm) {  
		atomic64_sub(npages, &mm->pinned_vm);
	}
}
