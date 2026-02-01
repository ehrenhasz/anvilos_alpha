
 

#include "i915_drv.h"
#include "i915_reg.h"
#include "intel_pcode.h"

static int gen6_check_mailbox_status(u32 mbox)
{
	switch (mbox & GEN6_PCODE_ERROR_MASK) {
	case GEN6_PCODE_SUCCESS:
		return 0;
	case GEN6_PCODE_UNIMPLEMENTED_CMD:
		return -ENODEV;
	case GEN6_PCODE_ILLEGAL_CMD:
		return -ENXIO;
	case GEN6_PCODE_MIN_FREQ_TABLE_GT_RATIO_OUT_OF_RANGE:
	case GEN7_PCODE_MIN_FREQ_TABLE_GT_RATIO_OUT_OF_RANGE:
		return -EOVERFLOW;
	case GEN6_PCODE_TIMEOUT:
		return -ETIMEDOUT;
	default:
		MISSING_CASE(mbox & GEN6_PCODE_ERROR_MASK);
		return 0;
	}
}

static int gen7_check_mailbox_status(u32 mbox)
{
	switch (mbox & GEN6_PCODE_ERROR_MASK) {
	case GEN6_PCODE_SUCCESS:
		return 0;
	case GEN6_PCODE_ILLEGAL_CMD:
		return -ENXIO;
	case GEN7_PCODE_TIMEOUT:
		return -ETIMEDOUT;
	case GEN7_PCODE_ILLEGAL_DATA:
		return -EINVAL;
	case GEN11_PCODE_ILLEGAL_SUBCOMMAND:
		return -ENXIO;
	case GEN11_PCODE_LOCKED:
		return -EBUSY;
	case GEN11_PCODE_REJECTED:
		return -EACCES;
	case GEN7_PCODE_MIN_FREQ_TABLE_GT_RATIO_OUT_OF_RANGE:
		return -EOVERFLOW;
	default:
		MISSING_CASE(mbox & GEN6_PCODE_ERROR_MASK);
		return 0;
	}
}

static int __snb_pcode_rw(struct intel_uncore *uncore, u32 mbox,
			  u32 *val, u32 *val1,
			  int fast_timeout_us, int slow_timeout_ms,
			  bool is_read)
{
	lockdep_assert_held(&uncore->i915->sb_lock);

	 

	if (intel_uncore_read_fw(uncore, GEN6_PCODE_MAILBOX) & GEN6_PCODE_READY)
		return -EAGAIN;

	intel_uncore_write_fw(uncore, GEN6_PCODE_DATA, *val);
	intel_uncore_write_fw(uncore, GEN6_PCODE_DATA1, val1 ? *val1 : 0);
	intel_uncore_write_fw(uncore,
			      GEN6_PCODE_MAILBOX, GEN6_PCODE_READY | mbox);

	if (__intel_wait_for_register_fw(uncore,
					 GEN6_PCODE_MAILBOX,
					 GEN6_PCODE_READY, 0,
					 fast_timeout_us,
					 slow_timeout_ms,
					 &mbox))
		return -ETIMEDOUT;

	if (is_read)
		*val = intel_uncore_read_fw(uncore, GEN6_PCODE_DATA);
	if (is_read && val1)
		*val1 = intel_uncore_read_fw(uncore, GEN6_PCODE_DATA1);

	if (GRAPHICS_VER(uncore->i915) > 6)
		return gen7_check_mailbox_status(mbox);
	else
		return gen6_check_mailbox_status(mbox);
}

int snb_pcode_read(struct intel_uncore *uncore, u32 mbox, u32 *val, u32 *val1)
{
	int err;

	mutex_lock(&uncore->i915->sb_lock);
	err = __snb_pcode_rw(uncore, mbox, val, val1, 500, 20, true);
	mutex_unlock(&uncore->i915->sb_lock);

	if (err) {
		drm_dbg(&uncore->i915->drm,
			"warning: pcode (read from mbox %x) mailbox access failed for %ps: %d\n",
			mbox, __builtin_return_address(0), err);
	}

	return err;
}

int snb_pcode_write_timeout(struct intel_uncore *uncore, u32 mbox, u32 val,
			    int fast_timeout_us, int slow_timeout_ms)
{
	int err;

	mutex_lock(&uncore->i915->sb_lock);
	err = __snb_pcode_rw(uncore, mbox, &val, NULL,
			     fast_timeout_us, slow_timeout_ms, false);
	mutex_unlock(&uncore->i915->sb_lock);

	if (err) {
		drm_dbg(&uncore->i915->drm,
			"warning: pcode (write of 0x%08x to mbox %x) mailbox access failed for %ps: %d\n",
			val, mbox, __builtin_return_address(0), err);
	}

	return err;
}

static bool skl_pcode_try_request(struct intel_uncore *uncore, u32 mbox,
				  u32 request, u32 reply_mask, u32 reply,
				  u32 *status)
{
	*status = __snb_pcode_rw(uncore, mbox, &request, NULL, 500, 0, true);

	return (*status == 0) && ((request & reply_mask) == reply);
}

 
int skl_pcode_request(struct intel_uncore *uncore, u32 mbox, u32 request,
		      u32 reply_mask, u32 reply, int timeout_base_ms)
{
	u32 status;
	int ret;

	mutex_lock(&uncore->i915->sb_lock);

#define COND \
	skl_pcode_try_request(uncore, mbox, request, reply_mask, reply, &status)

	 
	if (COND) {
		ret = 0;
		goto out;
	}
	ret = _wait_for(COND, timeout_base_ms * 1000, 10, 10);
	if (!ret)
		goto out;

	 
	drm_dbg_kms(&uncore->i915->drm,
		    "PCODE timeout, retrying with preemption disabled\n");
	drm_WARN_ON_ONCE(&uncore->i915->drm, timeout_base_ms > 3);
	preempt_disable();
	ret = wait_for_atomic(COND, 50);
	preempt_enable();

out:
	mutex_unlock(&uncore->i915->sb_lock);
	return status ? status : ret;
#undef COND
}

static int pcode_init_wait(struct intel_uncore *uncore, int timeout_ms)
{
	if (__intel_wait_for_register_fw(uncore,
					 GEN6_PCODE_MAILBOX,
					 GEN6_PCODE_READY, 0,
					 500, timeout_ms,
					 NULL))
		return -EPROBE_DEFER;

	return skl_pcode_request(uncore,
				 DG1_PCODE_STATUS,
				 DG1_UNCORE_GET_INIT_STATUS,
				 DG1_UNCORE_INIT_STATUS_COMPLETE,
				 DG1_UNCORE_INIT_STATUS_COMPLETE, timeout_ms);
}

int intel_pcode_init(struct intel_uncore *uncore)
{
	int err;

	if (!IS_DGFX(uncore->i915))
		return 0;

	 
	err = pcode_init_wait(uncore, 10000);

	if (err) {
		drm_notice(&uncore->i915->drm,
			   "Waiting for HW initialisation...\n");
		err = pcode_init_wait(uncore, 180000);
	}

	return err;
}

int snb_pcode_read_p(struct intel_uncore *uncore, u32 mbcmd, u32 p1, u32 p2, u32 *val)
{
	intel_wakeref_t wakeref;
	u32 mbox;
	int err;

	mbox = REG_FIELD_PREP(GEN6_PCODE_MB_COMMAND, mbcmd)
		| REG_FIELD_PREP(GEN6_PCODE_MB_PARAM1, p1)
		| REG_FIELD_PREP(GEN6_PCODE_MB_PARAM2, p2);

	with_intel_runtime_pm(uncore->rpm, wakeref)
		err = snb_pcode_read(uncore, mbox, val, NULL);

	return err;
}

int snb_pcode_write_p(struct intel_uncore *uncore, u32 mbcmd, u32 p1, u32 p2, u32 val)
{
	intel_wakeref_t wakeref;
	u32 mbox;
	int err;

	mbox = REG_FIELD_PREP(GEN6_PCODE_MB_COMMAND, mbcmd)
		| REG_FIELD_PREP(GEN6_PCODE_MB_PARAM1, p1)
		| REG_FIELD_PREP(GEN6_PCODE_MB_PARAM2, p2);

	with_intel_runtime_pm(uncore->rpm, wakeref)
		err = snb_pcode_write(uncore, mbox, val);

	return err;
}
