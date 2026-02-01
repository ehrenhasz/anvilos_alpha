 

#include <linux/string.h>
#include <linux/bitops.h>

#include "i915_drv.h"
#include "i915_gem.h"
#include "i915_gem_ioctls.h"
#include "i915_gem_mman.h"
#include "i915_gem_object.h"
#include "i915_gem_tiling.h"
#include "i915_reg.h"

 

 
u32 i915_gem_fence_size(struct drm_i915_private *i915,
			u32 size, unsigned int tiling, unsigned int stride)
{
	u32 ggtt_size;

	GEM_BUG_ON(!size);

	if (tiling == I915_TILING_NONE)
		return size;

	GEM_BUG_ON(!stride);

	if (GRAPHICS_VER(i915) >= 4) {
		stride *= i915_gem_tile_height(tiling);
		GEM_BUG_ON(!IS_ALIGNED(stride, I965_FENCE_PAGE));
		return roundup(size, stride);
	}

	 
	if (GRAPHICS_VER(i915) == 3)
		ggtt_size = 1024*1024;
	else
		ggtt_size = 512*1024;

	while (ggtt_size < size)
		ggtt_size <<= 1;

	return ggtt_size;
}

 
u32 i915_gem_fence_alignment(struct drm_i915_private *i915, u32 size,
			     unsigned int tiling, unsigned int stride)
{
	GEM_BUG_ON(!size);

	 
	if (tiling == I915_TILING_NONE)
		return I915_GTT_MIN_ALIGNMENT;

	if (GRAPHICS_VER(i915) >= 4)
		return I965_FENCE_PAGE;

	 
	return i915_gem_fence_size(i915, size, tiling, stride);
}

 
static bool
i915_tiling_ok(struct drm_i915_gem_object *obj,
	       unsigned int tiling, unsigned int stride)
{
	struct drm_i915_private *i915 = to_i915(obj->base.dev);
	unsigned int tile_width;

	 
	if (tiling == I915_TILING_NONE)
		return true;

	if (tiling > I915_TILING_LAST)
		return false;

	 
	 
	if (GRAPHICS_VER(i915) >= 7) {
		if (stride / 128 > GEN7_FENCE_MAX_PITCH_VAL)
			return false;
	} else if (GRAPHICS_VER(i915) >= 4) {
		if (stride / 128 > I965_FENCE_MAX_PITCH_VAL)
			return false;
	} else {
		if (stride > 8192)
			return false;

		if (!is_power_of_2(stride))
			return false;
	}

	if (GRAPHICS_VER(i915) == 2 ||
	    (tiling == I915_TILING_Y && HAS_128_BYTE_Y_TILING(i915)))
		tile_width = 128;
	else
		tile_width = 512;

	if (!stride || !IS_ALIGNED(stride, tile_width))
		return false;

	return true;
}

static bool i915_vma_fence_prepare(struct i915_vma *vma,
				   int tiling_mode, unsigned int stride)
{
	struct drm_i915_private *i915 = vma->vm->i915;
	u32 size, alignment;

	if (!i915_vma_is_map_and_fenceable(vma))
		return true;

	size = i915_gem_fence_size(i915, vma->size, tiling_mode, stride);
	if (i915_vma_size(vma) < size)
		return false;

	alignment = i915_gem_fence_alignment(i915, vma->size, tiling_mode, stride);
	if (!IS_ALIGNED(i915_ggtt_offset(vma), alignment))
		return false;

	return true;
}

 
static int
i915_gem_object_fence_prepare(struct drm_i915_gem_object *obj,
			      int tiling_mode, unsigned int stride)
{
	struct drm_i915_private *i915 = to_i915(obj->base.dev);
	struct i915_ggtt *ggtt = to_gt(i915)->ggtt;
	struct i915_vma *vma, *vn;
	LIST_HEAD(unbind);
	int ret = 0;

	if (tiling_mode == I915_TILING_NONE)
		return 0;

	mutex_lock(&ggtt->vm.mutex);

	spin_lock(&obj->vma.lock);
	for_each_ggtt_vma(vma, obj) {
		GEM_BUG_ON(vma->vm != &ggtt->vm);

		if (i915_vma_fence_prepare(vma, tiling_mode, stride))
			continue;

		list_move(&vma->vm_link, &unbind);
	}
	spin_unlock(&obj->vma.lock);

	list_for_each_entry_safe(vma, vn, &unbind, vm_link) {
		ret = __i915_vma_unbind(vma);
		if (ret) {
			 
			list_splice(&unbind, &ggtt->vm.bound_list);
			break;
		}
	}

	mutex_unlock(&ggtt->vm.mutex);

	return ret;
}

bool i915_gem_object_needs_bit17_swizzle(struct drm_i915_gem_object *obj)
{
	struct drm_i915_private *i915 = to_i915(obj->base.dev);

	return to_gt(i915)->ggtt->bit_6_swizzle_x == I915_BIT_6_SWIZZLE_9_10_17 &&
		i915_gem_object_is_tiled(obj);
}

int
i915_gem_object_set_tiling(struct drm_i915_gem_object *obj,
			   unsigned int tiling, unsigned int stride)
{
	struct drm_i915_private *i915 = to_i915(obj->base.dev);
	struct i915_vma *vma;
	int err;

	 
	BUILD_BUG_ON(I915_TILING_LAST & STRIDE_MASK);

	GEM_BUG_ON(!i915_tiling_ok(obj, tiling, stride));
	GEM_BUG_ON(!stride ^ (tiling == I915_TILING_NONE));

	if ((tiling | stride) == obj->tiling_and_stride)
		return 0;

	if (i915_gem_object_is_framebuffer(obj))
		return -EBUSY;

	 

	i915_gem_object_lock(obj, NULL);
	if (i915_gem_object_is_framebuffer(obj)) {
		i915_gem_object_unlock(obj);
		return -EBUSY;
	}

	err = i915_gem_object_fence_prepare(obj, tiling, stride);
	if (err) {
		i915_gem_object_unlock(obj);
		return err;
	}

	 
	if (i915_gem_object_has_pages(obj) &&
	    obj->mm.madv == I915_MADV_WILLNEED &&
	    i915->gem_quirks & GEM_QUIRK_PIN_SWIZZLED_PAGES) {
		if (tiling == I915_TILING_NONE) {
			GEM_BUG_ON(!i915_gem_object_has_tiling_quirk(obj));
			i915_gem_object_clear_tiling_quirk(obj);
			i915_gem_object_make_shrinkable(obj);
		}
		if (!i915_gem_object_is_tiled(obj)) {
			GEM_BUG_ON(i915_gem_object_has_tiling_quirk(obj));
			i915_gem_object_make_unshrinkable(obj);
			i915_gem_object_set_tiling_quirk(obj);
		}
	}

	spin_lock(&obj->vma.lock);
	for_each_ggtt_vma(vma, obj) {
		vma->fence_size =
			i915_gem_fence_size(i915, vma->size, tiling, stride);
		vma->fence_alignment =
			i915_gem_fence_alignment(i915,
						 vma->size, tiling, stride);

		if (vma->fence)
			vma->fence->dirty = true;
	}
	spin_unlock(&obj->vma.lock);

	obj->tiling_and_stride = tiling | stride;

	 
	if (i915_gem_object_needs_bit17_swizzle(obj)) {
		if (!obj->bit_17) {
			obj->bit_17 = bitmap_zalloc(obj->base.size >> PAGE_SHIFT,
						    GFP_KERNEL);
		}
	} else {
		bitmap_free(obj->bit_17);
		obj->bit_17 = NULL;
	}

	i915_gem_object_unlock(obj);

	 
	i915_gem_object_release_mmap_gtt(obj);

	return 0;
}

 
int
i915_gem_set_tiling_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file)
{
	struct drm_i915_private *dev_priv = to_i915(dev);
	struct drm_i915_gem_set_tiling *args = data;
	struct drm_i915_gem_object *obj;
	int err;

	if (!to_gt(dev_priv)->ggtt->num_fences)
		return -EOPNOTSUPP;

	obj = i915_gem_object_lookup(file, args->handle);
	if (!obj)
		return -ENOENT;

	 
	if (i915_gem_object_is_proxy(obj)) {
		err = -ENXIO;
		goto err;
	}

	if (!i915_tiling_ok(obj, args->tiling_mode, args->stride)) {
		err = -EINVAL;
		goto err;
	}

	if (args->tiling_mode == I915_TILING_NONE) {
		args->swizzle_mode = I915_BIT_6_SWIZZLE_NONE;
		args->stride = 0;
	} else {
		if (args->tiling_mode == I915_TILING_X)
			args->swizzle_mode = to_gt(dev_priv)->ggtt->bit_6_swizzle_x;
		else
			args->swizzle_mode = to_gt(dev_priv)->ggtt->bit_6_swizzle_y;

		 
		if (args->swizzle_mode == I915_BIT_6_SWIZZLE_9_17)
			args->swizzle_mode = I915_BIT_6_SWIZZLE_9;
		if (args->swizzle_mode == I915_BIT_6_SWIZZLE_9_10_17)
			args->swizzle_mode = I915_BIT_6_SWIZZLE_9_10;

		 
		if (args->swizzle_mode == I915_BIT_6_SWIZZLE_UNKNOWN) {
			args->tiling_mode = I915_TILING_NONE;
			args->swizzle_mode = I915_BIT_6_SWIZZLE_NONE;
			args->stride = 0;
		}
	}

	err = i915_gem_object_set_tiling(obj, args->tiling_mode, args->stride);

	 
	args->stride = i915_gem_object_get_stride(obj);
	args->tiling_mode = i915_gem_object_get_tiling(obj);

err:
	i915_gem_object_put(obj);
	return err;
}

 
int
i915_gem_get_tiling_ioctl(struct drm_device *dev, void *data,
			  struct drm_file *file)
{
	struct drm_i915_gem_get_tiling *args = data;
	struct drm_i915_private *dev_priv = to_i915(dev);
	struct drm_i915_gem_object *obj;
	int err = -ENOENT;

	if (!to_gt(dev_priv)->ggtt->num_fences)
		return -EOPNOTSUPP;

	rcu_read_lock();
	obj = i915_gem_object_lookup_rcu(file, args->handle);
	if (obj) {
		args->tiling_mode =
			READ_ONCE(obj->tiling_and_stride) & TILING_MASK;
		err = 0;
	}
	rcu_read_unlock();
	if (unlikely(err))
		return err;

	switch (args->tiling_mode) {
	case I915_TILING_X:
		args->swizzle_mode = to_gt(dev_priv)->ggtt->bit_6_swizzle_x;
		break;
	case I915_TILING_Y:
		args->swizzle_mode = to_gt(dev_priv)->ggtt->bit_6_swizzle_y;
		break;
	default:
	case I915_TILING_NONE:
		args->swizzle_mode = I915_BIT_6_SWIZZLE_NONE;
		break;
	}

	 
	if (dev_priv->gem_quirks & GEM_QUIRK_PIN_SWIZZLED_PAGES)
		args->phys_swizzle_mode = I915_BIT_6_SWIZZLE_UNKNOWN;
	else
		args->phys_swizzle_mode = args->swizzle_mode;
	if (args->swizzle_mode == I915_BIT_6_SWIZZLE_9_17)
		args->swizzle_mode = I915_BIT_6_SWIZZLE_9;
	if (args->swizzle_mode == I915_BIT_6_SWIZZLE_9_10_17)
		args->swizzle_mode = I915_BIT_6_SWIZZLE_9_10;

	return 0;
}
