 
#include "i915_drv.h"
#include "gvt.h"

 
struct intel_vgpu_page_track *intel_vgpu_find_page_track(
		struct intel_vgpu *vgpu, unsigned long gfn)
{
	return radix_tree_lookup(&vgpu->page_track_tree, gfn);
}

 
int intel_vgpu_register_page_track(struct intel_vgpu *vgpu, unsigned long gfn,
		gvt_page_track_handler_t handler, void *priv)
{
	struct intel_vgpu_page_track *track;
	int ret;

	track = intel_vgpu_find_page_track(vgpu, gfn);
	if (track)
		return -EEXIST;

	track = kzalloc(sizeof(*track), GFP_KERNEL);
	if (!track)
		return -ENOMEM;

	track->handler = handler;
	track->priv_data = priv;

	ret = radix_tree_insert(&vgpu->page_track_tree, gfn, track);
	if (ret) {
		kfree(track);
		return ret;
	}

	return 0;
}

 
void intel_vgpu_unregister_page_track(struct intel_vgpu *vgpu,
		unsigned long gfn)
{
	struct intel_vgpu_page_track *track;

	track = radix_tree_delete(&vgpu->page_track_tree, gfn);
	if (track) {
		if (track->tracked)
			intel_gvt_page_track_remove(vgpu, gfn);
		kfree(track);
	}
}

 
int intel_vgpu_enable_page_track(struct intel_vgpu *vgpu, unsigned long gfn)
{
	struct intel_vgpu_page_track *track;
	int ret;

	track = intel_vgpu_find_page_track(vgpu, gfn);
	if (!track)
		return -ENXIO;

	if (track->tracked)
		return 0;

	ret = intel_gvt_page_track_add(vgpu, gfn);
	if (ret)
		return ret;
	track->tracked = true;
	return 0;
}

 
int intel_vgpu_disable_page_track(struct intel_vgpu *vgpu, unsigned long gfn)
{
	struct intel_vgpu_page_track *track;
	int ret;

	track = intel_vgpu_find_page_track(vgpu, gfn);
	if (!track)
		return -ENXIO;

	if (!track->tracked)
		return 0;

	ret = intel_gvt_page_track_remove(vgpu, gfn);
	if (ret)
		return ret;
	track->tracked = false;
	return 0;
}

 
int intel_vgpu_page_track_handler(struct intel_vgpu *vgpu, u64 gpa,
		void *data, unsigned int bytes)
{
	struct intel_vgpu_page_track *page_track;
	int ret = 0;

	page_track = intel_vgpu_find_page_track(vgpu, gpa >> PAGE_SHIFT);
	if (!page_track)
		return -ENXIO;

	if (unlikely(vgpu->failsafe)) {
		 
		intel_gvt_page_track_remove(vgpu, gpa >> PAGE_SHIFT);
	} else {
		ret = page_track->handler(page_track, gpa, data, bytes);
		if (ret)
			gvt_err("guest page write error, gpa %llx\n", gpa);
	}

	return ret;
}
