
 

#include "i915_drv.h"

#include "intel_gt_mcr.h"
#include "intel_gt_print.h"
#include "intel_gt_regs.h"

 

#define HAS_MSLICE_STEERING(i915)	(INTEL_INFO(i915)->has_mslice_steering)

static const char * const intel_steering_types[] = {
	"L3BANK",
	"MSLICE",
	"LNCF",
	"GAM",
	"DSS",
	"OADDRM",
	"INSTANCE 0",
};

static const struct intel_mmio_range icl_l3bank_steering_table[] = {
	{ 0x00B100, 0x00B3FF },
	{},
};

 
static const struct intel_mmio_range xehpsdv_mslice_steering_table[] = {
	{ 0x00DD00, 0x00DDFF },
	{ 0x00E900, 0x00FFFF },  
	{},
};

static const struct intel_mmio_range xehpsdv_gam_steering_table[] = {
	{ 0x004000, 0x004AFF },
	{ 0x00C800, 0x00CFFF },
	{},
};

static const struct intel_mmio_range xehpsdv_lncf_steering_table[] = {
	{ 0x00B000, 0x00B0FF },
	{ 0x00D800, 0x00D8FF },
	{},
};

static const struct intel_mmio_range dg2_lncf_steering_table[] = {
	{ 0x00B000, 0x00B0FF },
	{ 0x00D880, 0x00D8FF },
	{},
};

 
static const struct intel_mmio_range pvc_instance0_steering_table[] = {
	{ 0x004000, 0x004AFF },		 
	{ 0x008800, 0x00887F },		 
	{ 0x008A80, 0x008AFF },		 
	{ 0x00B000, 0x00B0FF },		 
	{ 0x00B100, 0x00B3FF },		 
	{ 0x00C800, 0x00CFFF },		 
	{ 0x00D800, 0x00D8FF },		 
	{ 0x00DD00, 0x00DDFF },		 
	{ 0x00E900, 0x00E9FF },		 
	{ 0x00EC00, 0x00EEFF },		 
	{ 0x00F000, 0x00FFFF },		 
	{ 0x024180, 0x0241FF },		 
	{},
};

static const struct intel_mmio_range xelpg_instance0_steering_table[] = {
	{ 0x000B00, 0x000BFF },          
	{ 0x001000, 0x001FFF },          
	{ 0x004000, 0x0048FF },          
	{ 0x008700, 0x0087FF },          
	{ 0x00B000, 0x00B0FF },          
	{ 0x00C800, 0x00CFFF },          
	{ 0x00D880, 0x00D8FF },          
	{ 0x00DD00, 0x00DDFF },          
	{},
};

static const struct intel_mmio_range xelpg_l3bank_steering_table[] = {
	{ 0x00B100, 0x00B3FF },
	{},
};

 
static const struct intel_mmio_range xelpg_dss_steering_table[] = {
	{ 0x005200, 0x0052FF },		 
	{ 0x005500, 0x007FFF },		 
	{ 0x008140, 0x00815F },		 
	{ 0x0094D0, 0x00955F },		 
	{ 0x009680, 0x0096FF },		 
	{ 0x00D800, 0x00D87F },		 
	{ 0x00DC00, 0x00DCFF },		 
	{ 0x00DE80, 0x00E8FF },		 
	{},
};

static const struct intel_mmio_range xelpmp_oaddrm_steering_table[] = {
	{ 0x393200, 0x39323F },
	{ 0x393400, 0x3934FF },
	{},
};

void intel_gt_mcr_init(struct intel_gt *gt)
{
	struct drm_i915_private *i915 = gt->i915;
	unsigned long fuse;
	int i;

	spin_lock_init(&gt->mcr_lock);

	 
	if (HAS_MSLICE_STEERING(i915)) {
		gt->info.mslice_mask =
			intel_slicemask_from_xehp_dssmask(gt->info.sseu.subslice_mask,
							  GEN_DSS_PER_MSLICE);
		gt->info.mslice_mask |=
			(intel_uncore_read(gt->uncore, GEN10_MIRROR_FUSE3) &
			 GEN12_MEML3_EN_MASK);

		if (!gt->info.mslice_mask)  
			gt_warn(gt, "mslice mask all zero!\n");
	}

	if (MEDIA_VER(i915) >= 13 && gt->type == GT_MEDIA) {
		gt->steering_table[OADDRM] = xelpmp_oaddrm_steering_table;
	} else if (GRAPHICS_VER_FULL(i915) >= IP_VER(12, 70)) {
		 
		if (IS_MTL_GRAPHICS_STEP(i915, M, STEP_A0, STEP_B0) ||
		    IS_MTL_GRAPHICS_STEP(i915, P, STEP_A0, STEP_B0))
			fuse = REG_FIELD_GET(MTL_GT_L3_EXC_MASK,
					     intel_uncore_read(gt->uncore,
							       MTL_GT_ACTIVITY_FACTOR));
		else
			fuse = REG_FIELD_GET(GT_L3_EXC_MASK,
					     intel_uncore_read(gt->uncore, XEHP_FUSE4));

		 
		for_each_set_bit(i, &fuse, 3)
			gt->info.l3bank_mask |= 0x3 << 2 * i;

		gt->steering_table[INSTANCE0] = xelpg_instance0_steering_table;
		gt->steering_table[L3BANK] = xelpg_l3bank_steering_table;
		gt->steering_table[DSS] = xelpg_dss_steering_table;
	} else if (IS_PONTEVECCHIO(i915)) {
		gt->steering_table[INSTANCE0] = pvc_instance0_steering_table;
	} else if (IS_DG2(i915)) {
		gt->steering_table[MSLICE] = xehpsdv_mslice_steering_table;
		gt->steering_table[LNCF] = dg2_lncf_steering_table;
		 
	} else if (IS_XEHPSDV(i915)) {
		gt->steering_table[MSLICE] = xehpsdv_mslice_steering_table;
		gt->steering_table[LNCF] = xehpsdv_lncf_steering_table;
		gt->steering_table[GAM] = xehpsdv_gam_steering_table;
	} else if (GRAPHICS_VER(i915) >= 11 &&
		   GRAPHICS_VER_FULL(i915) < IP_VER(12, 50)) {
		gt->steering_table[L3BANK] = icl_l3bank_steering_table;
		gt->info.l3bank_mask =
			~intel_uncore_read(gt->uncore, GEN10_MIRROR_FUSE3) &
			GEN10_L3BANK_MASK;
		if (!gt->info.l3bank_mask)  
			gt_warn(gt, "L3 bank mask is all zero!\n");
	} else if (GRAPHICS_VER(i915) >= 11) {
		 
		MISSING_CASE(INTEL_INFO(i915)->platform);
	}
}

 
static i915_reg_t mcr_reg_cast(const i915_mcr_reg_t mcr)
{
	i915_reg_t r = { .reg = mcr.reg };

	return r;
}

 
static u32 rw_with_mcr_steering_fw(struct intel_gt *gt,
				   i915_mcr_reg_t reg, u8 rw_flag,
				   int group, int instance, u32 value)
{
	struct intel_uncore *uncore = gt->uncore;
	u32 mcr_mask, mcr_ss, mcr, old_mcr, val = 0;

	lockdep_assert_held(&gt->mcr_lock);

	if (GRAPHICS_VER_FULL(uncore->i915) >= IP_VER(12, 70)) {
		 
		intel_uncore_write_fw(uncore, MTL_MCR_SELECTOR,
				      REG_FIELD_PREP(MTL_MCR_GROUPID, group) |
				      REG_FIELD_PREP(MTL_MCR_INSTANCEID, instance) |
				      (rw_flag == FW_REG_READ ? GEN11_MCR_MULTICAST : 0));
	} else if (GRAPHICS_VER(uncore->i915) >= 11) {
		mcr_mask = GEN11_MCR_SLICE_MASK | GEN11_MCR_SUBSLICE_MASK;
		mcr_ss = GEN11_MCR_SLICE(group) | GEN11_MCR_SUBSLICE(instance);

		 
		if (rw_flag == FW_REG_WRITE)
			mcr_mask |= GEN11_MCR_MULTICAST;

		mcr = intel_uncore_read_fw(uncore, GEN8_MCR_SELECTOR);
		old_mcr = mcr;

		mcr &= ~mcr_mask;
		mcr |= mcr_ss;
		intel_uncore_write_fw(uncore, GEN8_MCR_SELECTOR, mcr);
	} else {
		mcr_mask = GEN8_MCR_SLICE_MASK | GEN8_MCR_SUBSLICE_MASK;
		mcr_ss = GEN8_MCR_SLICE(group) | GEN8_MCR_SUBSLICE(instance);

		mcr = intel_uncore_read_fw(uncore, GEN8_MCR_SELECTOR);
		old_mcr = mcr;

		mcr &= ~mcr_mask;
		mcr |= mcr_ss;
		intel_uncore_write_fw(uncore, GEN8_MCR_SELECTOR, mcr);
	}

	if (rw_flag == FW_REG_READ)
		val = intel_uncore_read_fw(uncore, mcr_reg_cast(reg));
	else
		intel_uncore_write_fw(uncore, mcr_reg_cast(reg), value);

	 
	if (GRAPHICS_VER_FULL(uncore->i915) >= IP_VER(12, 70) && rw_flag == FW_REG_WRITE)
		intel_uncore_write_fw(uncore, MTL_MCR_SELECTOR, GEN11_MCR_MULTICAST);
	else if (GRAPHICS_VER_FULL(uncore->i915) < IP_VER(12, 70))
		intel_uncore_write_fw(uncore, GEN8_MCR_SELECTOR, old_mcr);

	return val;
}

static u32 rw_with_mcr_steering(struct intel_gt *gt,
				i915_mcr_reg_t reg, u8 rw_flag,
				int group, int instance,
				u32 value)
{
	struct intel_uncore *uncore = gt->uncore;
	enum forcewake_domains fw_domains;
	unsigned long flags;
	u32 val;

	fw_domains = intel_uncore_forcewake_for_reg(uncore, mcr_reg_cast(reg),
						    rw_flag);
	fw_domains |= intel_uncore_forcewake_for_reg(uncore,
						     GEN8_MCR_SELECTOR,
						     FW_REG_READ | FW_REG_WRITE);

	intel_gt_mcr_lock(gt, &flags);
	spin_lock(&uncore->lock);
	intel_uncore_forcewake_get__locked(uncore, fw_domains);

	val = rw_with_mcr_steering_fw(gt, reg, rw_flag, group, instance, value);

	intel_uncore_forcewake_put__locked(uncore, fw_domains);
	spin_unlock(&uncore->lock);
	intel_gt_mcr_unlock(gt, flags);

	return val;
}

 
void intel_gt_mcr_lock(struct intel_gt *gt, unsigned long *flags)
	__acquires(&gt->mcr_lock)
{
	unsigned long __flags;
	int err = 0;

	lockdep_assert_not_held(&gt->uncore->lock);

	 
	if (GRAPHICS_VER_FULL(gt->i915) >= IP_VER(12, 70)) {
		 
		intel_uncore_forcewake_get(gt->uncore, FORCEWAKE_GT);

		err = wait_for(intel_uncore_read_fw(gt->uncore,
						    MTL_STEER_SEMAPHORE) == 0x1, 100);
	}

	 
	spin_lock_irqsave(&gt->mcr_lock, __flags);

	*flags = __flags;

	 
	if (err == -ETIMEDOUT) {
		gt_err_ratelimited(gt, "hardware MCR steering semaphore timed out");
		add_taint_for_CI(gt->i915, TAINT_WARN);   
	}
}

 
void intel_gt_mcr_unlock(struct intel_gt *gt, unsigned long flags)
	__releases(&gt->mcr_lock)
{
	spin_unlock_irqrestore(&gt->mcr_lock, flags);

	if (GRAPHICS_VER_FULL(gt->i915) >= IP_VER(12, 70)) {
		intel_uncore_write_fw(gt->uncore, MTL_STEER_SEMAPHORE, 0x1);

		intel_uncore_forcewake_put(gt->uncore, FORCEWAKE_GT);
	}
}

 
u32 intel_gt_mcr_read(struct intel_gt *gt,
		      i915_mcr_reg_t reg,
		      int group, int instance)
{
	return rw_with_mcr_steering(gt, reg, FW_REG_READ, group, instance, 0);
}

 
void intel_gt_mcr_unicast_write(struct intel_gt *gt, i915_mcr_reg_t reg, u32 value,
				int group, int instance)
{
	rw_with_mcr_steering(gt, reg, FW_REG_WRITE, group, instance, value);
}

 
void intel_gt_mcr_multicast_write(struct intel_gt *gt,
				  i915_mcr_reg_t reg, u32 value)
{
	unsigned long flags;

	intel_gt_mcr_lock(gt, &flags);

	 
	if (GRAPHICS_VER_FULL(gt->i915) >= IP_VER(12, 70))
		intel_uncore_write_fw(gt->uncore, MTL_MCR_SELECTOR, GEN11_MCR_MULTICAST);

	intel_uncore_write(gt->uncore, mcr_reg_cast(reg), value);

	intel_gt_mcr_unlock(gt, flags);
}

 
void intel_gt_mcr_multicast_write_fw(struct intel_gt *gt, i915_mcr_reg_t reg, u32 value)
{
	lockdep_assert_held(&gt->mcr_lock);

	 
	if (GRAPHICS_VER_FULL(gt->i915) >= IP_VER(12, 70))
		intel_uncore_write_fw(gt->uncore, MTL_MCR_SELECTOR, GEN11_MCR_MULTICAST);

	intel_uncore_write_fw(gt->uncore, mcr_reg_cast(reg), value);
}

 
u32 intel_gt_mcr_multicast_rmw(struct intel_gt *gt, i915_mcr_reg_t reg,
			       u32 clear, u32 set)
{
	u32 val = intel_gt_mcr_read_any(gt, reg);

	intel_gt_mcr_multicast_write(gt, reg, (val & ~clear) | set);

	return val;
}

 
static bool reg_needs_read_steering(struct intel_gt *gt,
				    i915_mcr_reg_t reg,
				    enum intel_steering_type type)
{
	u32 offset = i915_mmio_reg_offset(reg);
	const struct intel_mmio_range *entry;

	if (likely(!gt->steering_table[type]))
		return false;

	if (IS_GSI_REG(offset))
		offset += gt->uncore->gsi_offset;

	for (entry = gt->steering_table[type]; entry->end; entry++) {
		if (offset >= entry->start && offset <= entry->end)
			return true;
	}

	return false;
}

 
static void get_nonterminated_steering(struct intel_gt *gt,
				       enum intel_steering_type type,
				       u8 *group, u8 *instance)
{
	u32 dss;

	switch (type) {
	case L3BANK:
		*group = 0;		 
		*instance = __ffs(gt->info.l3bank_mask);
		break;
	case MSLICE:
		GEM_WARN_ON(!HAS_MSLICE_STEERING(gt->i915));
		*group = __ffs(gt->info.mslice_mask);
		*instance = 0;	 
		break;
	case LNCF:
		 
		GEM_WARN_ON(!HAS_MSLICE_STEERING(gt->i915));
		*group = __ffs(gt->info.mslice_mask) << 1;
		*instance = 0;	 
		break;
	case GAM:
		*group = IS_DG2(gt->i915) ? 1 : 0;
		*instance = 0;
		break;
	case DSS:
		dss = intel_sseu_find_first_xehp_dss(&gt->info.sseu, 0, 0);
		*group = dss / GEN_DSS_PER_GSLICE;
		*instance = dss % GEN_DSS_PER_GSLICE;
		break;
	case INSTANCE0:
		 
		*group = 0;
		*instance = 0;
		break;
	case OADDRM:
		if ((VDBOX_MASK(gt) | VEBOX_MASK(gt) | gt->info.sfc_mask) & BIT(0))
			*group = 0;
		else
			*group = 1;
		*instance = 0;
		break;
	default:
		MISSING_CASE(type);
		*group = 0;
		*instance = 0;
	}
}

 
void intel_gt_mcr_get_nonterminated_steering(struct intel_gt *gt,
					     i915_mcr_reg_t reg,
					     u8 *group, u8 *instance)
{
	int type;

	for (type = 0; type < NUM_STEERING_TYPES; type++) {
		if (reg_needs_read_steering(gt, reg, type)) {
			get_nonterminated_steering(gt, type, group, instance);
			return;
		}
	}

	*group = gt->default_steering.groupid;
	*instance = gt->default_steering.instanceid;
}

 
u32 intel_gt_mcr_read_any_fw(struct intel_gt *gt, i915_mcr_reg_t reg)
{
	int type;
	u8 group, instance;

	lockdep_assert_held(&gt->mcr_lock);

	for (type = 0; type < NUM_STEERING_TYPES; type++) {
		if (reg_needs_read_steering(gt, reg, type)) {
			get_nonterminated_steering(gt, type, &group, &instance);
			return rw_with_mcr_steering_fw(gt, reg,
						       FW_REG_READ,
						       group, instance, 0);
		}
	}

	return intel_uncore_read_fw(gt->uncore, mcr_reg_cast(reg));
}

 
u32 intel_gt_mcr_read_any(struct intel_gt *gt, i915_mcr_reg_t reg)
{
	int type;
	u8 group, instance;

	for (type = 0; type < NUM_STEERING_TYPES; type++) {
		if (reg_needs_read_steering(gt, reg, type)) {
			get_nonterminated_steering(gt, type, &group, &instance);
			return rw_with_mcr_steering(gt, reg,
						    FW_REG_READ,
						    group, instance, 0);
		}
	}

	return intel_uncore_read(gt->uncore, mcr_reg_cast(reg));
}

static void report_steering_type(struct drm_printer *p,
				 struct intel_gt *gt,
				 enum intel_steering_type type,
				 bool dump_table)
{
	const struct intel_mmio_range *entry;
	u8 group, instance;

	BUILD_BUG_ON(ARRAY_SIZE(intel_steering_types) != NUM_STEERING_TYPES);

	if (!gt->steering_table[type]) {
		drm_printf(p, "%s steering: uses default steering\n",
			   intel_steering_types[type]);
		return;
	}

	get_nonterminated_steering(gt, type, &group, &instance);
	drm_printf(p, "%s steering: group=0x%x, instance=0x%x\n",
		   intel_steering_types[type], group, instance);

	if (!dump_table)
		return;

	for (entry = gt->steering_table[type]; entry->end; entry++)
		drm_printf(p, "\t0x%06x - 0x%06x\n", entry->start, entry->end);
}

void intel_gt_mcr_report_steering(struct drm_printer *p, struct intel_gt *gt,
				  bool dump_table)
{
	 
	if (GRAPHICS_VER_FULL(gt->i915) < IP_VER(12, 70))
		drm_printf(p, "Default steering: group=0x%x, instance=0x%x\n",
			   gt->default_steering.groupid,
			   gt->default_steering.instanceid);

	if (GRAPHICS_VER_FULL(gt->i915) >= IP_VER(12, 70)) {
		for (int i = 0; i < NUM_STEERING_TYPES; i++)
			if (gt->steering_table[i])
				report_steering_type(p, gt, i, dump_table);
	} else if (IS_PONTEVECCHIO(gt->i915)) {
		report_steering_type(p, gt, INSTANCE0, dump_table);
	} else if (HAS_MSLICE_STEERING(gt->i915)) {
		report_steering_type(p, gt, MSLICE, dump_table);
		report_steering_type(p, gt, LNCF, dump_table);
	}
}

 
void intel_gt_mcr_get_ss_steering(struct intel_gt *gt, unsigned int dss,
				   unsigned int *group, unsigned int *instance)
{
	if (IS_PONTEVECCHIO(gt->i915)) {
		*group = dss / GEN_DSS_PER_CSLICE;
		*instance = dss % GEN_DSS_PER_CSLICE;
	} else if (GRAPHICS_VER_FULL(gt->i915) >= IP_VER(12, 50)) {
		*group = dss / GEN_DSS_PER_GSLICE;
		*instance = dss % GEN_DSS_PER_GSLICE;
	} else {
		*group = dss / GEN_MAX_SS_PER_HSW_SLICE;
		*instance = dss % GEN_MAX_SS_PER_HSW_SLICE;
		return;
	}
}

 
int intel_gt_mcr_wait_for_reg(struct intel_gt *gt,
			      i915_mcr_reg_t reg,
			      u32 mask,
			      u32 value,
			      unsigned int fast_timeout_us,
			      unsigned int slow_timeout_ms)
{
	int ret;

	lockdep_assert_not_held(&gt->mcr_lock);

#define done ((intel_gt_mcr_read_any(gt, reg) & mask) == value)

	 
	might_sleep_if(slow_timeout_ms);
	GEM_BUG_ON(fast_timeout_us > 20000);
	GEM_BUG_ON(!fast_timeout_us && !slow_timeout_ms);

	ret = -ETIMEDOUT;
	if (fast_timeout_us && fast_timeout_us <= 20000)
		ret = _wait_for_atomic(done, fast_timeout_us, 0);
	if (ret && slow_timeout_ms)
		ret = wait_for(done, slow_timeout_ms);

	return ret;
#undef done
}
