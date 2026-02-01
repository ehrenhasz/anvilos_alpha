
 

#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <rdma/uverbs_ioctl.h>

#include "rxe.h"
#include "rxe_loc.h"
#include "rxe_queue.h"

void rxe_mmap_release(struct kref *ref)
{
	struct rxe_mmap_info *ip = container_of(ref,
					struct rxe_mmap_info, ref);
	struct rxe_dev *rxe = to_rdev(ip->context->device);

	spin_lock_bh(&rxe->pending_lock);

	if (!list_empty(&ip->pending_mmaps))
		list_del(&ip->pending_mmaps);

	spin_unlock_bh(&rxe->pending_lock);

	vfree(ip->obj);		 
	kfree(ip);
}

 
static void rxe_vma_open(struct vm_area_struct *vma)
{
	struct rxe_mmap_info *ip = vma->vm_private_data;

	kref_get(&ip->ref);
}

static void rxe_vma_close(struct vm_area_struct *vma)
{
	struct rxe_mmap_info *ip = vma->vm_private_data;

	kref_put(&ip->ref, rxe_mmap_release);
}

static const struct vm_operations_struct rxe_vm_ops = {
	.open = rxe_vma_open,
	.close = rxe_vma_close,
};

 
int rxe_mmap(struct ib_ucontext *context, struct vm_area_struct *vma)
{
	struct rxe_dev *rxe = to_rdev(context->device);
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long size = vma->vm_end - vma->vm_start;
	struct rxe_mmap_info *ip, *pp;
	int ret;

	 
	spin_lock_bh(&rxe->pending_lock);
	list_for_each_entry_safe(ip, pp, &rxe->pending_mmaps, pending_mmaps) {
		if (context != ip->context || (__u64)offset != ip->info.offset)
			continue;

		 
		if (size > ip->info.size) {
			rxe_dbg_dev(rxe, "mmap region is larger than the object!\n");
			spin_unlock_bh(&rxe->pending_lock);
			ret = -EINVAL;
			goto done;
		}

		goto found_it;
	}
	rxe_dbg_dev(rxe, "unable to find pending mmap info\n");
	spin_unlock_bh(&rxe->pending_lock);
	ret = -EINVAL;
	goto done;

found_it:
	list_del_init(&ip->pending_mmaps);
	spin_unlock_bh(&rxe->pending_lock);

	ret = remap_vmalloc_range(vma, ip->obj, 0);
	if (ret) {
		rxe_dbg_dev(rxe, "err %d from remap_vmalloc_range\n", ret);
		goto done;
	}

	vma->vm_ops = &rxe_vm_ops;
	vma->vm_private_data = ip;
	rxe_vma_open(vma);
done:
	return ret;
}

 
struct rxe_mmap_info *rxe_create_mmap_info(struct rxe_dev *rxe, u32 size,
					   struct ib_udata *udata, void *obj)
{
	struct rxe_mmap_info *ip;

	if (!udata)
		return ERR_PTR(-EINVAL);

	ip = kmalloc(sizeof(*ip), GFP_KERNEL);
	if (!ip)
		return ERR_PTR(-ENOMEM);

	size = PAGE_ALIGN(size);

	spin_lock_bh(&rxe->mmap_offset_lock);

	if (rxe->mmap_offset == 0)
		rxe->mmap_offset = ALIGN(PAGE_SIZE, SHMLBA);

	ip->info.offset = rxe->mmap_offset;
	rxe->mmap_offset += ALIGN(size, SHMLBA);

	spin_unlock_bh(&rxe->mmap_offset_lock);

	INIT_LIST_HEAD(&ip->pending_mmaps);
	ip->info.size = size;
	ip->context =
		container_of(udata, struct uverbs_attr_bundle, driver_udata)
			->context;
	ip->obj = obj;
	kref_init(&ip->ref);

	return ip;
}
