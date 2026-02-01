 

#include <linux/slab.h>
#include <rdma/uverbs_ioctl.h>

#include "mlx4_ib.h"

struct mlx4_ib_user_db_page {
	struct list_head	list;
	struct ib_umem	       *umem;
	unsigned long		user_virt;
	int			refcnt;
};

int mlx4_ib_db_map_user(struct ib_udata *udata, unsigned long virt,
			struct mlx4_db *db)
{
	struct mlx4_ib_user_db_page *page;
	int err = 0;
	struct mlx4_ib_ucontext *context = rdma_udata_to_drv_context(
		udata, struct mlx4_ib_ucontext, ibucontext);

	mutex_lock(&context->db_page_mutex);

	list_for_each_entry(page, &context->db_page_list, list)
		if (page->user_virt == (virt & PAGE_MASK))
			goto found;

	page = kmalloc(sizeof *page, GFP_KERNEL);
	if (!page) {
		err = -ENOMEM;
		goto out;
	}

	page->user_virt = (virt & PAGE_MASK);
	page->refcnt    = 0;
	page->umem = ib_umem_get(context->ibucontext.device, virt & PAGE_MASK,
				 PAGE_SIZE, 0);
	if (IS_ERR(page->umem)) {
		err = PTR_ERR(page->umem);
		kfree(page);
		goto out;
	}

	list_add(&page->list, &context->db_page_list);

found:
	db->dma = sg_dma_address(page->umem->sgt_append.sgt.sgl) +
		  (virt & ~PAGE_MASK);
	db->u.user_page = page;
	++page->refcnt;

out:
	mutex_unlock(&context->db_page_mutex);

	return err;
}

void mlx4_ib_db_unmap_user(struct mlx4_ib_ucontext *context, struct mlx4_db *db)
{
	mutex_lock(&context->db_page_mutex);

	if (!--db->u.user_page->refcnt) {
		list_del(&db->u.user_page->list);
		ib_umem_release(db->u.user_page->umem);
		kfree(db->u.user_page);
	}

	mutex_unlock(&context->db_page_mutex);
}
