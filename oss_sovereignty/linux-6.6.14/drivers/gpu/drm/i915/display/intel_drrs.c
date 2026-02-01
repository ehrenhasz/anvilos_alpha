
 

#include "i915_drv.h"
#include "i915_reg.h"
#include "intel_atomic.h"
#include "intel_de.h"
#include "intel_display_types.h"
#include "intel_drrs.h"
#include "intel_panel.h"

 

const char *intel_drrs_type_str(enum drrs_type drrs_type)
{
	static const char * const str[] = {
		[DRRS_TYPE_NONE] = "none",
		[DRRS_TYPE_STATIC] = "static",
		[DRRS_TYPE_SEAMLESS] = "seamless",
	};

	if (drrs_type >= ARRAY_SIZE(str))
		return "<invalid>";

	return str[drrs_type];
}

static void
intel_drrs_set_refresh_rate_pipeconf(struct intel_crtc *crtc,
				     enum drrs_refresh_rate refresh_rate)
{
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum transcoder cpu_transcoder = crtc->drrs.cpu_transcoder;
	u32 bit;

	if (IS_VALLEYVIEW(dev_priv) || IS_CHERRYVIEW(dev_priv))
		bit = TRANSCONF_REFRESH_RATE_ALT_VLV;
	else
		bit = TRANSCONF_REFRESH_RATE_ALT_ILK;

	intel_de_rmw(dev_priv, TRANSCONF(cpu_transcoder),
		     bit, refresh_rate == DRRS_REFRESH_RATE_LOW ? bit : 0);
}

static void
intel_drrs_set_refresh_rate_m_n(struct intel_crtc *crtc,
				enum drrs_refresh_rate refresh_rate)
{
	intel_cpu_transcoder_set_m1_n1(crtc, crtc->drrs.cpu_transcoder,
				       refresh_rate == DRRS_REFRESH_RATE_LOW ?
				       &crtc->drrs.m2_n2 : &crtc->drrs.m_n);
}

bool intel_drrs_is_active(struct intel_crtc *crtc)
{
	return crtc->drrs.cpu_transcoder != INVALID_TRANSCODER;
}

static void intel_drrs_set_state(struct intel_crtc *crtc,
				 enum drrs_refresh_rate refresh_rate)
{
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);

	if (refresh_rate == crtc->drrs.refresh_rate)
		return;

	if (intel_cpu_transcoder_has_m2_n2(dev_priv, crtc->drrs.cpu_transcoder))
		intel_drrs_set_refresh_rate_pipeconf(crtc, refresh_rate);
	else
		intel_drrs_set_refresh_rate_m_n(crtc, refresh_rate);

	crtc->drrs.refresh_rate = refresh_rate;
}

static void intel_drrs_schedule_work(struct intel_crtc *crtc)
{
	struct drm_i915_private *i915 = to_i915(crtc->base.dev);

	mod_delayed_work(i915->unordered_wq, &crtc->drrs.work, msecs_to_jiffies(1000));
}

static unsigned int intel_drrs_frontbuffer_bits(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *i915 = to_i915(crtc->base.dev);
	unsigned int frontbuffer_bits;

	frontbuffer_bits = INTEL_FRONTBUFFER_ALL_MASK(crtc->pipe);

	for_each_intel_crtc_in_pipe_mask(&i915->drm, crtc,
					 crtc_state->bigjoiner_pipes)
		frontbuffer_bits |= INTEL_FRONTBUFFER_ALL_MASK(crtc->pipe);

	return frontbuffer_bits;
}

 
void intel_drrs_activate(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);

	if (!crtc_state->has_drrs)
		return;

	if (!crtc_state->hw.active)
		return;

	if (intel_crtc_is_bigjoiner_slave(crtc_state))
		return;

	mutex_lock(&crtc->drrs.mutex);

	crtc->drrs.cpu_transcoder = crtc_state->cpu_transcoder;
	crtc->drrs.m_n = crtc_state->dp_m_n;
	crtc->drrs.m2_n2 = crtc_state->dp_m2_n2;
	crtc->drrs.frontbuffer_bits = intel_drrs_frontbuffer_bits(crtc_state);
	crtc->drrs.busy_frontbuffer_bits = 0;

	intel_drrs_schedule_work(crtc);

	mutex_unlock(&crtc->drrs.mutex);
}

 
void intel_drrs_deactivate(const struct intel_crtc_state *old_crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(old_crtc_state->uapi.crtc);

	if (!old_crtc_state->has_drrs)
		return;

	if (!old_crtc_state->hw.active)
		return;

	if (intel_crtc_is_bigjoiner_slave(old_crtc_state))
		return;

	mutex_lock(&crtc->drrs.mutex);

	if (intel_drrs_is_active(crtc))
		intel_drrs_set_state(crtc, DRRS_REFRESH_RATE_HIGH);

	crtc->drrs.cpu_transcoder = INVALID_TRANSCODER;
	crtc->drrs.frontbuffer_bits = 0;
	crtc->drrs.busy_frontbuffer_bits = 0;

	mutex_unlock(&crtc->drrs.mutex);

	cancel_delayed_work_sync(&crtc->drrs.work);
}

static void intel_drrs_downclock_work(struct work_struct *work)
{
	struct intel_crtc *crtc = container_of(work, typeof(*crtc), drrs.work.work);

	mutex_lock(&crtc->drrs.mutex);

	if (intel_drrs_is_active(crtc) && !crtc->drrs.busy_frontbuffer_bits)
		intel_drrs_set_state(crtc, DRRS_REFRESH_RATE_LOW);

	mutex_unlock(&crtc->drrs.mutex);
}

static void intel_drrs_frontbuffer_update(struct drm_i915_private *dev_priv,
					  unsigned int all_frontbuffer_bits,
					  bool invalidate)
{
	struct intel_crtc *crtc;

	for_each_intel_crtc(&dev_priv->drm, crtc) {
		unsigned int frontbuffer_bits;

		mutex_lock(&crtc->drrs.mutex);

		frontbuffer_bits = all_frontbuffer_bits & crtc->drrs.frontbuffer_bits;
		if (!frontbuffer_bits) {
			mutex_unlock(&crtc->drrs.mutex);
			continue;
		}

		if (invalidate)
			crtc->drrs.busy_frontbuffer_bits |= frontbuffer_bits;
		else
			crtc->drrs.busy_frontbuffer_bits &= ~frontbuffer_bits;

		 
		intel_drrs_set_state(crtc, DRRS_REFRESH_RATE_HIGH);

		 
		if (!crtc->drrs.busy_frontbuffer_bits)
			intel_drrs_schedule_work(crtc);
		else
			cancel_delayed_work(&crtc->drrs.work);

		mutex_unlock(&crtc->drrs.mutex);
	}
}

 
void intel_drrs_invalidate(struct drm_i915_private *dev_priv,
			   unsigned int frontbuffer_bits)
{
	intel_drrs_frontbuffer_update(dev_priv, frontbuffer_bits, true);
}

 
void intel_drrs_flush(struct drm_i915_private *dev_priv,
		      unsigned int frontbuffer_bits)
{
	intel_drrs_frontbuffer_update(dev_priv, frontbuffer_bits, false);
}

 
void intel_drrs_crtc_init(struct intel_crtc *crtc)
{
	INIT_DELAYED_WORK(&crtc->drrs.work, intel_drrs_downclock_work);
	mutex_init(&crtc->drrs.mutex);
	crtc->drrs.cpu_transcoder = INVALID_TRANSCODER;
}

static int intel_drrs_debugfs_status_show(struct seq_file *m, void *unused)
{
	struct intel_crtc *crtc = m->private;
	const struct intel_crtc_state *crtc_state;
	int ret;

	ret = drm_modeset_lock_single_interruptible(&crtc->base.mutex);
	if (ret)
		return ret;

	crtc_state = to_intel_crtc_state(crtc->base.state);

	mutex_lock(&crtc->drrs.mutex);

	seq_printf(m, "DRRS enabled: %s\n",
		   str_yes_no(crtc_state->has_drrs));

	seq_printf(m, "DRRS active: %s\n",
		   str_yes_no(intel_drrs_is_active(crtc)));

	seq_printf(m, "DRRS refresh rate: %s\n",
		   crtc->drrs.refresh_rate == DRRS_REFRESH_RATE_LOW ?
		   "low" : "high");

	seq_printf(m, "DRRS busy frontbuffer bits: 0x%x\n",
		   crtc->drrs.busy_frontbuffer_bits);

	mutex_unlock(&crtc->drrs.mutex);

	drm_modeset_unlock(&crtc->base.mutex);

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(intel_drrs_debugfs_status);

static int intel_drrs_debugfs_ctl_set(void *data, u64 val)
{
	struct intel_crtc *crtc = data;
	struct drm_i915_private *i915 = to_i915(crtc->base.dev);
	struct intel_crtc_state *crtc_state;
	struct drm_crtc_commit *commit;
	int ret;

	ret = drm_modeset_lock_single_interruptible(&crtc->base.mutex);
	if (ret)
		return ret;

	crtc_state = to_intel_crtc_state(crtc->base.state);

	if (!crtc_state->hw.active ||
	    !crtc_state->has_drrs)
		goto out;

	commit = crtc_state->uapi.commit;
	if (commit) {
		ret = wait_for_completion_interruptible(&commit->hw_done);
		if (ret)
			goto out;
	}

	drm_dbg(&i915->drm,
		"Manually %sactivating DRRS\n", val ? "" : "de");

	if (val)
		intel_drrs_activate(crtc_state);
	else
		intel_drrs_deactivate(crtc_state);

out:
	drm_modeset_unlock(&crtc->base.mutex);

	return ret;
}

DEFINE_DEBUGFS_ATTRIBUTE(intel_drrs_debugfs_ctl_fops,
			 NULL, intel_drrs_debugfs_ctl_set, "%llu\n");

void intel_drrs_crtc_debugfs_add(struct intel_crtc *crtc)
{
	debugfs_create_file("i915_drrs_status", 0444, crtc->base.debugfs_entry,
			    crtc, &intel_drrs_debugfs_status_fops);

	debugfs_create_file_unsafe("i915_drrs_ctl", 0644, crtc->base.debugfs_entry,
				   crtc, &intel_drrs_debugfs_ctl_fops);
}

static int intel_drrs_debugfs_type_show(struct seq_file *m, void *unused)
{
	struct intel_connector *connector = m->private;

	seq_printf(m, "DRRS type: %s\n",
		   intel_drrs_type_str(intel_panel_drrs_type(connector)));

	return 0;
}

DEFINE_SHOW_ATTRIBUTE(intel_drrs_debugfs_type);

void intel_drrs_connector_debugfs_add(struct intel_connector *connector)
{
	if (intel_panel_drrs_type(connector) == DRRS_TYPE_NONE)
		return;

	debugfs_create_file("i915_drrs_type", 0444, connector->base.debugfs_entry,
			    connector, &intel_drrs_debugfs_type_fops);
}
