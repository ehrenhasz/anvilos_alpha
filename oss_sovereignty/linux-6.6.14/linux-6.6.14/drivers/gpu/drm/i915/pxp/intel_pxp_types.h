#ifndef __INTEL_PXP_TYPES_H__
#define __INTEL_PXP_TYPES_H__
#include <linux/completion.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/workqueue.h>
struct intel_context;
struct intel_gt;
struct i915_pxp_component;
struct drm_i915_private;
struct intel_pxp {
	struct intel_gt *ctrl_gt;
	u32 kcr_base;
	struct gsccs_session_resources {
		u64 host_session_handle;  
		struct intel_context *ce;  
		struct i915_vma *pkt_vma;  
		void *pkt_vaddr;   
		struct i915_vma *bb_vma;  
		void *bb_vaddr;  
	} gsccs_res;
	struct i915_pxp_component *pxp_component;
	struct device_link *dev_link;
	bool pxp_component_added;
	struct intel_context *ce;
	struct mutex arb_mutex;
	bool arb_is_valid;
	u32 key_instance;
	struct mutex tee_mutex;
	struct {
		struct drm_i915_gem_object *obj;  
		void *vaddr;  
	} stream_cmd;
	bool hw_state_invalidated;
	bool irq_enabled;
	struct completion termination;
	struct work_struct session_work;
	u32 session_events;
#define PXP_TERMINATION_REQUEST  BIT(0)
#define PXP_TERMINATION_COMPLETE BIT(1)
#define PXP_INVAL_REQUIRED       BIT(2)
};
#endif  
