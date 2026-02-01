

 

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/mm.h>

#include <asm/xen/hypervisor.h>
#include <xen/balloon.h>
#include <xen/xen.h>
#include <xen/xenbus.h>
#include <xen/interface/io/ring.h>

#include <xen/xen-front-pgdir-shbuf.h>

 
struct xen_page_directory {
	grant_ref_t gref_dir_next_page;
#define XEN_GREF_LIST_END	0
	grant_ref_t gref[];  
};

 
struct xen_front_pgdir_shbuf_ops {
	 
	void (*calc_num_grefs)(struct xen_front_pgdir_shbuf *buf);

	 
	void (*fill_page_dir)(struct xen_front_pgdir_shbuf *buf);

	 
	int (*grant_refs_for_buffer)(struct xen_front_pgdir_shbuf *buf,
				     grant_ref_t *priv_gref_head, int gref_idx);

	 
	int (*map)(struct xen_front_pgdir_shbuf *buf);

	 
	int (*unmap)(struct xen_front_pgdir_shbuf *buf);
};

 
grant_ref_t
xen_front_pgdir_shbuf_get_dir_start(struct xen_front_pgdir_shbuf *buf)
{
	if (!buf->grefs)
		return INVALID_GRANT_REF;

	return buf->grefs[0];
}
EXPORT_SYMBOL_GPL(xen_front_pgdir_shbuf_get_dir_start);

 
int xen_front_pgdir_shbuf_map(struct xen_front_pgdir_shbuf *buf)
{
	if (buf->ops && buf->ops->map)
		return buf->ops->map(buf);

	 
	return 0;
}
EXPORT_SYMBOL_GPL(xen_front_pgdir_shbuf_map);

 
int xen_front_pgdir_shbuf_unmap(struct xen_front_pgdir_shbuf *buf)
{
	if (buf->ops && buf->ops->unmap)
		return buf->ops->unmap(buf);

	 
	return 0;
}
EXPORT_SYMBOL_GPL(xen_front_pgdir_shbuf_unmap);

 
void xen_front_pgdir_shbuf_free(struct xen_front_pgdir_shbuf *buf)
{
	if (buf->grefs) {
		int i;

		for (i = 0; i < buf->num_grefs; i++)
			if (buf->grefs[i] != INVALID_GRANT_REF)
				gnttab_end_foreign_access(buf->grefs[i], NULL);
	}
	kfree(buf->grefs);
	kfree(buf->directory);
}
EXPORT_SYMBOL_GPL(xen_front_pgdir_shbuf_free);

 
#define XEN_NUM_GREFS_PER_PAGE ((PAGE_SIZE - \
				 offsetof(struct xen_page_directory, \
					  gref)) / sizeof(grant_ref_t))

 
static int get_num_pages_dir(struct xen_front_pgdir_shbuf *buf)
{
	return DIV_ROUND_UP(buf->num_pages, XEN_NUM_GREFS_PER_PAGE);
}

 
static void backend_calc_num_grefs(struct xen_front_pgdir_shbuf *buf)
{
	 
	buf->num_grefs = get_num_pages_dir(buf);
}

 
static void guest_calc_num_grefs(struct xen_front_pgdir_shbuf *buf)
{
	 
	buf->num_grefs = get_num_pages_dir(buf) + buf->num_pages;
}

#define xen_page_to_vaddr(page) \
	((uintptr_t)pfn_to_kaddr(page_to_xen_pfn(page)))

 
static int backend_unmap(struct xen_front_pgdir_shbuf *buf)
{
	struct gnttab_unmap_grant_ref *unmap_ops;
	int i, ret;

	if (!buf->pages || !buf->backend_map_handles || !buf->grefs)
		return 0;

	unmap_ops = kcalloc(buf->num_pages, sizeof(*unmap_ops),
			    GFP_KERNEL);
	if (!unmap_ops)
		return -ENOMEM;

	for (i = 0; i < buf->num_pages; i++) {
		phys_addr_t addr;

		addr = xen_page_to_vaddr(buf->pages[i]);
		gnttab_set_unmap_op(&unmap_ops[i], addr, GNTMAP_host_map,
				    buf->backend_map_handles[i]);
	}

	ret = gnttab_unmap_refs(unmap_ops, NULL, buf->pages,
				buf->num_pages);

	for (i = 0; i < buf->num_pages; i++) {
		if (unlikely(unmap_ops[i].status != GNTST_okay))
			dev_err(&buf->xb_dev->dev,
				"Failed to unmap page %d: %d\n",
				i, unmap_ops[i].status);
	}

	if (ret)
		dev_err(&buf->xb_dev->dev,
			"Failed to unmap grant references, ret %d", ret);

	kfree(unmap_ops);
	kfree(buf->backend_map_handles);
	buf->backend_map_handles = NULL;
	return ret;
}

 
static int backend_map(struct xen_front_pgdir_shbuf *buf)
{
	struct gnttab_map_grant_ref *map_ops = NULL;
	unsigned char *ptr;
	int ret, cur_gref, cur_dir_page, cur_page, grefs_left;

	map_ops = kcalloc(buf->num_pages, sizeof(*map_ops), GFP_KERNEL);
	if (!map_ops)
		return -ENOMEM;

	buf->backend_map_handles = kcalloc(buf->num_pages,
					   sizeof(*buf->backend_map_handles),
					   GFP_KERNEL);
	if (!buf->backend_map_handles) {
		kfree(map_ops);
		return -ENOMEM;
	}

	 
	ptr = buf->directory;
	grefs_left = buf->num_pages;
	cur_page = 0;
	for (cur_dir_page = 0; cur_dir_page < buf->num_grefs; cur_dir_page++) {
		struct xen_page_directory *page_dir =
			(struct xen_page_directory *)ptr;
		int to_copy = XEN_NUM_GREFS_PER_PAGE;

		if (to_copy > grefs_left)
			to_copy = grefs_left;

		for (cur_gref = 0; cur_gref < to_copy; cur_gref++) {
			phys_addr_t addr;

			addr = xen_page_to_vaddr(buf->pages[cur_page]);
			gnttab_set_map_op(&map_ops[cur_page], addr,
					  GNTMAP_host_map,
					  page_dir->gref[cur_gref],
					  buf->xb_dev->otherend_id);
			cur_page++;
		}

		grefs_left -= to_copy;
		ptr += PAGE_SIZE;
	}
	ret = gnttab_map_refs(map_ops, NULL, buf->pages, buf->num_pages);

	 
	for (cur_page = 0; cur_page < buf->num_pages; cur_page++) {
		if (likely(map_ops[cur_page].status == GNTST_okay)) {
			buf->backend_map_handles[cur_page] =
				map_ops[cur_page].handle;
		} else {
			buf->backend_map_handles[cur_page] =
				INVALID_GRANT_HANDLE;
			if (!ret)
				ret = -ENXIO;
			dev_err(&buf->xb_dev->dev,
				"Failed to map page %d: %d\n",
				cur_page, map_ops[cur_page].status);
		}
	}

	if (ret) {
		dev_err(&buf->xb_dev->dev,
			"Failed to map grant references, ret %d", ret);
		backend_unmap(buf);
	}

	kfree(map_ops);
	return ret;
}

 
static void backend_fill_page_dir(struct xen_front_pgdir_shbuf *buf)
{
	struct xen_page_directory *page_dir;
	unsigned char *ptr;
	int i, num_pages_dir;

	ptr = buf->directory;
	num_pages_dir = get_num_pages_dir(buf);

	 
	for (i = 0; i < num_pages_dir - 1; i++) {
		page_dir = (struct xen_page_directory *)ptr;

		page_dir->gref_dir_next_page = buf->grefs[i + 1];
		ptr += PAGE_SIZE;
	}
	 
	page_dir = (struct xen_page_directory *)ptr;
	page_dir->gref_dir_next_page = XEN_GREF_LIST_END;
}

 
static void guest_fill_page_dir(struct xen_front_pgdir_shbuf *buf)
{
	unsigned char *ptr;
	int cur_gref, grefs_left, to_copy, i, num_pages_dir;

	ptr = buf->directory;
	num_pages_dir = get_num_pages_dir(buf);

	 
	cur_gref = num_pages_dir;
	grefs_left = buf->num_pages;
	for (i = 0; i < num_pages_dir; i++) {
		struct xen_page_directory *page_dir =
			(struct xen_page_directory *)ptr;

		if (grefs_left <= XEN_NUM_GREFS_PER_PAGE) {
			to_copy = grefs_left;
			page_dir->gref_dir_next_page = XEN_GREF_LIST_END;
		} else {
			to_copy = XEN_NUM_GREFS_PER_PAGE;
			page_dir->gref_dir_next_page = buf->grefs[i + 1];
		}
		memcpy(&page_dir->gref, &buf->grefs[cur_gref],
		       to_copy * sizeof(grant_ref_t));
		ptr += PAGE_SIZE;
		grefs_left -= to_copy;
		cur_gref += to_copy;
	}
}

 
static int guest_grant_refs_for_buffer(struct xen_front_pgdir_shbuf *buf,
				       grant_ref_t *priv_gref_head,
				       int gref_idx)
{
	int i, cur_ref, otherend_id;

	otherend_id = buf->xb_dev->otherend_id;
	for (i = 0; i < buf->num_pages; i++) {
		cur_ref = gnttab_claim_grant_reference(priv_gref_head);
		if (cur_ref < 0)
			return cur_ref;

		gnttab_grant_foreign_access_ref(cur_ref, otherend_id,
						xen_page_to_gfn(buf->pages[i]),
						0);
		buf->grefs[gref_idx++] = cur_ref;
	}
	return 0;
}

 
static int grant_references(struct xen_front_pgdir_shbuf *buf)
{
	grant_ref_t priv_gref_head;
	int ret, i, j, cur_ref;
	int otherend_id, num_pages_dir;

	ret = gnttab_alloc_grant_references(buf->num_grefs, &priv_gref_head);
	if (ret < 0) {
		dev_err(&buf->xb_dev->dev,
			"Cannot allocate grant references\n");
		return ret;
	}

	otherend_id = buf->xb_dev->otherend_id;
	j = 0;
	num_pages_dir = get_num_pages_dir(buf);
	for (i = 0; i < num_pages_dir; i++) {
		unsigned long frame;

		cur_ref = gnttab_claim_grant_reference(&priv_gref_head);
		if (cur_ref < 0)
			return cur_ref;

		frame = xen_page_to_gfn(virt_to_page(buf->directory +
						     PAGE_SIZE * i));
		gnttab_grant_foreign_access_ref(cur_ref, otherend_id, frame, 0);
		buf->grefs[j++] = cur_ref;
	}

	if (buf->ops->grant_refs_for_buffer) {
		ret = buf->ops->grant_refs_for_buffer(buf, &priv_gref_head, j);
		if (ret)
			return ret;
	}

	gnttab_free_grant_references(priv_gref_head);
	return 0;
}

 
static int alloc_storage(struct xen_front_pgdir_shbuf *buf)
{
	buf->grefs = kcalloc(buf->num_grefs, sizeof(*buf->grefs), GFP_KERNEL);
	if (!buf->grefs)
		return -ENOMEM;

	buf->directory = kcalloc(get_num_pages_dir(buf), PAGE_SIZE, GFP_KERNEL);
	if (!buf->directory)
		return -ENOMEM;

	return 0;
}

 
static const struct xen_front_pgdir_shbuf_ops backend_ops = {
	.calc_num_grefs = backend_calc_num_grefs,
	.fill_page_dir = backend_fill_page_dir,
	.map = backend_map,
	.unmap = backend_unmap
};

 
static const struct xen_front_pgdir_shbuf_ops local_ops = {
	.calc_num_grefs = guest_calc_num_grefs,
	.fill_page_dir = guest_fill_page_dir,
	.grant_refs_for_buffer = guest_grant_refs_for_buffer,
};

 
int xen_front_pgdir_shbuf_alloc(struct xen_front_pgdir_shbuf_cfg *cfg)
{
	struct xen_front_pgdir_shbuf *buf = cfg->pgdir;
	int ret;

	if (cfg->be_alloc)
		buf->ops = &backend_ops;
	else
		buf->ops = &local_ops;
	buf->xb_dev = cfg->xb_dev;
	buf->num_pages = cfg->num_pages;
	buf->pages = cfg->pages;

	buf->ops->calc_num_grefs(buf);

	ret = alloc_storage(buf);
	if (ret)
		goto fail;

	ret = grant_references(buf);
	if (ret)
		goto fail;

	buf->ops->fill_page_dir(buf);

	return 0;

fail:
	xen_front_pgdir_shbuf_free(buf);
	return ret;
}
EXPORT_SYMBOL_GPL(xen_front_pgdir_shbuf_alloc);

MODULE_DESCRIPTION("Xen frontend/backend page directory based "
		   "shared buffer handling");
MODULE_AUTHOR("Oleksandr Andrushchenko");
MODULE_LICENSE("GPL");
