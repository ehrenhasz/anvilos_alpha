 

#include <linux/anon_inodes.h>
#include <linux/mman.h>
#include <linux/pfn_t.h>
#include <linux/sizes.h>

#include <drm/drm_cache.h>

#include "gt/intel_gt.h"
#include "gt/intel_gt_requests.h"

#include "i915_drv.h"
#include "i915_gem_evict.h"
#include "i915_gem_gtt.h"
#include "i915_gem_ioctls.h"
#include "i915_gem_object.h"
#include "i915_gem_mman.h"
#include "i915_mm.h"
#include "i915_trace.h"
#include "i915_user_extensions.h"
#include "i915_gem_ttm.h"
#include "i915_vma.h"

static inline bool
__vma_matches(struct vm_area_struct *vma, struct file *filp,
	      unsigned long addr, unsigned long size)
{
	if (vma->vm_file != filp)
		return false;

	return vma->vm_start == addr &&
	       (vma->vm_end - vma->vm_start) == PAGE_ALIGN(size);
}

 
int
i915_gem_mmap_ioctl(struct drm_device *dev, void *data,
		    struct drm_file *file)
{
	struct drm_i915_private *i915 = to_i915(dev);
	struct drm_i915_gem_mmap *args = data;
	struct drm_i915_gem_object *obj;
	unsigned long addr;

	 
	if (IS_DGFX(i915) || GRAPHICS_VER_FULL(i915) > IP_VER(12, 0))
		return -EOPNOTSUPP;

	if (args->flags & ~(I915_MMAP_WC))
		return -EINVAL;

	if (args->flags & I915_MMAP_WC && !pat_enabled())
		return -ENODEV;

	obj = i915_gem_object_lookup(file, args->handle);
	if (!obj)
		return -ENOENT;

	 
	if (!obj->base.filp) {
		addr = -ENXIO;
		goto err;
	}

	if (range_overflows(args->offset, args->size, (u64)obj->base.size)) {
		addr = -EINVAL;
		goto err;
	}

	addr = vm_mmap(obj->base.filp, 0, args->size,
		       PROT_READ | PROT_WRITE, MAP_SHARED,
		       args->offset);
	if (IS_ERR_VALUE(addr))
		goto err;

	if (args->flags & I915_MMAP_WC) {
		struct mm_struct *mm = current->mm;
		struct vm_area_struct *vma;

		if (mmap_write_lock_killable(mm)) {
			addr = -EINTR;
			goto err;
		}
		vma = find_vma(mm, addr);
		if (vma && __vma_matches(vma, obj->base.filp, addr, args->size))
			vma->vm_page_prot =
				pgprot_writecombine(vm_get_page_prot(vma->vm_flags));
		else
			addr = -ENOMEM;
		mmap_write_unlock(mm);
		if (IS_ERR_VALUE(addr))
			goto err;
	}
	i915_gem_object_put(obj);

	args->addr_ptr = (u64)addr;
	return 0;

err:
	i915_gem_object_put(obj);
	return addr;
}

static unsigned int tile_row_pages(const struct drm_i915_gem_object *obj)
{
	return i915_gem_object_get_tile_row_size(obj) >> PAGE_SHIFT;
}

 
int i915_gem_mmap_gtt_version(void)
{
	return 4;
}

static inline struct i915_gtt_view
compute_partial_view(const struct drm_i915_gem_object *obj,
		     pgoff_t page_offset,
		     unsigned int chunk)
{
	struct i915_gtt_view view;

	if (i915_gem_object_is_tiled(obj))
		chunk = roundup(chunk, tile_row_pages(obj) ?: 1);

	view.type = I915_GTT_VIEW_PARTIAL;
	view.partial.offset = rounddown(page_offset, chunk);
	view.partial.size =
		min_t(unsigned int, chunk,
		      (obj->base.size >> PAGE_SHIFT) - view.partial.offset);

	 
	if (chunk >= obj->base.size >> PAGE_SHIFT)
		view.type = I915_GTT_VIEW_NORMAL;

	return view;
}

static vm_fault_t i915_error_to_vmf_fault(int err)
{
	switch (err) {
	default:
		WARN_ONCE(err, "unhandled error in %s: %i\n", __func__, err);
		fallthrough;
	case -EIO:  
	case -EFAULT:  
	case -ENODEV:  
	case -ENXIO:  
		return VM_FAULT_SIGBUS;

	case -ENOMEM:  
		return VM_FAULT_OOM;

	case 0:
	case -EAGAIN:
	case -ENOSPC:  
	case -ENOBUFS:  
	case -ERESTARTSYS:
	case -EINTR:
	case -EBUSY:
		 
		return VM_FAULT_NOPAGE;
	}
}

static vm_fault_t vm_fault_cpu(struct vm_fault *vmf)
{
	struct vm_area_struct *area = vmf->vma;
	struct i915_mmap_offset *mmo = area->vm_private_data;
	struct drm_i915_gem_object *obj = mmo->obj;
	resource_size_t iomap;
	int err;

	 
	if (unlikely(i915_gem_object_is_readonly(obj) &&
		     area->vm_flags & VM_WRITE))
		return VM_FAULT_SIGBUS;

	if (i915_gem_object_lock_interruptible(obj, NULL))
		return VM_FAULT_NOPAGE;

	err = i915_gem_object_pin_pages(obj);
	if (err)
		goto out;

	iomap = -1;
	if (!i915_gem_object_has_struct_page(obj)) {
		iomap = obj->mm.region->iomap.base;
		iomap -= obj->mm.region->region.start;
	}

	 
	err = remap_io_sg(area,
			  area->vm_start, area->vm_end - area->vm_start,
			  obj->mm.pages->sgl, iomap);

	if (area->vm_flags & VM_WRITE) {
		GEM_BUG_ON(!i915_gem_object_has_pinned_pages(obj));
		obj->mm.dirty = true;
	}

	i915_gem_object_unpin_pages(obj);

out:
	i915_gem_object_unlock(obj);
	return i915_error_to_vmf_fault(err);
}

static vm_fault_t vm_fault_gtt(struct vm_fault *vmf)
{
#define MIN_CHUNK_PAGES (SZ_1M >> PAGE_SHIFT)
	struct vm_area_struct *area = vmf->vma;
	struct i915_mmap_offset *mmo = area->vm_private_data;
	struct drm_i915_gem_object *obj = mmo->obj;
	struct drm_device *dev = obj->base.dev;
	struct drm_i915_private *i915 = to_i915(dev);
	struct intel_runtime_pm *rpm = &i915->runtime_pm;
	struct i915_ggtt *ggtt = to_gt(i915)->ggtt;
	bool write = area->vm_flags & VM_WRITE;
	struct i915_gem_ww_ctx ww;
	intel_wakeref_t wakeref;
	struct i915_vma *vma;
	pgoff_t page_offset;
	int srcu;
	int ret;

	 
	page_offset = (vmf->address - area->vm_start) >> PAGE_SHIFT;

	trace_i915_gem_object_fault(obj, page_offset, true, write);

	wakeref = intel_runtime_pm_get(rpm);

	i915_gem_ww_ctx_init(&ww, true);
retry:
	ret = i915_gem_object_lock(obj, &ww);
	if (ret)
		goto err_rpm;

	 
	if (i915_gem_object_is_readonly(obj) && write) {
		ret = -EFAULT;
		goto err_rpm;
	}

	ret = i915_gem_object_pin_pages(obj);
	if (ret)
		goto err_rpm;

	ret = intel_gt_reset_lock_interruptible(ggtt->vm.gt, &srcu);
	if (ret)
		goto err_pages;

	 
	vma = i915_gem_object_ggtt_pin_ww(obj, &ww, NULL, 0, 0,
					  PIN_MAPPABLE |
					  PIN_NONBLOCK   |
					  PIN_NOEVICT);
	if (IS_ERR(vma) && vma != ERR_PTR(-EDEADLK)) {
		 
		struct i915_gtt_view view =
			compute_partial_view(obj, page_offset, MIN_CHUNK_PAGES);
		unsigned int flags;

		flags = PIN_MAPPABLE | PIN_NOSEARCH;
		if (view.type == I915_GTT_VIEW_NORMAL)
			flags |= PIN_NONBLOCK;  

		 

		vma = i915_gem_object_ggtt_pin_ww(obj, &ww, &view, 0, 0, flags);
		if (IS_ERR(vma) && vma != ERR_PTR(-EDEADLK)) {
			flags = PIN_MAPPABLE;
			view.type = I915_GTT_VIEW_PARTIAL;
			vma = i915_gem_object_ggtt_pin_ww(obj, &ww, &view, 0, 0, flags);
		}

		 
		if (vma == ERR_PTR(-ENOSPC)) {
			ret = mutex_lock_interruptible(&ggtt->vm.mutex);
			if (!ret) {
				ret = i915_gem_evict_vm(&ggtt->vm, &ww, NULL);
				mutex_unlock(&ggtt->vm.mutex);
			}
			if (ret)
				goto err_reset;
			vma = i915_gem_object_ggtt_pin_ww(obj, &ww, &view, 0, 0, flags);
		}
	}
	if (IS_ERR(vma)) {
		ret = PTR_ERR(vma);
		goto err_reset;
	}

	 
	 
	if (!(i915_gem_object_has_cache_level(obj, I915_CACHE_NONE) ||
	      HAS_LLC(i915))) {
		ret = -EFAULT;
		goto err_unpin;
	}

	ret = i915_vma_pin_fence(vma);
	if (ret)
		goto err_unpin;

	 
	ret = remap_io_mapping(area,
			       area->vm_start + (vma->gtt_view.partial.offset << PAGE_SHIFT),
			       (ggtt->gmadr.start + i915_ggtt_offset(vma)) >> PAGE_SHIFT,
			       min_t(u64, vma->size, area->vm_end - area->vm_start),
			       &ggtt->iomap);
	if (ret)
		goto err_fence;

	assert_rpm_wakelock_held(rpm);

	 
	mutex_lock(&to_gt(i915)->ggtt->vm.mutex);
	if (!i915_vma_set_userfault(vma) && !obj->userfault_count++)
		list_add(&obj->userfault_link, &to_gt(i915)->ggtt->userfault_list);
	mutex_unlock(&to_gt(i915)->ggtt->vm.mutex);

	 
	vma->mmo = mmo;

	if (CONFIG_DRM_I915_USERFAULT_AUTOSUSPEND)
		intel_wakeref_auto(&i915->runtime_pm.userfault_wakeref,
				   msecs_to_jiffies_timeout(CONFIG_DRM_I915_USERFAULT_AUTOSUSPEND));

	if (write) {
		GEM_BUG_ON(!i915_gem_object_has_pinned_pages(obj));
		i915_vma_set_ggtt_write(vma);
		obj->mm.dirty = true;
	}

err_fence:
	i915_vma_unpin_fence(vma);
err_unpin:
	__i915_vma_unpin(vma);
err_reset:
	intel_gt_reset_unlock(ggtt->vm.gt, srcu);
err_pages:
	i915_gem_object_unpin_pages(obj);
err_rpm:
	if (ret == -EDEADLK) {
		ret = i915_gem_ww_ctx_backoff(&ww);
		if (!ret)
			goto retry;
	}
	i915_gem_ww_ctx_fini(&ww);
	intel_runtime_pm_put(rpm, wakeref);
	return i915_error_to_vmf_fault(ret);
}

static int
vm_access(struct vm_area_struct *area, unsigned long addr,
	  void *buf, int len, int write)
{
	struct i915_mmap_offset *mmo = area->vm_private_data;
	struct drm_i915_gem_object *obj = mmo->obj;
	struct i915_gem_ww_ctx ww;
	void *vaddr;
	int err = 0;

	if (i915_gem_object_is_readonly(obj) && write)
		return -EACCES;

	addr -= area->vm_start;
	if (range_overflows_t(u64, addr, len, obj->base.size))
		return -EINVAL;

	i915_gem_ww_ctx_init(&ww, true);
retry:
	err = i915_gem_object_lock(obj, &ww);
	if (err)
		goto out;

	 
	vaddr = i915_gem_object_pin_map(obj, I915_MAP_FORCE_WC);
	if (IS_ERR(vaddr)) {
		err = PTR_ERR(vaddr);
		goto out;
	}

	if (write) {
		memcpy(vaddr + addr, buf, len);
		__i915_gem_object_flush_map(obj, addr, len);
	} else {
		memcpy(buf, vaddr + addr, len);
	}

	i915_gem_object_unpin_map(obj);
out:
	if (err == -EDEADLK) {
		err = i915_gem_ww_ctx_backoff(&ww);
		if (!err)
			goto retry;
	}
	i915_gem_ww_ctx_fini(&ww);

	if (err)
		return err;

	return len;
}

void __i915_gem_object_release_mmap_gtt(struct drm_i915_gem_object *obj)
{
	struct i915_vma *vma;

	GEM_BUG_ON(!obj->userfault_count);

	for_each_ggtt_vma(vma, obj)
		i915_vma_revoke_mmap(vma);

	GEM_BUG_ON(obj->userfault_count);
}

 
void i915_gem_object_release_mmap_gtt(struct drm_i915_gem_object *obj)
{
	struct drm_i915_private *i915 = to_i915(obj->base.dev);
	intel_wakeref_t wakeref;

	 
	wakeref = intel_runtime_pm_get(&i915->runtime_pm);
	mutex_lock(&to_gt(i915)->ggtt->vm.mutex);

	if (!obj->userfault_count)
		goto out;

	__i915_gem_object_release_mmap_gtt(obj);

	 
	wmb();

out:
	mutex_unlock(&to_gt(i915)->ggtt->vm.mutex);
	intel_runtime_pm_put(&i915->runtime_pm, wakeref);
}

void i915_gem_object_runtime_pm_release_mmap_offset(struct drm_i915_gem_object *obj)
{
	struct ttm_buffer_object *bo = i915_gem_to_ttm(obj);
	struct ttm_device *bdev = bo->bdev;

	drm_vma_node_unmap(&bo->base.vma_node, bdev->dev_mapping);

	 
	GEM_BUG_ON(!obj->userfault_count);
	list_del(&obj->userfault_link);
	obj->userfault_count = 0;
}

void i915_gem_object_release_mmap_offset(struct drm_i915_gem_object *obj)
{
	struct i915_mmap_offset *mmo, *mn;

	if (obj->ops->unmap_virtual)
		obj->ops->unmap_virtual(obj);

	spin_lock(&obj->mmo.lock);
	rbtree_postorder_for_each_entry_safe(mmo, mn,
					     &obj->mmo.offsets, offset) {
		 
		if (mmo->mmap_type == I915_MMAP_TYPE_GTT)
			continue;

		spin_unlock(&obj->mmo.lock);
		drm_vma_node_unmap(&mmo->vma_node,
				   obj->base.dev->anon_inode->i_mapping);
		spin_lock(&obj->mmo.lock);
	}
	spin_unlock(&obj->mmo.lock);
}

static struct i915_mmap_offset *
lookup_mmo(struct drm_i915_gem_object *obj,
	   enum i915_mmap_type mmap_type)
{
	struct rb_node *rb;

	spin_lock(&obj->mmo.lock);
	rb = obj->mmo.offsets.rb_node;
	while (rb) {
		struct i915_mmap_offset *mmo =
			rb_entry(rb, typeof(*mmo), offset);

		if (mmo->mmap_type == mmap_type) {
			spin_unlock(&obj->mmo.lock);
			return mmo;
		}

		if (mmo->mmap_type < mmap_type)
			rb = rb->rb_right;
		else
			rb = rb->rb_left;
	}
	spin_unlock(&obj->mmo.lock);

	return NULL;
}

static struct i915_mmap_offset *
insert_mmo(struct drm_i915_gem_object *obj, struct i915_mmap_offset *mmo)
{
	struct rb_node *rb, **p;

	spin_lock(&obj->mmo.lock);
	rb = NULL;
	p = &obj->mmo.offsets.rb_node;
	while (*p) {
		struct i915_mmap_offset *pos;

		rb = *p;
		pos = rb_entry(rb, typeof(*pos), offset);

		if (pos->mmap_type == mmo->mmap_type) {
			spin_unlock(&obj->mmo.lock);
			drm_vma_offset_remove(obj->base.dev->vma_offset_manager,
					      &mmo->vma_node);
			kfree(mmo);
			return pos;
		}

		if (pos->mmap_type < mmo->mmap_type)
			p = &rb->rb_right;
		else
			p = &rb->rb_left;
	}
	rb_link_node(&mmo->offset, rb, p);
	rb_insert_color(&mmo->offset, &obj->mmo.offsets);
	spin_unlock(&obj->mmo.lock);

	return mmo;
}

static struct i915_mmap_offset *
mmap_offset_attach(struct drm_i915_gem_object *obj,
		   enum i915_mmap_type mmap_type,
		   struct drm_file *file)
{
	struct drm_i915_private *i915 = to_i915(obj->base.dev);
	struct i915_mmap_offset *mmo;
	int err;

	GEM_BUG_ON(obj->ops->mmap_offset || obj->ops->mmap_ops);

	mmo = lookup_mmo(obj, mmap_type);
	if (mmo)
		goto out;

	mmo = kmalloc(sizeof(*mmo), GFP_KERNEL);
	if (!mmo)
		return ERR_PTR(-ENOMEM);

	mmo->obj = obj;
	mmo->mmap_type = mmap_type;
	drm_vma_node_reset(&mmo->vma_node);

	err = drm_vma_offset_add(obj->base.dev->vma_offset_manager,
				 &mmo->vma_node, obj->base.size / PAGE_SIZE);
	if (likely(!err))
		goto insert;

	 
	err = intel_gt_retire_requests_timeout(to_gt(i915), MAX_SCHEDULE_TIMEOUT,
					       NULL);
	if (err)
		goto err;

	i915_gem_drain_freed_objects(i915);
	err = drm_vma_offset_add(obj->base.dev->vma_offset_manager,
				 &mmo->vma_node, obj->base.size / PAGE_SIZE);
	if (err)
		goto err;

insert:
	mmo = insert_mmo(obj, mmo);
	GEM_BUG_ON(lookup_mmo(obj, mmap_type) != mmo);
out:
	if (file)
		drm_vma_node_allow_once(&mmo->vma_node, file);
	return mmo;

err:
	kfree(mmo);
	return ERR_PTR(err);
}

static int
__assign_mmap_offset(struct drm_i915_gem_object *obj,
		     enum i915_mmap_type mmap_type,
		     u64 *offset, struct drm_file *file)
{
	struct i915_mmap_offset *mmo;

	if (i915_gem_object_never_mmap(obj))
		return -ENODEV;

	if (obj->ops->mmap_offset)  {
		if (mmap_type != I915_MMAP_TYPE_FIXED)
			return -ENODEV;

		*offset = obj->ops->mmap_offset(obj);
		return 0;
	}

	if (mmap_type == I915_MMAP_TYPE_FIXED)
		return -ENODEV;

	if (mmap_type != I915_MMAP_TYPE_GTT &&
	    !i915_gem_object_has_struct_page(obj) &&
	    !i915_gem_object_has_iomem(obj))
		return -ENODEV;

	mmo = mmap_offset_attach(obj, mmap_type, file);
	if (IS_ERR(mmo))
		return PTR_ERR(mmo);

	*offset = drm_vma_node_offset_addr(&mmo->vma_node);
	return 0;
}

static int
__assign_mmap_offset_handle(struct drm_file *file,
			    u32 handle,
			    enum i915_mmap_type mmap_type,
			    u64 *offset)
{
	struct drm_i915_gem_object *obj;
	int err;

	obj = i915_gem_object_lookup(file, handle);
	if (!obj)
		return -ENOENT;

	err = i915_gem_object_lock_interruptible(obj, NULL);
	if (err)
		goto out_put;
	err = __assign_mmap_offset(obj, mmap_type, offset, file);
	i915_gem_object_unlock(obj);
out_put:
	i915_gem_object_put(obj);
	return err;
}

int
i915_gem_dumb_mmap_offset(struct drm_file *file,
			  struct drm_device *dev,
			  u32 handle,
			  u64 *offset)
{
	struct drm_i915_private *i915 = to_i915(dev);
	enum i915_mmap_type mmap_type;

	if (HAS_LMEM(to_i915(dev)))
		mmap_type = I915_MMAP_TYPE_FIXED;
	else if (pat_enabled())
		mmap_type = I915_MMAP_TYPE_WC;
	else if (!i915_ggtt_has_aperture(to_gt(i915)->ggtt))
		return -ENODEV;
	else
		mmap_type = I915_MMAP_TYPE_GTT;

	return __assign_mmap_offset_handle(file, handle, mmap_type, offset);
}

 
int
i915_gem_mmap_offset_ioctl(struct drm_device *dev, void *data,
			   struct drm_file *file)
{
	struct drm_i915_private *i915 = to_i915(dev);
	struct drm_i915_gem_mmap_offset *args = data;
	enum i915_mmap_type type;
	int err;

	 

	err = i915_user_extensions(u64_to_user_ptr(args->extensions),
				   NULL, 0, NULL);
	if (err)
		return err;

	switch (args->flags) {
	case I915_MMAP_OFFSET_GTT:
		if (!i915_ggtt_has_aperture(to_gt(i915)->ggtt))
			return -ENODEV;
		type = I915_MMAP_TYPE_GTT;
		break;

	case I915_MMAP_OFFSET_WC:
		if (!pat_enabled())
			return -ENODEV;
		type = I915_MMAP_TYPE_WC;
		break;

	case I915_MMAP_OFFSET_WB:
		type = I915_MMAP_TYPE_WB;
		break;

	case I915_MMAP_OFFSET_UC:
		if (!pat_enabled())
			return -ENODEV;
		type = I915_MMAP_TYPE_UC;
		break;

	case I915_MMAP_OFFSET_FIXED:
		type = I915_MMAP_TYPE_FIXED;
		break;

	default:
		return -EINVAL;
	}

	return __assign_mmap_offset_handle(file, args->handle, type, &args->offset);
}

static void vm_open(struct vm_area_struct *vma)
{
	struct i915_mmap_offset *mmo = vma->vm_private_data;
	struct drm_i915_gem_object *obj = mmo->obj;

	GEM_BUG_ON(!obj);
	i915_gem_object_get(obj);
}

static void vm_close(struct vm_area_struct *vma)
{
	struct i915_mmap_offset *mmo = vma->vm_private_data;
	struct drm_i915_gem_object *obj = mmo->obj;

	GEM_BUG_ON(!obj);
	i915_gem_object_put(obj);
}

static const struct vm_operations_struct vm_ops_gtt = {
	.fault = vm_fault_gtt,
	.access = vm_access,
	.open = vm_open,
	.close = vm_close,
};

static const struct vm_operations_struct vm_ops_cpu = {
	.fault = vm_fault_cpu,
	.access = vm_access,
	.open = vm_open,
	.close = vm_close,
};

static int singleton_release(struct inode *inode, struct file *file)
{
	struct drm_i915_private *i915 = file->private_data;

	cmpxchg(&i915->gem.mmap_singleton, file, NULL);
	drm_dev_put(&i915->drm);

	return 0;
}

static const struct file_operations singleton_fops = {
	.owner = THIS_MODULE,
	.release = singleton_release,
};

static struct file *mmap_singleton(struct drm_i915_private *i915)
{
	struct file *file;

	rcu_read_lock();
	file = READ_ONCE(i915->gem.mmap_singleton);
	if (file && !get_file_rcu(file))
		file = NULL;
	rcu_read_unlock();
	if (file)
		return file;

	file = anon_inode_getfile("i915.gem", &singleton_fops, i915, O_RDWR);
	if (IS_ERR(file))
		return file;

	 
	file->f_mapping = i915->drm.anon_inode->i_mapping;

	smp_store_mb(i915->gem.mmap_singleton, file);
	drm_dev_get(&i915->drm);

	return file;
}

static int
i915_gem_object_mmap(struct drm_i915_gem_object *obj,
		     struct i915_mmap_offset *mmo,
		     struct vm_area_struct *vma)
{
	struct drm_i915_private *i915 = to_i915(obj->base.dev);
	struct drm_device *dev = &i915->drm;
	struct file *anon;

	if (i915_gem_object_is_readonly(obj)) {
		if (vma->vm_flags & VM_WRITE) {
			i915_gem_object_put(obj);
			return -EINVAL;
		}
		vm_flags_clear(vma, VM_MAYWRITE);
	}

	anon = mmap_singleton(to_i915(dev));
	if (IS_ERR(anon)) {
		i915_gem_object_put(obj);
		return PTR_ERR(anon);
	}

	vm_flags_set(vma, VM_PFNMAP | VM_DONTEXPAND | VM_DONTDUMP | VM_IO);

	 
	vma_set_file(vma, anon);
	 
	fput(anon);

	if (obj->ops->mmap_ops) {
		vma->vm_page_prot = pgprot_decrypted(vm_get_page_prot(vma->vm_flags));
		vma->vm_ops = obj->ops->mmap_ops;
		vma->vm_private_data = obj->base.vma_node.driver_private;
		return 0;
	}

	vma->vm_private_data = mmo;

	switch (mmo->mmap_type) {
	case I915_MMAP_TYPE_WC:
		vma->vm_page_prot =
			pgprot_writecombine(vm_get_page_prot(vma->vm_flags));
		vma->vm_ops = &vm_ops_cpu;
		break;

	case I915_MMAP_TYPE_FIXED:
		GEM_WARN_ON(1);
		fallthrough;
	case I915_MMAP_TYPE_WB:
		vma->vm_page_prot = vm_get_page_prot(vma->vm_flags);
		vma->vm_ops = &vm_ops_cpu;
		break;

	case I915_MMAP_TYPE_UC:
		vma->vm_page_prot =
			pgprot_noncached(vm_get_page_prot(vma->vm_flags));
		vma->vm_ops = &vm_ops_cpu;
		break;

	case I915_MMAP_TYPE_GTT:
		vma->vm_page_prot =
			pgprot_writecombine(vm_get_page_prot(vma->vm_flags));
		vma->vm_ops = &vm_ops_gtt;
		break;
	}
	vma->vm_page_prot = pgprot_decrypted(vma->vm_page_prot);

	return 0;
}

 
int i915_gem_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct drm_vma_offset_node *node;
	struct drm_file *priv = filp->private_data;
	struct drm_device *dev = priv->minor->dev;
	struct drm_i915_gem_object *obj = NULL;
	struct i915_mmap_offset *mmo = NULL;

	if (drm_dev_is_unplugged(dev))
		return -ENODEV;

	rcu_read_lock();
	drm_vma_offset_lock_lookup(dev->vma_offset_manager);
	node = drm_vma_offset_exact_lookup_locked(dev->vma_offset_manager,
						  vma->vm_pgoff,
						  vma_pages(vma));
	if (node && drm_vma_node_is_allowed(node, priv)) {
		 
		if (!node->driver_private) {
			mmo = container_of(node, struct i915_mmap_offset, vma_node);
			obj = i915_gem_object_get_rcu(mmo->obj);

			GEM_BUG_ON(obj && obj->ops->mmap_ops);
		} else {
			obj = i915_gem_object_get_rcu
				(container_of(node, struct drm_i915_gem_object,
					      base.vma_node));

			GEM_BUG_ON(obj && !obj->ops->mmap_ops);
		}
	}
	drm_vma_offset_unlock_lookup(dev->vma_offset_manager);
	rcu_read_unlock();
	if (!obj)
		return node ? -EACCES : -EINVAL;

	return i915_gem_object_mmap(obj, mmo, vma);
}

int i915_gem_fb_mmap(struct drm_i915_gem_object *obj, struct vm_area_struct *vma)
{
	struct drm_i915_private *i915 = to_i915(obj->base.dev);
	struct drm_device *dev = &i915->drm;
	struct i915_mmap_offset *mmo = NULL;
	enum i915_mmap_type mmap_type;
	struct i915_ggtt *ggtt = to_gt(i915)->ggtt;

	if (drm_dev_is_unplugged(dev))
		return -ENODEV;

	 
	if (obj->ops->mmap_ops) {
		 
		vma->vm_pgoff += drm_vma_node_start(&obj->base.vma_node);
	} else {
		 
		mmap_type = i915_ggtt_has_aperture(ggtt) ? I915_MMAP_TYPE_GTT : I915_MMAP_TYPE_WC;
		mmo = mmap_offset_attach(obj, mmap_type, NULL);
		if (IS_ERR(mmo))
			return PTR_ERR(mmo);
	}

	 
	obj = i915_gem_object_get(obj);
	return i915_gem_object_mmap(obj, mmo, vma);
}

#if IS_ENABLED(CONFIG_DRM_I915_SELFTEST)
#include "selftests/i915_gem_mman.c"
#endif
