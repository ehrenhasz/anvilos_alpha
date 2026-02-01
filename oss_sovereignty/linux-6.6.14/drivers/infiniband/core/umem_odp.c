 

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#include <linux/pid.h>
#include <linux/slab.h>
#include <linux/export.h>
#include <linux/vmalloc.h>
#include <linux/hugetlb.h>
#include <linux/interval_tree.h>
#include <linux/hmm.h>
#include <linux/pagemap.h>

#include <rdma/ib_umem_odp.h>

#include "uverbs.h"

static inline int ib_init_umem_odp(struct ib_umem_odp *umem_odp,
				   const struct mmu_interval_notifier_ops *ops)
{
	int ret;

	umem_odp->umem.is_odp = 1;
	mutex_init(&umem_odp->umem_mutex);

	if (!umem_odp->is_implicit_odp) {
		size_t page_size = 1UL << umem_odp->page_shift;
		unsigned long start;
		unsigned long end;
		size_t ndmas, npfns;

		start = ALIGN_DOWN(umem_odp->umem.address, page_size);
		if (check_add_overflow(umem_odp->umem.address,
				       (unsigned long)umem_odp->umem.length,
				       &end))
			return -EOVERFLOW;
		end = ALIGN(end, page_size);
		if (unlikely(end < page_size))
			return -EOVERFLOW;

		ndmas = (end - start) >> umem_odp->page_shift;
		if (!ndmas)
			return -EINVAL;

		npfns = (end - start) >> PAGE_SHIFT;
		umem_odp->pfn_list = kvcalloc(
			npfns, sizeof(*umem_odp->pfn_list), GFP_KERNEL);
		if (!umem_odp->pfn_list)
			return -ENOMEM;

		umem_odp->dma_list = kvcalloc(
			ndmas, sizeof(*umem_odp->dma_list), GFP_KERNEL);
		if (!umem_odp->dma_list) {
			ret = -ENOMEM;
			goto out_pfn_list;
		}

		ret = mmu_interval_notifier_insert(&umem_odp->notifier,
						   umem_odp->umem.owning_mm,
						   start, end - start, ops);
		if (ret)
			goto out_dma_list;
	}

	return 0;

out_dma_list:
	kvfree(umem_odp->dma_list);
out_pfn_list:
	kvfree(umem_odp->pfn_list);
	return ret;
}

 
struct ib_umem_odp *ib_umem_odp_alloc_implicit(struct ib_device *device,
					       int access)
{
	struct ib_umem *umem;
	struct ib_umem_odp *umem_odp;
	int ret;

	if (access & IB_ACCESS_HUGETLB)
		return ERR_PTR(-EINVAL);

	umem_odp = kzalloc(sizeof(*umem_odp), GFP_KERNEL);
	if (!umem_odp)
		return ERR_PTR(-ENOMEM);
	umem = &umem_odp->umem;
	umem->ibdev = device;
	umem->writable = ib_access_writable(access);
	umem->owning_mm = current->mm;
	umem_odp->is_implicit_odp = 1;
	umem_odp->page_shift = PAGE_SHIFT;

	umem_odp->tgid = get_task_pid(current->group_leader, PIDTYPE_PID);
	ret = ib_init_umem_odp(umem_odp, NULL);
	if (ret) {
		put_pid(umem_odp->tgid);
		kfree(umem_odp);
		return ERR_PTR(ret);
	}
	return umem_odp;
}
EXPORT_SYMBOL(ib_umem_odp_alloc_implicit);

 
struct ib_umem_odp *
ib_umem_odp_alloc_child(struct ib_umem_odp *root, unsigned long addr,
			size_t size,
			const struct mmu_interval_notifier_ops *ops)
{
	 
	struct ib_umem_odp *odp_data;
	struct ib_umem *umem;
	int ret;

	if (WARN_ON(!root->is_implicit_odp))
		return ERR_PTR(-EINVAL);

	odp_data = kzalloc(sizeof(*odp_data), GFP_KERNEL);
	if (!odp_data)
		return ERR_PTR(-ENOMEM);
	umem = &odp_data->umem;
	umem->ibdev = root->umem.ibdev;
	umem->length     = size;
	umem->address    = addr;
	umem->writable   = root->umem.writable;
	umem->owning_mm  = root->umem.owning_mm;
	odp_data->page_shift = PAGE_SHIFT;
	odp_data->notifier.ops = ops;

	 
	if (!mmget_not_zero(umem->owning_mm)) {
		ret = -EFAULT;
		goto out_free;
	}

	odp_data->tgid = get_pid(root->tgid);
	ret = ib_init_umem_odp(odp_data, ops);
	if (ret)
		goto out_tgid;
	mmput(umem->owning_mm);
	return odp_data;

out_tgid:
	put_pid(odp_data->tgid);
	mmput(umem->owning_mm);
out_free:
	kfree(odp_data);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(ib_umem_odp_alloc_child);

 
struct ib_umem_odp *ib_umem_odp_get(struct ib_device *device,
				    unsigned long addr, size_t size, int access,
				    const struct mmu_interval_notifier_ops *ops)
{
	struct ib_umem_odp *umem_odp;
	int ret;

	if (WARN_ON_ONCE(!(access & IB_ACCESS_ON_DEMAND)))
		return ERR_PTR(-EINVAL);

	umem_odp = kzalloc(sizeof(struct ib_umem_odp), GFP_KERNEL);
	if (!umem_odp)
		return ERR_PTR(-ENOMEM);

	umem_odp->umem.ibdev = device;
	umem_odp->umem.length = size;
	umem_odp->umem.address = addr;
	umem_odp->umem.writable = ib_access_writable(access);
	umem_odp->umem.owning_mm = current->mm;
	umem_odp->notifier.ops = ops;

	umem_odp->page_shift = PAGE_SHIFT;
#ifdef CONFIG_HUGETLB_PAGE
	if (access & IB_ACCESS_HUGETLB)
		umem_odp->page_shift = HPAGE_SHIFT;
#endif

	umem_odp->tgid = get_task_pid(current->group_leader, PIDTYPE_PID);
	ret = ib_init_umem_odp(umem_odp, ops);
	if (ret)
		goto err_put_pid;
	return umem_odp;

err_put_pid:
	put_pid(umem_odp->tgid);
	kfree(umem_odp);
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(ib_umem_odp_get);

void ib_umem_odp_release(struct ib_umem_odp *umem_odp)
{
	 
	if (!umem_odp->is_implicit_odp) {
		mutex_lock(&umem_odp->umem_mutex);
		ib_umem_odp_unmap_dma_pages(umem_odp, ib_umem_start(umem_odp),
					    ib_umem_end(umem_odp));
		mutex_unlock(&umem_odp->umem_mutex);
		mmu_interval_notifier_remove(&umem_odp->notifier);
		kvfree(umem_odp->dma_list);
		kvfree(umem_odp->pfn_list);
	}
	put_pid(umem_odp->tgid);
	kfree(umem_odp);
}
EXPORT_SYMBOL(ib_umem_odp_release);

 
static int ib_umem_odp_map_dma_single_page(
		struct ib_umem_odp *umem_odp,
		unsigned int dma_index,
		struct page *page,
		u64 access_mask)
{
	struct ib_device *dev = umem_odp->umem.ibdev;
	dma_addr_t *dma_addr = &umem_odp->dma_list[dma_index];

	if (*dma_addr) {
		 
		*dma_addr = (*dma_addr & ODP_DMA_ADDR_MASK) | access_mask;
		return 0;
	}

	*dma_addr = ib_dma_map_page(dev, page, 0, 1 << umem_odp->page_shift,
				    DMA_BIDIRECTIONAL);
	if (ib_dma_mapping_error(dev, *dma_addr)) {
		*dma_addr = 0;
		return -EFAULT;
	}
	umem_odp->npages++;
	*dma_addr |= access_mask;
	return 0;
}

 
int ib_umem_odp_map_dma_and_lock(struct ib_umem_odp *umem_odp, u64 user_virt,
				 u64 bcnt, u64 access_mask, bool fault)
			__acquires(&umem_odp->umem_mutex)
{
	struct task_struct *owning_process  = NULL;
	struct mm_struct *owning_mm = umem_odp->umem.owning_mm;
	int pfn_index, dma_index, ret = 0, start_idx;
	unsigned int page_shift, hmm_order, pfn_start_idx;
	unsigned long num_pfns, current_seq;
	struct hmm_range range = {};
	unsigned long timeout;

	if (access_mask == 0)
		return -EINVAL;

	if (user_virt < ib_umem_start(umem_odp) ||
	    user_virt + bcnt > ib_umem_end(umem_odp))
		return -EFAULT;

	page_shift = umem_odp->page_shift;

	 
	owning_process = get_pid_task(umem_odp->tgid, PIDTYPE_PID);
	if (!owning_process || !mmget_not_zero(owning_mm)) {
		ret = -EINVAL;
		goto out_put_task;
	}

	range.notifier = &umem_odp->notifier;
	range.start = ALIGN_DOWN(user_virt, 1UL << page_shift);
	range.end = ALIGN(user_virt + bcnt, 1UL << page_shift);
	pfn_start_idx = (range.start - ib_umem_start(umem_odp)) >> PAGE_SHIFT;
	num_pfns = (range.end - range.start) >> PAGE_SHIFT;
	if (fault) {
		range.default_flags = HMM_PFN_REQ_FAULT;

		if (access_mask & ODP_WRITE_ALLOWED_BIT)
			range.default_flags |= HMM_PFN_REQ_WRITE;
	}

	range.hmm_pfns = &(umem_odp->pfn_list[pfn_start_idx]);
	timeout = jiffies + msecs_to_jiffies(HMM_RANGE_DEFAULT_TIMEOUT);

retry:
	current_seq = range.notifier_seq =
		mmu_interval_read_begin(&umem_odp->notifier);

	mmap_read_lock(owning_mm);
	ret = hmm_range_fault(&range);
	mmap_read_unlock(owning_mm);
	if (unlikely(ret)) {
		if (ret == -EBUSY && !time_after(jiffies, timeout))
			goto retry;
		goto out_put_mm;
	}

	start_idx = (range.start - ib_umem_start(umem_odp)) >> page_shift;
	dma_index = start_idx;

	mutex_lock(&umem_odp->umem_mutex);
	if (mmu_interval_read_retry(&umem_odp->notifier, current_seq)) {
		mutex_unlock(&umem_odp->umem_mutex);
		goto retry;
	}

	for (pfn_index = 0; pfn_index < num_pfns;
		pfn_index += 1 << (page_shift - PAGE_SHIFT), dma_index++) {

		if (fault) {
			 
			WARN_ON(range.hmm_pfns[pfn_index] & HMM_PFN_ERROR);
			WARN_ON(!(range.hmm_pfns[pfn_index] & HMM_PFN_VALID));
		} else {
			if (!(range.hmm_pfns[pfn_index] & HMM_PFN_VALID)) {
				WARN_ON(umem_odp->dma_list[dma_index]);
				continue;
			}
			access_mask = ODP_READ_ALLOWED_BIT;
			if (range.hmm_pfns[pfn_index] & HMM_PFN_WRITE)
				access_mask |= ODP_WRITE_ALLOWED_BIT;
		}

		hmm_order = hmm_pfn_to_map_order(range.hmm_pfns[pfn_index]);
		 
		if (hmm_order + PAGE_SHIFT < page_shift) {
			ret = -EINVAL;
			ibdev_dbg(umem_odp->umem.ibdev,
				  "%s: un-expected hmm_order %u, page_shift %u\n",
				  __func__, hmm_order, page_shift);
			break;
		}

		ret = ib_umem_odp_map_dma_single_page(
				umem_odp, dma_index, hmm_pfn_to_page(range.hmm_pfns[pfn_index]),
				access_mask);
		if (ret < 0) {
			ibdev_dbg(umem_odp->umem.ibdev,
				  "ib_umem_odp_map_dma_single_page failed with error %d\n", ret);
			break;
		}
	}
	 
	if (!ret)
		ret = dma_index - start_idx;
	else
		mutex_unlock(&umem_odp->umem_mutex);

out_put_mm:
	mmput_async(owning_mm);
out_put_task:
	if (owning_process)
		put_task_struct(owning_process);
	return ret;
}
EXPORT_SYMBOL(ib_umem_odp_map_dma_and_lock);

void ib_umem_odp_unmap_dma_pages(struct ib_umem_odp *umem_odp, u64 virt,
				 u64 bound)
{
	dma_addr_t dma_addr;
	dma_addr_t dma;
	int idx;
	u64 addr;
	struct ib_device *dev = umem_odp->umem.ibdev;

	lockdep_assert_held(&umem_odp->umem_mutex);

	virt = max_t(u64, virt, ib_umem_start(umem_odp));
	bound = min_t(u64, bound, ib_umem_end(umem_odp));
	for (addr = virt; addr < bound; addr += BIT(umem_odp->page_shift)) {
		idx = (addr - ib_umem_start(umem_odp)) >> umem_odp->page_shift;
		dma = umem_odp->dma_list[idx];

		 
		if (dma) {
			unsigned long pfn_idx = (addr - ib_umem_start(umem_odp)) >> PAGE_SHIFT;
			struct page *page = hmm_pfn_to_page(umem_odp->pfn_list[pfn_idx]);

			dma_addr = dma & ODP_DMA_ADDR_MASK;
			ib_dma_unmap_page(dev, dma_addr,
					  BIT(umem_odp->page_shift),
					  DMA_BIDIRECTIONAL);
			if (dma & ODP_WRITE_ALLOWED_BIT) {
				struct page *head_page = compound_head(page);
				 
				set_page_dirty(head_page);
			}
			umem_odp->dma_list[idx] = 0;
			umem_odp->npages--;
		}
	}
}
EXPORT_SYMBOL(ib_umem_odp_unmap_dma_pages);
