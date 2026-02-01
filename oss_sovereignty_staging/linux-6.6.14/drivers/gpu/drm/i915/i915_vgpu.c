 

#include "i915_drv.h"
#include "i915_pvinfo.h"
#include "i915_vgpu.h"

 

 
void intel_vgpu_detect(struct drm_i915_private *dev_priv)
{
	struct pci_dev *pdev = to_pci_dev(dev_priv->drm.dev);
	u64 magic;
	u16 version_major;
	void __iomem *shared_area;

	BUILD_BUG_ON(sizeof(struct vgt_if) != VGT_PVINFO_SIZE);

	 
	if (GRAPHICS_VER(dev_priv) < 6)
		return;

	shared_area = pci_iomap_range(pdev, 0, VGT_PVINFO_PAGE, VGT_PVINFO_SIZE);
	if (!shared_area) {
		drm_err(&dev_priv->drm,
			"failed to map MMIO bar to check for VGT\n");
		return;
	}

	magic = readq(shared_area + vgtif_offset(magic));
	if (magic != VGT_MAGIC)
		goto out;

	version_major = readw(shared_area + vgtif_offset(version_major));
	if (version_major < VGT_VERSION_MAJOR) {
		drm_info(&dev_priv->drm, "VGT interface version mismatch!\n");
		goto out;
	}

	dev_priv->vgpu.caps = readl(shared_area + vgtif_offset(vgt_caps));

	dev_priv->vgpu.active = true;
	mutex_init(&dev_priv->vgpu.lock);
	drm_info(&dev_priv->drm, "Virtual GPU for Intel GVT-g detected.\n");

out:
	pci_iounmap(pdev, shared_area);
}

void intel_vgpu_register(struct drm_i915_private *i915)
{
	 
	if (intel_vgpu_active(i915))
		intel_uncore_write(&i915->uncore, vgtif_reg(display_ready),
				   VGT_DRV_DISPLAY_READY);
}

bool intel_vgpu_active(struct drm_i915_private *dev_priv)
{
	return dev_priv->vgpu.active;
}

bool intel_vgpu_has_full_ppgtt(struct drm_i915_private *dev_priv)
{
	return dev_priv->vgpu.caps & VGT_CAPS_FULL_PPGTT;
}

bool intel_vgpu_has_hwsp_emulation(struct drm_i915_private *dev_priv)
{
	return dev_priv->vgpu.caps & VGT_CAPS_HWSP_EMULATION;
}

bool intel_vgpu_has_huge_gtt(struct drm_i915_private *dev_priv)
{
	return dev_priv->vgpu.caps & VGT_CAPS_HUGE_GTT;
}

struct _balloon_info_ {
	 
	struct drm_mm_node space[4];
};

static struct _balloon_info_ bl_info;

static void vgt_deballoon_space(struct i915_ggtt *ggtt,
				struct drm_mm_node *node)
{
	struct drm_i915_private *dev_priv = ggtt->vm.i915;
	if (!drm_mm_node_allocated(node))
		return;

	drm_dbg(&dev_priv->drm,
		"deballoon space: range [0x%llx - 0x%llx] %llu KiB.\n",
		node->start,
		node->start + node->size,
		node->size / 1024);

	ggtt->vm.reserved -= node->size;
	drm_mm_remove_node(node);
}

 
void intel_vgt_deballoon(struct i915_ggtt *ggtt)
{
	struct drm_i915_private *dev_priv = ggtt->vm.i915;
	int i;

	if (!intel_vgpu_active(ggtt->vm.i915))
		return;

	drm_dbg(&dev_priv->drm, "VGT deballoon.\n");

	for (i = 0; i < 4; i++)
		vgt_deballoon_space(ggtt, &bl_info.space[i]);
}

static int vgt_balloon_space(struct i915_ggtt *ggtt,
			     struct drm_mm_node *node,
			     unsigned long start, unsigned long end)
{
	struct drm_i915_private *dev_priv = ggtt->vm.i915;
	unsigned long size = end - start;
	int ret;

	if (start >= end)
		return -EINVAL;

	drm_info(&dev_priv->drm,
		 "balloon space: range [ 0x%lx - 0x%lx ] %lu KiB.\n",
		 start, end, size / 1024);
	ret = i915_gem_gtt_reserve(&ggtt->vm, NULL, node,
				   size, start, I915_COLOR_UNEVICTABLE,
				   0);
	if (!ret)
		ggtt->vm.reserved += size;

	return ret;
}

 
int intel_vgt_balloon(struct i915_ggtt *ggtt)
{
	struct drm_i915_private *dev_priv = ggtt->vm.i915;
	struct intel_uncore *uncore = &dev_priv->uncore;
	unsigned long ggtt_end = ggtt->vm.total;

	unsigned long mappable_base, mappable_size, mappable_end;
	unsigned long unmappable_base, unmappable_size, unmappable_end;
	int ret;

	if (!intel_vgpu_active(ggtt->vm.i915))
		return 0;

	mappable_base =
	  intel_uncore_read(uncore, vgtif_reg(avail_rs.mappable_gmadr.base));
	mappable_size =
	  intel_uncore_read(uncore, vgtif_reg(avail_rs.mappable_gmadr.size));
	unmappable_base =
	  intel_uncore_read(uncore, vgtif_reg(avail_rs.nonmappable_gmadr.base));
	unmappable_size =
	  intel_uncore_read(uncore, vgtif_reg(avail_rs.nonmappable_gmadr.size));

	mappable_end = mappable_base + mappable_size;
	unmappable_end = unmappable_base + unmappable_size;

	drm_info(&dev_priv->drm, "VGT ballooning configuration:\n");
	drm_info(&dev_priv->drm,
		 "Mappable graphic memory: base 0x%lx size %ldKiB\n",
		 mappable_base, mappable_size / 1024);
	drm_info(&dev_priv->drm,
		 "Unmappable graphic memory: base 0x%lx size %ldKiB\n",
		 unmappable_base, unmappable_size / 1024);

	if (mappable_end > ggtt->mappable_end ||
	    unmappable_base < ggtt->mappable_end ||
	    unmappable_end > ggtt_end) {
		drm_err(&dev_priv->drm, "Invalid ballooning configuration!\n");
		return -EINVAL;
	}

	 
	if (unmappable_base > ggtt->mappable_end) {
		ret = vgt_balloon_space(ggtt, &bl_info.space[2],
					ggtt->mappable_end, unmappable_base);

		if (ret)
			goto err;
	}

	if (unmappable_end < ggtt_end) {
		ret = vgt_balloon_space(ggtt, &bl_info.space[3],
					unmappable_end, ggtt_end);
		if (ret)
			goto err_upon_mappable;
	}

	 
	if (mappable_base) {
		ret = vgt_balloon_space(ggtt, &bl_info.space[0],
					0, mappable_base);

		if (ret)
			goto err_upon_unmappable;
	}

	if (mappable_end < ggtt->mappable_end) {
		ret = vgt_balloon_space(ggtt, &bl_info.space[1],
					mappable_end, ggtt->mappable_end);

		if (ret)
			goto err_below_mappable;
	}

	drm_info(&dev_priv->drm, "VGT balloon successfully\n");
	return 0;

err_below_mappable:
	vgt_deballoon_space(ggtt, &bl_info.space[0]);
err_upon_unmappable:
	vgt_deballoon_space(ggtt, &bl_info.space[3]);
err_upon_mappable:
	vgt_deballoon_space(ggtt, &bl_info.space[2]);
err:
	drm_err(&dev_priv->drm, "VGT balloon fail\n");
	return ret;
}
