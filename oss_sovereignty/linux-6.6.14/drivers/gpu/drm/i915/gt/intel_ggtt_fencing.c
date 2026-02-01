
 

#include <linux/highmem.h>

#include "display/intel_display.h"
#include "i915_drv.h"
#include "i915_reg.h"
#include "i915_scatterlist.h"
#include "i915_pvinfo.h"
#include "i915_vgpu.h"
#include "intel_gt_regs.h"
#include "intel_mchbar_regs.h"

 

#define pipelined 0

static struct drm_i915_private *fence_to_i915(struct i915_fence_reg *fence)
{
	return fence->ggtt->vm.i915;
}

static struct intel_uncore *fence_to_uncore(struct i915_fence_reg *fence)
{
	return fence->ggtt->vm.gt->uncore;
}

static void i965_write_fence_reg(struct i915_fence_reg *fence)
{
	i915_reg_t fence_reg_lo, fence_reg_hi;
	int fence_pitch_shift;
	u64 val;

	if (GRAPHICS_VER(fence_to_i915(fence)) >= 6) {
		fence_reg_lo = FENCE_REG_GEN6_LO(fence->id);
		fence_reg_hi = FENCE_REG_GEN6_HI(fence->id);
		fence_pitch_shift = GEN6_FENCE_PITCH_SHIFT;

	} else {
		fence_reg_lo = FENCE_REG_965_LO(fence->id);
		fence_reg_hi = FENCE_REG_965_HI(fence->id);
		fence_pitch_shift = I965_FENCE_PITCH_SHIFT;
	}

	val = 0;
	if (fence->tiling) {
		unsigned int stride = fence->stride;

		GEM_BUG_ON(!IS_ALIGNED(stride, 128));

		val = fence->start + fence->size - I965_FENCE_PAGE;
		val <<= 32;
		val |= fence->start;
		val |= (u64)((stride / 128) - 1) << fence_pitch_shift;
		if (fence->tiling == I915_TILING_Y)
			val |= BIT(I965_FENCE_TILING_Y_SHIFT);
		val |= I965_FENCE_REG_VALID;
	}

	if (!pipelined) {
		struct intel_uncore *uncore = fence_to_uncore(fence);

		 
		intel_uncore_write_fw(uncore, fence_reg_lo, 0);
		intel_uncore_posting_read_fw(uncore, fence_reg_lo);

		intel_uncore_write_fw(uncore, fence_reg_hi, upper_32_bits(val));
		intel_uncore_write_fw(uncore, fence_reg_lo, lower_32_bits(val));
		intel_uncore_posting_read_fw(uncore, fence_reg_lo);
	}
}

static void i915_write_fence_reg(struct i915_fence_reg *fence)
{
	u32 val;

	val = 0;
	if (fence->tiling) {
		unsigned int stride = fence->stride;
		unsigned int tiling = fence->tiling;
		bool is_y_tiled = tiling == I915_TILING_Y;

		if (is_y_tiled && HAS_128_BYTE_Y_TILING(fence_to_i915(fence)))
			stride /= 128;
		else
			stride /= 512;
		GEM_BUG_ON(!is_power_of_2(stride));

		val = fence->start;
		if (is_y_tiled)
			val |= BIT(I830_FENCE_TILING_Y_SHIFT);
		val |= I915_FENCE_SIZE_BITS(fence->size);
		val |= ilog2(stride) << I830_FENCE_PITCH_SHIFT;

		val |= I830_FENCE_REG_VALID;
	}

	if (!pipelined) {
		struct intel_uncore *uncore = fence_to_uncore(fence);
		i915_reg_t reg = FENCE_REG(fence->id);

		intel_uncore_write_fw(uncore, reg, val);
		intel_uncore_posting_read_fw(uncore, reg);
	}
}

static void i830_write_fence_reg(struct i915_fence_reg *fence)
{
	u32 val;

	val = 0;
	if (fence->tiling) {
		unsigned int stride = fence->stride;

		val = fence->start;
		if (fence->tiling == I915_TILING_Y)
			val |= BIT(I830_FENCE_TILING_Y_SHIFT);
		val |= I830_FENCE_SIZE_BITS(fence->size);
		val |= ilog2(stride / 128) << I830_FENCE_PITCH_SHIFT;
		val |= I830_FENCE_REG_VALID;
	}

	if (!pipelined) {
		struct intel_uncore *uncore = fence_to_uncore(fence);
		i915_reg_t reg = FENCE_REG(fence->id);

		intel_uncore_write_fw(uncore, reg, val);
		intel_uncore_posting_read_fw(uncore, reg);
	}
}

static void fence_write(struct i915_fence_reg *fence)
{
	struct drm_i915_private *i915 = fence_to_i915(fence);

	 

	if (GRAPHICS_VER(i915) == 2)
		i830_write_fence_reg(fence);
	else if (GRAPHICS_VER(i915) == 3)
		i915_write_fence_reg(fence);
	else
		i965_write_fence_reg(fence);

	 
}

static bool gpu_uses_fence_registers(struct i915_fence_reg *fence)
{
	return GRAPHICS_VER(fence_to_i915(fence)) < 4;
}

static int fence_update(struct i915_fence_reg *fence,
			struct i915_vma *vma)
{
	struct i915_ggtt *ggtt = fence->ggtt;
	struct intel_uncore *uncore = fence_to_uncore(fence);
	intel_wakeref_t wakeref;
	struct i915_vma *old;
	int ret;

	fence->tiling = 0;
	if (vma) {
		GEM_BUG_ON(!i915_gem_object_get_stride(vma->obj) ||
			   !i915_gem_object_get_tiling(vma->obj));

		if (!i915_vma_is_map_and_fenceable(vma))
			return -EINVAL;

		if (gpu_uses_fence_registers(fence)) {
			 
			ret = i915_vma_sync(vma);
			if (ret)
				return ret;
		}

		GEM_BUG_ON(vma->fence_size > i915_vma_size(vma));
		fence->start = i915_ggtt_offset(vma);
		fence->size = vma->fence_size;
		fence->stride = i915_gem_object_get_stride(vma->obj);
		fence->tiling = i915_gem_object_get_tiling(vma->obj);
	}
	WRITE_ONCE(fence->dirty, false);

	old = xchg(&fence->vma, NULL);
	if (old) {
		 
		ret = i915_active_wait(&fence->active);
		if (ret) {
			fence->vma = old;
			return ret;
		}

		i915_vma_flush_writes(old);

		 
		if (old != vma) {
			GEM_BUG_ON(old->fence != fence);
			i915_vma_revoke_mmap(old);
			old->fence = NULL;
		}

		list_move(&fence->link, &ggtt->fence_list);
	}

	 
	wakeref = intel_runtime_pm_get_if_in_use(uncore->rpm);
	if (!wakeref) {
		GEM_BUG_ON(vma);
		return 0;
	}

	WRITE_ONCE(fence->vma, vma);
	fence_write(fence);

	if (vma) {
		vma->fence = fence;
		list_move_tail(&fence->link, &ggtt->fence_list);
	}

	intel_runtime_pm_put(uncore->rpm, wakeref);
	return 0;
}

 
void i915_vma_revoke_fence(struct i915_vma *vma)
{
	struct i915_fence_reg *fence = vma->fence;
	intel_wakeref_t wakeref;

	lockdep_assert_held(&vma->vm->mutex);
	if (!fence)
		return;

	GEM_BUG_ON(fence->vma != vma);
	GEM_BUG_ON(!i915_active_is_idle(&fence->active));
	GEM_BUG_ON(atomic_read(&fence->pin_count));

	fence->tiling = 0;
	WRITE_ONCE(fence->vma, NULL);
	vma->fence = NULL;

	 
	with_intel_runtime_pm_if_active(fence_to_uncore(fence)->rpm, wakeref)
		fence_write(fence);
}

static bool fence_is_active(const struct i915_fence_reg *fence)
{
	return fence->vma && i915_vma_is_active(fence->vma);
}

static struct i915_fence_reg *fence_find(struct i915_ggtt *ggtt)
{
	struct i915_fence_reg *active = NULL;
	struct i915_fence_reg *fence, *fn;

	list_for_each_entry_safe(fence, fn, &ggtt->fence_list, link) {
		GEM_BUG_ON(fence->vma && fence->vma->fence != fence);

		if (fence == active)  
			active = ERR_PTR(-EAGAIN);

		 
		if (active != ERR_PTR(-EAGAIN) && fence_is_active(fence)) {
			if (!active)
				active = fence;

			list_move_tail(&fence->link, &ggtt->fence_list);
			continue;
		}

		if (atomic_read(&fence->pin_count))
			continue;

		return fence;
	}

	 
	if (intel_has_pending_fb_unpin(ggtt->vm.i915))
		return ERR_PTR(-EAGAIN);

	return ERR_PTR(-ENOBUFS);
}

int __i915_vma_pin_fence(struct i915_vma *vma)
{
	struct i915_ggtt *ggtt = i915_vm_to_ggtt(vma->vm);
	struct i915_fence_reg *fence;
	struct i915_vma *set = i915_gem_object_is_tiled(vma->obj) ? vma : NULL;
	int err;

	lockdep_assert_held(&vma->vm->mutex);

	 
	if (vma->fence) {
		fence = vma->fence;
		GEM_BUG_ON(fence->vma != vma);
		atomic_inc(&fence->pin_count);
		if (!fence->dirty) {
			list_move_tail(&fence->link, &ggtt->fence_list);
			return 0;
		}
	} else if (set) {
		fence = fence_find(ggtt);
		if (IS_ERR(fence))
			return PTR_ERR(fence);

		GEM_BUG_ON(atomic_read(&fence->pin_count));
		atomic_inc(&fence->pin_count);
	} else {
		return 0;
	}

	err = fence_update(fence, set);
	if (err)
		goto out_unpin;

	GEM_BUG_ON(fence->vma != set);
	GEM_BUG_ON(vma->fence != (set ? fence : NULL));

	if (set)
		return 0;

out_unpin:
	atomic_dec(&fence->pin_count);
	return err;
}

 
int i915_vma_pin_fence(struct i915_vma *vma)
{
	int err;

	if (!vma->fence && !i915_gem_object_is_tiled(vma->obj))
		return 0;

	 
	assert_rpm_wakelock_held(vma->vm->gt->uncore->rpm);
	GEM_BUG_ON(!i915_vma_is_ggtt(vma));

	err = mutex_lock_interruptible(&vma->vm->mutex);
	if (err)
		return err;

	err = __i915_vma_pin_fence(vma);
	mutex_unlock(&vma->vm->mutex);

	return err;
}

 
struct i915_fence_reg *i915_reserve_fence(struct i915_ggtt *ggtt)
{
	struct i915_fence_reg *fence;
	int count;
	int ret;

	lockdep_assert_held(&ggtt->vm.mutex);

	 
	count = 0;
	list_for_each_entry(fence, &ggtt->fence_list, link)
		count += !atomic_read(&fence->pin_count);
	if (count <= 1)
		return ERR_PTR(-ENOSPC);

	fence = fence_find(ggtt);
	if (IS_ERR(fence))
		return fence;

	if (fence->vma) {
		 
		ret = fence_update(fence, NULL);
		if (ret)
			return ERR_PTR(ret);
	}

	list_del(&fence->link);

	return fence;
}

 
void i915_unreserve_fence(struct i915_fence_reg *fence)
{
	struct i915_ggtt *ggtt = fence->ggtt;

	lockdep_assert_held(&ggtt->vm.mutex);

	list_add(&fence->link, &ggtt->fence_list);
}

 
void intel_ggtt_restore_fences(struct i915_ggtt *ggtt)
{
	int i;

	for (i = 0; i < ggtt->num_fences; i++)
		fence_write(&ggtt->fence_regs[i]);
}

 

 
static void detect_bit_6_swizzle(struct i915_ggtt *ggtt)
{
	struct intel_uncore *uncore = ggtt->vm.gt->uncore;
	struct drm_i915_private *i915 = ggtt->vm.i915;
	u32 swizzle_x = I915_BIT_6_SWIZZLE_UNKNOWN;
	u32 swizzle_y = I915_BIT_6_SWIZZLE_UNKNOWN;

	if (GRAPHICS_VER(i915) >= 8 || IS_VALLEYVIEW(i915)) {
		 
		swizzle_x = I915_BIT_6_SWIZZLE_NONE;
		swizzle_y = I915_BIT_6_SWIZZLE_NONE;
	} else if (GRAPHICS_VER(i915) >= 6) {
		if (i915->preserve_bios_swizzle) {
			if (intel_uncore_read(uncore, DISP_ARB_CTL) &
			    DISP_TILE_SURFACE_SWIZZLING) {
				swizzle_x = I915_BIT_6_SWIZZLE_9_10;
				swizzle_y = I915_BIT_6_SWIZZLE_9;
			} else {
				swizzle_x = I915_BIT_6_SWIZZLE_NONE;
				swizzle_y = I915_BIT_6_SWIZZLE_NONE;
			}
		} else {
			u32 dimm_c0, dimm_c1;

			dimm_c0 = intel_uncore_read(uncore, MAD_DIMM_C0);
			dimm_c1 = intel_uncore_read(uncore, MAD_DIMM_C1);
			dimm_c0 &= MAD_DIMM_A_SIZE_MASK | MAD_DIMM_B_SIZE_MASK;
			dimm_c1 &= MAD_DIMM_A_SIZE_MASK | MAD_DIMM_B_SIZE_MASK;
			 
			if (dimm_c0 == dimm_c1) {
				swizzle_x = I915_BIT_6_SWIZZLE_9_10;
				swizzle_y = I915_BIT_6_SWIZZLE_9;
			} else {
				swizzle_x = I915_BIT_6_SWIZZLE_NONE;
				swizzle_y = I915_BIT_6_SWIZZLE_NONE;
			}
		}
	} else if (GRAPHICS_VER(i915) == 5) {
		 
		swizzle_x = I915_BIT_6_SWIZZLE_9_10;
		swizzle_y = I915_BIT_6_SWIZZLE_9;
	} else if (GRAPHICS_VER(i915) == 2) {
		 
		swizzle_x = I915_BIT_6_SWIZZLE_NONE;
		swizzle_y = I915_BIT_6_SWIZZLE_NONE;
	} else if (IS_G45(i915) || IS_I965G(i915) || IS_G33(i915)) {
		 
		if (intel_uncore_read16(uncore, C0DRB3_BW) ==
		    intel_uncore_read16(uncore, C1DRB3_BW)) {
			swizzle_x = I915_BIT_6_SWIZZLE_9_10;
			swizzle_y = I915_BIT_6_SWIZZLE_9;
		}
	} else {
		u32 dcc = intel_uncore_read(uncore, DCC);

		 
		switch (dcc & DCC_ADDRESSING_MODE_MASK) {
		case DCC_ADDRESSING_MODE_SINGLE_CHANNEL:
		case DCC_ADDRESSING_MODE_DUAL_CHANNEL_ASYMMETRIC:
			swizzle_x = I915_BIT_6_SWIZZLE_NONE;
			swizzle_y = I915_BIT_6_SWIZZLE_NONE;
			break;
		case DCC_ADDRESSING_MODE_DUAL_CHANNEL_INTERLEAVED:
			if (dcc & DCC_CHANNEL_XOR_DISABLE) {
				 
				swizzle_x = I915_BIT_6_SWIZZLE_9_10;
				swizzle_y = I915_BIT_6_SWIZZLE_9;
			} else if ((dcc & DCC_CHANNEL_XOR_BIT_17) == 0) {
				 
				swizzle_x = I915_BIT_6_SWIZZLE_9_10_11;
				swizzle_y = I915_BIT_6_SWIZZLE_9_11;
			} else {
				 
				swizzle_x = I915_BIT_6_SWIZZLE_9_10_17;
				swizzle_y = I915_BIT_6_SWIZZLE_9_17;
			}
			break;
		}

		 
		if (GRAPHICS_VER(i915) == 4 &&
		    !(intel_uncore_read(uncore, DCC2) & DCC2_MODIFIED_ENHANCED_DISABLE)) {
			swizzle_x = I915_BIT_6_SWIZZLE_UNKNOWN;
			swizzle_y = I915_BIT_6_SWIZZLE_UNKNOWN;
		}

		if (dcc == 0xffffffff) {
			drm_err(&i915->drm, "Couldn't read from MCHBAR.  "
				  "Disabling tiling.\n");
			swizzle_x = I915_BIT_6_SWIZZLE_UNKNOWN;
			swizzle_y = I915_BIT_6_SWIZZLE_UNKNOWN;
		}
	}

	if (swizzle_x == I915_BIT_6_SWIZZLE_UNKNOWN ||
	    swizzle_y == I915_BIT_6_SWIZZLE_UNKNOWN) {
		 
		i915->gem_quirks |= GEM_QUIRK_PIN_SWIZZLED_PAGES;
		swizzle_x = I915_BIT_6_SWIZZLE_NONE;
		swizzle_y = I915_BIT_6_SWIZZLE_NONE;
	}

	to_gt(i915)->ggtt->bit_6_swizzle_x = swizzle_x;
	to_gt(i915)->ggtt->bit_6_swizzle_y = swizzle_y;
}

 
static void swizzle_page(struct page *page)
{
	char temp[64];
	char *vaddr;
	int i;

	vaddr = kmap(page);

	for (i = 0; i < PAGE_SIZE; i += 128) {
		memcpy(temp, &vaddr[i], 64);
		memcpy(&vaddr[i], &vaddr[i + 64], 64);
		memcpy(&vaddr[i + 64], temp, 64);
	}

	kunmap(page);
}

 
void
i915_gem_object_do_bit_17_swizzle(struct drm_i915_gem_object *obj,
				  struct sg_table *pages)
{
	struct sgt_iter sgt_iter;
	struct page *page;
	int i;

	if (obj->bit_17 == NULL)
		return;

	i = 0;
	for_each_sgt_page(page, sgt_iter, pages) {
		char new_bit_17 = page_to_phys(page) >> 17;

		if ((new_bit_17 & 0x1) != (test_bit(i, obj->bit_17) != 0)) {
			swizzle_page(page);
			set_page_dirty(page);
		}

		i++;
	}
}

 
void
i915_gem_object_save_bit_17_swizzle(struct drm_i915_gem_object *obj,
				    struct sg_table *pages)
{
	const unsigned int page_count = obj->base.size >> PAGE_SHIFT;
	struct sgt_iter sgt_iter;
	struct page *page;
	int i;

	if (obj->bit_17 == NULL) {
		obj->bit_17 = bitmap_zalloc(page_count, GFP_KERNEL);
		if (obj->bit_17 == NULL) {
			drm_err(obj->base.dev,
				"Failed to allocate memory for bit 17 record\n");
			return;
		}
	}

	i = 0;

	for_each_sgt_page(page, sgt_iter, pages) {
		if (page_to_phys(page) & (1 << 17))
			__set_bit(i, obj->bit_17);
		else
			__clear_bit(i, obj->bit_17);
		i++;
	}
}

void intel_ggtt_init_fences(struct i915_ggtt *ggtt)
{
	struct drm_i915_private *i915 = ggtt->vm.i915;
	struct intel_uncore *uncore = ggtt->vm.gt->uncore;
	int num_fences;
	int i;

	INIT_LIST_HEAD(&ggtt->fence_list);
	INIT_LIST_HEAD(&ggtt->userfault_list);

	detect_bit_6_swizzle(ggtt);

	if (!i915_ggtt_has_aperture(ggtt))
		num_fences = 0;
	else if (GRAPHICS_VER(i915) >= 7 &&
		 !(IS_VALLEYVIEW(i915) || IS_CHERRYVIEW(i915)))
		num_fences = 32;
	else if (GRAPHICS_VER(i915) >= 4 ||
		 IS_I945G(i915) || IS_I945GM(i915) ||
		 IS_G33(i915) || IS_PINEVIEW(i915))
		num_fences = 16;
	else
		num_fences = 8;

	if (intel_vgpu_active(i915))
		num_fences = intel_uncore_read(uncore,
					       vgtif_reg(avail_rs.fence_num));
	ggtt->fence_regs = kcalloc(num_fences,
				   sizeof(*ggtt->fence_regs),
				   GFP_KERNEL);
	if (!ggtt->fence_regs)
		num_fences = 0;

	 
	for (i = 0; i < num_fences; i++) {
		struct i915_fence_reg *fence = &ggtt->fence_regs[i];

		i915_active_init(&fence->active, NULL, NULL, 0);
		fence->ggtt = ggtt;
		fence->id = i;
		list_add_tail(&fence->link, &ggtt->fence_list);
	}
	ggtt->num_fences = num_fences;

	intel_ggtt_restore_fences(ggtt);
}

void intel_ggtt_fini_fences(struct i915_ggtt *ggtt)
{
	int i;

	for (i = 0; i < ggtt->num_fences; i++) {
		struct i915_fence_reg *fence = &ggtt->fence_regs[i];

		i915_active_fini(&fence->active);
	}

	kfree(ggtt->fence_regs);
}

void intel_gt_init_swizzling(struct intel_gt *gt)
{
	struct drm_i915_private *i915 = gt->i915;
	struct intel_uncore *uncore = gt->uncore;

	if (GRAPHICS_VER(i915) < 5 ||
	    to_gt(i915)->ggtt->bit_6_swizzle_x == I915_BIT_6_SWIZZLE_NONE)
		return;

	intel_uncore_rmw(uncore, DISP_ARB_CTL, 0, DISP_TILE_SURFACE_SWIZZLING);

	if (GRAPHICS_VER(i915) == 5)
		return;

	intel_uncore_rmw(uncore, TILECTL, 0, TILECTL_SWZCTL);

	if (GRAPHICS_VER(i915) == 6)
		intel_uncore_write(uncore,
				   ARB_MODE,
				   _MASKED_BIT_ENABLE(ARB_MODE_SWIZZLE_SNB));
	else if (GRAPHICS_VER(i915) == 7)
		intel_uncore_write(uncore,
				   ARB_MODE,
				   _MASKED_BIT_ENABLE(ARB_MODE_SWIZZLE_IVB));
	else if (GRAPHICS_VER(i915) == 8)
		intel_uncore_write(uncore,
				   GAMTARBMODE,
				   _MASKED_BIT_ENABLE(ARB_MODE_SWIZZLE_BDW));
	else
		MISSING_CASE(GRAPHICS_VER(i915));
}
