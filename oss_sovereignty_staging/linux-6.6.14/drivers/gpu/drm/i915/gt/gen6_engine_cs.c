
 

#include "gen6_engine_cs.h"
#include "intel_engine.h"
#include "intel_engine_regs.h"
#include "intel_gpu_commands.h"
#include "intel_gt.h"
#include "intel_gt_irq.h"
#include "intel_gt_pm_irq.h"
#include "intel_ring.h"

#define HWS_SCRATCH_ADDR	(I915_GEM_HWS_SCRATCH * sizeof(u32))

 
static int
gen6_emit_post_sync_nonzero_flush(struct i915_request *rq)
{
	u32 scratch_addr =
		intel_gt_scratch_offset(rq->engine->gt,
					INTEL_GT_SCRATCH_FIELD_RENDER_FLUSH);
	u32 *cs;

	cs = intel_ring_begin(rq, 6);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	*cs++ = GFX_OP_PIPE_CONTROL(5);
	*cs++ = PIPE_CONTROL_CS_STALL | PIPE_CONTROL_STALL_AT_SCOREBOARD;
	*cs++ = scratch_addr | PIPE_CONTROL_GLOBAL_GTT;
	*cs++ = 0;  
	*cs++ = 0;  
	*cs++ = MI_NOOP;
	intel_ring_advance(rq, cs);

	cs = intel_ring_begin(rq, 6);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	*cs++ = GFX_OP_PIPE_CONTROL(5);
	*cs++ = PIPE_CONTROL_QW_WRITE;
	*cs++ = scratch_addr | PIPE_CONTROL_GLOBAL_GTT;
	*cs++ = 0;
	*cs++ = 0;
	*cs++ = MI_NOOP;
	intel_ring_advance(rq, cs);

	return 0;
}

int gen6_emit_flush_rcs(struct i915_request *rq, u32 mode)
{
	u32 scratch_addr =
		intel_gt_scratch_offset(rq->engine->gt,
					INTEL_GT_SCRATCH_FIELD_RENDER_FLUSH);
	u32 *cs, flags = 0;
	int ret;

	 
	ret = gen6_emit_post_sync_nonzero_flush(rq);
	if (ret)
		return ret;

	 
	if (mode & EMIT_FLUSH) {
		flags |= PIPE_CONTROL_RENDER_TARGET_CACHE_FLUSH;
		flags |= PIPE_CONTROL_DEPTH_CACHE_FLUSH;
		 
		flags |= PIPE_CONTROL_CS_STALL;
	}
	if (mode & EMIT_INVALIDATE) {
		flags |= PIPE_CONTROL_TLB_INVALIDATE;
		flags |= PIPE_CONTROL_INSTRUCTION_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_TEXTURE_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_VF_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_CONST_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_STATE_CACHE_INVALIDATE;
		 
		flags |= PIPE_CONTROL_QW_WRITE | PIPE_CONTROL_CS_STALL;
	}

	cs = intel_ring_begin(rq, 4);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	*cs++ = GFX_OP_PIPE_CONTROL(4);
	*cs++ = flags;
	*cs++ = scratch_addr | PIPE_CONTROL_GLOBAL_GTT;
	*cs++ = 0;
	intel_ring_advance(rq, cs);

	return 0;
}

u32 *gen6_emit_breadcrumb_rcs(struct i915_request *rq, u32 *cs)
{
	 
	*cs++ = GFX_OP_PIPE_CONTROL(4);
	*cs++ = PIPE_CONTROL_CS_STALL | PIPE_CONTROL_STALL_AT_SCOREBOARD;
	*cs++ = 0;
	*cs++ = 0;

	*cs++ = GFX_OP_PIPE_CONTROL(4);
	*cs++ = PIPE_CONTROL_QW_WRITE;
	*cs++ = intel_gt_scratch_offset(rq->engine->gt,
					INTEL_GT_SCRATCH_FIELD_DEFAULT) |
		PIPE_CONTROL_GLOBAL_GTT;
	*cs++ = 0;

	 
	*cs++ = GFX_OP_PIPE_CONTROL(4);
	*cs++ = (PIPE_CONTROL_RENDER_TARGET_CACHE_FLUSH |
		 PIPE_CONTROL_DEPTH_CACHE_FLUSH |
		 PIPE_CONTROL_DC_FLUSH_ENABLE |
		 PIPE_CONTROL_QW_WRITE |
		 PIPE_CONTROL_CS_STALL);
	*cs++ = i915_request_active_seqno(rq) |
		PIPE_CONTROL_GLOBAL_GTT;
	*cs++ = rq->fence.seqno;

	*cs++ = MI_USER_INTERRUPT;
	*cs++ = MI_NOOP;

	rq->tail = intel_ring_offset(rq, cs);
	assert_ring_tail_valid(rq->ring, rq->tail);

	return cs;
}

static int mi_flush_dw(struct i915_request *rq, u32 flags)
{
	u32 cmd, *cs;

	cs = intel_ring_begin(rq, 4);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	cmd = MI_FLUSH_DW;

	 
	cmd |= MI_FLUSH_DW_STORE_INDEX | MI_FLUSH_DW_OP_STOREDW;

	 
	cmd |= flags;

	*cs++ = cmd;
	*cs++ = HWS_SCRATCH_ADDR | MI_FLUSH_DW_USE_GTT;
	*cs++ = 0;
	*cs++ = MI_NOOP;

	intel_ring_advance(rq, cs);

	return 0;
}

static int gen6_flush_dw(struct i915_request *rq, u32 mode, u32 invflags)
{
	return mi_flush_dw(rq, mode & EMIT_INVALIDATE ? invflags : 0);
}

int gen6_emit_flush_xcs(struct i915_request *rq, u32 mode)
{
	return gen6_flush_dw(rq, mode, MI_INVALIDATE_TLB);
}

int gen6_emit_flush_vcs(struct i915_request *rq, u32 mode)
{
	return gen6_flush_dw(rq, mode, MI_INVALIDATE_TLB | MI_INVALIDATE_BSD);
}

int gen6_emit_bb_start(struct i915_request *rq,
		       u64 offset, u32 len,
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

	cs = __gen6_emit_bb_start(cs, offset, security);
	intel_ring_advance(rq, cs);

	return 0;
}

int
hsw_emit_bb_start(struct i915_request *rq,
		  u64 offset, u32 len,
		  unsigned int dispatch_flags)
{
	u32 security;
	u32 *cs;

	security = MI_BATCH_PPGTT_HSW | MI_BATCH_NON_SECURE_HSW;
	if (dispatch_flags & I915_DISPATCH_SECURE)
		security = 0;

	cs = intel_ring_begin(rq, 2);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	cs = __gen6_emit_bb_start(cs, offset, security);
	intel_ring_advance(rq, cs);

	return 0;
}

static int gen7_stall_cs(struct i915_request *rq)
{
	u32 *cs;

	cs = intel_ring_begin(rq, 4);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	*cs++ = GFX_OP_PIPE_CONTROL(4);
	*cs++ = PIPE_CONTROL_CS_STALL | PIPE_CONTROL_STALL_AT_SCOREBOARD;
	*cs++ = 0;
	*cs++ = 0;
	intel_ring_advance(rq, cs);

	return 0;
}

int gen7_emit_flush_rcs(struct i915_request *rq, u32 mode)
{
	u32 scratch_addr =
		intel_gt_scratch_offset(rq->engine->gt,
					INTEL_GT_SCRATCH_FIELD_RENDER_FLUSH);
	u32 *cs, flags = 0;

	 
	flags |= PIPE_CONTROL_CS_STALL;

	 
	flags |= PIPE_CONTROL_QW_WRITE;
	flags |= PIPE_CONTROL_GLOBAL_GTT_IVB;

	 
	if (mode & EMIT_FLUSH) {
		flags |= PIPE_CONTROL_RENDER_TARGET_CACHE_FLUSH;
		flags |= PIPE_CONTROL_DEPTH_CACHE_FLUSH;
		flags |= PIPE_CONTROL_DC_FLUSH_ENABLE;
		flags |= PIPE_CONTROL_FLUSH_ENABLE;
	}
	if (mode & EMIT_INVALIDATE) {
		flags |= PIPE_CONTROL_TLB_INVALIDATE;
		flags |= PIPE_CONTROL_INSTRUCTION_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_TEXTURE_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_VF_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_CONST_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_STATE_CACHE_INVALIDATE;
		flags |= PIPE_CONTROL_MEDIA_STATE_CLEAR;

		 
		gen7_stall_cs(rq);
	}

	cs = intel_ring_begin(rq, 4);
	if (IS_ERR(cs))
		return PTR_ERR(cs);

	*cs++ = GFX_OP_PIPE_CONTROL(4);
	*cs++ = flags;
	*cs++ = scratch_addr;
	*cs++ = 0;
	intel_ring_advance(rq, cs);

	return 0;
}

u32 *gen7_emit_breadcrumb_rcs(struct i915_request *rq, u32 *cs)
{
	*cs++ = GFX_OP_PIPE_CONTROL(4);
	*cs++ = (PIPE_CONTROL_RENDER_TARGET_CACHE_FLUSH |
		 PIPE_CONTROL_DEPTH_CACHE_FLUSH |
		 PIPE_CONTROL_DC_FLUSH_ENABLE |
		 PIPE_CONTROL_FLUSH_ENABLE |
		 PIPE_CONTROL_QW_WRITE |
		 PIPE_CONTROL_GLOBAL_GTT_IVB |
		 PIPE_CONTROL_CS_STALL);
	*cs++ = i915_request_active_seqno(rq);
	*cs++ = rq->fence.seqno;

	*cs++ = MI_USER_INTERRUPT;
	*cs++ = MI_NOOP;

	rq->tail = intel_ring_offset(rq, cs);
	assert_ring_tail_valid(rq->ring, rq->tail);

	return cs;
}

u32 *gen6_emit_breadcrumb_xcs(struct i915_request *rq, u32 *cs)
{
	GEM_BUG_ON(i915_request_active_timeline(rq)->hwsp_ggtt != rq->engine->status_page.vma);
	GEM_BUG_ON(offset_in_page(rq->hwsp_seqno) != I915_GEM_HWS_SEQNO_ADDR);

	*cs++ = MI_FLUSH_DW | MI_FLUSH_DW_OP_STOREDW | MI_FLUSH_DW_STORE_INDEX;
	*cs++ = I915_GEM_HWS_SEQNO_ADDR | MI_FLUSH_DW_USE_GTT;
	*cs++ = rq->fence.seqno;

	*cs++ = MI_USER_INTERRUPT;

	rq->tail = intel_ring_offset(rq, cs);
	assert_ring_tail_valid(rq->ring, rq->tail);

	return cs;
}

#define GEN7_XCS_WA 32
u32 *gen7_emit_breadcrumb_xcs(struct i915_request *rq, u32 *cs)
{
	int i;

	GEM_BUG_ON(i915_request_active_timeline(rq)->hwsp_ggtt != rq->engine->status_page.vma);
	GEM_BUG_ON(offset_in_page(rq->hwsp_seqno) != I915_GEM_HWS_SEQNO_ADDR);

	*cs++ = MI_FLUSH_DW | MI_INVALIDATE_TLB |
		MI_FLUSH_DW_OP_STOREDW | MI_FLUSH_DW_STORE_INDEX;
	*cs++ = I915_GEM_HWS_SEQNO_ADDR | MI_FLUSH_DW_USE_GTT;
	*cs++ = rq->fence.seqno;

	for (i = 0; i < GEN7_XCS_WA; i++) {
		*cs++ = MI_STORE_DWORD_INDEX;
		*cs++ = I915_GEM_HWS_SEQNO_ADDR;
		*cs++ = rq->fence.seqno;
	}

	*cs++ = MI_FLUSH_DW;
	*cs++ = 0;
	*cs++ = 0;

	*cs++ = MI_USER_INTERRUPT;
	*cs++ = MI_NOOP;

	rq->tail = intel_ring_offset(rq, cs);
	assert_ring_tail_valid(rq->ring, rq->tail);

	return cs;
}
#undef GEN7_XCS_WA

void gen6_irq_enable(struct intel_engine_cs *engine)
{
	ENGINE_WRITE(engine, RING_IMR,
		     ~(engine->irq_enable_mask | engine->irq_keep_mask));

	 
	ENGINE_POSTING_READ(engine, RING_IMR);

	gen5_gt_enable_irq(engine->gt, engine->irq_enable_mask);
}

void gen6_irq_disable(struct intel_engine_cs *engine)
{
	ENGINE_WRITE(engine, RING_IMR, ~engine->irq_keep_mask);
	gen5_gt_disable_irq(engine->gt, engine->irq_enable_mask);
}

void hsw_irq_enable_vecs(struct intel_engine_cs *engine)
{
	ENGINE_WRITE(engine, RING_IMR, ~engine->irq_enable_mask);

	 
	ENGINE_POSTING_READ(engine, RING_IMR);

	gen6_gt_pm_unmask_irq(engine->gt, engine->irq_enable_mask);
}

void hsw_irq_disable_vecs(struct intel_engine_cs *engine)
{
	ENGINE_WRITE(engine, RING_IMR, ~0);
	gen6_gt_pm_mask_irq(engine->gt, engine->irq_enable_mask);
}
