 

#include "i915_drv.h"
#include "gvt.h"
#include "i915_pvinfo.h"

void populate_pvinfo_page(struct intel_vgpu *vgpu)
{
	struct drm_i915_private *i915 = vgpu->gvt->gt->i915;
	 
	vgpu_vreg64_t(vgpu, vgtif_reg(magic)) = VGT_MAGIC;
	vgpu_vreg_t(vgpu, vgtif_reg(version_major)) = 1;
	vgpu_vreg_t(vgpu, vgtif_reg(version_minor)) = 0;
	vgpu_vreg_t(vgpu, vgtif_reg(display_ready)) = 0;
	vgpu_vreg_t(vgpu, vgtif_reg(vgt_id)) = vgpu->id;

	vgpu_vreg_t(vgpu, vgtif_reg(vgt_caps)) = VGT_CAPS_FULL_PPGTT;
	vgpu_vreg_t(vgpu, vgtif_reg(vgt_caps)) |= VGT_CAPS_HWSP_EMULATION;
	vgpu_vreg_t(vgpu, vgtif_reg(vgt_caps)) |= VGT_CAPS_HUGE_GTT;

	vgpu_vreg_t(vgpu, vgtif_reg(avail_rs.mappable_gmadr.base)) =
		vgpu_aperture_gmadr_base(vgpu);
	vgpu_vreg_t(vgpu, vgtif_reg(avail_rs.mappable_gmadr.size)) =
		vgpu_aperture_sz(vgpu);
	vgpu_vreg_t(vgpu, vgtif_reg(avail_rs.nonmappable_gmadr.base)) =
		vgpu_hidden_gmadr_base(vgpu);
	vgpu_vreg_t(vgpu, vgtif_reg(avail_rs.nonmappable_gmadr.size)) =
		vgpu_hidden_sz(vgpu);

	vgpu_vreg_t(vgpu, vgtif_reg(avail_rs.fence_num)) = vgpu_fence_sz(vgpu);

	vgpu_vreg_t(vgpu, vgtif_reg(cursor_x_hot)) = UINT_MAX;
	vgpu_vreg_t(vgpu, vgtif_reg(cursor_y_hot)) = UINT_MAX;

	gvt_dbg_core("Populate PVINFO PAGE for vGPU %d\n", vgpu->id);
	gvt_dbg_core("aperture base [GMADR] 0x%llx size 0x%llx\n",
		vgpu_aperture_gmadr_base(vgpu), vgpu_aperture_sz(vgpu));
	gvt_dbg_core("hidden base [GMADR] 0x%llx size=0x%llx\n",
		vgpu_hidden_gmadr_base(vgpu), vgpu_hidden_sz(vgpu));
	gvt_dbg_core("fence size %d\n", vgpu_fence_sz(vgpu));

	drm_WARN_ON(&i915->drm, sizeof(struct vgt_if) != VGT_PVINFO_SIZE);
}

 
#define VGPU_MAX_WEIGHT 16
#define VGPU_WEIGHT(vgpu_num)	\
	(VGPU_MAX_WEIGHT / (vgpu_num))

static const struct intel_vgpu_config intel_vgpu_configs[] = {
	{ MB_TO_BYTES(64), MB_TO_BYTES(384), 4, VGPU_WEIGHT(8), GVT_EDID_1024_768, "8" },
	{ MB_TO_BYTES(128), MB_TO_BYTES(512), 4, VGPU_WEIGHT(4), GVT_EDID_1920_1200, "4" },
	{ MB_TO_BYTES(256), MB_TO_BYTES(1024), 4, VGPU_WEIGHT(2), GVT_EDID_1920_1200, "2" },
	{ MB_TO_BYTES(512), MB_TO_BYTES(2048), 4, VGPU_WEIGHT(1), GVT_EDID_1920_1200, "1" },
};

 
int intel_gvt_init_vgpu_types(struct intel_gvt *gvt)
{
	unsigned int low_avail = gvt_aperture_sz(gvt) - HOST_LOW_GM_SIZE;
	unsigned int high_avail = gvt_hidden_sz(gvt) - HOST_HIGH_GM_SIZE;
	unsigned int num_types = ARRAY_SIZE(intel_vgpu_configs);
	unsigned int i;

	gvt->types = kcalloc(num_types, sizeof(struct intel_vgpu_type),
			     GFP_KERNEL);
	if (!gvt->types)
		return -ENOMEM;

	gvt->mdev_types = kcalloc(num_types, sizeof(*gvt->mdev_types),
			     GFP_KERNEL);
	if (!gvt->mdev_types)
		goto out_free_types;

	for (i = 0; i < num_types; ++i) {
		const struct intel_vgpu_config *conf = &intel_vgpu_configs[i];

		if (low_avail / conf->low_mm == 0)
			break;
		if (conf->weight < 1 || conf->weight > VGPU_MAX_WEIGHT)
			goto out_free_mdev_types;

		sprintf(gvt->types[i].name, "GVTg_V%u_%s",
			GRAPHICS_VER(gvt->gt->i915) == 8 ? 4 : 5, conf->name);
		gvt->types[i].conf = conf;

		gvt_dbg_core("type[%d]: %s avail %u low %u high %u fence %u weight %u res %s\n",
			     i, gvt->types[i].name,
			     min(low_avail / conf->low_mm,
				 high_avail / conf->high_mm),
			     conf->low_mm, conf->high_mm, conf->fence,
			     conf->weight, vgpu_edid_str(conf->edid));

		gvt->mdev_types[i] = &gvt->types[i].type;
		gvt->mdev_types[i]->sysfs_name = gvt->types[i].name;
	}

	gvt->num_types = i;
	return 0;

out_free_mdev_types:
	kfree(gvt->mdev_types);
out_free_types:
	kfree(gvt->types);
	return -EINVAL;
}

void intel_gvt_clean_vgpu_types(struct intel_gvt *gvt)
{
	kfree(gvt->mdev_types);
	kfree(gvt->types);
}

 
void intel_gvt_activate_vgpu(struct intel_vgpu *vgpu)
{
	set_bit(INTEL_VGPU_STATUS_ACTIVE, vgpu->status);
}

 
void intel_gvt_deactivate_vgpu(struct intel_vgpu *vgpu)
{
	mutex_lock(&vgpu->vgpu_lock);

	clear_bit(INTEL_VGPU_STATUS_ACTIVE, vgpu->status);

	if (atomic_read(&vgpu->submission.running_workload_num)) {
		mutex_unlock(&vgpu->vgpu_lock);
		intel_gvt_wait_vgpu_idle(vgpu);
		mutex_lock(&vgpu->vgpu_lock);
	}

	intel_vgpu_stop_schedule(vgpu);

	mutex_unlock(&vgpu->vgpu_lock);
}

 
void intel_gvt_release_vgpu(struct intel_vgpu *vgpu)
{
	intel_gvt_deactivate_vgpu(vgpu);

	mutex_lock(&vgpu->vgpu_lock);
	vgpu->d3_entered = false;
	intel_vgpu_clean_workloads(vgpu, ALL_ENGINES);
	intel_vgpu_dmabuf_cleanup(vgpu);
	mutex_unlock(&vgpu->vgpu_lock);
}

 
void intel_gvt_destroy_vgpu(struct intel_vgpu *vgpu)
{
	struct intel_gvt *gvt = vgpu->gvt;
	struct drm_i915_private *i915 = gvt->gt->i915;

	drm_WARN(&i915->drm, test_bit(INTEL_VGPU_STATUS_ACTIVE, vgpu->status),
		 "vGPU is still active!\n");

	 
	mutex_lock(&gvt->lock);
	idr_remove(&gvt->vgpu_idr, vgpu->id);
	mutex_unlock(&gvt->lock);

	mutex_lock(&vgpu->vgpu_lock);
	intel_gvt_debugfs_remove_vgpu(vgpu);
	intel_vgpu_clean_sched_policy(vgpu);
	intel_vgpu_clean_submission(vgpu);
	intel_vgpu_clean_display(vgpu);
	intel_vgpu_clean_opregion(vgpu);
	intel_vgpu_reset_ggtt(vgpu, true);
	intel_vgpu_clean_gtt(vgpu);
	intel_vgpu_detach_regions(vgpu);
	intel_vgpu_free_resource(vgpu);
	intel_vgpu_clean_mmio(vgpu);
	intel_vgpu_dmabuf_cleanup(vgpu);
	mutex_unlock(&vgpu->vgpu_lock);
}

#define IDLE_VGPU_IDR 0

 
struct intel_vgpu *intel_gvt_create_idle_vgpu(struct intel_gvt *gvt)
{
	struct intel_vgpu *vgpu;
	enum intel_engine_id i;
	int ret;

	vgpu = vzalloc(sizeof(*vgpu));
	if (!vgpu)
		return ERR_PTR(-ENOMEM);

	vgpu->id = IDLE_VGPU_IDR;
	vgpu->gvt = gvt;
	mutex_init(&vgpu->vgpu_lock);

	for (i = 0; i < I915_NUM_ENGINES; i++)
		INIT_LIST_HEAD(&vgpu->submission.workload_q_head[i]);

	ret = intel_vgpu_init_sched_policy(vgpu);
	if (ret)
		goto out_free_vgpu;

	clear_bit(INTEL_VGPU_STATUS_ACTIVE, vgpu->status);
	return vgpu;

out_free_vgpu:
	vfree(vgpu);
	return ERR_PTR(ret);
}

 
void intel_gvt_destroy_idle_vgpu(struct intel_vgpu *vgpu)
{
	mutex_lock(&vgpu->vgpu_lock);
	intel_vgpu_clean_sched_policy(vgpu);
	mutex_unlock(&vgpu->vgpu_lock);

	vfree(vgpu);
}

int intel_gvt_create_vgpu(struct intel_vgpu *vgpu,
		const struct intel_vgpu_config *conf)
{
	struct intel_gvt *gvt = vgpu->gvt;
	struct drm_i915_private *dev_priv = gvt->gt->i915;
	int ret;

	gvt_dbg_core("low %u MB high %u MB fence %u\n",
			BYTES_TO_MB(conf->low_mm), BYTES_TO_MB(conf->high_mm),
			conf->fence);

	mutex_lock(&gvt->lock);
	ret = idr_alloc(&gvt->vgpu_idr, vgpu, IDLE_VGPU_IDR + 1, GVT_MAX_VGPU,
		GFP_KERNEL);
	if (ret < 0)
		goto out_unlock;

	vgpu->id = ret;
	vgpu->sched_ctl.weight = conf->weight;
	mutex_init(&vgpu->vgpu_lock);
	mutex_init(&vgpu->dmabuf_lock);
	INIT_LIST_HEAD(&vgpu->dmabuf_obj_list_head);
	INIT_RADIX_TREE(&vgpu->page_track_tree, GFP_KERNEL);
	idr_init_base(&vgpu->object_idr, 1);
	intel_vgpu_init_cfg_space(vgpu, 1);
	vgpu->d3_entered = false;

	ret = intel_vgpu_init_mmio(vgpu);
	if (ret)
		goto out_clean_idr;

	ret = intel_vgpu_alloc_resource(vgpu, conf);
	if (ret)
		goto out_clean_vgpu_mmio;

	populate_pvinfo_page(vgpu);

	ret = intel_vgpu_init_gtt(vgpu);
	if (ret)
		goto out_clean_vgpu_resource;

	ret = intel_vgpu_init_opregion(vgpu);
	if (ret)
		goto out_clean_gtt;

	ret = intel_vgpu_init_display(vgpu, conf->edid);
	if (ret)
		goto out_clean_opregion;

	ret = intel_vgpu_setup_submission(vgpu);
	if (ret)
		goto out_clean_display;

	ret = intel_vgpu_init_sched_policy(vgpu);
	if (ret)
		goto out_clean_submission;

	intel_gvt_debugfs_add_vgpu(vgpu);

	ret = intel_gvt_set_opregion(vgpu);
	if (ret)
		goto out_clean_sched_policy;

	if (IS_BROADWELL(dev_priv) || IS_BROXTON(dev_priv))
		ret = intel_gvt_set_edid(vgpu, PORT_B);
	else
		ret = intel_gvt_set_edid(vgpu, PORT_D);
	if (ret)
		goto out_clean_sched_policy;

	intel_gvt_update_reg_whitelist(vgpu);
	mutex_unlock(&gvt->lock);
	return 0;

out_clean_sched_policy:
	intel_vgpu_clean_sched_policy(vgpu);
out_clean_submission:
	intel_vgpu_clean_submission(vgpu);
out_clean_display:
	intel_vgpu_clean_display(vgpu);
out_clean_opregion:
	intel_vgpu_clean_opregion(vgpu);
out_clean_gtt:
	intel_vgpu_clean_gtt(vgpu);
out_clean_vgpu_resource:
	intel_vgpu_free_resource(vgpu);
out_clean_vgpu_mmio:
	intel_vgpu_clean_mmio(vgpu);
out_clean_idr:
	idr_remove(&gvt->vgpu_idr, vgpu->id);
out_unlock:
	mutex_unlock(&gvt->lock);
	return ret;
}

 
void intel_gvt_reset_vgpu_locked(struct intel_vgpu *vgpu, bool dmlr,
				 intel_engine_mask_t engine_mask)
{
	struct intel_gvt *gvt = vgpu->gvt;
	struct intel_gvt_workload_scheduler *scheduler = &gvt->scheduler;
	intel_engine_mask_t resetting_eng = dmlr ? ALL_ENGINES : engine_mask;

	gvt_dbg_core("------------------------------------------\n");
	gvt_dbg_core("resseting vgpu%d, dmlr %d, engine_mask %08x\n",
		     vgpu->id, dmlr, engine_mask);

	vgpu->resetting_eng = resetting_eng;

	intel_vgpu_stop_schedule(vgpu);
	 
	if (scheduler->current_vgpu == NULL) {
		mutex_unlock(&vgpu->vgpu_lock);
		intel_gvt_wait_vgpu_idle(vgpu);
		mutex_lock(&vgpu->vgpu_lock);
	}

	intel_vgpu_reset_submission(vgpu, resetting_eng);
	 
	if (engine_mask == ALL_ENGINES || dmlr) {
		intel_vgpu_select_submission_ops(vgpu, ALL_ENGINES, 0);
		if (engine_mask == ALL_ENGINES)
			intel_vgpu_invalidate_ppgtt(vgpu);
		 
		if (dmlr) {
			if(!vgpu->d3_entered) {
				intel_vgpu_invalidate_ppgtt(vgpu);
				intel_vgpu_destroy_all_ppgtt_mm(vgpu);
			}
			intel_vgpu_reset_ggtt(vgpu, true);
			intel_vgpu_reset_resource(vgpu);
		}

		intel_vgpu_reset_mmio(vgpu, dmlr);
		populate_pvinfo_page(vgpu);

		if (dmlr) {
			intel_vgpu_reset_display(vgpu);
			intel_vgpu_reset_cfg_space(vgpu);
			 
			vgpu->failsafe = false;
			 
			if(vgpu->d3_entered)
				vgpu->d3_entered = false;
			else
				vgpu->pv_notified = false;
		}
	}

	vgpu->resetting_eng = 0;
	gvt_dbg_core("reset vgpu%d done\n", vgpu->id);
	gvt_dbg_core("------------------------------------------\n");
}

 
void intel_gvt_reset_vgpu(struct intel_vgpu *vgpu)
{
	mutex_lock(&vgpu->vgpu_lock);
	intel_gvt_reset_vgpu_locked(vgpu, true, 0);
	mutex_unlock(&vgpu->vgpu_lock);
}
