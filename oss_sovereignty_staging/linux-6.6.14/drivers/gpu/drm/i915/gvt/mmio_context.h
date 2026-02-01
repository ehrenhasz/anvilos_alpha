 

#ifndef __GVT_RENDER_H__
#define __GVT_RENDER_H__

#include <linux/types.h>

#include "gt/intel_engine_regs.h"
#include "gt/intel_engine_types.h"
#include "gt/intel_lrc_reg.h"

struct i915_request;
struct intel_context;
struct intel_engine_cs;
struct intel_gvt;
struct intel_vgpu;

struct engine_mmio {
	enum intel_engine_id id;
	i915_reg_t reg;
	u32 mask;
	bool in_context;
	u32 value;
};

void intel_gvt_switch_mmio(struct intel_vgpu *pre,
			   struct intel_vgpu *next,
			   const struct intel_engine_cs *engine);

void intel_gvt_init_engine_mmio_context(struct intel_gvt *gvt);

bool is_inhibit_context(struct intel_context *ce);

int intel_vgpu_restore_inhibit_context(struct intel_vgpu *vgpu,
				       struct i915_request *req);

#define IS_RESTORE_INHIBIT(a) \
	IS_MASKED_BITS_ENABLED(a, CTX_CTRL_ENGINE_CTX_RESTORE_INHIBIT)

#endif
