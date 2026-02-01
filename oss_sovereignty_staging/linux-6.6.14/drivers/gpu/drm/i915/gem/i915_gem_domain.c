 

#include "display/intel_display.h"
#include "display/intel_frontbuffer.h"
#include "gt/intel_gt.h"

#include "i915_drv.h"
#include "i915_gem_clflush.h"
#include "i915_gem_domain.h"
#include "i915_gem_gtt.h"
#include "i915_gem_ioctls.h"
#include "i915_gem_lmem.h"
#include "i915_gem_mman.h"
#include "i915_gem_object.h"
#include "i915_vma.h"

#define VTD_GUARD (168u * I915_GTT_PAGE_SIZE)  

static bool gpu_write_needs_clflush(struct drm_i915_gem_object *obj)
{
	struct drm_i915_private *i915 = to_i915(obj->base.dev);

	if (IS_DGFX(i915))
		return false;

	 
	return !(i915_gem_object_has_cache_level(obj, I915_CACHE_NONE) ||
		 i915_gem_object_has_cache_level(obj, I915_CACHE_WT));
}

bool i915_gem_cpu_write_needs_clflush(struct drm_i915_gem_object *obj)
{
	struct drm_i915_private *i915 = to_i915(obj->base.dev);

	if (obj->cache_dirty)
		return false;

	if (IS_DGFX(i915))
		return false;

	if (!(obj->cache_coherent & I915_BO_CACHE_COHERENT_FOR_WRITE))
		return true;

	 
	return i915_gem_object_is_framebuffer(obj);
}

static void
flush_write_domain(struct drm_i915_gem_object *obj, unsigned int flush_domains)
{
	struct i915_vma *vma;

	assert_object_held(obj);

	if (!(obj->write_domain & flush_domains))
		return;

	switch (obj->write_domain) {
	case I915_GEM_DOMAIN_GTT:
		spin_lock(&obj->vma.lock);
		for_each_ggtt_vma(vma, obj)
			i915_vma_flush_writes(vma);
		spin_unlock(&obj->vma.lock);

		i915_gem_object_flush_frontbuffer(obj, ORIGIN_CPU);
		break;

	case I915_GEM_DOMAIN_WC:
		wmb();
		break;

	case I915_GEM_DOMAIN_CPU:
		i915_gem_clflush_object(obj, I915_CLFLUSH_SYNC);
		break;

	case I915_GEM_DOMAIN_RENDER:
		if (gpu_write_needs_clflush(obj))
			obj->cache_dirty = true;
		break;
	}

	obj->write_domain = 0;
}

static void __i915_gem_object_flush_for_display(struct drm_i915_gem_object *obj)
{
	 
	flush_write_domain(obj, ~I915_GEM_DOMAIN_CPU);
	if (obj->cache_dirty)
		i915_gem_clflush_object(obj, I915_CLFLUSH_FORCE);
	obj->write_domain = 0;
}

void i915_gem_object_flush_if_display(struct drm_i915_gem_object *obj)
{
	if (!i915_gem_object_is_framebuffer(obj))
		return;

	i915_gem_object_lock(obj, NULL);
	__i915_gem_object_flush_for_display(obj);
	i915_gem_object_unlock(obj);
}

void i915_gem_object_flush_if_display_locked(struct drm_i915_gem_object *obj)
{
	if (i915_gem_object_is_framebuffer(obj))
		__i915_gem_object_flush_for_display(obj);
}

 
int
i915_gem_object_set_to_wc_domain(struct drm_i915_gem_object *obj, bool write)
{
	int ret;

	assert_object_held(obj);

	ret = i915_gem_object_wait(obj,
				   I915_WAIT_INTERRUPTIBLE |
				   (write ? I915_WAIT_ALL : 0),
				   MAX_SCHEDULE_TIMEOUT);
	if (ret)
		return ret;

	if (obj->write_domain == I915_GEM_DOMAIN_WC)
		return 0;

	 
	ret = i915_gem_object_pin_pages(obj);
	if (ret)
		return ret;

	flush_write_domain(obj, ~I915_GEM_DOMAIN_WC);

	 
	if ((obj->read_domains & I915_GEM_DOMAIN_WC) == 0)
		mb();

	 
	GEM_BUG_ON((obj->write_domain & ~I915_GEM_DOMAIN_WC) != 0);
	obj->read_domains |= I915_GEM_DOMAIN_WC;
	if (write) {
		obj->read_domains = I915_GEM_DOMAIN_WC;
		obj->write_domain = I915_GEM_DOMAIN_WC;
		obj->mm.dirty = true;
	}

	i915_gem_object_unpin_pages(obj);
	return 0;
}

 
int
i915_gem_object_set_to_gtt_domain(struct drm_i915_gem_object *obj, bool write)
{
	int ret;

	assert_object_held(obj);

	ret = i915_gem_object_wait(obj,
				   I915_WAIT_INTERRUPTIBLE |
				   (write ? I915_WAIT_ALL : 0),
				   MAX_SCHEDULE_TIMEOUT);
	if (ret)
		return ret;

	if (obj->write_domain == I915_GEM_DOMAIN_GTT)
		return 0;

	 
	ret = i915_gem_object_pin_pages(obj);
	if (ret)
		return ret;

	flush_write_domain(obj, ~I915_GEM_DOMAIN_GTT);

	 
	if ((obj->read_domains & I915_GEM_DOMAIN_GTT) == 0)
		mb();

	 
	GEM_BUG_ON((obj->write_domain & ~I915_GEM_DOMAIN_GTT) != 0);
	obj->read_domains |= I915_GEM_DOMAIN_GTT;
	if (write) {
		struct i915_vma *vma;

		obj->read_domains = I915_GEM_DOMAIN_GTT;
		obj->write_domain = I915_GEM_DOMAIN_GTT;
		obj->mm.dirty = true;

		spin_lock(&obj->vma.lock);
		for_each_ggtt_vma(vma, obj)
			if (i915_vma_is_bound(vma, I915_VMA_GLOBAL_BIND))
				i915_vma_set_ggtt_write(vma);
		spin_unlock(&obj->vma.lock);
	}

	i915_gem_object_unpin_pages(obj);
	return 0;
}

 
int i915_gem_object_set_cache_level(struct drm_i915_gem_object *obj,
				    enum i915_cache_level cache_level)
{
	int ret;

	 
	if (i915_gem_object_has_cache_level(obj, cache_level))
		return 0;

	ret = i915_gem_object_wait(obj,
				   I915_WAIT_INTERRUPTIBLE |
				   I915_WAIT_ALL,
				   MAX_SCHEDULE_TIMEOUT);
	if (ret)
		return ret;

	 
	i915_gem_object_set_cache_coherency(obj, cache_level);
	obj->cache_dirty = true;

	 
	return i915_gem_object_unbind(obj,
				      I915_GEM_OBJECT_UNBIND_ACTIVE |
				      I915_GEM_OBJECT_UNBIND_BARRIER);
}

int i915_gem_get_caching_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file)
{
	struct drm_i915_gem_caching *args = data;
	struct drm_i915_gem_object *obj;
	int err = 0;

	if (IS_DGFX(to_i915(dev)))
		return -ENODEV;

	rcu_read_lock();
	obj = i915_gem_object_lookup_rcu(file, args->handle);
	if (!obj) {
		err = -ENOENT;
		goto out;
	}

	 
	if (obj->pat_set_by_user) {
		err = -EOPNOTSUPP;
		goto out;
	}

	if (i915_gem_object_has_cache_level(obj, I915_CACHE_LLC) ||
	    i915_gem_object_has_cache_level(obj, I915_CACHE_L3_LLC))
		args->caching = I915_CACHING_CACHED;
	else if (i915_gem_object_has_cache_level(obj, I915_CACHE_WT))
		args->caching = I915_CACHING_DISPLAY;
	else
		args->caching = I915_CACHING_NONE;
out:
	rcu_read_unlock();
	return err;
}

int i915_gem_set_caching_ioctl(struct drm_device *dev, void *data,
			       struct drm_file *file)
{
	struct drm_i915_private *i915 = to_i915(dev);
	struct drm_i915_gem_caching *args = data;
	struct drm_i915_gem_object *obj;
	enum i915_cache_level level;
	int ret = 0;

	if (IS_DGFX(i915))
		return -ENODEV;

	if (GRAPHICS_VER_FULL(i915) >= IP_VER(12, 70))
		return -EOPNOTSUPP;

	switch (args->caching) {
	case I915_CACHING_NONE:
		level = I915_CACHE_NONE;
		break;
	case I915_CACHING_CACHED:
		 
		if (!HAS_LLC(i915) && !HAS_SNOOP(i915))
			return -ENODEV;

		level = I915_CACHE_LLC;
		break;
	case I915_CACHING_DISPLAY:
		level = HAS_WT(i915) ? I915_CACHE_WT : I915_CACHE_NONE;
		break;
	default:
		return -EINVAL;
	}

	obj = i915_gem_object_lookup(file, args->handle);
	if (!obj)
		return -ENOENT;

	 
	if (obj->pat_set_by_user) {
		ret = -EOPNOTSUPP;
		goto out;
	}

	 
	if (i915_gem_object_is_proxy(obj)) {
		 
		if (!i915_gem_object_is_userptr(obj) ||
		    args->caching != I915_CACHING_CACHED)
			ret = -ENXIO;

		goto out;
	}

	ret = i915_gem_object_lock_interruptible(obj, NULL);
	if (ret)
		goto out;

	ret = i915_gem_object_set_cache_level(obj, level);
	i915_gem_object_unlock(obj);

out:
	i915_gem_object_put(obj);
	return ret;
}

 
struct i915_vma *
i915_gem_object_pin_to_display_plane(struct drm_i915_gem_object *obj,
				     struct i915_gem_ww_ctx *ww,
				     u32 alignment,
				     const struct i915_gtt_view *view,
				     unsigned int flags)
{
	struct drm_i915_private *i915 = to_i915(obj->base.dev);
	struct i915_vma *vma;
	int ret;

	 
	if (HAS_LMEM(i915) && !i915_gem_object_is_lmem(obj))
		return ERR_PTR(-EINVAL);

	 
	ret = i915_gem_object_set_cache_level(obj,
					      HAS_WT(i915) ?
					      I915_CACHE_WT : I915_CACHE_NONE);
	if (ret)
		return ERR_PTR(ret);

	 
	if (intel_scanout_needs_vtd_wa(i915)) {
		unsigned int guard = VTD_GUARD;

		if (i915_gem_object_is_tiled(obj))
			guard = max(guard,
				    i915_gem_object_get_tile_row_size(obj));

		flags |= PIN_OFFSET_GUARD | guard;
	}

	 
	vma = ERR_PTR(-ENOSPC);
	if ((flags & PIN_MAPPABLE) == 0 &&
	    (!view || view->type == I915_GTT_VIEW_NORMAL))
		vma = i915_gem_object_ggtt_pin_ww(obj, ww, view, 0, alignment,
						  flags | PIN_MAPPABLE |
						  PIN_NONBLOCK);
	if (IS_ERR(vma) && vma != ERR_PTR(-EDEADLK))
		vma = i915_gem_object_ggtt_pin_ww(obj, ww, view, 0,
						  alignment, flags);
	if (IS_ERR(vma))
		return vma;

	vma->display_alignment = max(vma->display_alignment, alignment);
	i915_vma_mark_scanout(vma);

	i915_gem_object_flush_if_display_locked(obj);

	return vma;
}

 
int
i915_gem_object_set_to_cpu_domain(struct drm_i915_gem_object *obj, bool write)
{
	int ret;

	assert_object_held(obj);

	ret = i915_gem_object_wait(obj,
				   I915_WAIT_INTERRUPTIBLE |
				   (write ? I915_WAIT_ALL : 0),
				   MAX_SCHEDULE_TIMEOUT);
	if (ret)
		return ret;

	flush_write_domain(obj, ~I915_GEM_DOMAIN_CPU);

	 
	if ((obj->read_domains & I915_GEM_DOMAIN_CPU) == 0) {
		i915_gem_clflush_object(obj, I915_CLFLUSH_SYNC);
		obj->read_domains |= I915_GEM_DOMAIN_CPU;
	}

	 
	GEM_BUG_ON(obj->write_domain & ~I915_GEM_DOMAIN_CPU);

	 
	if (write)
		__start_cpu_write(obj);

	return 0;
}

 
int
i915_gem_set_domain_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file)
{
	struct drm_i915_gem_set_domain *args = data;
	struct drm_i915_gem_object *obj;
	u32 read_domains = args->read_domains;
	u32 write_domain = args->write_domain;
	int err;

	if (IS_DGFX(to_i915(dev)))
		return -ENODEV;

	 
	if ((write_domain | read_domains) & I915_GEM_GPU_DOMAINS)
		return -EINVAL;

	 
	if (write_domain && read_domains != write_domain)
		return -EINVAL;

	if (!read_domains)
		return 0;

	obj = i915_gem_object_lookup(file, args->handle);
	if (!obj)
		return -ENOENT;

	 
	err = i915_gem_object_wait(obj,
				   I915_WAIT_INTERRUPTIBLE |
				   I915_WAIT_PRIORITY |
				   (write_domain ? I915_WAIT_ALL : 0),
				   MAX_SCHEDULE_TIMEOUT);
	if (err)
		goto out;

	if (i915_gem_object_is_userptr(obj)) {
		 
		err = i915_gem_object_userptr_validate(obj);
		if (!err)
			err = i915_gem_object_wait(obj,
						   I915_WAIT_INTERRUPTIBLE |
						   I915_WAIT_PRIORITY |
						   (write_domain ? I915_WAIT_ALL : 0),
						   MAX_SCHEDULE_TIMEOUT);
		goto out;
	}

	 
	if (i915_gem_object_is_proxy(obj)) {
		err = -ENXIO;
		goto out;
	}

	err = i915_gem_object_lock_interruptible(obj, NULL);
	if (err)
		goto out;

	 
	err = i915_gem_object_pin_pages(obj);
	if (err)
		goto out_unlock;

	 
	if (READ_ONCE(obj->write_domain) == read_domains)
		goto out_unpin;

	if (read_domains & I915_GEM_DOMAIN_WC)
		err = i915_gem_object_set_to_wc_domain(obj, write_domain);
	else if (read_domains & I915_GEM_DOMAIN_GTT)
		err = i915_gem_object_set_to_gtt_domain(obj, write_domain);
	else
		err = i915_gem_object_set_to_cpu_domain(obj, write_domain);

out_unpin:
	i915_gem_object_unpin_pages(obj);

out_unlock:
	i915_gem_object_unlock(obj);

	if (!err && write_domain)
		i915_gem_object_invalidate_frontbuffer(obj, ORIGIN_CPU);

out:
	i915_gem_object_put(obj);
	return err;
}

 
int i915_gem_object_prepare_read(struct drm_i915_gem_object *obj,
				 unsigned int *needs_clflush)
{
	int ret;

	*needs_clflush = 0;
	if (!i915_gem_object_has_struct_page(obj))
		return -ENODEV;

	assert_object_held(obj);

	ret = i915_gem_object_wait(obj,
				   I915_WAIT_INTERRUPTIBLE,
				   MAX_SCHEDULE_TIMEOUT);
	if (ret)
		return ret;

	ret = i915_gem_object_pin_pages(obj);
	if (ret)
		return ret;

	if (obj->cache_coherent & I915_BO_CACHE_COHERENT_FOR_READ ||
	    !static_cpu_has(X86_FEATURE_CLFLUSH)) {
		ret = i915_gem_object_set_to_cpu_domain(obj, false);
		if (ret)
			goto err_unpin;
		else
			goto out;
	}

	flush_write_domain(obj, ~I915_GEM_DOMAIN_CPU);

	 
	if (!obj->cache_dirty &&
	    !(obj->read_domains & I915_GEM_DOMAIN_CPU))
		*needs_clflush = CLFLUSH_BEFORE;

out:
	 
	return 0;

err_unpin:
	i915_gem_object_unpin_pages(obj);
	return ret;
}

int i915_gem_object_prepare_write(struct drm_i915_gem_object *obj,
				  unsigned int *needs_clflush)
{
	int ret;

	*needs_clflush = 0;
	if (!i915_gem_object_has_struct_page(obj))
		return -ENODEV;

	assert_object_held(obj);

	ret = i915_gem_object_wait(obj,
				   I915_WAIT_INTERRUPTIBLE |
				   I915_WAIT_ALL,
				   MAX_SCHEDULE_TIMEOUT);
	if (ret)
		return ret;

	ret = i915_gem_object_pin_pages(obj);
	if (ret)
		return ret;

	if (obj->cache_coherent & I915_BO_CACHE_COHERENT_FOR_WRITE ||
	    !static_cpu_has(X86_FEATURE_CLFLUSH)) {
		ret = i915_gem_object_set_to_cpu_domain(obj, true);
		if (ret)
			goto err_unpin;
		else
			goto out;
	}

	flush_write_domain(obj, ~I915_GEM_DOMAIN_CPU);

	 
	if (!obj->cache_dirty) {
		*needs_clflush |= CLFLUSH_AFTER;

		 
		if (!(obj->read_domains & I915_GEM_DOMAIN_CPU))
			*needs_clflush |= CLFLUSH_BEFORE;
	}

out:
	i915_gem_object_invalidate_frontbuffer(obj, ORIGIN_CPU);
	obj->mm.dirty = true;
	 
	return 0;

err_unpin:
	i915_gem_object_unpin_pages(obj);
	return ret;
}
