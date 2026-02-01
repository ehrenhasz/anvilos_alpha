
 

#include <linux/dma-mapping.h>
#include <linux/seq_file.h>
#include <linux/shmem_fs.h>
#include <linux/spinlock.h>
#include <linux/pfn_t.h>

#include <drm/drm_prime.h>
#include <drm/drm_vma_manager.h>

#include "omap_drv.h"
#include "omap_dmm_tiler.h"

 

 
#define OMAP_BO_MEM_DMA_API	0x01000000	 
#define OMAP_BO_MEM_SHMEM	0x02000000	 
#define OMAP_BO_MEM_DMABUF	0x08000000	 

struct omap_gem_object {
	struct drm_gem_object base;

	struct list_head mm_list;

	u32 flags;

	 
	u16 width, height;

	 
	u32 roll;

	 
	struct mutex lock;

	 
	dma_addr_t dma_addr;

	 
	refcount_t pin_cnt;

	 
	struct sg_table *sgt;

	 
	struct tiler_block *block;

	 
	struct page **pages;

	 
	dma_addr_t *dma_addrs;

	 
	void *vaddr;
};

#define to_omap_bo(x) container_of(x, struct omap_gem_object, base)

 
#define NUM_USERGART_ENTRIES 2
struct omap_drm_usergart_entry {
	struct tiler_block *block;	 
	dma_addr_t dma_addr;
	struct drm_gem_object *obj;	 
	pgoff_t obj_pgoff;		 
};

struct omap_drm_usergart {
	struct omap_drm_usergart_entry entry[NUM_USERGART_ENTRIES];
	int height;				 
	int height_shift;		 
	int slot_shift;			 
	int stride_pfn;			 
	int last;				 
};

 

 
u64 omap_gem_mmap_offset(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	int ret;
	size_t size;

	 
	size = omap_gem_mmap_size(obj);
	ret = drm_gem_create_mmap_offset_size(obj, size);
	if (ret) {
		dev_err(dev->dev, "could not allocate mmap offset\n");
		return 0;
	}

	return drm_vma_node_offset_addr(&obj->vma_node);
}

static bool omap_gem_is_contiguous(struct omap_gem_object *omap_obj)
{
	if (omap_obj->flags & OMAP_BO_MEM_DMA_API)
		return true;

	if ((omap_obj->flags & OMAP_BO_MEM_DMABUF) && omap_obj->sgt->nents == 1)
		return true;

	return false;
}

 

static void omap_gem_evict_entry(struct drm_gem_object *obj,
		enum tiler_fmt fmt, struct omap_drm_usergart_entry *entry)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	struct omap_drm_private *priv = obj->dev->dev_private;
	int n = priv->usergart[fmt].height;
	size_t size = PAGE_SIZE * n;
	loff_t off = omap_gem_mmap_offset(obj) +
			(entry->obj_pgoff << PAGE_SHIFT);
	const int m = DIV_ROUND_UP(omap_obj->width << fmt, PAGE_SIZE);

	if (m > 1) {
		int i;
		 
		for (i = n; i > 0; i--) {
			unmap_mapping_range(obj->dev->anon_inode->i_mapping,
					    off, PAGE_SIZE, 1);
			off += PAGE_SIZE * m;
		}
	} else {
		unmap_mapping_range(obj->dev->anon_inode->i_mapping,
				    off, size, 1);
	}

	entry->obj = NULL;
}

 
static void omap_gem_evict(struct drm_gem_object *obj)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	struct omap_drm_private *priv = obj->dev->dev_private;

	if (omap_obj->flags & OMAP_BO_TILED_MASK) {
		enum tiler_fmt fmt = gem2fmt(omap_obj->flags);
		int i;

		for (i = 0; i < NUM_USERGART_ENTRIES; i++) {
			struct omap_drm_usergart_entry *entry =
				&priv->usergart[fmt].entry[i];

			if (entry->obj == obj)
				omap_gem_evict_entry(obj, fmt, entry);
		}
	}
}

 

 
static int omap_gem_attach_pages(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	struct page **pages;
	int npages = obj->size >> PAGE_SHIFT;
	int i, ret;
	dma_addr_t *addrs;

	lockdep_assert_held(&omap_obj->lock);

	 
	if (!(omap_obj->flags & OMAP_BO_MEM_SHMEM) || omap_obj->pages)
		return 0;

	pages = drm_gem_get_pages(obj);
	if (IS_ERR(pages)) {
		dev_err(obj->dev->dev, "could not get pages: %ld\n", PTR_ERR(pages));
		return PTR_ERR(pages);
	}

	 
	if (omap_obj->flags & (OMAP_BO_WC|OMAP_BO_UNCACHED)) {
		addrs = kmalloc_array(npages, sizeof(*addrs), GFP_KERNEL);
		if (!addrs) {
			ret = -ENOMEM;
			goto free_pages;
		}

		for (i = 0; i < npages; i++) {
			addrs[i] = dma_map_page(dev->dev, pages[i],
					0, PAGE_SIZE, DMA_TO_DEVICE);

			if (dma_mapping_error(dev->dev, addrs[i])) {
				dev_warn(dev->dev,
					"%s: failed to map page\n", __func__);

				for (i = i - 1; i >= 0; --i) {
					dma_unmap_page(dev->dev, addrs[i],
						PAGE_SIZE, DMA_TO_DEVICE);
				}

				ret = -ENOMEM;
				goto free_addrs;
			}
		}
	} else {
		addrs = kcalloc(npages, sizeof(*addrs), GFP_KERNEL);
		if (!addrs) {
			ret = -ENOMEM;
			goto free_pages;
		}
	}

	omap_obj->dma_addrs = addrs;
	omap_obj->pages = pages;

	return 0;

free_addrs:
	kfree(addrs);
free_pages:
	drm_gem_put_pages(obj, pages, true, false);

	return ret;
}

 
static void omap_gem_detach_pages(struct drm_gem_object *obj)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	unsigned int npages = obj->size >> PAGE_SHIFT;
	unsigned int i;

	lockdep_assert_held(&omap_obj->lock);

	for (i = 0; i < npages; i++) {
		if (omap_obj->dma_addrs[i])
			dma_unmap_page(obj->dev->dev, omap_obj->dma_addrs[i],
				       PAGE_SIZE, DMA_TO_DEVICE);
	}

	kfree(omap_obj->dma_addrs);
	omap_obj->dma_addrs = NULL;

	drm_gem_put_pages(obj, omap_obj->pages, true, false);
	omap_obj->pages = NULL;
}

 
u32 omap_gem_flags(struct drm_gem_object *obj)
{
	return to_omap_bo(obj)->flags;
}

 
size_t omap_gem_mmap_size(struct drm_gem_object *obj)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	size_t size = obj->size;

	if (omap_obj->flags & OMAP_BO_TILED_MASK) {
		 
		size = tiler_vsize(gem2fmt(omap_obj->flags),
				omap_obj->width, omap_obj->height);
	}

	return size;
}

 

 
static vm_fault_t omap_gem_fault_1d(struct drm_gem_object *obj,
		struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	unsigned long pfn;
	pgoff_t pgoff;

	 
	pgoff = (vmf->address - vma->vm_start) >> PAGE_SHIFT;

	if (omap_obj->pages) {
		omap_gem_cpu_sync_page(obj, pgoff);
		pfn = page_to_pfn(omap_obj->pages[pgoff]);
	} else {
		BUG_ON(!omap_gem_is_contiguous(omap_obj));
		pfn = (omap_obj->dma_addr >> PAGE_SHIFT) + pgoff;
	}

	VERB("Inserting %p pfn %lx, pa %lx", (void *)vmf->address,
			pfn, pfn << PAGE_SHIFT);

	return vmf_insert_mixed(vma, vmf->address,
			__pfn_to_pfn_t(pfn, PFN_DEV));
}

 
static vm_fault_t omap_gem_fault_2d(struct drm_gem_object *obj,
		struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	struct omap_drm_private *priv = obj->dev->dev_private;
	struct omap_drm_usergart_entry *entry;
	enum tiler_fmt fmt = gem2fmt(omap_obj->flags);
	struct page *pages[64];   
	unsigned long pfn;
	pgoff_t pgoff, base_pgoff;
	unsigned long vaddr;
	int i, err, slots;
	vm_fault_t ret = VM_FAULT_NOPAGE;

	 
	const int n = priv->usergart[fmt].height;
	const int n_shift = priv->usergart[fmt].height_shift;

	 
	const int m = DIV_ROUND_UP(omap_obj->width << fmt, PAGE_SIZE);

	 
	pgoff = (vmf->address - vma->vm_start) >> PAGE_SHIFT;

	 
	base_pgoff = round_down(pgoff, m << n_shift);

	 
	slots = omap_obj->width >> priv->usergart[fmt].slot_shift;

	vaddr = vmf->address - ((pgoff - base_pgoff) << PAGE_SHIFT);

	entry = &priv->usergart[fmt].entry[priv->usergart[fmt].last];

	 
	if (entry->obj)
		omap_gem_evict_entry(entry->obj, fmt, entry);

	entry->obj = obj;
	entry->obj_pgoff = base_pgoff;

	 
	base_pgoff = (base_pgoff >> n_shift) * slots;

	 
	if (m > 1) {
		int off = pgoff % m;
		entry->obj_pgoff += off;
		base_pgoff /= m;
		slots = min(slots - (off << n_shift), n);
		base_pgoff += off << n_shift;
		vaddr += off << PAGE_SHIFT;
	}

	 
	memcpy(pages, &omap_obj->pages[base_pgoff],
			sizeof(struct page *) * slots);
	memset(pages + slots, 0,
			sizeof(struct page *) * (n - slots));

	err = tiler_pin(entry->block, pages, ARRAY_SIZE(pages), 0, true);
	if (err) {
		ret = vmf_error(err);
		dev_err(obj->dev->dev, "failed to pin: %d\n", err);
		return ret;
	}

	pfn = entry->dma_addr >> PAGE_SHIFT;

	VERB("Inserting %p pfn %lx, pa %lx", (void *)vmf->address,
			pfn, pfn << PAGE_SHIFT);

	for (i = n; i > 0; i--) {
		ret = vmf_insert_mixed(vma,
			vaddr, __pfn_to_pfn_t(pfn, PFN_DEV));
		if (ret & VM_FAULT_ERROR)
			break;
		pfn += priv->usergart[fmt].stride_pfn;
		vaddr += PAGE_SIZE * m;
	}

	 
	priv->usergart[fmt].last = (priv->usergart[fmt].last + 1)
				 % NUM_USERGART_ENTRIES;

	return ret;
}

 
static vm_fault_t omap_gem_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct drm_gem_object *obj = vma->vm_private_data;
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	int err;
	vm_fault_t ret;

	 
	mutex_lock(&omap_obj->lock);

	 
	err = omap_gem_attach_pages(obj);
	if (err) {
		ret = vmf_error(err);
		goto fail;
	}

	 

	if (omap_obj->flags & OMAP_BO_TILED_MASK)
		ret = omap_gem_fault_2d(obj, vma, vmf);
	else
		ret = omap_gem_fault_1d(obj, vma, vmf);


fail:
	mutex_unlock(&omap_obj->lock);
	return ret;
}

static int omap_gem_object_mmap(struct drm_gem_object *obj, struct vm_area_struct *vma)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);

	vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP | VM_IO | VM_MIXEDMAP);

	if (omap_obj->flags & OMAP_BO_WC) {
		vma->vm_page_prot = pgprot_writecombine(vm_get_page_prot(vma->vm_flags));
	} else if (omap_obj->flags & OMAP_BO_UNCACHED) {
		vma->vm_page_prot = pgprot_noncached(vm_get_page_prot(vma->vm_flags));
	} else {
		 
		if (WARN_ON(!obj->filp))
			return -EINVAL;

		 
		vma->vm_pgoff -= drm_vma_node_start(&obj->vma_node);
		vma_set_file(vma, obj->filp);

		vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
	}

	vma->vm_page_prot = pgprot_decrypted(vma->vm_page_prot);

	return 0;
}

 

 
int omap_gem_dumb_create(struct drm_file *file, struct drm_device *dev,
		struct drm_mode_create_dumb *args)
{
	union omap_gem_size gsize;

	args->pitch = DIV_ROUND_UP(args->width * args->bpp, 8);

	args->size = PAGE_ALIGN(args->pitch * args->height);

	gsize = (union omap_gem_size){
		.bytes = args->size,
	};

	return omap_gem_new_handle(dev, file, gsize,
			OMAP_BO_SCANOUT | OMAP_BO_WC, &args->handle);
}

 
int omap_gem_dumb_map_offset(struct drm_file *file, struct drm_device *dev,
		u32 handle, u64 *offset)
{
	struct drm_gem_object *obj;
	int ret = 0;

	 
	obj = drm_gem_object_lookup(file, handle);
	if (obj == NULL) {
		ret = -ENOENT;
		goto fail;
	}

	*offset = omap_gem_mmap_offset(obj);

	drm_gem_object_put(obj);

fail:
	return ret;
}

#ifdef CONFIG_DRM_FBDEV_EMULATION
 
int omap_gem_roll(struct drm_gem_object *obj, u32 roll)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	u32 npages = obj->size >> PAGE_SHIFT;
	int ret = 0;

	if (roll > npages) {
		dev_err(obj->dev->dev, "invalid roll: %d\n", roll);
		return -EINVAL;
	}

	omap_obj->roll = roll;

	mutex_lock(&omap_obj->lock);

	 
	if (omap_obj->block) {
		ret = omap_gem_attach_pages(obj);
		if (ret)
			goto fail;

		ret = tiler_pin(omap_obj->block, omap_obj->pages, npages,
				roll, true);
		if (ret)
			dev_err(obj->dev->dev, "could not repin: %d\n", ret);
	}

fail:
	mutex_unlock(&omap_obj->lock);

	return ret;
}
#endif

 

 
static inline bool omap_gem_is_cached_coherent(struct drm_gem_object *obj)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);

	return !((omap_obj->flags & OMAP_BO_MEM_SHMEM) &&
		((omap_obj->flags & OMAP_BO_CACHE_MASK) == OMAP_BO_CACHED));
}

 
void omap_gem_cpu_sync_page(struct drm_gem_object *obj, int pgoff)
{
	struct drm_device *dev = obj->dev;
	struct omap_gem_object *omap_obj = to_omap_bo(obj);

	if (omap_gem_is_cached_coherent(obj))
		return;

	if (omap_obj->dma_addrs[pgoff]) {
		dma_unmap_page(dev->dev, omap_obj->dma_addrs[pgoff],
				PAGE_SIZE, DMA_TO_DEVICE);
		omap_obj->dma_addrs[pgoff] = 0;
	}
}

 
void omap_gem_dma_sync_buffer(struct drm_gem_object *obj,
		enum dma_data_direction dir)
{
	struct drm_device *dev = obj->dev;
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	int i, npages = obj->size >> PAGE_SHIFT;
	struct page **pages = omap_obj->pages;
	bool dirty = false;

	if (omap_gem_is_cached_coherent(obj))
		return;

	for (i = 0; i < npages; i++) {
		if (!omap_obj->dma_addrs[i]) {
			dma_addr_t addr;

			addr = dma_map_page(dev->dev, pages[i], 0,
					    PAGE_SIZE, dir);
			if (dma_mapping_error(dev->dev, addr)) {
				dev_warn(dev->dev, "%s: failed to map page\n",
					__func__);
				break;
			}

			dirty = true;
			omap_obj->dma_addrs[i] = addr;
		}
	}

	if (dirty) {
		unmap_mapping_range(obj->filp->f_mapping, 0,
				    omap_gem_mmap_size(obj), 1);
	}
}

static int omap_gem_pin_tiler(struct drm_gem_object *obj)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	u32 npages = obj->size >> PAGE_SHIFT;
	enum tiler_fmt fmt = gem2fmt(omap_obj->flags);
	struct tiler_block *block;
	int ret;

	BUG_ON(omap_obj->block);

	if (omap_obj->flags & OMAP_BO_TILED_MASK) {
		block = tiler_reserve_2d(fmt, omap_obj->width, omap_obj->height,
					 PAGE_SIZE);
	} else {
		block = tiler_reserve_1d(obj->size);
	}

	if (IS_ERR(block)) {
		ret = PTR_ERR(block);
		dev_err(obj->dev->dev, "could not remap: %d (%d)\n", ret, fmt);
		goto fail;
	}

	 
	ret = tiler_pin(block, omap_obj->pages, npages, omap_obj->roll, true);
	if (ret) {
		tiler_release(block);
		dev_err(obj->dev->dev, "could not pin: %d\n", ret);
		goto fail;
	}

	omap_obj->dma_addr = tiler_ssptr(block);
	omap_obj->block = block;

	DBG("got dma address: %pad", &omap_obj->dma_addr);

fail:
	return ret;
}

 
int omap_gem_pin(struct drm_gem_object *obj, dma_addr_t *dma_addr)
{
	struct omap_drm_private *priv = obj->dev->dev_private;
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	int ret = 0;

	mutex_lock(&omap_obj->lock);

	if (!omap_gem_is_contiguous(omap_obj)) {
		if (refcount_read(&omap_obj->pin_cnt) == 0) {

			refcount_set(&omap_obj->pin_cnt, 1);

			ret = omap_gem_attach_pages(obj);
			if (ret)
				goto fail;

			if (omap_obj->flags & OMAP_BO_SCANOUT) {
				if (priv->has_dmm) {
					ret = omap_gem_pin_tiler(obj);
					if (ret)
						goto fail;
				}
			}
		} else {
			refcount_inc(&omap_obj->pin_cnt);
		}
	}

	if (dma_addr)
		*dma_addr = omap_obj->dma_addr;

fail:
	mutex_unlock(&omap_obj->lock);

	return ret;
}

 
static void omap_gem_unpin_locked(struct drm_gem_object *obj)
{
	struct omap_drm_private *priv = obj->dev->dev_private;
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	int ret;

	if (omap_gem_is_contiguous(omap_obj))
		return;

	if (refcount_dec_and_test(&omap_obj->pin_cnt)) {
		if (omap_obj->sgt) {
			sg_free_table(omap_obj->sgt);
			kfree(omap_obj->sgt);
			omap_obj->sgt = NULL;
		}
		if (!(omap_obj->flags & OMAP_BO_SCANOUT))
			return;
		if (priv->has_dmm) {
			ret = tiler_unpin(omap_obj->block);
			if (ret) {
				dev_err(obj->dev->dev,
					"could not unpin pages: %d\n", ret);
			}
			ret = tiler_release(omap_obj->block);
			if (ret) {
				dev_err(obj->dev->dev,
					"could not release unmap: %d\n", ret);
			}
			omap_obj->dma_addr = 0;
			omap_obj->block = NULL;
		}
	}
}

 
void omap_gem_unpin(struct drm_gem_object *obj)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);

	mutex_lock(&omap_obj->lock);
	omap_gem_unpin_locked(obj);
	mutex_unlock(&omap_obj->lock);
}

 
int omap_gem_rotated_dma_addr(struct drm_gem_object *obj, u32 orient,
		int x, int y, dma_addr_t *dma_addr)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	int ret = -EINVAL;

	mutex_lock(&omap_obj->lock);

	if ((refcount_read(&omap_obj->pin_cnt) > 0) && omap_obj->block &&
			(omap_obj->flags & OMAP_BO_TILED_MASK)) {
		*dma_addr = tiler_tsptr(omap_obj->block, orient, x, y);
		ret = 0;
	}

	mutex_unlock(&omap_obj->lock);

	return ret;
}

 
int omap_gem_tiled_stride(struct drm_gem_object *obj, u32 orient)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	int ret = -EINVAL;
	if (omap_obj->flags & OMAP_BO_TILED_MASK)
		ret = tiler_stride(gem2fmt(omap_obj->flags), orient);
	return ret;
}

 
int omap_gem_get_pages(struct drm_gem_object *obj, struct page ***pages,
		bool remap)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	int ret = 0;

	mutex_lock(&omap_obj->lock);

	if (remap) {
		ret = omap_gem_attach_pages(obj);
		if (ret)
			goto unlock;
	}

	if (!omap_obj->pages) {
		ret = -ENOMEM;
		goto unlock;
	}

	*pages = omap_obj->pages;

unlock:
	mutex_unlock(&omap_obj->lock);

	return ret;
}

 
int omap_gem_put_pages(struct drm_gem_object *obj)
{
	 
	return 0;
}

struct sg_table *omap_gem_get_sg(struct drm_gem_object *obj,
		enum dma_data_direction dir)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	dma_addr_t addr;
	struct sg_table *sgt;
	struct scatterlist *sg;
	unsigned int count, len, stride, i;
	int ret;

	ret = omap_gem_pin(obj, &addr);
	if (ret)
		return ERR_PTR(ret);

	mutex_lock(&omap_obj->lock);

	sgt = omap_obj->sgt;
	if (sgt)
		goto out;

	sgt = kzalloc(sizeof(*sgt), GFP_KERNEL);
	if (!sgt) {
		ret = -ENOMEM;
		goto err_unpin;
	}

	if (addr) {
		if (omap_obj->flags & OMAP_BO_TILED_MASK) {
			enum tiler_fmt fmt = gem2fmt(omap_obj->flags);

			len = omap_obj->width << (int)fmt;
			count = omap_obj->height;
			stride = tiler_stride(fmt, 0);
		} else {
			len = obj->size;
			count = 1;
			stride = 0;
		}
	} else {
		count = obj->size >> PAGE_SHIFT;
	}

	ret = sg_alloc_table(sgt, count, GFP_KERNEL);
	if (ret)
		goto err_free;

	 
	omap_gem_dma_sync_buffer(obj, dir);

	if (addr) {
		for_each_sg(sgt->sgl, sg, count, i) {
			sg_set_page(sg, phys_to_page(addr), len,
				offset_in_page(addr));
			sg_dma_address(sg) = addr;
			sg_dma_len(sg) = len;

			addr += stride;
		}
	} else {
		for_each_sg(sgt->sgl, sg, count, i) {
			sg_set_page(sg, omap_obj->pages[i], PAGE_SIZE, 0);
			sg_dma_address(sg) = omap_obj->dma_addrs[i];
			sg_dma_len(sg) =  PAGE_SIZE;
		}
	}

	omap_obj->sgt = sgt;
out:
	mutex_unlock(&omap_obj->lock);
	return sgt;

err_free:
	kfree(sgt);
err_unpin:
	mutex_unlock(&omap_obj->lock);
	omap_gem_unpin(obj);
	return ERR_PTR(ret);
}

void omap_gem_put_sg(struct drm_gem_object *obj, struct sg_table *sgt)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);

	if (WARN_ON(omap_obj->sgt != sgt))
		return;

	omap_gem_unpin(obj);
}

#ifdef CONFIG_DRM_FBDEV_EMULATION
 
void *omap_gem_vaddr(struct drm_gem_object *obj)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	void *vaddr;
	int ret;

	mutex_lock(&omap_obj->lock);

	if (!omap_obj->vaddr) {
		ret = omap_gem_attach_pages(obj);
		if (ret) {
			vaddr = ERR_PTR(ret);
			goto unlock;
		}

		omap_obj->vaddr = vmap(omap_obj->pages, obj->size >> PAGE_SHIFT,
				VM_MAP, pgprot_writecombine(PAGE_KERNEL));
	}

	vaddr = omap_obj->vaddr;

unlock:
	mutex_unlock(&omap_obj->lock);
	return vaddr;
}
#endif

 

#ifdef CONFIG_PM
 
int omap_gem_resume(struct drm_device *dev)
{
	struct omap_drm_private *priv = dev->dev_private;
	struct omap_gem_object *omap_obj;
	int ret = 0;

	mutex_lock(&priv->list_lock);
	list_for_each_entry(omap_obj, &priv->obj_list, mm_list) {
		if (omap_obj->block) {
			struct drm_gem_object *obj = &omap_obj->base;
			u32 npages = obj->size >> PAGE_SHIFT;

			WARN_ON(!omap_obj->pages);   
			ret = tiler_pin(omap_obj->block,
					omap_obj->pages, npages,
					omap_obj->roll, true);
			if (ret) {
				dev_err(dev->dev, "could not repin: %d\n", ret);
				goto done;
			}
		}
	}

done:
	mutex_unlock(&priv->list_lock);
	return ret;
}
#endif

 

#ifdef CONFIG_DEBUG_FS
void omap_gem_describe(struct drm_gem_object *obj, struct seq_file *m)
{
	struct omap_gem_object *omap_obj = to_omap_bo(obj);
	u64 off;

	off = drm_vma_node_start(&obj->vma_node);

	mutex_lock(&omap_obj->lock);

	seq_printf(m, "%08x: %2d (%2d) %08llx %pad (%2d) %p %4d",
			omap_obj->flags, obj->name, kref_read(&obj->refcount),
			off, &omap_obj->dma_addr,
			refcount_read(&omap_obj->pin_cnt),
			omap_obj->vaddr, omap_obj->roll);

	if (omap_obj->flags & OMAP_BO_TILED_MASK) {
		seq_printf(m, " %dx%d", omap_obj->width, omap_obj->height);
		if (omap_obj->block) {
			struct tcm_area *area = &omap_obj->block->area;
			seq_printf(m, " (%dx%d, %dx%d)",
					area->p0.x, area->p0.y,
					area->p1.x, area->p1.y);
		}
	} else {
		seq_printf(m, " %zu", obj->size);
	}

	mutex_unlock(&omap_obj->lock);

	seq_printf(m, "\n");
}

void omap_gem_describe_objects(struct list_head *list, struct seq_file *m)
{
	struct omap_gem_object *omap_obj;
	int count = 0;
	size_t size = 0;

	list_for_each_entry(omap_obj, list, mm_list) {
		struct drm_gem_object *obj = &omap_obj->base;
		seq_printf(m, "   ");
		omap_gem_describe(obj, m);
		count++;
		size += obj->size;
	}

	seq_printf(m, "Total %d objects, %zu bytes\n", count, size);
}
#endif

 

static void omap_gem_free_object(struct drm_gem_object *obj)
{
	struct drm_device *dev = obj->dev;
	struct omap_drm_private *priv = dev->dev_private;
	struct omap_gem_object *omap_obj = to_omap_bo(obj);

	omap_gem_evict(obj);

	mutex_lock(&priv->list_lock);
	list_del(&omap_obj->mm_list);
	mutex_unlock(&priv->list_lock);

	 
	mutex_lock(&omap_obj->lock);

	 
	WARN_ON(refcount_read(&omap_obj->pin_cnt) > 0);

	if (omap_obj->pages) {
		if (omap_obj->flags & OMAP_BO_MEM_DMABUF)
			kfree(omap_obj->pages);
		else
			omap_gem_detach_pages(obj);
	}

	if (omap_obj->flags & OMAP_BO_MEM_DMA_API) {
		dma_free_wc(dev->dev, obj->size, omap_obj->vaddr,
			    omap_obj->dma_addr);
	} else if (omap_obj->vaddr) {
		vunmap(omap_obj->vaddr);
	} else if (obj->import_attach) {
		drm_prime_gem_destroy(obj, omap_obj->sgt);
	}

	mutex_unlock(&omap_obj->lock);

	drm_gem_object_release(obj);

	mutex_destroy(&omap_obj->lock);

	kfree(omap_obj);
}

static bool omap_gem_validate_flags(struct drm_device *dev, u32 flags)
{
	struct omap_drm_private *priv = dev->dev_private;

	switch (flags & OMAP_BO_CACHE_MASK) {
	case OMAP_BO_CACHED:
	case OMAP_BO_WC:
	case OMAP_BO_CACHE_MASK:
		break;

	default:
		return false;
	}

	if (flags & OMAP_BO_TILED_MASK) {
		if (!priv->usergart)
			return false;

		switch (flags & OMAP_BO_TILED_MASK) {
		case OMAP_BO_TILED_8:
		case OMAP_BO_TILED_16:
		case OMAP_BO_TILED_32:
			break;

		default:
			return false;
		}
	}

	return true;
}

static const struct vm_operations_struct omap_gem_vm_ops = {
	.fault = omap_gem_fault,
	.open = drm_gem_vm_open,
	.close = drm_gem_vm_close,
};

static const struct drm_gem_object_funcs omap_gem_object_funcs = {
	.free = omap_gem_free_object,
	.export = omap_gem_prime_export,
	.mmap = omap_gem_object_mmap,
	.vm_ops = &omap_gem_vm_ops,
};

 
struct drm_gem_object *omap_gem_new(struct drm_device *dev,
		union omap_gem_size gsize, u32 flags)
{
	struct omap_drm_private *priv = dev->dev_private;
	struct omap_gem_object *omap_obj;
	struct drm_gem_object *obj;
	struct address_space *mapping;
	size_t size;
	int ret;

	if (!omap_gem_validate_flags(dev, flags))
		return NULL;

	 
	if (flags & OMAP_BO_TILED_MASK) {
		 
		flags |= OMAP_BO_MEM_SHMEM;

		 
		flags &= ~(OMAP_BO_CACHED|OMAP_BO_WC|OMAP_BO_UNCACHED);
		flags |= tiler_get_cpu_cache_flags();
	} else if ((flags & OMAP_BO_SCANOUT) && !priv->has_dmm) {
		 
		flags |= OMAP_BO_MEM_DMA_API;
	} else if (!(flags & OMAP_BO_MEM_DMABUF)) {
		 
		flags |= OMAP_BO_MEM_SHMEM;
	}

	 
	omap_obj = kzalloc(sizeof(*omap_obj), GFP_KERNEL);
	if (!omap_obj)
		return NULL;

	obj = &omap_obj->base;
	omap_obj->flags = flags;
	mutex_init(&omap_obj->lock);

	if (flags & OMAP_BO_TILED_MASK) {
		 
		tiler_align(gem2fmt(flags), &gsize.tiled.width,
			    &gsize.tiled.height);

		size = tiler_size(gem2fmt(flags), gsize.tiled.width,
				  gsize.tiled.height);

		omap_obj->width = gsize.tiled.width;
		omap_obj->height = gsize.tiled.height;
	} else {
		size = PAGE_ALIGN(gsize.bytes);
	}

	obj->funcs = &omap_gem_object_funcs;

	 
	if (!(flags & OMAP_BO_MEM_SHMEM)) {
		drm_gem_private_object_init(dev, obj, size);
	} else {
		ret = drm_gem_object_init(dev, obj, size);
		if (ret)
			goto err_free;

		mapping = obj->filp->f_mapping;
		mapping_set_gfp_mask(mapping, GFP_USER | __GFP_DMA32);
	}

	 
	if (flags & OMAP_BO_MEM_DMA_API) {
		omap_obj->vaddr = dma_alloc_wc(dev->dev, size,
					       &omap_obj->dma_addr,
					       GFP_KERNEL);
		if (!omap_obj->vaddr)
			goto err_release;
	}

	mutex_lock(&priv->list_lock);
	list_add(&omap_obj->mm_list, &priv->obj_list);
	mutex_unlock(&priv->list_lock);

	return obj;

err_release:
	drm_gem_object_release(obj);
err_free:
	kfree(omap_obj);
	return NULL;
}

struct drm_gem_object *omap_gem_new_dmabuf(struct drm_device *dev, size_t size,
					   struct sg_table *sgt)
{
	struct omap_drm_private *priv = dev->dev_private;
	struct omap_gem_object *omap_obj;
	struct drm_gem_object *obj;
	union omap_gem_size gsize;

	 
	if (sgt->orig_nents != 1 && !priv->has_dmm)
		return ERR_PTR(-EINVAL);

	gsize.bytes = PAGE_ALIGN(size);
	obj = omap_gem_new(dev, gsize, OMAP_BO_MEM_DMABUF | OMAP_BO_WC);
	if (!obj)
		return ERR_PTR(-ENOMEM);

	omap_obj = to_omap_bo(obj);

	mutex_lock(&omap_obj->lock);

	omap_obj->sgt = sgt;

	if (sgt->orig_nents == 1) {
		omap_obj->dma_addr = sg_dma_address(sgt->sgl);
	} else {
		 
		struct page **pages;
		unsigned int npages;
		unsigned int ret;

		npages = DIV_ROUND_UP(size, PAGE_SIZE);
		pages = kcalloc(npages, sizeof(*pages), GFP_KERNEL);
		if (!pages) {
			omap_gem_free_object(obj);
			obj = ERR_PTR(-ENOMEM);
			goto done;
		}

		omap_obj->pages = pages;
		ret = drm_prime_sg_to_page_array(sgt, pages, npages);
		if (ret) {
			omap_gem_free_object(obj);
			obj = ERR_PTR(-ENOMEM);
			goto done;
		}
	}

done:
	mutex_unlock(&omap_obj->lock);
	return obj;
}

 
int omap_gem_new_handle(struct drm_device *dev, struct drm_file *file,
		union omap_gem_size gsize, u32 flags, u32 *handle)
{
	struct drm_gem_object *obj;
	int ret;

	obj = omap_gem_new(dev, gsize, flags);
	if (!obj)
		return -ENOMEM;

	ret = drm_gem_handle_create(file, obj, handle);
	if (ret) {
		omap_gem_free_object(obj);
		return ret;
	}

	 
	drm_gem_object_put(obj);

	return 0;
}

 

 
void omap_gem_init(struct drm_device *dev)
{
	struct omap_drm_private *priv = dev->dev_private;
	struct omap_drm_usergart *usergart;
	const enum tiler_fmt fmts[] = {
			TILFMT_8BIT, TILFMT_16BIT, TILFMT_32BIT
	};
	int i, j;

	if (!dmm_is_available()) {
		 
		dev_warn(dev->dev, "DMM not available, disable DMM support\n");
		return;
	}

	usergart = kcalloc(3, sizeof(*usergart), GFP_KERNEL);
	if (!usergart)
		return;

	 
	for (i = 0; i < ARRAY_SIZE(fmts); i++) {
		u16 h = 1, w = PAGE_SIZE >> i;

		tiler_align(fmts[i], &w, &h);
		 
		usergart[i].height = h;
		usergart[i].height_shift = ilog2(h);
		usergart[i].stride_pfn = tiler_stride(fmts[i], 0) >> PAGE_SHIFT;
		usergart[i].slot_shift = ilog2((PAGE_SIZE / h) >> i);
		for (j = 0; j < NUM_USERGART_ENTRIES; j++) {
			struct omap_drm_usergart_entry *entry;
			struct tiler_block *block;

			entry = &usergart[i].entry[j];
			block = tiler_reserve_2d(fmts[i], w, h, PAGE_SIZE);
			if (IS_ERR(block)) {
				dev_err(dev->dev,
						"reserve failed: %d, %d, %ld\n",
						i, j, PTR_ERR(block));
				return;
			}
			entry->dma_addr = tiler_ssptr(block);
			entry->block = block;

			DBG("%d:%d: %dx%d: dma_addr=%pad stride=%d", i, j, w, h,
					&entry->dma_addr,
					usergart[i].stride_pfn << PAGE_SHIFT);
		}
	}

	priv->usergart = usergart;
	priv->has_dmm = true;
}

void omap_gem_deinit(struct drm_device *dev)
{
	struct omap_drm_private *priv = dev->dev_private;

	 
	kfree(priv->usergart);
}
