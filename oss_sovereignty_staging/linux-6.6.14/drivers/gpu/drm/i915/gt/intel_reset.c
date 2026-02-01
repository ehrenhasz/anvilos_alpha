
 

#include <linux/sched/mm.h>
#include <linux/stop_machine.h>
#include <linux/string_helpers.h>

#include "display/intel_display_reset.h"
#include "display/intel_overlay.h"

#include "gem/i915_gem_context.h"

#include "gt/intel_gt_regs.h"

#include "gt/uc/intel_gsc_fw.h"

#include "i915_drv.h"
#include "i915_file_private.h"
#include "i915_gpu_error.h"
#include "i915_irq.h"
#include "i915_reg.h"
#include "intel_breadcrumbs.h"
#include "intel_engine_pm.h"
#include "intel_engine_regs.h"
#include "intel_gt.h"
#include "intel_gt_pm.h"
#include "intel_gt_requests.h"
#include "intel_mchbar_regs.h"
#include "intel_pci_config.h"
#include "intel_reset.h"

#include "uc/intel_guc.h"

#define RESET_MAX_RETRIES 3

static void client_mark_guilty(struct i915_gem_context *ctx, bool banned)
{
	struct drm_i915_file_private *file_priv = ctx->file_priv;
	unsigned long prev_hang;
	unsigned int score;

	if (IS_ERR_OR_NULL(file_priv))
		return;

	score = 0;
	if (banned)
		score = I915_CLIENT_SCORE_CONTEXT_BAN;

	prev_hang = xchg(&file_priv->hang_timestamp, jiffies);
	if (time_before(jiffies, prev_hang + I915_CLIENT_FAST_HANG_JIFFIES))
		score += I915_CLIENT_SCORE_HANG_FAST;

	if (score) {
		atomic_add(score, &file_priv->ban_score);

		drm_dbg(&ctx->i915->drm,
			"client %s: gained %u ban score, now %u\n",
			ctx->name, score,
			atomic_read(&file_priv->ban_score));
	}
}

static bool mark_guilty(struct i915_request *rq)
{
	struct i915_gem_context *ctx;
	unsigned long prev_hang;
	bool banned;
	int i;

	if (intel_context_is_closed(rq->context))
		return true;

	rcu_read_lock();
	ctx = rcu_dereference(rq->context->gem_context);
	if (ctx && !kref_get_unless_zero(&ctx->ref))
		ctx = NULL;
	rcu_read_unlock();
	if (!ctx)
		return intel_context_is_banned(rq->context);

	atomic_inc(&ctx->guilty_count);

	 
	if (!i915_gem_context_is_bannable(ctx)) {
		banned = false;
		goto out;
	}

	drm_notice(&ctx->i915->drm,
		   "%s context reset due to GPU hang\n",
		   ctx->name);

	 
	prev_hang = ctx->hang_timestamp[0];
	for (i = 0; i < ARRAY_SIZE(ctx->hang_timestamp) - 1; i++)
		ctx->hang_timestamp[i] = ctx->hang_timestamp[i + 1];
	ctx->hang_timestamp[i] = jiffies;

	 
	banned = !i915_gem_context_is_recoverable(ctx);
	if (time_before(jiffies, prev_hang + CONTEXT_FAST_HANG_JIFFIES))
		banned = true;
	if (banned)
		drm_dbg(&ctx->i915->drm, "context %s: guilty %d, banned\n",
			ctx->name, atomic_read(&ctx->guilty_count));

	client_mark_guilty(ctx, banned);

out:
	i915_gem_context_put(ctx);
	return banned;
}

static void mark_innocent(struct i915_request *rq)
{
	struct i915_gem_context *ctx;

	rcu_read_lock();
	ctx = rcu_dereference(rq->context->gem_context);
	if (ctx)
		atomic_inc(&ctx->active_count);
	rcu_read_unlock();
}

void __i915_request_reset(struct i915_request *rq, bool guilty)
{
	bool banned = false;

	RQ_TRACE(rq, "guilty? %s\n", str_yes_no(guilty));
	GEM_BUG_ON(__i915_request_is_complete(rq));

	rcu_read_lock();  
	if (guilty) {
		i915_request_set_error_once(rq, -EIO);
		__i915_request_skip(rq);
		banned = mark_guilty(rq);
	} else {
		i915_request_set_error_once(rq, -EAGAIN);
		mark_innocent(rq);
	}
	rcu_read_unlock();

	if (banned)
		intel_context_ban(rq->context, rq);
}

static bool i915_in_reset(struct pci_dev *pdev)
{
	u8 gdrst;

	pci_read_config_byte(pdev, I915_GDRST, &gdrst);
	return gdrst & GRDOM_RESET_STATUS;
}

static int i915_do_reset(struct intel_gt *gt,
			 intel_engine_mask_t engine_mask,
			 unsigned int retry)
{
	struct pci_dev *pdev = to_pci_dev(gt->i915->drm.dev);
	int err;

	 
	pci_write_config_byte(pdev, I915_GDRST, GRDOM_RESET_ENABLE);
	udelay(50);
	err = wait_for_atomic(i915_in_reset(pdev), 50);

	 
	pci_write_config_byte(pdev, I915_GDRST, 0);
	udelay(50);
	if (!err)
		err = wait_for_atomic(!i915_in_reset(pdev), 50);

	return err;
}

static bool g4x_reset_complete(struct pci_dev *pdev)
{
	u8 gdrst;

	pci_read_config_byte(pdev, I915_GDRST, &gdrst);
	return (gdrst & GRDOM_RESET_ENABLE) == 0;
}

static int g33_do_reset(struct intel_gt *gt,
			intel_engine_mask_t engine_mask,
			unsigned int retry)
{
	struct pci_dev *pdev = to_pci_dev(gt->i915->drm.dev);

	pci_write_config_byte(pdev, I915_GDRST, GRDOM_RESET_ENABLE);
	return wait_for_atomic(g4x_reset_complete(pdev), 50);
}

static int g4x_do_reset(struct intel_gt *gt,
			intel_engine_mask_t engine_mask,
			unsigned int retry)
{
	struct pci_dev *pdev = to_pci_dev(gt->i915->drm.dev);
	struct intel_uncore *uncore = gt->uncore;
	int ret;

	 
	intel_uncore_rmw_fw(uncore, VDECCLK_GATE_D, 0, VCP_UNIT_CLOCK_GATE_DISABLE);
	intel_uncore_posting_read_fw(uncore, VDECCLK_GATE_D);

	pci_write_config_byte(pdev, I915_GDRST,
			      GRDOM_MEDIA | GRDOM_RESET_ENABLE);
	ret =  wait_for_atomic(g4x_reset_complete(pdev), 50);
	if (ret) {
		GT_TRACE(gt, "Wait for media reset failed\n");
		goto out;
	}

	pci_write_config_byte(pdev, I915_GDRST,
			      GRDOM_RENDER | GRDOM_RESET_ENABLE);
	ret =  wait_for_atomic(g4x_reset_complete(pdev), 50);
	if (ret) {
		GT_TRACE(gt, "Wait for render reset failed\n");
		goto out;
	}

out:
	pci_write_config_byte(pdev, I915_GDRST, 0);

	intel_uncore_rmw_fw(uncore, VDECCLK_GATE_D, VCP_UNIT_CLOCK_GATE_DISABLE, 0);
	intel_uncore_posting_read_fw(uncore, VDECCLK_GATE_D);

	return ret;
}

static int ilk_do_reset(struct intel_gt *gt, intel_engine_mask_t engine_mask,
			unsigned int retry)
{
	struct intel_uncore *uncore = gt->uncore;
	int ret;

	intel_uncore_write_fw(uncore, ILK_GDSR,
			      ILK_GRDOM_RENDER | ILK_GRDOM_RESET_ENABLE);
	ret = __intel_wait_for_register_fw(uncore, ILK_GDSR,
					   ILK_GRDOM_RESET_ENABLE, 0,
					   5000, 0,
					   NULL);
	if (ret) {
		GT_TRACE(gt, "Wait for render reset failed\n");
		goto out;
	}

	intel_uncore_write_fw(uncore, ILK_GDSR,
			      ILK_GRDOM_MEDIA | ILK_GRDOM_RESET_ENABLE);
	ret = __intel_wait_for_register_fw(uncore, ILK_GDSR,
					   ILK_GRDOM_RESET_ENABLE, 0,
					   5000, 0,
					   NULL);
	if (ret) {
		GT_TRACE(gt, "Wait for media reset failed\n");
		goto out;
	}

out:
	intel_uncore_write_fw(uncore, ILK_GDSR, 0);
	intel_uncore_posting_read_fw(uncore, ILK_GDSR);
	return ret;
}

 
static int gen6_hw_domain_reset(struct intel_gt *gt, u32 hw_domain_mask)
{
	struct intel_uncore *uncore = gt->uncore;
	int loops;
	int err;

	 
	loops = GRAPHICS_VER_FULL(gt->i915) < IP_VER(12, 70) ? 2 : 1;

	 
	do {
		intel_uncore_write_fw(uncore, GEN6_GDRST, hw_domain_mask);

		 
		err = __intel_wait_for_register_fw(uncore, GEN6_GDRST,
						   hw_domain_mask, 0,
						   2000, 0,
						   NULL);
	} while (err == 0 && --loops);
	if (err)
		GT_TRACE(gt,
			 "Wait for 0x%08x engines reset failed\n",
			 hw_domain_mask);

	 
	udelay(50);

	return err;
}

static int __gen6_reset_engines(struct intel_gt *gt,
				intel_engine_mask_t engine_mask,
				unsigned int retry)
{
	struct intel_engine_cs *engine;
	u32 hw_mask;

	if (engine_mask == ALL_ENGINES) {
		hw_mask = GEN6_GRDOM_FULL;
	} else {
		intel_engine_mask_t tmp;

		hw_mask = 0;
		for_each_engine_masked(engine, gt, engine_mask, tmp) {
			hw_mask |= engine->reset_domain;
		}
	}

	return gen6_hw_domain_reset(gt, hw_mask);
}

static int gen6_reset_engines(struct intel_gt *gt,
			      intel_engine_mask_t engine_mask,
			      unsigned int retry)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&gt->uncore->lock, flags);
	ret = __gen6_reset_engines(gt, engine_mask, retry);
	spin_unlock_irqrestore(&gt->uncore->lock, flags);

	return ret;
}

static struct intel_engine_cs *find_sfc_paired_vecs_engine(struct intel_engine_cs *engine)
{
	int vecs_id;

	GEM_BUG_ON(engine->class != VIDEO_DECODE_CLASS);

	vecs_id = _VECS((engine->instance) / 2);

	return engine->gt->engine[vecs_id];
}

struct sfc_lock_data {
	i915_reg_t lock_reg;
	i915_reg_t ack_reg;
	i915_reg_t usage_reg;
	u32 lock_bit;
	u32 ack_bit;
	u32 usage_bit;
	u32 reset_bit;
};

static void get_sfc_forced_lock_data(struct intel_engine_cs *engine,
				     struct sfc_lock_data *sfc_lock)
{
	switch (engine->class) {
	default:
		MISSING_CASE(engine->class);
		fallthrough;
	case VIDEO_DECODE_CLASS:
		sfc_lock->lock_reg = GEN11_VCS_SFC_FORCED_LOCK(engine->mmio_base);
		sfc_lock->lock_bit = GEN11_VCS_SFC_FORCED_LOCK_BIT;

		sfc_lock->ack_reg = GEN11_VCS_SFC_LOCK_STATUS(engine->mmio_base);
		sfc_lock->ack_bit  = GEN11_VCS_SFC_LOCK_ACK_BIT;

		sfc_lock->usage_reg = GEN11_VCS_SFC_LOCK_STATUS(engine->mmio_base);
		sfc_lock->usage_bit = GEN11_VCS_SFC_USAGE_BIT;
		sfc_lock->reset_bit = GEN11_VCS_SFC_RESET_BIT(engine->instance);

		break;
	case VIDEO_ENHANCEMENT_CLASS:
		sfc_lock->lock_reg = GEN11_VECS_SFC_FORCED_LOCK(engine->mmio_base);
		sfc_lock->lock_bit = GEN11_VECS_SFC_FORCED_LOCK_BIT;

		sfc_lock->ack_reg = GEN11_VECS_SFC_LOCK_ACK(engine->mmio_base);
		sfc_lock->ack_bit  = GEN11_VECS_SFC_LOCK_ACK_BIT;

		sfc_lock->usage_reg = GEN11_VECS_SFC_USAGE(engine->mmio_base);
		sfc_lock->usage_bit = GEN11_VECS_SFC_USAGE_BIT;
		sfc_lock->reset_bit = GEN11_VECS_SFC_RESET_BIT(engine->instance);

		break;
	}
}

static int gen11_lock_sfc(struct intel_engine_cs *engine,
			  u32 *reset_mask,
			  u32 *unlock_mask)
{
	struct intel_uncore *uncore = engine->uncore;
	u8 vdbox_sfc_access = engine->gt->info.vdbox_sfc_access;
	struct sfc_lock_data sfc_lock;
	bool lock_obtained, lock_to_other = false;
	int ret;

	switch (engine->class) {
	case VIDEO_DECODE_CLASS:
		if ((BIT(engine->instance) & vdbox_sfc_access) == 0)
			return 0;

		fallthrough;
	case VIDEO_ENHANCEMENT_CLASS:
		get_sfc_forced_lock_data(engine, &sfc_lock);

		break;
	default:
		return 0;
	}

	if (!(intel_uncore_read_fw(uncore, sfc_lock.usage_reg) & sfc_lock.usage_bit)) {
		struct intel_engine_cs *paired_vecs;

		if (engine->class != VIDEO_DECODE_CLASS ||
		    GRAPHICS_VER(engine->i915) != 12)
			return 0;

		 
		if (!(intel_uncore_read_fw(uncore,
					   GEN12_HCP_SFC_LOCK_STATUS(engine->mmio_base)) &
		      GEN12_HCP_SFC_USAGE_BIT))
			return 0;

		paired_vecs = find_sfc_paired_vecs_engine(engine);
		get_sfc_forced_lock_data(paired_vecs, &sfc_lock);
		lock_to_other = true;
		*unlock_mask |= paired_vecs->mask;
	} else {
		*unlock_mask |= engine->mask;
	}

	 
	intel_uncore_rmw_fw(uncore, sfc_lock.lock_reg, 0, sfc_lock.lock_bit);

	ret = __intel_wait_for_register_fw(uncore,
					   sfc_lock.ack_reg,
					   sfc_lock.ack_bit,
					   sfc_lock.ack_bit,
					   1000, 0, NULL);

	 
	lock_obtained = (intel_uncore_read_fw(uncore, sfc_lock.usage_reg) &
			sfc_lock.usage_bit) != 0;
	if (lock_obtained == lock_to_other)
		return 0;

	if (ret) {
		ENGINE_TRACE(engine, "Wait for SFC forced lock ack failed\n");
		return ret;
	}

	*reset_mask |= sfc_lock.reset_bit;
	return 0;
}

static void gen11_unlock_sfc(struct intel_engine_cs *engine)
{
	struct intel_uncore *uncore = engine->uncore;
	u8 vdbox_sfc_access = engine->gt->info.vdbox_sfc_access;
	struct sfc_lock_data sfc_lock = {};

	if (engine->class != VIDEO_DECODE_CLASS &&
	    engine->class != VIDEO_ENHANCEMENT_CLASS)
		return;

	if (engine->class == VIDEO_DECODE_CLASS &&
	    (BIT(engine->instance) & vdbox_sfc_access) == 0)
		return;

	get_sfc_forced_lock_data(engine, &sfc_lock);

	intel_uncore_rmw_fw(uncore, sfc_lock.lock_reg, sfc_lock.lock_bit, 0);
}

static int __gen11_reset_engines(struct intel_gt *gt,
				 intel_engine_mask_t engine_mask,
				 unsigned int retry)
{
	struct intel_engine_cs *engine;
	intel_engine_mask_t tmp;
	u32 reset_mask, unlock_mask = 0;
	int ret;

	if (engine_mask == ALL_ENGINES) {
		reset_mask = GEN11_GRDOM_FULL;
	} else {
		reset_mask = 0;
		for_each_engine_masked(engine, gt, engine_mask, tmp) {
			reset_mask |= engine->reset_domain;
			ret = gen11_lock_sfc(engine, &reset_mask, &unlock_mask);
			if (ret)
				goto sfc_unlock;
		}
	}

	ret = gen6_hw_domain_reset(gt, reset_mask);

sfc_unlock:
	 
	for_each_engine_masked(engine, gt, unlock_mask, tmp)
		gen11_unlock_sfc(engine);

	return ret;
}

static int gen8_engine_reset_prepare(struct intel_engine_cs *engine)
{
	struct intel_uncore *uncore = engine->uncore;
	const i915_reg_t reg = RING_RESET_CTL(engine->mmio_base);
	u32 request, mask, ack;
	int ret;

	if (I915_SELFTEST_ONLY(should_fail(&engine->reset_timeout, 1)))
		return -ETIMEDOUT;

	ack = intel_uncore_read_fw(uncore, reg);
	if (ack & RESET_CTL_CAT_ERROR) {
		 
		request = RESET_CTL_CAT_ERROR;
		mask = RESET_CTL_CAT_ERROR;

		 
		ack = 0;
	} else if (!(ack & RESET_CTL_READY_TO_RESET)) {
		request = RESET_CTL_REQUEST_RESET;
		mask = RESET_CTL_READY_TO_RESET;
		ack = RESET_CTL_READY_TO_RESET;
	} else {
		return 0;
	}

	intel_uncore_write_fw(uncore, reg, _MASKED_BIT_ENABLE(request));
	ret = __intel_wait_for_register_fw(uncore, reg, mask, ack,
					   700, 0, NULL);
	if (ret)
		drm_err(&engine->i915->drm,
			"%s reset request timed out: {request: %08x, RESET_CTL: %08x}\n",
			engine->name, request,
			intel_uncore_read_fw(uncore, reg));

	return ret;
}

static void gen8_engine_reset_cancel(struct intel_engine_cs *engine)
{
	intel_uncore_write_fw(engine->uncore,
			      RING_RESET_CTL(engine->mmio_base),
			      _MASKED_BIT_DISABLE(RESET_CTL_REQUEST_RESET));
}

static int gen8_reset_engines(struct intel_gt *gt,
			      intel_engine_mask_t engine_mask,
			      unsigned int retry)
{
	struct intel_engine_cs *engine;
	const bool reset_non_ready = retry >= 1;
	intel_engine_mask_t tmp;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&gt->uncore->lock, flags);

	for_each_engine_masked(engine, gt, engine_mask, tmp) {
		ret = gen8_engine_reset_prepare(engine);
		if (ret && !reset_non_ready)
			goto skip_reset;

		 
	}

	 
	if (IS_DG2(gt->i915) && engine_mask == ALL_ENGINES)
		__gen11_reset_engines(gt, gt->info.engine_mask, 0);

	if (GRAPHICS_VER(gt->i915) >= 11)
		ret = __gen11_reset_engines(gt, engine_mask, retry);
	else
		ret = __gen6_reset_engines(gt, engine_mask, retry);

skip_reset:
	for_each_engine_masked(engine, gt, engine_mask, tmp)
		gen8_engine_reset_cancel(engine);

	spin_unlock_irqrestore(&gt->uncore->lock, flags);

	return ret;
}

static int mock_reset(struct intel_gt *gt,
		      intel_engine_mask_t mask,
		      unsigned int retry)
{
	return 0;
}

typedef int (*reset_func)(struct intel_gt *,
			  intel_engine_mask_t engine_mask,
			  unsigned int retry);

static reset_func intel_get_gpu_reset(const struct intel_gt *gt)
{
	struct drm_i915_private *i915 = gt->i915;

	if (is_mock_gt(gt))
		return mock_reset;
	else if (GRAPHICS_VER(i915) >= 8)
		return gen8_reset_engines;
	else if (GRAPHICS_VER(i915) >= 6)
		return gen6_reset_engines;
	else if (GRAPHICS_VER(i915) >= 5)
		return ilk_do_reset;
	else if (IS_G4X(i915))
		return g4x_do_reset;
	else if (IS_G33(i915) || IS_PINEVIEW(i915))
		return g33_do_reset;
	else if (GRAPHICS_VER(i915) >= 3)
		return i915_do_reset;
	else
		return NULL;
}

static int __reset_guc(struct intel_gt *gt)
{
	u32 guc_domain =
		GRAPHICS_VER(gt->i915) >= 11 ? GEN11_GRDOM_GUC : GEN9_GRDOM_GUC;

	return gen6_hw_domain_reset(gt, guc_domain);
}

static bool needs_wa_14015076503(struct intel_gt *gt, intel_engine_mask_t engine_mask)
{
	if (!IS_METEORLAKE(gt->i915) || !HAS_ENGINE(gt, GSC0))
		return false;

	if (!__HAS_ENGINE(engine_mask, GSC0))
		return false;

	return intel_gsc_uc_fw_init_done(&gt->uc.gsc);
}

static intel_engine_mask_t
wa_14015076503_start(struct intel_gt *gt, intel_engine_mask_t engine_mask, bool first)
{
	if (!needs_wa_14015076503(gt, engine_mask))
		return engine_mask;

	 
	if (engine_mask == ALL_ENGINES && first && intel_engine_is_idle(gt->engine[GSC0])) {
		__reset_guc(gt);
		engine_mask = gt->info.engine_mask & ~BIT(GSC0);
	} else {
		intel_uncore_rmw(gt->uncore,
				 HECI_H_GS1(MTL_GSC_HECI2_BASE),
				 0, HECI_H_GS1_ER_PREP);

		 
		intel_uncore_rmw(gt->uncore,
				 HECI_H_CSR(MTL_GSC_HECI2_BASE),
				 HECI_H_CSR_RST, HECI_H_CSR_IG);
		msleep(200);
	}

	return engine_mask;
}

static void
wa_14015076503_end(struct intel_gt *gt, intel_engine_mask_t engine_mask)
{
	if (!needs_wa_14015076503(gt, engine_mask))
		return;

	intel_uncore_rmw(gt->uncore,
			 HECI_H_GS1(MTL_GSC_HECI2_BASE),
			 HECI_H_GS1_ER_PREP, 0);
}

int __intel_gt_reset(struct intel_gt *gt, intel_engine_mask_t engine_mask)
{
	const int retries = engine_mask == ALL_ENGINES ? RESET_MAX_RETRIES : 1;
	reset_func reset;
	int ret = -ETIMEDOUT;
	int retry;

	reset = intel_get_gpu_reset(gt);
	if (!reset)
		return -ENODEV;

	 
	intel_uncore_forcewake_get(gt->uncore, FORCEWAKE_ALL);
	for (retry = 0; ret == -ETIMEDOUT && retry < retries; retry++) {
		intel_engine_mask_t reset_mask;

		reset_mask = wa_14015076503_start(gt, engine_mask, !retry);

		GT_TRACE(gt, "engine_mask=%x\n", reset_mask);
		preempt_disable();
		ret = reset(gt, reset_mask, retry);
		preempt_enable();

		wa_14015076503_end(gt, reset_mask);
	}
	intel_uncore_forcewake_put(gt->uncore, FORCEWAKE_ALL);

	return ret;
}

bool intel_has_gpu_reset(const struct intel_gt *gt)
{
	if (!gt->i915->params.reset)
		return NULL;

	return intel_get_gpu_reset(gt);
}

bool intel_has_reset_engine(const struct intel_gt *gt)
{
	if (gt->i915->params.reset < 2)
		return false;

	return INTEL_INFO(gt->i915)->has_reset_engine;
}

int intel_reset_guc(struct intel_gt *gt)
{
	int ret;

	GEM_BUG_ON(!HAS_GT_UC(gt->i915));

	intel_uncore_forcewake_get(gt->uncore, FORCEWAKE_ALL);
	ret = __reset_guc(gt);
	intel_uncore_forcewake_put(gt->uncore, FORCEWAKE_ALL);

	return ret;
}

 
static void reset_prepare_engine(struct intel_engine_cs *engine)
{
	 
	intel_uncore_forcewake_get(engine->uncore, FORCEWAKE_ALL);
	if (engine->reset.prepare)
		engine->reset.prepare(engine);
}

static void revoke_mmaps(struct intel_gt *gt)
{
	int i;

	for (i = 0; i < gt->ggtt->num_fences; i++) {
		struct drm_vma_offset_node *node;
		struct i915_vma *vma;
		u64 vma_offset;

		vma = READ_ONCE(gt->ggtt->fence_regs[i].vma);
		if (!vma)
			continue;

		if (!i915_vma_has_userfault(vma))
			continue;

		GEM_BUG_ON(vma->fence != &gt->ggtt->fence_regs[i]);

		if (!vma->mmo)
			continue;

		node = &vma->mmo->vma_node;
		vma_offset = vma->gtt_view.partial.offset << PAGE_SHIFT;

		unmap_mapping_range(gt->i915->drm.anon_inode->i_mapping,
				    drm_vma_node_offset_addr(node) + vma_offset,
				    vma->size,
				    1);
	}
}

static intel_engine_mask_t reset_prepare(struct intel_gt *gt)
{
	struct intel_engine_cs *engine;
	intel_engine_mask_t awake = 0;
	enum intel_engine_id id;

	 
	intel_uc_reset_prepare(&gt->uc);

	for_each_engine(engine, gt, id) {
		if (intel_engine_pm_get_if_awake(engine))
			awake |= engine->mask;
		reset_prepare_engine(engine);
	}

	return awake;
}

static void gt_revoke(struct intel_gt *gt)
{
	revoke_mmaps(gt);
}

static int gt_reset(struct intel_gt *gt, intel_engine_mask_t stalled_mask)
{
	struct intel_engine_cs *engine;
	enum intel_engine_id id;
	int err;

	 
	err = i915_ggtt_enable_hw(gt->i915);
	if (err)
		return err;

	local_bh_disable();
	for_each_engine(engine, gt, id)
		__intel_engine_reset(engine, stalled_mask & engine->mask);
	local_bh_enable();

	intel_uc_reset(&gt->uc, ALL_ENGINES);

	intel_ggtt_restore_fences(gt->ggtt);

	return err;
}

static void reset_finish_engine(struct intel_engine_cs *engine)
{
	if (engine->reset.finish)
		engine->reset.finish(engine);
	intel_uncore_forcewake_put(engine->uncore, FORCEWAKE_ALL);

	intel_engine_signal_breadcrumbs(engine);
}

static void reset_finish(struct intel_gt *gt, intel_engine_mask_t awake)
{
	struct intel_engine_cs *engine;
	enum intel_engine_id id;

	for_each_engine(engine, gt, id) {
		reset_finish_engine(engine);
		if (awake & engine->mask)
			intel_engine_pm_put(engine);
	}

	intel_uc_reset_finish(&gt->uc);
}

static void nop_submit_request(struct i915_request *request)
{
	RQ_TRACE(request, "-EIO\n");

	request = i915_request_mark_eio(request);
	if (request) {
		i915_request_submit(request);
		intel_engine_signal_breadcrumbs(request->engine);

		i915_request_put(request);
	}
}

static void __intel_gt_set_wedged(struct intel_gt *gt)
{
	struct intel_engine_cs *engine;
	intel_engine_mask_t awake;
	enum intel_engine_id id;

	if (test_bit(I915_WEDGED, &gt->reset.flags))
		return;

	GT_TRACE(gt, "start\n");

	 
	awake = reset_prepare(gt);

	 
	if (!INTEL_INFO(gt->i915)->gpu_reset_clobbers_display)
		__intel_gt_reset(gt, ALL_ENGINES);

	for_each_engine(engine, gt, id)
		engine->submit_request = nop_submit_request;

	 
	synchronize_rcu_expedited();
	set_bit(I915_WEDGED, &gt->reset.flags);

	 
	local_bh_disable();
	for_each_engine(engine, gt, id)
		if (engine->reset.cancel)
			engine->reset.cancel(engine);
	intel_uc_cancel_requests(&gt->uc);
	local_bh_enable();

	reset_finish(gt, awake);

	GT_TRACE(gt, "end\n");
}

void intel_gt_set_wedged(struct intel_gt *gt)
{
	intel_wakeref_t wakeref;

	if (test_bit(I915_WEDGED, &gt->reset.flags))
		return;

	wakeref = intel_runtime_pm_get(gt->uncore->rpm);
	mutex_lock(&gt->reset.mutex);

	if (GEM_SHOW_DEBUG()) {
		struct drm_printer p = drm_debug_printer(__func__);
		struct intel_engine_cs *engine;
		enum intel_engine_id id;

		drm_printf(&p, "called from %pS\n", (void *)_RET_IP_);
		for_each_engine(engine, gt, id) {
			if (intel_engine_is_idle(engine))
				continue;

			intel_engine_dump(engine, &p, "%s\n", engine->name);
		}
	}

	__intel_gt_set_wedged(gt);

	mutex_unlock(&gt->reset.mutex);
	intel_runtime_pm_put(gt->uncore->rpm, wakeref);
}

static bool __intel_gt_unset_wedged(struct intel_gt *gt)
{
	struct intel_gt_timelines *timelines = &gt->timelines;
	struct intel_timeline *tl;
	bool ok;

	if (!test_bit(I915_WEDGED, &gt->reset.flags))
		return true;

	 
	if (intel_gt_has_unrecoverable_error(gt))
		return false;

	GT_TRACE(gt, "start\n");

	 
	spin_lock(&timelines->lock);
	list_for_each_entry(tl, &timelines->active_list, link) {
		struct dma_fence *fence;

		fence = i915_active_fence_get(&tl->last_request);
		if (!fence)
			continue;

		spin_unlock(&timelines->lock);

		 
		dma_fence_default_wait(fence, false, MAX_SCHEDULE_TIMEOUT);
		dma_fence_put(fence);

		 
		spin_lock(&timelines->lock);
		tl = list_entry(&timelines->active_list, typeof(*tl), link);
	}
	spin_unlock(&timelines->lock);

	 
	ok = !HAS_EXECLISTS(gt->i915);  
	if (!INTEL_INFO(gt->i915)->gpu_reset_clobbers_display)
		ok = __intel_gt_reset(gt, ALL_ENGINES) == 0;
	if (!ok) {
		 
		add_taint_for_CI(gt->i915, TAINT_WARN);
		return false;
	}

	 
	intel_engines_reset_default_submission(gt);

	GT_TRACE(gt, "end\n");

	smp_mb__before_atomic();  
	clear_bit(I915_WEDGED, &gt->reset.flags);

	return true;
}

bool intel_gt_unset_wedged(struct intel_gt *gt)
{
	bool result;

	mutex_lock(&gt->reset.mutex);
	result = __intel_gt_unset_wedged(gt);
	mutex_unlock(&gt->reset.mutex);

	return result;
}

static int do_reset(struct intel_gt *gt, intel_engine_mask_t stalled_mask)
{
	int err, i;

	err = __intel_gt_reset(gt, ALL_ENGINES);
	for (i = 0; err && i < RESET_MAX_RETRIES; i++) {
		msleep(10 * (i + 1));
		err = __intel_gt_reset(gt, ALL_ENGINES);
	}
	if (err)
		return err;

	return gt_reset(gt, stalled_mask);
}

static int resume(struct intel_gt *gt)
{
	struct intel_engine_cs *engine;
	enum intel_engine_id id;
	int ret;

	for_each_engine(engine, gt, id) {
		ret = intel_engine_resume(engine);
		if (ret)
			return ret;
	}

	return 0;
}

 
void intel_gt_reset(struct intel_gt *gt,
		    intel_engine_mask_t stalled_mask,
		    const char *reason)
{
	intel_engine_mask_t awake;
	int ret;

	GT_TRACE(gt, "flags=%lx\n", gt->reset.flags);

	might_sleep();
	GEM_BUG_ON(!test_bit(I915_RESET_BACKOFF, &gt->reset.flags));

	 
	gt_revoke(gt);

	mutex_lock(&gt->reset.mutex);

	 
	if (!__intel_gt_unset_wedged(gt))
		goto unlock;

	if (reason)
		drm_notice(&gt->i915->drm,
			   "Resetting chip for %s\n", reason);
	atomic_inc(&gt->i915->gpu_error.reset_count);

	awake = reset_prepare(gt);

	if (!intel_has_gpu_reset(gt)) {
		if (gt->i915->params.reset)
			drm_err(&gt->i915->drm, "GPU reset not supported\n");
		else
			drm_dbg(&gt->i915->drm, "GPU reset disabled\n");
		goto error;
	}

	if (INTEL_INFO(gt->i915)->gpu_reset_clobbers_display)
		intel_runtime_pm_disable_interrupts(gt->i915);

	if (do_reset(gt, stalled_mask)) {
		drm_err(&gt->i915->drm, "Failed to reset chip\n");
		goto taint;
	}

	if (INTEL_INFO(gt->i915)->gpu_reset_clobbers_display)
		intel_runtime_pm_enable_interrupts(gt->i915);

	intel_overlay_reset(gt->i915);

	 
	ret = intel_gt_init_hw(gt);
	if (ret) {
		drm_err(&gt->i915->drm,
			"Failed to initialise HW following reset (%d)\n",
			ret);
		goto taint;
	}

	ret = resume(gt);
	if (ret)
		goto taint;

finish:
	reset_finish(gt, awake);
unlock:
	mutex_unlock(&gt->reset.mutex);
	return;

taint:
	 
	add_taint_for_CI(gt->i915, TAINT_WARN);
error:
	__intel_gt_set_wedged(gt);
	goto finish;
}

static int intel_gt_reset_engine(struct intel_engine_cs *engine)
{
	return __intel_gt_reset(engine->gt, engine->mask);
}

int __intel_engine_reset_bh(struct intel_engine_cs *engine, const char *msg)
{
	struct intel_gt *gt = engine->gt;
	int ret;

	ENGINE_TRACE(engine, "flags=%lx\n", gt->reset.flags);
	GEM_BUG_ON(!test_bit(I915_RESET_ENGINE + engine->id, &gt->reset.flags));

	if (intel_engine_uses_guc(engine))
		return -ENODEV;

	if (!intel_engine_pm_get_if_awake(engine))
		return 0;

	reset_prepare_engine(engine);

	if (msg)
		drm_notice(&engine->i915->drm,
			   "Resetting %s for %s\n", engine->name, msg);
	i915_increase_reset_engine_count(&engine->i915->gpu_error, engine);

	ret = intel_gt_reset_engine(engine);
	if (ret) {
		 
		ENGINE_TRACE(engine, "Failed to reset %s, err: %d\n", engine->name, ret);
		goto out;
	}

	 
	__intel_engine_reset(engine, true);

	 
	ret = intel_engine_resume(engine);

out:
	intel_engine_cancel_stop_cs(engine);
	reset_finish_engine(engine);
	intel_engine_pm_put_async(engine);
	return ret;
}

 
int intel_engine_reset(struct intel_engine_cs *engine, const char *msg)
{
	int err;

	local_bh_disable();
	err = __intel_engine_reset_bh(engine, msg);
	local_bh_enable();

	return err;
}

static void intel_gt_reset_global(struct intel_gt *gt,
				  u32 engine_mask,
				  const char *reason)
{
	struct kobject *kobj = &gt->i915->drm.primary->kdev->kobj;
	char *error_event[] = { I915_ERROR_UEVENT "=1", NULL };
	char *reset_event[] = { I915_RESET_UEVENT "=1", NULL };
	char *reset_done_event[] = { I915_ERROR_UEVENT "=0", NULL };
	struct intel_wedge_me w;

	kobject_uevent_env(kobj, KOBJ_CHANGE, error_event);

	GT_TRACE(gt, "resetting chip, engines=%x\n", engine_mask);
	kobject_uevent_env(kobj, KOBJ_CHANGE, reset_event);

	 
	intel_wedge_on_timeout(&w, gt, 60 * HZ) {
		intel_display_reset_prepare(gt->i915);

		intel_gt_reset(gt, engine_mask, reason);

		intel_display_reset_finish(gt->i915);
	}

	if (!test_bit(I915_WEDGED, &gt->reset.flags))
		kobject_uevent_env(kobj, KOBJ_CHANGE, reset_done_event);
}

 
void intel_gt_handle_error(struct intel_gt *gt,
			   intel_engine_mask_t engine_mask,
			   unsigned long flags,
			   const char *fmt, ...)
{
	struct intel_engine_cs *engine;
	intel_wakeref_t wakeref;
	intel_engine_mask_t tmp;
	char error_msg[80];
	char *msg = NULL;

	if (fmt) {
		va_list args;

		va_start(args, fmt);
		vscnprintf(error_msg, sizeof(error_msg), fmt, args);
		va_end(args);

		msg = error_msg;
	}

	 
	wakeref = intel_runtime_pm_get(gt->uncore->rpm);

	engine_mask &= gt->info.engine_mask;

	if (flags & I915_ERROR_CAPTURE) {
		i915_capture_error_state(gt, engine_mask, CORE_DUMP_FLAG_NONE);
		intel_gt_clear_error_registers(gt, engine_mask);
	}

	 
	if (!intel_uc_uses_guc_submission(&gt->uc) &&
	    intel_has_reset_engine(gt) && !intel_gt_is_wedged(gt)) {
		local_bh_disable();
		for_each_engine_masked(engine, gt, engine_mask, tmp) {
			BUILD_BUG_ON(I915_RESET_MODESET >= I915_RESET_ENGINE);
			if (test_and_set_bit(I915_RESET_ENGINE + engine->id,
					     &gt->reset.flags))
				continue;

			if (__intel_engine_reset_bh(engine, msg) == 0)
				engine_mask &= ~engine->mask;

			clear_and_wake_up_bit(I915_RESET_ENGINE + engine->id,
					      &gt->reset.flags);
		}
		local_bh_enable();
	}

	if (!engine_mask)
		goto out;

	 
	if (test_and_set_bit(I915_RESET_BACKOFF, &gt->reset.flags)) {
		wait_event(gt->reset.queue,
			   !test_bit(I915_RESET_BACKOFF, &gt->reset.flags));
		goto out;  
	}

	 
	synchronize_rcu_expedited();

	 
	if (!intel_uc_uses_guc_submission(&gt->uc)) {
		for_each_engine(engine, gt, tmp) {
			while (test_and_set_bit(I915_RESET_ENGINE + engine->id,
						&gt->reset.flags))
				wait_on_bit(&gt->reset.flags,
					    I915_RESET_ENGINE + engine->id,
					    TASK_UNINTERRUPTIBLE);
		}
	}

	 
	synchronize_srcu_expedited(&gt->reset.backoff_srcu);

	intel_gt_reset_global(gt, engine_mask, msg);

	if (!intel_uc_uses_guc_submission(&gt->uc)) {
		for_each_engine(engine, gt, tmp)
			clear_bit_unlock(I915_RESET_ENGINE + engine->id,
					 &gt->reset.flags);
	}
	clear_bit_unlock(I915_RESET_BACKOFF, &gt->reset.flags);
	smp_mb__after_atomic();
	wake_up_all(&gt->reset.queue);

out:
	intel_runtime_pm_put(gt->uncore->rpm, wakeref);
}

static int _intel_gt_reset_lock(struct intel_gt *gt, int *srcu, bool retry)
{
	might_lock(&gt->reset.backoff_srcu);
	if (retry)
		might_sleep();

	rcu_read_lock();
	while (test_bit(I915_RESET_BACKOFF, &gt->reset.flags)) {
		rcu_read_unlock();

		if (!retry)
			return -EBUSY;

		if (wait_event_interruptible(gt->reset.queue,
					     !test_bit(I915_RESET_BACKOFF,
						       &gt->reset.flags)))
			return -EINTR;

		rcu_read_lock();
	}
	*srcu = srcu_read_lock(&gt->reset.backoff_srcu);
	rcu_read_unlock();

	return 0;
}

int intel_gt_reset_trylock(struct intel_gt *gt, int *srcu)
{
	return _intel_gt_reset_lock(gt, srcu, false);
}

int intel_gt_reset_lock_interruptible(struct intel_gt *gt, int *srcu)
{
	return _intel_gt_reset_lock(gt, srcu, true);
}

void intel_gt_reset_unlock(struct intel_gt *gt, int tag)
__releases(&gt->reset.backoff_srcu)
{
	srcu_read_unlock(&gt->reset.backoff_srcu, tag);
}

int intel_gt_terminally_wedged(struct intel_gt *gt)
{
	might_sleep();

	if (!intel_gt_is_wedged(gt))
		return 0;

	if (intel_gt_has_unrecoverable_error(gt))
		return -EIO;

	 
	if (wait_event_interruptible(gt->reset.queue,
				     !test_bit(I915_RESET_BACKOFF,
					       &gt->reset.flags)))
		return -EINTR;

	return intel_gt_is_wedged(gt) ? -EIO : 0;
}

void intel_gt_set_wedged_on_init(struct intel_gt *gt)
{
	BUILD_BUG_ON(I915_RESET_ENGINE + I915_NUM_ENGINES >
		     I915_WEDGED_ON_INIT);
	intel_gt_set_wedged(gt);
	i915_disable_error_state(gt->i915, -ENODEV);
	set_bit(I915_WEDGED_ON_INIT, &gt->reset.flags);

	 
	add_taint_for_CI(gt->i915, TAINT_WARN);
}

void intel_gt_set_wedged_on_fini(struct intel_gt *gt)
{
	intel_gt_set_wedged(gt);
	i915_disable_error_state(gt->i915, -ENODEV);
	set_bit(I915_WEDGED_ON_FINI, &gt->reset.flags);
	intel_gt_retire_requests(gt);  
}

void intel_gt_init_reset(struct intel_gt *gt)
{
	init_waitqueue_head(&gt->reset.queue);
	mutex_init(&gt->reset.mutex);
	init_srcu_struct(&gt->reset.backoff_srcu);

	 
	i915_gem_shrinker_taints_mutex(gt->i915, &gt->reset.mutex);

	 
	__set_bit(I915_WEDGED, &gt->reset.flags);
}

void intel_gt_fini_reset(struct intel_gt *gt)
{
	cleanup_srcu_struct(&gt->reset.backoff_srcu);
}

static void intel_wedge_me(struct work_struct *work)
{
	struct intel_wedge_me *w = container_of(work, typeof(*w), work.work);

	drm_err(&w->gt->i915->drm,
		"%s timed out, cancelling all in-flight rendering.\n",
		w->name);
	intel_gt_set_wedged(w->gt);
}

void __intel_init_wedge(struct intel_wedge_me *w,
			struct intel_gt *gt,
			long timeout,
			const char *name)
{
	w->gt = gt;
	w->name = name;

	INIT_DELAYED_WORK_ONSTACK(&w->work, intel_wedge_me);
	queue_delayed_work(gt->i915->unordered_wq, &w->work, timeout);
}

void __intel_fini_wedge(struct intel_wedge_me *w)
{
	cancel_delayed_work_sync(&w->work);
	destroy_delayed_work_on_stack(&w->work);
	w->gt = NULL;
}

#if IS_ENABLED(CONFIG_DRM_I915_SELFTEST)
#include "selftest_reset.c"
#include "selftest_hangcheck.c"
#endif
