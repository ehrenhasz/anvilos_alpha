
 
#include <linux/xarray.h>
#include "uverbs.h"
#include "core_priv.h"

 
void rdma_umap_priv_init(struct rdma_umap_priv *priv,
			 struct vm_area_struct *vma,
			 struct rdma_user_mmap_entry *entry)
{
	struct ib_uverbs_file *ufile = vma->vm_file->private_data;

	priv->vma = vma;
	if (entry) {
		kref_get(&entry->ref);
		priv->entry = entry;
	}
	vma->vm_private_data = priv;
	 

	mutex_lock(&ufile->umap_lock);
	list_add(&priv->list, &ufile->umaps);
	mutex_unlock(&ufile->umap_lock);
}
EXPORT_SYMBOL(rdma_umap_priv_init);

 
int rdma_user_mmap_io(struct ib_ucontext *ucontext, struct vm_area_struct *vma,
		      unsigned long pfn, unsigned long size, pgprot_t prot,
		      struct rdma_user_mmap_entry *entry)
{
	struct ib_uverbs_file *ufile = ucontext->ufile;
	struct rdma_umap_priv *priv;

	if (!(vma->vm_flags & VM_SHARED))
		return -EINVAL;

	if (vma->vm_end - vma->vm_start != size)
		return -EINVAL;

	 
	if (WARN_ON(!vma->vm_file ||
		    vma->vm_file->private_data != ufile))
		return -EINVAL;
	lockdep_assert_held(&ufile->device->disassociate_srcu);

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	vma->vm_page_prot = prot;
	if (io_remap_pfn_range(vma, vma->vm_start, pfn, size, prot)) {
		kfree(priv);
		return -EAGAIN;
	}

	rdma_umap_priv_init(priv, vma, entry);
	return 0;
}
EXPORT_SYMBOL(rdma_user_mmap_io);

 
struct rdma_user_mmap_entry *
rdma_user_mmap_entry_get_pgoff(struct ib_ucontext *ucontext,
			       unsigned long pgoff)
{
	struct rdma_user_mmap_entry *entry;

	if (pgoff > U32_MAX)
		return NULL;

	xa_lock(&ucontext->mmap_xa);

	entry = xa_load(&ucontext->mmap_xa, pgoff);

	 
	if (!entry || entry->start_pgoff != pgoff || entry->driver_removed ||
	    !kref_get_unless_zero(&entry->ref))
		goto err;

	xa_unlock(&ucontext->mmap_xa);

	ibdev_dbg(ucontext->device, "mmap: pgoff[%#lx] npages[%#zx] returned\n",
		  pgoff, entry->npages);

	return entry;

err:
	xa_unlock(&ucontext->mmap_xa);
	return NULL;
}
EXPORT_SYMBOL(rdma_user_mmap_entry_get_pgoff);

 
struct rdma_user_mmap_entry *
rdma_user_mmap_entry_get(struct ib_ucontext *ucontext,
			 struct vm_area_struct *vma)
{
	struct rdma_user_mmap_entry *entry;

	if (!(vma->vm_flags & VM_SHARED))
		return NULL;
	entry = rdma_user_mmap_entry_get_pgoff(ucontext, vma->vm_pgoff);
	if (!entry)
		return NULL;
	if (entry->npages * PAGE_SIZE != vma->vm_end - vma->vm_start) {
		rdma_user_mmap_entry_put(entry);
		return NULL;
	}
	return entry;
}
EXPORT_SYMBOL(rdma_user_mmap_entry_get);

static void rdma_user_mmap_entry_free(struct kref *kref)
{
	struct rdma_user_mmap_entry *entry =
		container_of(kref, struct rdma_user_mmap_entry, ref);
	struct ib_ucontext *ucontext = entry->ucontext;
	unsigned long i;

	 
	xa_lock(&ucontext->mmap_xa);
	for (i = 0; i < entry->npages; i++)
		__xa_erase(&ucontext->mmap_xa, entry->start_pgoff + i);
	xa_unlock(&ucontext->mmap_xa);

	ibdev_dbg(ucontext->device, "mmap: pgoff[%#lx] npages[%#zx] removed\n",
		  entry->start_pgoff, entry->npages);

	if (ucontext->device->ops.mmap_free)
		ucontext->device->ops.mmap_free(entry);
}

 
void rdma_user_mmap_entry_put(struct rdma_user_mmap_entry *entry)
{
	kref_put(&entry->ref, rdma_user_mmap_entry_free);
}
EXPORT_SYMBOL(rdma_user_mmap_entry_put);

 
void rdma_user_mmap_entry_remove(struct rdma_user_mmap_entry *entry)
{
	if (!entry)
		return;

	xa_lock(&entry->ucontext->mmap_xa);
	entry->driver_removed = true;
	xa_unlock(&entry->ucontext->mmap_xa);
	kref_put(&entry->ref, rdma_user_mmap_entry_free);
}
EXPORT_SYMBOL(rdma_user_mmap_entry_remove);

 
int rdma_user_mmap_entry_insert_range(struct ib_ucontext *ucontext,
				      struct rdma_user_mmap_entry *entry,
				      size_t length, u32 min_pgoff,
				      u32 max_pgoff)
{
	struct ib_uverbs_file *ufile = ucontext->ufile;
	XA_STATE(xas, &ucontext->mmap_xa, min_pgoff);
	u32 xa_first, xa_last, npages;
	int err;
	u32 i;

	if (!entry)
		return -EINVAL;

	kref_init(&entry->ref);
	entry->ucontext = ucontext;

	 
	mutex_lock(&ufile->umap_lock);

	xa_lock(&ucontext->mmap_xa);

	 
	npages = (u32)DIV_ROUND_UP(length, PAGE_SIZE);
	entry->npages = npages;
	while (true) {
		 
		xas_find_marked(&xas, max_pgoff, XA_FREE_MARK);
		if (xas.xa_node == XAS_RESTART)
			goto err_unlock;

		xa_first = xas.xa_index;

		 
		if (check_add_overflow(xa_first, npages, &xa_last))
			goto err_unlock;

		 
		xas_next_entry(&xas, xa_last - 1);
		if (xas.xa_node == XAS_BOUNDS || xas.xa_index >= xa_last)
			break;
	}

	for (i = xa_first; i < xa_last; i++) {
		err = __xa_insert(&ucontext->mmap_xa, i, entry, GFP_KERNEL);
		if (err)
			goto err_undo;
	}

	 
	entry->start_pgoff = xa_first;
	xa_unlock(&ucontext->mmap_xa);
	mutex_unlock(&ufile->umap_lock);

	ibdev_dbg(ucontext->device, "mmap: pgoff[%#lx] npages[%#x] inserted\n",
		  entry->start_pgoff, npages);

	return 0;

err_undo:
	for (; i > xa_first; i--)
		__xa_erase(&ucontext->mmap_xa, i - 1);

err_unlock:
	xa_unlock(&ucontext->mmap_xa);
	mutex_unlock(&ufile->umap_lock);
	return -ENOMEM;
}
EXPORT_SYMBOL(rdma_user_mmap_entry_insert_range);

 
int rdma_user_mmap_entry_insert(struct ib_ucontext *ucontext,
				struct rdma_user_mmap_entry *entry,
				size_t length)
{
	return rdma_user_mmap_entry_insert_range(ucontext, entry, length, 0,
						 U32_MAX);
}
EXPORT_SYMBOL(rdma_user_mmap_entry_insert);
