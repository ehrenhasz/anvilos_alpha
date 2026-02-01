
 

#include "gen2_engine_cs.h"
#include "i915_drv.h"
#include "i915_reg.h"
#include "intel_engine.h"
#include "intel_engine_regs.h"
#include "intel_gpu_commands.h"
#include "intel_gt.h"
#include "intel_gt_irq.h"
#include "intel_ring.h"

int gen2_emit_flush(struct i915_request *rq, u32 mode)
{
	unsigned int num_store_dw = 12;
	u32 cmd, *cs;

	cmd = MI_FLUSH;
	if (mode & EMIT_INVALIDATE)
		cmd |= MI_READ_FLUSH;

	cs = intel_ring_begin(rq, 2 + 4 * num_store_dw);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	*cs++ = cmd;
	while (num_store_dw--) {
		*cs++ = MI_STORE_DWORD_INDEX;
		*cs++ = I915_GEM_HWS_SCRATCH * sizeof(u32);
		*cs++ = 0;
		*cs++ = MI_FLUSH | MI_NO_WRITE_FLUSH;
	}
	*cs++ = cmd;

	intel_ring_advance(rq, cs);

	return 0;
}

int gen4_emit_flush_rcs(struct i915_request *rq, u32 mode)
{
	u32 cmd, *cs;
	int i;

	 

	cmd = MI_FLUSH;
	if (mode & EMIT_INVALIDATE) {
		cmd |= MI_EXE_FLUSH;
		if (IS_G4X(rq->i915) || GRAPHICS_VER(rq->i915) == 5)
			cmd |= MI_INVALIDATE_ISP;
	}

	i = 2;
	if (mode & EMIT_INVALIDATE)
		i += 20;

	cs = intel_ring_begin(rq, i);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	*cs++ = cmd;

	 
	if (mode & EMIT_INVALIDATE) {
		*cs++ = GFX_OP_PIPE_CONTROL(4) | PIPE_CONTROL_QW_WRITE;
		*cs++ = intel_gt_scratch_offset(rq->engine->gt,
						INTEL_GT_SCRATCH_FIELD_DEFAULT) |
			PIPE_CONTROL_GLOBAL_GTT;
		*cs++ = 0;
		*cs++ = 0;

		for (i = 0; i < 12; i++)
			*cs++ = MI_FLUSH;

		*cs++ = GFX_OP_PIPE_CONTROL(4) | PIPE_CONTROL_QW_WRITE;
		*cs++ = intel_gt_scratch_offset(rq->engine->gt,
						INTEL_GT_SCRATCH_FIELD_DEFAULT) |
			PIPE_CONTROL_GLOBAL_GTT;
		*cs++ = 0;
		*cs++ = 0;
	}

	*cs++ = cmd;

	intel_ring_advance(rq, cs);

	return 0;
}

int gen4_emit_flush_vcs(struct i915_request *rq, u32 mode)
{
	u32 *cs;

	cs = intel_ring_begin(rq, 2);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	*cs++ = MI_FLUSH;
	*cs++ = MI_NOOP;
	intel_ring_advance(rq, cs);

	return 0;
}

static u32 *__gen2_emit_breadcrumb(struct i915_request *rq, u32 *cs,
				   int flush, int post)
{
	GEM_BUG_ON(i915_request_active_timeline(rq)->hwsp_ggtt != rq->engine->status_page.vma);
	GEM_BUG_ON(offset_in_page(rq->hwsp_seqno) != I915_GEM_HWS_SEQNO_ADDR);

	*cs++ = MI_FLUSH;

	while (flush--) {
		*cs++ = MI_STORE_DWORD_INDEX;
		*cs++ = I915_GEM_HWS_SCRATCH * sizeof(u32);
		*cs++ = rq->fence.seqno;
	}

	while (post--) {
		*cs++ = MI_STORE_DWORD_INDEX;
		*cs++ = I915_GEM_HWS_SEQNO_ADDR;
		*cs++ = rq->fence.seqno;
	}

	*cs++ = MI_USER_INTERRUPT;

	rq->tail = intel_ring_offset(rq, cs);
	assert_ring_tail_valid(rq->ring, rq->tail);

	return cs;
}

u32 *gen3_emit_breadcrumb(struct i915_request *rq, u32 *cs)
{
	return __gen2_emit_breadcrumb(rq, cs, 16, 8);
}

u32 *gen5_emit_breadcrumb(struct i915_request *rq, u32 *cs)
{
	return __gen2_emit_breadcrumb(rq, cs, 8, 8);
}

 
#define I830_BATCH_LIMIT SZ_256K
#define I830_TLB_ENTRIES (2)
#define I830_WA_SIZE max(I830_TLB_ENTRIES * SZ_4K, I830_BATCH_LIMIT)
int i830_emit_bb_start(struct i915_request *rq,
		       u64 offset, u32 len,
		       unsigned int dispatch_flags)
{
	u32 *cs, cs_offset =
		intel_gt_scratch_offset(rq->engine->gt,
					INTEL_GT_SCRATCH_FIELD_DEFAULT);

	GEM_BUG_ON(rq->engine->gt->scratch->size < I830_WA_SIZE);

	cs = intel_ring_begin(rq, 6);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	 
	*cs++ = COLOR_BLT_CMD | BLT_WRITE_RGBA;
	*cs++ = BLT_DEPTH_32 | BLT_ROP_COLOR_COPY | 4096;
	*cs++ = I830_TLB_ENTRIES << 16 | 4;  
	*cs++ = cs_offset;
	*cs++ = 0xdeadbeef;
	*cs++ = MI_NOOP;
	intel_ring_advance(rq, cs);

	if ((dispatch_flags & I915_DISPATCH_PINNED) == 0) {
		if (len > I830_BATCH_LIMIT)
			return -ENOSPC;

		cs = intel_ring_begin(rq, 6 + 2);
		if (IS_ERR(cs))
			return PTR_ERR(cs);

		 
		*cs++ = SRC_COPY_BLT_CMD | BLT_WRITE_RGBA | (6 - 2);
		*cs++ = BLT_DEPTH_32 | BLT_ROP_SRC_COPY | 4096;
		*cs++ = DIV_ROUND_UP(len, 4096) << 16 | 4096;
		*cs++ = cs_offset;
		*cs++ = 4096;
		*cs++ = offset;

		*cs++ = MI_FLUSH;
		*cs++ = MI_NOOP;
		intel_ring_advance(rq, cs);

		 
		offset = cs_offset;
	}

	if (!(dispatch_flags & I915_DISPATCH_SECURE))
		offset |= MI_BATCH_NON_SECURE;

	cs = intel_ring_begin(rq, 2);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	*cs++ = MI_BATCH_BUFFER_START | MI_BATCH_GTT;
	*cs++ = offset;
	intel_ring_advance(rq, cs);

	return 0;
}

int gen3_emit_bb_start(struct i915_request *rq,
		       u64 offset, u32 len,
		       unsigned int dispatch_flags)
{
	u32 *cs;

	if (!(dispatch_flags & I915_DISPATCH_SECURE))
		offset |= MI_BATCH_NON_SECURE;

	cs = intel_ring_begin(rq, 2);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	*cs++ = MI_BATCH_BUFFER_START | MI_BATCH_GTT;
	*cs++ = offset;
	intel_ring_advance(rq, cs);

	return 0;
}

int gen4_emit_bb_start(struct i915_request *rq,
		       u64 offset, u32 length,
		       unsigned int dispatch_flags)
{
	u32 security;
	u32 *cs;

	security = MI_BATCH_NON_SECURE_I965;
	if (dispatch_flags & I915_DISPATCH_SECURE)
		security = 0;

	cs = intel_ring_begin(rq, 2);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	*cs++ = MI_BATCH_BUFFER_START | MI_BATCH_GTT | security;
	*cs++ = offset;
	intel_ring_advance(rq, cs);

	return 0;
}

void gen2_irq_enable(struct intel_engine_cs *engine)
{
	struct drm_i915_private *i915 = engine->i915;

	i915->irq_mask &= ~engine->irq_enable_mask;
	intel_uncore_write16(&i915->uncore, GEN2_IMR, i915->irq_mask);
	ENGINE_POSTING_READ16(engine, RING_IMR);
}

void gen2_irq_disable(struct intel_engine_cs *engine)
{
	struct drm_i915_private *i915 = engine->i915;

	i915->irq_mask |= engine->irq_enable_mask;
	intel_uncore_write16(&i915->uncore, GEN2_IMR, i915->irq_mask);
}

void gen3_irq_enable(struct intel_engine_cs *engine)
{
	engine->i915->irq_mask &= ~engine->irq_enable_mask;
	intel_uncore_write(engine->uncore, GEN2_IMR, engine->i915->irq_mask);
	intel_uncore_posting_read_fw(engine->uncore, GEN2_IMR);
}

void gen3_irq_disable(struct intel_engine_cs *engine)
{
	engine->i915->irq_mask |= engine->irq_enable_mask;
	intel_uncore_write(engine->uncore, GEN2_IMR, engine->i915->irq_mask);
}

void gen5_irq_enable(struct intel_engine_cs *engine)
{
	gen5_gt_enable_irq(engine->gt, engine->irq_enable_mask);
}

void gen5_irq_disable(struct intel_engine_cs *engine)
{
	gen5_gt_disable_irq(engine->gt, engine->irq_enable_mask);
}
