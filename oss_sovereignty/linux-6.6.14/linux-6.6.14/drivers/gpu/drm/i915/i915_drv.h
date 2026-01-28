#ifndef _I915_DRV_H_
#define _I915_DRV_H_
#include <uapi/drm/i915_drm.h>
#include <linux/pm_qos.h>
#include <drm/ttm/ttm_device.h>
#include "display/intel_display_limits.h"
#include "display/intel_display_core.h"
#include "gem/i915_gem_context_types.h"
#include "gem/i915_gem_shrinker.h"
#include "gem/i915_gem_stolen.h"
#include "gt/intel_engine.h"
#include "gt/intel_gt_types.h"
#include "gt/intel_region_lmem.h"
#include "gt/intel_workarounds.h"
#include "gt/uc/intel_uc.h"
#include "soc/intel_pch.h"
#include "i915_drm_client.h"
#include "i915_gem.h"
#include "i915_gpu_error.h"
#include "i915_params.h"
#include "i915_perf_types.h"
#include "i915_scheduler.h"
#include "i915_utils.h"
#include "intel_device_info.h"
#include "intel_memory_region.h"
#include "intel_runtime_pm.h"
#include "intel_step.h"
#include "intel_uncore.h"
struct drm_i915_clock_gating_funcs;
struct vlv_s0ix_state;
struct intel_pxp;
#define GEM_QUIRK_PIN_SWIZZLED_PAGES	BIT(0)
struct i915_dsm {
	struct resource stolen;
	struct resource reserved;
	resource_size_t usable_size;
};
struct i915_suspend_saved_registers {
	u32 saveDSPARB;
	u32 saveSWF0[16];
	u32 saveSWF1[16];
	u32 saveSWF3[3];
	u16 saveGCDGMBUS;
};
#define MAX_L3_SLICES 2
struct intel_l3_parity {
	u32 *remap_info[MAX_L3_SLICES];
	struct work_struct error_work;
	int which_slice;
};
struct i915_gem_mm {
	struct intel_memory_region *stolen_region;
	struct drm_mm stolen;
	struct mutex stolen_lock;
	spinlock_t obj_lock;
	struct list_head purge_list;
	struct list_head shrink_list;
	struct llist_head free_list;
	struct work_struct free_work;
	atomic_t free_count;
	struct vfsmount *gemfs;
	struct intel_memory_region *regions[INTEL_REGION_UNKNOWN];
	struct notifier_block oom_notifier;
	struct notifier_block vmap_notifier;
	struct shrinker shrinker;
#ifdef CONFIG_MMU_NOTIFIER
	rwlock_t notifier_lock;
#endif
	u64 shrink_memory;
	u32 shrink_count;
};
struct i915_virtual_gpu {
	struct mutex lock;  
	bool active;
	u32 caps;
	u32 *initial_mmio;
	u8 *initial_cfg_space;
	struct list_head entry;
};
struct i915_selftest_stash {
	atomic_t counter;
	struct ida mock_region_instances;
};
struct drm_i915_private {
	struct drm_device drm;
	struct intel_display display;
	bool do_release;
	struct i915_params params;
	const struct intel_device_info *__info;  
	struct intel_runtime_info __runtime;  
	struct intel_driver_caps caps;
	struct i915_dsm dsm;
	struct intel_uncore uncore;
	struct intel_uncore_mmio_debug mmio_debug;
	struct i915_virtual_gpu vgpu;
	struct intel_gvt *gvt;
	struct {
		struct pci_dev *pdev;
		struct resource mch_res;
		bool mchbar_need_disable;
	} gmch;
	struct rb_root uabi_engines;
	unsigned int engine_uabi_class_count[I915_LAST_UABI_ENGINE_CLASS + 1];
	spinlock_t irq_lock;
	bool display_irqs_enabled;
	struct mutex sb_lock;
	struct pm_qos_request sb_qos;
	union {
		u32 irq_mask;
		u32 de_irq_mask[I915_MAX_PIPES];
	};
	u32 pipestat_irq_mask[I915_MAX_PIPES];
	bool preserve_bios_swizzle;
	unsigned int fsb_freq, mem_freq, is_ddr3;
	unsigned int skl_preferred_vco_freq;
	unsigned int max_dotclk_freq;
	unsigned int hpll_freq;
	unsigned int czclk_freq;
	struct workqueue_struct *wq;
	struct workqueue_struct *unordered_wq;
	const struct drm_i915_clock_gating_funcs *clock_gating_funcs;
	enum intel_pch pch_type;
	unsigned short pch_id;
	unsigned long gem_quirks;
	struct i915_gem_mm mm;
	struct intel_l3_parity l3_parity;
	u32 edram_size_mb;
	struct i915_gpu_error gpu_error;
	u32 suspend_count;
	struct i915_suspend_saved_registers regfile;
	struct vlv_s0ix_state *vlv_s0ix_state;
	struct dram_info {
		bool wm_lv_0_adjust_needed;
		u8 num_channels;
		bool symmetric_memory;
		enum intel_dram_type {
			INTEL_DRAM_UNKNOWN,
			INTEL_DRAM_DDR3,
			INTEL_DRAM_DDR4,
			INTEL_DRAM_LPDDR3,
			INTEL_DRAM_LPDDR4,
			INTEL_DRAM_DDR5,
			INTEL_DRAM_LPDDR5,
		} type;
		u8 num_qgv_points;
		u8 num_psf_gv_points;
	} dram_info;
	struct intel_runtime_pm runtime_pm;
	struct i915_perf perf;
	struct i915_hwmon *hwmon;
	struct intel_gt gt0;
	struct intel_gt *gt[I915_MAX_GT];
	struct kobject *sysfs_gt;
	struct intel_gt *media_gt;
	struct {
		struct i915_gem_contexts {
			spinlock_t lock;  
			struct list_head list;
		} contexts;
		struct file *mmap_singleton;
	} gem;
	struct intel_pxp *pxp;
	u8 vblank_enabled;
	bool irq_enabled;
	struct i915_pmu pmu;
	struct ttm_device bdev;
	I915_SELFTEST_DECLARE(struct i915_selftest_stash selftest;)
};
static inline struct drm_i915_private *to_i915(const struct drm_device *dev)
{
	return container_of(dev, struct drm_i915_private, drm);
}
static inline struct drm_i915_private *kdev_to_i915(struct device *kdev)
{
	return dev_get_drvdata(kdev);
}
static inline struct drm_i915_private *pdev_to_i915(struct pci_dev *pdev)
{
	return pci_get_drvdata(pdev);
}
static inline struct intel_gt *to_gt(struct drm_i915_private *i915)
{
	return &i915->gt0;
}
#define for_each_engine(engine__, gt__, id__) \
	for ((id__) = 0; \
	     (id__) < I915_NUM_ENGINES; \
	     (id__)++) \
		for_each_if ((engine__) = (gt__)->engine[(id__)])
#define for_each_engine_masked(engine__, gt__, mask__, tmp__) \
	for ((tmp__) = (mask__) & (gt__)->info.engine_mask; \
	     (tmp__) ? \
	     ((engine__) = (gt__)->engine[__mask_next_bit(tmp__)]), 1 : \
	     0;)
#define rb_to_uabi_engine(rb) \
	rb_entry_safe(rb, struct intel_engine_cs, uabi_node)
#define for_each_uabi_engine(engine__, i915__) \
	for ((engine__) = rb_to_uabi_engine(rb_first(&(i915__)->uabi_engines));\
	     (engine__); \
	     (engine__) = rb_to_uabi_engine(rb_next(&(engine__)->uabi_node)))
#define for_each_uabi_class_engine(engine__, class__, i915__) \
	for ((engine__) = intel_engine_lookup_user((i915__), (class__), 0); \
	     (engine__) && (engine__)->uabi_class == (class__); \
	     (engine__) = rb_to_uabi_engine(rb_next(&(engine__)->uabi_node)))
#define INTEL_INFO(i915)	((i915)->__info)
#define RUNTIME_INFO(i915)	(&(i915)->__runtime)
#define DISPLAY_INFO(i915)	((i915)->display.info.__device_info)
#define DISPLAY_RUNTIME_INFO(i915)	(&(i915)->display.info.__runtime_info)
#define DRIVER_CAPS(i915)	(&(i915)->caps)
#define INTEL_DEVID(i915)	(RUNTIME_INFO(i915)->device_id)
#define IP_VER(ver, rel)		((ver) << 8 | (rel))
#define GRAPHICS_VER(i915)		(RUNTIME_INFO(i915)->graphics.ip.ver)
#define GRAPHICS_VER_FULL(i915)		IP_VER(RUNTIME_INFO(i915)->graphics.ip.ver, \
					       RUNTIME_INFO(i915)->graphics.ip.rel)
#define IS_GRAPHICS_VER(i915, from, until) \
	(GRAPHICS_VER(i915) >= (from) && GRAPHICS_VER(i915) <= (until))
#define MEDIA_VER(i915)			(RUNTIME_INFO(i915)->media.ip.ver)
#define MEDIA_VER_FULL(i915)		IP_VER(RUNTIME_INFO(i915)->media.ip.ver, \
					       RUNTIME_INFO(i915)->media.ip.rel)
#define IS_MEDIA_VER(i915, from, until) \
	(MEDIA_VER(i915) >= (from) && MEDIA_VER(i915) <= (until))
#define DISPLAY_VER(i915)	(DISPLAY_RUNTIME_INFO(i915)->ip.ver)
#define IS_DISPLAY_VER(i915, from, until) \
	(DISPLAY_VER(i915) >= (from) && DISPLAY_VER(i915) <= (until))
#define INTEL_REVID(i915)	(to_pci_dev((i915)->drm.dev)->revision)
#define INTEL_DISPLAY_STEP(__i915) (RUNTIME_INFO(__i915)->step.display_step)
#define INTEL_GRAPHICS_STEP(__i915) (RUNTIME_INFO(__i915)->step.graphics_step)
#define INTEL_MEDIA_STEP(__i915) (RUNTIME_INFO(__i915)->step.media_step)
#define INTEL_BASEDIE_STEP(__i915) (RUNTIME_INFO(__i915)->step.basedie_step)
#define IS_DISPLAY_STEP(__i915, since, until) \
	(drm_WARN_ON(&(__i915)->drm, INTEL_DISPLAY_STEP(__i915) == STEP_NONE), \
	 INTEL_DISPLAY_STEP(__i915) >= (since) && INTEL_DISPLAY_STEP(__i915) < (until))
#define IS_GRAPHICS_STEP(__i915, since, until) \
	(drm_WARN_ON(&(__i915)->drm, INTEL_GRAPHICS_STEP(__i915) == STEP_NONE), \
	 INTEL_GRAPHICS_STEP(__i915) >= (since) && INTEL_GRAPHICS_STEP(__i915) < (until))
#define IS_MEDIA_STEP(__i915, since, until) \
	(drm_WARN_ON(&(__i915)->drm, INTEL_MEDIA_STEP(__i915) == STEP_NONE), \
	 INTEL_MEDIA_STEP(__i915) >= (since) && INTEL_MEDIA_STEP(__i915) < (until))
#define IS_BASEDIE_STEP(__i915, since, until) \
	(drm_WARN_ON(&(__i915)->drm, INTEL_BASEDIE_STEP(__i915) == STEP_NONE), \
	 INTEL_BASEDIE_STEP(__i915) >= (since) && INTEL_BASEDIE_STEP(__i915) < (until))
static __always_inline unsigned int
__platform_mask_index(const struct intel_runtime_info *info,
		      enum intel_platform p)
{
	const unsigned int pbits =
		BITS_PER_TYPE(info->platform_mask[0]) - INTEL_SUBPLATFORM_BITS;
	BUILD_BUG_ON(INTEL_MAX_PLATFORMS >
		     pbits * ARRAY_SIZE(info->platform_mask));
	return p / pbits;
}
static __always_inline unsigned int
__platform_mask_bit(const struct intel_runtime_info *info,
		    enum intel_platform p)
{
	const unsigned int pbits =
		BITS_PER_TYPE(info->platform_mask[0]) - INTEL_SUBPLATFORM_BITS;
	return p % pbits + INTEL_SUBPLATFORM_BITS;
}
static inline u32
intel_subplatform(const struct intel_runtime_info *info, enum intel_platform p)
{
	const unsigned int pi = __platform_mask_index(info, p);
	return info->platform_mask[pi] & INTEL_SUBPLATFORM_MASK;
}
static __always_inline bool
IS_PLATFORM(const struct drm_i915_private *i915, enum intel_platform p)
{
	const struct intel_runtime_info *info = RUNTIME_INFO(i915);
	const unsigned int pi = __platform_mask_index(info, p);
	const unsigned int pb = __platform_mask_bit(info, p);
	BUILD_BUG_ON(!__builtin_constant_p(p));
	return info->platform_mask[pi] & BIT(pb);
}
static __always_inline bool
IS_SUBPLATFORM(const struct drm_i915_private *i915,
	       enum intel_platform p, unsigned int s)
{
	const struct intel_runtime_info *info = RUNTIME_INFO(i915);
	const unsigned int pi = __platform_mask_index(info, p);
	const unsigned int pb = __platform_mask_bit(info, p);
	const unsigned int msb = BITS_PER_TYPE(info->platform_mask[0]) - 1;
	const u32 mask = info->platform_mask[pi];
	BUILD_BUG_ON(!__builtin_constant_p(p));
	BUILD_BUG_ON(!__builtin_constant_p(s));
	BUILD_BUG_ON((s) >= INTEL_SUBPLATFORM_BITS);
	return ((mask << (msb - pb)) & (mask << (msb - s))) & BIT(msb);
}
#define IS_MOBILE(i915)	(INTEL_INFO(i915)->is_mobile)
#define IS_DGFX(i915)   (INTEL_INFO(i915)->is_dgfx)
#define IS_I830(i915)	IS_PLATFORM(i915, INTEL_I830)
#define IS_I845G(i915)	IS_PLATFORM(i915, INTEL_I845G)
#define IS_I85X(i915)	IS_PLATFORM(i915, INTEL_I85X)
#define IS_I865G(i915)	IS_PLATFORM(i915, INTEL_I865G)
#define IS_I915G(i915)	IS_PLATFORM(i915, INTEL_I915G)
#define IS_I915GM(i915)	IS_PLATFORM(i915, INTEL_I915GM)
#define IS_I945G(i915)	IS_PLATFORM(i915, INTEL_I945G)
#define IS_I945GM(i915)	IS_PLATFORM(i915, INTEL_I945GM)
#define IS_I965G(i915)	IS_PLATFORM(i915, INTEL_I965G)
#define IS_I965GM(i915)	IS_PLATFORM(i915, INTEL_I965GM)
#define IS_G45(i915)	IS_PLATFORM(i915, INTEL_G45)
#define IS_GM45(i915)	IS_PLATFORM(i915, INTEL_GM45)
#define IS_G4X(i915)	(IS_G45(i915) || IS_GM45(i915))
#define IS_PINEVIEW(i915)	IS_PLATFORM(i915, INTEL_PINEVIEW)
#define IS_G33(i915)	IS_PLATFORM(i915, INTEL_G33)
#define IS_IRONLAKE(i915)	IS_PLATFORM(i915, INTEL_IRONLAKE)
#define IS_IRONLAKE_M(i915) \
	(IS_PLATFORM(i915, INTEL_IRONLAKE) && IS_MOBILE(i915))
#define IS_SANDYBRIDGE(i915) IS_PLATFORM(i915, INTEL_SANDYBRIDGE)
#define IS_IVYBRIDGE(i915)	IS_PLATFORM(i915, INTEL_IVYBRIDGE)
#define IS_IVB_GT1(i915)	(IS_IVYBRIDGE(i915) && \
				 INTEL_INFO(i915)->gt == 1)
#define IS_VALLEYVIEW(i915)	IS_PLATFORM(i915, INTEL_VALLEYVIEW)
#define IS_CHERRYVIEW(i915)	IS_PLATFORM(i915, INTEL_CHERRYVIEW)
#define IS_HASWELL(i915)	IS_PLATFORM(i915, INTEL_HASWELL)
#define IS_BROADWELL(i915)	IS_PLATFORM(i915, INTEL_BROADWELL)
#define IS_SKYLAKE(i915)	IS_PLATFORM(i915, INTEL_SKYLAKE)
#define IS_BROXTON(i915)	IS_PLATFORM(i915, INTEL_BROXTON)
#define IS_KABYLAKE(i915)	IS_PLATFORM(i915, INTEL_KABYLAKE)
#define IS_GEMINILAKE(i915)	IS_PLATFORM(i915, INTEL_GEMINILAKE)
#define IS_COFFEELAKE(i915)	IS_PLATFORM(i915, INTEL_COFFEELAKE)
#define IS_COMETLAKE(i915)	IS_PLATFORM(i915, INTEL_COMETLAKE)
#define IS_ICELAKE(i915)	IS_PLATFORM(i915, INTEL_ICELAKE)
#define IS_JASPERLAKE(i915)	IS_PLATFORM(i915, INTEL_JASPERLAKE)
#define IS_ELKHARTLAKE(i915)	IS_PLATFORM(i915, INTEL_ELKHARTLAKE)
#define IS_TIGERLAKE(i915)	IS_PLATFORM(i915, INTEL_TIGERLAKE)
#define IS_ROCKETLAKE(i915)	IS_PLATFORM(i915, INTEL_ROCKETLAKE)
#define IS_DG1(i915)        IS_PLATFORM(i915, INTEL_DG1)
#define IS_ALDERLAKE_S(i915) IS_PLATFORM(i915, INTEL_ALDERLAKE_S)
#define IS_ALDERLAKE_P(i915) IS_PLATFORM(i915, INTEL_ALDERLAKE_P)
#define IS_XEHPSDV(i915) IS_PLATFORM(i915, INTEL_XEHPSDV)
#define IS_DG2(i915)	IS_PLATFORM(i915, INTEL_DG2)
#define IS_PONTEVECCHIO(i915) IS_PLATFORM(i915, INTEL_PONTEVECCHIO)
#define IS_METEORLAKE(i915) IS_PLATFORM(i915, INTEL_METEORLAKE)
#define IS_METEORLAKE_M(i915) \
	IS_SUBPLATFORM(i915, INTEL_METEORLAKE, INTEL_SUBPLATFORM_M)
#define IS_METEORLAKE_P(i915) \
	IS_SUBPLATFORM(i915, INTEL_METEORLAKE, INTEL_SUBPLATFORM_P)
#define IS_DG2_G10(i915) \
	IS_SUBPLATFORM(i915, INTEL_DG2, INTEL_SUBPLATFORM_G10)
#define IS_DG2_G11(i915) \
	IS_SUBPLATFORM(i915, INTEL_DG2, INTEL_SUBPLATFORM_G11)
#define IS_DG2_G12(i915) \
	IS_SUBPLATFORM(i915, INTEL_DG2, INTEL_SUBPLATFORM_G12)
#define IS_RAPTORLAKE_S(i915) \
	IS_SUBPLATFORM(i915, INTEL_ALDERLAKE_S, INTEL_SUBPLATFORM_RPL)
#define IS_ALDERLAKE_P_N(i915) \
	IS_SUBPLATFORM(i915, INTEL_ALDERLAKE_P, INTEL_SUBPLATFORM_N)
#define IS_RAPTORLAKE_P(i915) \
	IS_SUBPLATFORM(i915, INTEL_ALDERLAKE_P, INTEL_SUBPLATFORM_RPL)
#define IS_RAPTORLAKE_U(i915) \
	IS_SUBPLATFORM(i915, INTEL_ALDERLAKE_P, INTEL_SUBPLATFORM_RPLU)
#define IS_HASWELL_EARLY_SDV(i915) (IS_HASWELL(i915) && \
				    (INTEL_DEVID(i915) & 0xFF00) == 0x0C00)
#define IS_BROADWELL_ULT(i915) \
	IS_SUBPLATFORM(i915, INTEL_BROADWELL, INTEL_SUBPLATFORM_ULT)
#define IS_BROADWELL_ULX(i915) \
	IS_SUBPLATFORM(i915, INTEL_BROADWELL, INTEL_SUBPLATFORM_ULX)
#define IS_BROADWELL_GT3(i915)	(IS_BROADWELL(i915) && \
				 INTEL_INFO(i915)->gt == 3)
#define IS_HASWELL_ULT(i915) \
	IS_SUBPLATFORM(i915, INTEL_HASWELL, INTEL_SUBPLATFORM_ULT)
#define IS_HASWELL_GT3(i915)	(IS_HASWELL(i915) && \
				 INTEL_INFO(i915)->gt == 3)
#define IS_HASWELL_GT1(i915)	(IS_HASWELL(i915) && \
				 INTEL_INFO(i915)->gt == 1)
#define IS_HASWELL_ULX(i915) \
	IS_SUBPLATFORM(i915, INTEL_HASWELL, INTEL_SUBPLATFORM_ULX)
#define IS_SKYLAKE_ULT(i915) \
	IS_SUBPLATFORM(i915, INTEL_SKYLAKE, INTEL_SUBPLATFORM_ULT)
#define IS_SKYLAKE_ULX(i915) \
	IS_SUBPLATFORM(i915, INTEL_SKYLAKE, INTEL_SUBPLATFORM_ULX)
#define IS_KABYLAKE_ULT(i915) \
	IS_SUBPLATFORM(i915, INTEL_KABYLAKE, INTEL_SUBPLATFORM_ULT)
#define IS_KABYLAKE_ULX(i915) \
	IS_SUBPLATFORM(i915, INTEL_KABYLAKE, INTEL_SUBPLATFORM_ULX)
#define IS_SKYLAKE_GT2(i915)	(IS_SKYLAKE(i915) && \
				 INTEL_INFO(i915)->gt == 2)
#define IS_SKYLAKE_GT3(i915)	(IS_SKYLAKE(i915) && \
				 INTEL_INFO(i915)->gt == 3)
#define IS_SKYLAKE_GT4(i915)	(IS_SKYLAKE(i915) && \
				 INTEL_INFO(i915)->gt == 4)
#define IS_KABYLAKE_GT2(i915)	(IS_KABYLAKE(i915) && \
				 INTEL_INFO(i915)->gt == 2)
#define IS_KABYLAKE_GT3(i915)	(IS_KABYLAKE(i915) && \
				 INTEL_INFO(i915)->gt == 3)
#define IS_COFFEELAKE_ULT(i915) \
	IS_SUBPLATFORM(i915, INTEL_COFFEELAKE, INTEL_SUBPLATFORM_ULT)
#define IS_COFFEELAKE_ULX(i915) \
	IS_SUBPLATFORM(i915, INTEL_COFFEELAKE, INTEL_SUBPLATFORM_ULX)
#define IS_COFFEELAKE_GT2(i915)	(IS_COFFEELAKE(i915) && \
				 INTEL_INFO(i915)->gt == 2)
#define IS_COFFEELAKE_GT3(i915)	(IS_COFFEELAKE(i915) && \
				 INTEL_INFO(i915)->gt == 3)
#define IS_COMETLAKE_ULT(i915) \
	IS_SUBPLATFORM(i915, INTEL_COMETLAKE, INTEL_SUBPLATFORM_ULT)
#define IS_COMETLAKE_ULX(i915) \
	IS_SUBPLATFORM(i915, INTEL_COMETLAKE, INTEL_SUBPLATFORM_ULX)
#define IS_COMETLAKE_GT2(i915)	(IS_COMETLAKE(i915) && \
				 INTEL_INFO(i915)->gt == 2)
#define IS_ICL_WITH_PORT_F(i915) \
	IS_SUBPLATFORM(i915, INTEL_ICELAKE, INTEL_SUBPLATFORM_PORTF)
#define IS_TIGERLAKE_UY(i915) \
	IS_SUBPLATFORM(i915, INTEL_TIGERLAKE, INTEL_SUBPLATFORM_UY)
#define IS_XEHPSDV_GRAPHICS_STEP(__i915, since, until) \
	(IS_XEHPSDV(__i915) && IS_GRAPHICS_STEP(__i915, since, until))
#define IS_MTL_GRAPHICS_STEP(__i915, variant, since, until) \
	(IS_SUBPLATFORM(__i915, INTEL_METEORLAKE, INTEL_SUBPLATFORM_##variant) && \
	 IS_GRAPHICS_STEP(__i915, since, until))
#define IS_MTL_DISPLAY_STEP(__i915, since, until) \
	(IS_METEORLAKE(__i915) && \
	 IS_DISPLAY_STEP(__i915, since, until))
#define IS_MTL_MEDIA_STEP(__i915, since, until) \
	(IS_METEORLAKE(__i915) && \
	 IS_MEDIA_STEP(__i915, since, until))
#define IS_DG2_GRAPHICS_STEP(__i915, variant, since, until) \
	(IS_SUBPLATFORM(__i915, INTEL_DG2, INTEL_SUBPLATFORM_##variant) && \
	 IS_GRAPHICS_STEP(__i915, since, until))
#define IS_DG2_DISPLAY_STEP(__i915, since, until) \
	(IS_DG2(__i915) && \
	 IS_DISPLAY_STEP(__i915, since, until))
#define IS_PVC_BD_STEP(__i915, since, until) \
	(IS_PONTEVECCHIO(__i915) && \
	 IS_BASEDIE_STEP(__i915, since, until))
#define IS_PVC_CT_STEP(__i915, since, until) \
	(IS_PONTEVECCHIO(__i915) && \
	 IS_GRAPHICS_STEP(__i915, since, until))
#define IS_LP(i915)		(INTEL_INFO(i915)->is_lp)
#define IS_GEN9_LP(i915)	(GRAPHICS_VER(i915) == 9 && IS_LP(i915))
#define IS_GEN9_BC(i915)	(GRAPHICS_VER(i915) == 9 && !IS_LP(i915))
#define __HAS_ENGINE(engine_mask, id) ((engine_mask) & BIT(id))
#define HAS_ENGINE(gt, id) __HAS_ENGINE((gt)->info.engine_mask, id)
#define __ENGINE_INSTANCES_MASK(mask, first, count) ({			\
	unsigned int first__ = (first);					\
	unsigned int count__ = (count);					\
	((mask) & GENMASK(first__ + count__ - 1, first__)) >> first__;	\
})
#define ENGINE_INSTANCES_MASK(gt, first, count) \
	__ENGINE_INSTANCES_MASK((gt)->info.engine_mask, first, count)
#define RCS_MASK(gt) \
	ENGINE_INSTANCES_MASK(gt, RCS0, I915_MAX_RCS)
#define BCS_MASK(gt) \
	ENGINE_INSTANCES_MASK(gt, BCS0, I915_MAX_BCS)
#define VDBOX_MASK(gt) \
	ENGINE_INSTANCES_MASK(gt, VCS0, I915_MAX_VCS)
#define VEBOX_MASK(gt) \
	ENGINE_INSTANCES_MASK(gt, VECS0, I915_MAX_VECS)
#define CCS_MASK(gt) \
	ENGINE_INSTANCES_MASK(gt, CCS0, I915_MAX_CCS)
#define HAS_MEDIA_RATIO_MODE(i915) (INTEL_INFO(i915)->has_media_ratio_mode)
#define CMDPARSER_USES_GGTT(i915) (GRAPHICS_VER(i915) == 7)
#define HAS_LLC(i915)	(INTEL_INFO(i915)->has_llc)
#define HAS_4TILE(i915)	(INTEL_INFO(i915)->has_4tile)
#define HAS_SNOOP(i915)	(INTEL_INFO(i915)->has_snoop)
#define HAS_EDRAM(i915)	((i915)->edram_size_mb)
#define HAS_SECURE_BATCHES(i915) (GRAPHICS_VER(i915) < 6)
#define HAS_WT(i915)	HAS_EDRAM(i915)
#define HWS_NEEDS_PHYSICAL(i915)	(INTEL_INFO(i915)->hws_needs_physical)
#define HAS_LOGICAL_RING_CONTEXTS(i915) \
		(INTEL_INFO(i915)->has_logical_ring_contexts)
#define HAS_LOGICAL_RING_ELSQ(i915) \
		(INTEL_INFO(i915)->has_logical_ring_elsq)
#define HAS_EXECLISTS(i915) HAS_LOGICAL_RING_CONTEXTS(i915)
#define INTEL_PPGTT(i915) (RUNTIME_INFO(i915)->ppgtt_type)
#define HAS_PPGTT(i915) \
	(INTEL_PPGTT(i915) != INTEL_PPGTT_NONE)
#define HAS_FULL_PPGTT(i915) \
	(INTEL_PPGTT(i915) >= INTEL_PPGTT_FULL)
#define HAS_PAGE_SIZES(i915, sizes) ({ \
	GEM_BUG_ON((sizes) == 0); \
	((sizes) & ~RUNTIME_INFO(i915)->page_sizes) == 0; \
})
#define HAS_BROKEN_CS_TLB(i915)	(IS_I830(i915) || IS_I845G(i915))
#define NEEDS_RC6_CTX_CORRUPTION_WA(i915)	\
	(IS_BROADWELL(i915) || GRAPHICS_VER(i915) == 9)
#define NEEDS_WaRsDisableCoarsePowerGating(i915)			\
	(IS_SKYLAKE_GT3(i915) || IS_SKYLAKE_GT4(i915))
#define HAS_128_BYTE_Y_TILING(i915) (GRAPHICS_VER(i915) != 2 && \
					 !(IS_I915G(i915) || IS_I915GM(i915)))
#define HAS_RC6(i915)		 (INTEL_INFO(i915)->has_rc6)
#define HAS_RC6p(i915)		 (INTEL_INFO(i915)->has_rc6p)
#define HAS_RC6pp(i915)		 (false)  
#define HAS_RPS(i915)	(INTEL_INFO(i915)->has_rps)
#define HAS_HECI_PXP(i915) \
	(INTEL_INFO(i915)->has_heci_pxp)
#define HAS_HECI_GSCFI(i915) \
	(INTEL_INFO(i915)->has_heci_gscfi)
#define HAS_HECI_GSC(i915) (HAS_HECI_PXP(i915) || HAS_HECI_GSCFI(i915))
#define HAS_RUNTIME_PM(i915) (INTEL_INFO(i915)->has_runtime_pm)
#define HAS_64BIT_RELOC(i915) (INTEL_INFO(i915)->has_64bit_reloc)
#define HAS_OA_BPC_REPORTING(i915) \
	(INTEL_INFO(i915)->has_oa_bpc_reporting)
#define HAS_OA_SLICE_CONTRIB_LIMITS(i915) \
	(INTEL_INFO(i915)->has_oa_slice_contrib_limits)
#define HAS_OAM(i915) \
	(INTEL_INFO(i915)->has_oam)
#define HAS_64K_PAGES(i915) (INTEL_INFO(i915)->has_64k_pages)
#define HAS_REGION(i915, i) (INTEL_INFO(i915)->memory_regions & (i))
#define HAS_LMEM(i915) HAS_REGION(i915, REGION_LMEM)
#define HAS_EXTRA_GT_LIST(i915)   (INTEL_INFO(i915)->extra_gt_list)
#define HAS_FLAT_CCS(i915)   (INTEL_INFO(i915)->has_flat_ccs)
#define HAS_GT_UC(i915)	(INTEL_INFO(i915)->has_gt_uc)
#define HAS_POOLED_EU(i915)	(RUNTIME_INFO(i915)->has_pooled_eu)
#define HAS_GLOBAL_MOCS_REGISTERS(i915)	(INTEL_INFO(i915)->has_global_mocs)
#define HAS_GMD_ID(i915)	(INTEL_INFO(i915)->has_gmd_id)
#define HAS_L3_CCS_READ(i915) (INTEL_INFO(i915)->has_l3_ccs_read)
#define HAS_L3_DPF(i915) (INTEL_INFO(i915)->has_l3_dpf)
#define NUM_L3_SLICES(i915) (IS_HASWELL_GT3(i915) ? \
				 2 : HAS_L3_DPF(i915))
#define INTEL_DISPLAY_ENABLED(i915) \
	(drm_WARN_ON(&(i915)->drm, !HAS_DISPLAY(i915)),		\
	 !(i915)->params.disable_display &&				\
	 !intel_opregion_headless_sku(i915))
#define HAS_GUC_DEPRIVILEGE(i915) \
	(INTEL_INFO(i915)->has_guc_deprivilege)
#define HAS_3D_PIPELINE(i915)	(INTEL_INFO(i915)->has_3d_pipeline)
#define HAS_ONE_EU_PER_FUSE_BIT(i915)	(INTEL_INFO(i915)->has_one_eu_per_fuse_bit)
#define HAS_LMEMBAR_SMEM_STOLEN(i915) (!HAS_LMEM(i915) && \
				       GRAPHICS_VER_FULL(i915) >= IP_VER(12, 70))
#endif
