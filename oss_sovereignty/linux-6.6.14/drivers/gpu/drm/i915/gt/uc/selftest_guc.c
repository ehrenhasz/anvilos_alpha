
 

#include "gt/intel_gt_print.h"
#include "intel_guc_print.h"
#include "selftests/igt_spinner.h"
#include "selftests/intel_scheduler_helpers.h"

static int request_add_spin(struct i915_request *rq, struct igt_spinner *spin)
{
	int err = 0;

	i915_request_get(rq);
	i915_request_add(rq);
	if (spin && !igt_wait_for_spinner(spin, rq))
		err = -ETIMEDOUT;

	return err;
}

static struct i915_request *nop_user_request(struct intel_context *ce,
					     struct i915_request *from)
{
	struct i915_request *rq;
	int ret;

	rq = intel_context_create_request(ce);
	if (IS_ERR(rq))
		return rq;

	if (from) {
		ret = i915_sw_fence_await_dma_fence(&rq->submit,
						    &from->fence, 0,
						    I915_FENCE_GFP);
		if (ret < 0) {
			i915_request_put(rq);
			return ERR_PTR(ret);
		}
	}

	i915_request_get(rq);
	i915_request_add(rq);

	return rq;
}

static int intel_guc_scrub_ctbs(void *arg)
{
	struct intel_gt *gt = arg;
	int ret = 0;
	int i;
	struct i915_request *last[3] = {NULL, NULL, NULL}, *rq;
	intel_wakeref_t wakeref;
	struct intel_engine_cs *engine;
	struct intel_context *ce;

	if (!intel_has_gpu_reset(gt))
		return 0;

	wakeref = intel_runtime_pm_get(gt->uncore->rpm);
	engine = intel_selftest_find_any_engine(gt);

	 
	for (i = 0; i < 3; ++i) {
		ce = intel_context_create(engine);
		if (IS_ERR(ce)) {
			ret = PTR_ERR(ce);
			gt_err(gt, "Failed to create context %d: %pe\n", i, ce);
			goto err;
		}

		switch (i) {
		case 0:
			ce->drop_schedule_enable = true;
			break;
		case 1:
			ce->drop_schedule_disable = true;
			break;
		case 2:
			ce->drop_deregister = true;
			break;
		}

		rq = nop_user_request(ce, NULL);
		intel_context_put(ce);

		if (IS_ERR(rq)) {
			ret = PTR_ERR(rq);
			gt_err(gt, "Failed to create request %d: %pe\n", i, rq);
			goto err;
		}

		last[i] = rq;
	}

	for (i = 0; i < 3; ++i) {
		ret = i915_request_wait(last[i], 0, HZ);
		if (ret < 0) {
			gt_err(gt, "Last request failed to complete: %pe\n", ERR_PTR(ret));
			goto err;
		}
		i915_request_put(last[i]);
		last[i] = NULL;
	}

	 
	intel_gt_retire_requests(gt);
	msleep(500);

	 
	intel_gt_handle_error(engine->gt, -1, 0, "selftest reset");

	 
	ret = intel_gt_wait_for_idle(gt, HZ);
	if (ret < 0) {
		gt_err(gt, "GT failed to idle: %pe\n", ERR_PTR(ret));
		goto err;
	}

err:
	for (i = 0; i < 3; ++i)
		if (last[i])
			i915_request_put(last[i]);
	intel_runtime_pm_put(gt->uncore->rpm, wakeref);

	return ret;
}

 
static int intel_guc_steal_guc_ids(void *arg)
{
	struct intel_gt *gt = arg;
	struct intel_guc *guc = &gt->uc.guc;
	int ret, sv, context_index = 0;
	intel_wakeref_t wakeref;
	struct intel_engine_cs *engine;
	struct intel_context **ce;
	struct igt_spinner spin;
	struct i915_request *spin_rq = NULL, *rq, *last = NULL;
	int number_guc_id_stolen = guc->number_guc_id_stolen;

	ce = kcalloc(GUC_MAX_CONTEXT_ID, sizeof(*ce), GFP_KERNEL);
	if (!ce) {
		guc_err(guc, "Context array allocation failed\n");
		return -ENOMEM;
	}

	wakeref = intel_runtime_pm_get(gt->uncore->rpm);
	engine = intel_selftest_find_any_engine(gt);
	sv = guc->submission_state.num_guc_ids;
	guc->submission_state.num_guc_ids = 512;

	 
	ce[context_index] = intel_context_create(engine);
	if (IS_ERR(ce[context_index])) {
		ret = PTR_ERR(ce[context_index]);
		guc_err(guc, "Failed to create context: %pe\n", ce[context_index]);
		ce[context_index] = NULL;
		goto err_wakeref;
	}
	ret = igt_spinner_init(&spin, engine->gt);
	if (ret) {
		guc_err(guc, "Failed to create spinner: %pe\n", ERR_PTR(ret));
		goto err_contexts;
	}
	spin_rq = igt_spinner_create_request(&spin, ce[context_index],
					     MI_ARB_CHECK);
	if (IS_ERR(spin_rq)) {
		ret = PTR_ERR(spin_rq);
		guc_err(guc, "Failed to create spinner request: %pe\n", spin_rq);
		goto err_contexts;
	}
	ret = request_add_spin(spin_rq, &spin);
	if (ret) {
		guc_err(guc, "Failed to add Spinner request: %pe\n", ERR_PTR(ret));
		goto err_spin_rq;
	}

	 
	while (ret != -EAGAIN) {
		ce[++context_index] = intel_context_create(engine);
		if (IS_ERR(ce[context_index])) {
			ret = PTR_ERR(ce[context_index]);
			guc_err(guc, "Failed to create context: %pe\n", ce[context_index]);
			ce[context_index--] = NULL;
			goto err_spin_rq;
		}

		rq = nop_user_request(ce[context_index], spin_rq);
		if (IS_ERR(rq)) {
			ret = PTR_ERR(rq);
			rq = NULL;
			if ((ret != -EAGAIN) || !last) {
				guc_err(guc, "Failed to create %srequest %d: %pe\n",
					last ? "" : "first ", context_index, ERR_PTR(ret));
				goto err_spin_rq;
			}
		} else {
			if (last)
				i915_request_put(last);
			last = rq;
		}
	}

	 
	igt_spinner_end(&spin);
	ret = intel_selftest_wait_for_rq(spin_rq);
	if (ret) {
		guc_err(guc, "Spin request failed to complete: %pe\n", ERR_PTR(ret));
		i915_request_put(last);
		goto err_spin_rq;
	}
	i915_request_put(spin_rq);
	igt_spinner_fini(&spin);
	spin_rq = NULL;

	 
	ret = i915_request_wait(last, 0, HZ * 30);
	i915_request_put(last);
	if (ret < 0) {
		guc_err(guc, "Last request failed to complete: %pe\n", ERR_PTR(ret));
		goto err_spin_rq;
	}

	 
	rq = nop_user_request(ce[context_index], NULL);
	if (IS_ERR(rq)) {
		ret = PTR_ERR(rq);
		guc_err(guc, "Failed to steal guc_id %d: %pe\n", context_index, rq);
		goto err_spin_rq;
	}

	 
	ret = i915_request_wait(rq, 0, HZ);
	i915_request_put(rq);
	if (ret < 0) {
		guc_err(guc, "Request with stolen guc_id failed to complete: %pe\n", ERR_PTR(ret));
		goto err_spin_rq;
	}

	 
	ret = intel_gt_wait_for_idle(gt, HZ * 30);
	if (ret < 0) {
		guc_err(guc, "GT failed to idle: %pe\n", ERR_PTR(ret));
		goto err_spin_rq;
	}

	 
	if (guc->number_guc_id_stolen == number_guc_id_stolen) {
		guc_err(guc, "No guc_id was stolen");
		ret = -EINVAL;
	} else {
		ret = 0;
	}

err_spin_rq:
	if (spin_rq) {
		igt_spinner_end(&spin);
		intel_selftest_wait_for_rq(spin_rq);
		i915_request_put(spin_rq);
		igt_spinner_fini(&spin);
		intel_gt_wait_for_idle(gt, HZ * 30);
	}
err_contexts:
	for (; context_index >= 0 && ce[context_index]; --context_index)
		intel_context_put(ce[context_index]);
err_wakeref:
	intel_runtime_pm_put(gt->uncore->rpm, wakeref);
	kfree(ce);
	guc->submission_state.num_guc_ids = sv;

	return ret;
}

int intel_guc_live_selftests(struct drm_i915_private *i915)
{
	static const struct i915_subtest tests[] = {
		SUBTEST(intel_guc_scrub_ctbs),
		SUBTEST(intel_guc_steal_guc_ids),
	};
	struct intel_gt *gt = to_gt(i915);

	if (intel_gt_is_wedged(gt))
		return 0;

	if (!intel_uc_uses_guc_submission(&gt->uc))
		return 0;

	return intel_gt_live_subtests(tests, gt);
}
