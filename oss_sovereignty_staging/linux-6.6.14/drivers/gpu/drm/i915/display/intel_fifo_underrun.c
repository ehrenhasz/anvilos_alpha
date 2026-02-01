 

#include "i915_drv.h"
#include "i915_reg.h"
#include "intel_de.h"
#include "intel_display_irq.h"
#include "intel_display_trace.h"
#include "intel_display_types.h"
#include "intel_fbc.h"
#include "intel_fifo_underrun.h"
#include "intel_pch_display.h"

 

static bool ivb_can_enable_err_int(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = to_i915(dev);
	struct intel_crtc *crtc;
	enum pipe pipe;

	lockdep_assert_held(&dev_priv->irq_lock);

	for_each_pipe(dev_priv, pipe) {
		crtc = intel_crtc_for_pipe(dev_priv, pipe);

		if (crtc->cpu_fifo_underrun_disabled)
			return false;
	}

	return true;
}

static bool cpt_can_enable_serr_int(struct drm_device *dev)
{
	struct drm_i915_private *dev_priv = to_i915(dev);
	enum pipe pipe;
	struct intel_crtc *crtc;

	lockdep_assert_held(&dev_priv->irq_lock);

	for_each_pipe(dev_priv, pipe) {
		crtc = intel_crtc_for_pipe(dev_priv, pipe);

		if (crtc->pch_fifo_underrun_disabled)
			return false;
	}

	return true;
}

static void i9xx_check_fifo_underruns(struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	i915_reg_t reg = PIPESTAT(crtc->pipe);
	u32 enable_mask;

	lockdep_assert_held(&dev_priv->irq_lock);

	if ((intel_de_read(dev_priv, reg) & PIPE_FIFO_UNDERRUN_STATUS) == 0)
		return;

	enable_mask = i915_pipestat_enable_mask(dev_priv, crtc->pipe);
	intel_de_write(dev_priv, reg, enable_mask | PIPE_FIFO_UNDERRUN_STATUS);
	intel_de_posting_read(dev_priv, reg);

	trace_intel_cpu_fifo_underrun(dev_priv, crtc->pipe);
	drm_err(&dev_priv->drm, "pipe %c underrun\n", pipe_name(crtc->pipe));
}

static void i9xx_set_fifo_underrun_reporting(struct drm_device *dev,
					     enum pipe pipe,
					     bool enable, bool old)
{
	struct drm_i915_private *dev_priv = to_i915(dev);
	i915_reg_t reg = PIPESTAT(pipe);

	lockdep_assert_held(&dev_priv->irq_lock);

	if (enable) {
		u32 enable_mask = i915_pipestat_enable_mask(dev_priv, pipe);

		intel_de_write(dev_priv, reg,
			       enable_mask | PIPE_FIFO_UNDERRUN_STATUS);
		intel_de_posting_read(dev_priv, reg);
	} else {
		if (old && intel_de_read(dev_priv, reg) & PIPE_FIFO_UNDERRUN_STATUS)
			drm_err(&dev_priv->drm, "pipe %c underrun\n",
				pipe_name(pipe));
	}
}

static void ilk_set_fifo_underrun_reporting(struct drm_device *dev,
					    enum pipe pipe, bool enable)
{
	struct drm_i915_private *dev_priv = to_i915(dev);
	u32 bit = (pipe == PIPE_A) ?
		DE_PIPEA_FIFO_UNDERRUN : DE_PIPEB_FIFO_UNDERRUN;

	if (enable)
		ilk_enable_display_irq(dev_priv, bit);
	else
		ilk_disable_display_irq(dev_priv, bit);
}

static void ivb_check_fifo_underruns(struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;
	u32 err_int = intel_de_read(dev_priv, GEN7_ERR_INT);

	lockdep_assert_held(&dev_priv->irq_lock);

	if ((err_int & ERR_INT_FIFO_UNDERRUN(pipe)) == 0)
		return;

	intel_de_write(dev_priv, GEN7_ERR_INT, ERR_INT_FIFO_UNDERRUN(pipe));
	intel_de_posting_read(dev_priv, GEN7_ERR_INT);

	trace_intel_cpu_fifo_underrun(dev_priv, pipe);
	drm_err(&dev_priv->drm, "fifo underrun on pipe %c\n", pipe_name(pipe));
}

static void ivb_set_fifo_underrun_reporting(struct drm_device *dev,
					    enum pipe pipe, bool enable,
					    bool old)
{
	struct drm_i915_private *dev_priv = to_i915(dev);
	if (enable) {
		intel_de_write(dev_priv, GEN7_ERR_INT,
			       ERR_INT_FIFO_UNDERRUN(pipe));

		if (!ivb_can_enable_err_int(dev))
			return;

		ilk_enable_display_irq(dev_priv, DE_ERR_INT_IVB);
	} else {
		ilk_disable_display_irq(dev_priv, DE_ERR_INT_IVB);

		if (old &&
		    intel_de_read(dev_priv, GEN7_ERR_INT) & ERR_INT_FIFO_UNDERRUN(pipe)) {
			drm_err(&dev_priv->drm,
				"uncleared fifo underrun on pipe %c\n",
				pipe_name(pipe));
		}
	}
}

static u32
icl_pipe_status_underrun_mask(struct drm_i915_private *dev_priv)
{
	u32 mask = PIPE_STATUS_UNDERRUN;

	if (DISPLAY_VER(dev_priv) >= 13)
		mask |= PIPE_STATUS_SOFT_UNDERRUN_XELPD |
			PIPE_STATUS_HARD_UNDERRUN_XELPD |
			PIPE_STATUS_PORT_UNDERRUN_XELPD;

	return mask;
}

static void bdw_set_fifo_underrun_reporting(struct drm_device *dev,
					    enum pipe pipe, bool enable)
{
	struct drm_i915_private *dev_priv = to_i915(dev);
	u32 mask = gen8_de_pipe_underrun_mask(dev_priv);

	if (enable) {
		if (DISPLAY_VER(dev_priv) >= 11)
			intel_de_write(dev_priv, ICL_PIPESTATUS(pipe),
				       icl_pipe_status_underrun_mask(dev_priv));

		bdw_enable_pipe_irq(dev_priv, pipe, mask);
	} else {
		bdw_disable_pipe_irq(dev_priv, pipe, mask);
	}
}

static void ibx_set_fifo_underrun_reporting(struct drm_device *dev,
					    enum pipe pch_transcoder,
					    bool enable)
{
	struct drm_i915_private *dev_priv = to_i915(dev);
	u32 bit = (pch_transcoder == PIPE_A) ?
		SDE_TRANSA_FIFO_UNDER : SDE_TRANSB_FIFO_UNDER;

	if (enable)
		ibx_enable_display_interrupt(dev_priv, bit);
	else
		ibx_disable_display_interrupt(dev_priv, bit);
}

static void cpt_check_pch_fifo_underruns(struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pch_transcoder = crtc->pipe;
	u32 serr_int = intel_de_read(dev_priv, SERR_INT);

	lockdep_assert_held(&dev_priv->irq_lock);

	if ((serr_int & SERR_INT_TRANS_FIFO_UNDERRUN(pch_transcoder)) == 0)
		return;

	intel_de_write(dev_priv, SERR_INT,
		       SERR_INT_TRANS_FIFO_UNDERRUN(pch_transcoder));
	intel_de_posting_read(dev_priv, SERR_INT);

	trace_intel_pch_fifo_underrun(dev_priv, pch_transcoder);
	drm_err(&dev_priv->drm, "pch fifo underrun on pch transcoder %c\n",
		pipe_name(pch_transcoder));
}

static void cpt_set_fifo_underrun_reporting(struct drm_device *dev,
					    enum pipe pch_transcoder,
					    bool enable, bool old)
{
	struct drm_i915_private *dev_priv = to_i915(dev);

	if (enable) {
		intel_de_write(dev_priv, SERR_INT,
			       SERR_INT_TRANS_FIFO_UNDERRUN(pch_transcoder));

		if (!cpt_can_enable_serr_int(dev))
			return;

		ibx_enable_display_interrupt(dev_priv, SDE_ERROR_CPT);
	} else {
		ibx_disable_display_interrupt(dev_priv, SDE_ERROR_CPT);

		if (old && intel_de_read(dev_priv, SERR_INT) &
		    SERR_INT_TRANS_FIFO_UNDERRUN(pch_transcoder)) {
			drm_err(&dev_priv->drm,
				"uncleared pch fifo underrun on pch transcoder %c\n",
				pipe_name(pch_transcoder));
		}
	}
}

static bool __intel_set_cpu_fifo_underrun_reporting(struct drm_device *dev,
						    enum pipe pipe, bool enable)
{
	struct drm_i915_private *dev_priv = to_i915(dev);
	struct intel_crtc *crtc = intel_crtc_for_pipe(dev_priv, pipe);
	bool old;

	lockdep_assert_held(&dev_priv->irq_lock);

	old = !crtc->cpu_fifo_underrun_disabled;
	crtc->cpu_fifo_underrun_disabled = !enable;

	if (HAS_GMCH(dev_priv))
		i9xx_set_fifo_underrun_reporting(dev, pipe, enable, old);
	else if (IS_IRONLAKE(dev_priv) || IS_SANDYBRIDGE(dev_priv))
		ilk_set_fifo_underrun_reporting(dev, pipe, enable);
	else if (DISPLAY_VER(dev_priv) == 7)
		ivb_set_fifo_underrun_reporting(dev, pipe, enable, old);
	else if (DISPLAY_VER(dev_priv) >= 8)
		bdw_set_fifo_underrun_reporting(dev, pipe, enable);

	return old;
}

 
bool intel_set_cpu_fifo_underrun_reporting(struct drm_i915_private *dev_priv,
					   enum pipe pipe, bool enable)
{
	unsigned long flags;
	bool ret;

	spin_lock_irqsave(&dev_priv->irq_lock, flags);
	ret = __intel_set_cpu_fifo_underrun_reporting(&dev_priv->drm, pipe,
						      enable);
	spin_unlock_irqrestore(&dev_priv->irq_lock, flags);

	return ret;
}

 
bool intel_set_pch_fifo_underrun_reporting(struct drm_i915_private *dev_priv,
					   enum pipe pch_transcoder,
					   bool enable)
{
	struct intel_crtc *crtc =
		intel_crtc_for_pipe(dev_priv, pch_transcoder);
	unsigned long flags;
	bool old;

	 

	spin_lock_irqsave(&dev_priv->irq_lock, flags);

	old = !crtc->pch_fifo_underrun_disabled;
	crtc->pch_fifo_underrun_disabled = !enable;

	if (HAS_PCH_IBX(dev_priv))
		ibx_set_fifo_underrun_reporting(&dev_priv->drm,
						pch_transcoder,
						enable);
	else
		cpt_set_fifo_underrun_reporting(&dev_priv->drm,
						pch_transcoder,
						enable, old);

	spin_unlock_irqrestore(&dev_priv->irq_lock, flags);
	return old;
}

 
void intel_cpu_fifo_underrun_irq_handler(struct drm_i915_private *dev_priv,
					 enum pipe pipe)
{
	struct intel_crtc *crtc = intel_crtc_for_pipe(dev_priv, pipe);
	u32 underruns = 0;

	 
	if (crtc == NULL)
		return;

	 
	if (HAS_GMCH(dev_priv) &&
	    crtc->cpu_fifo_underrun_disabled)
		return;

	 
	if (DISPLAY_VER(dev_priv) >= 11) {
		underruns = intel_de_read(dev_priv, ICL_PIPESTATUS(pipe)) &
			icl_pipe_status_underrun_mask(dev_priv);
		intel_de_write(dev_priv, ICL_PIPESTATUS(pipe), underruns);
	}

	if (intel_set_cpu_fifo_underrun_reporting(dev_priv, pipe, false)) {
		trace_intel_cpu_fifo_underrun(dev_priv, pipe);

		if (DISPLAY_VER(dev_priv) >= 11)
			drm_err(&dev_priv->drm, "CPU pipe %c FIFO underrun: %s%s%s%s\n",
				pipe_name(pipe),
				underruns & PIPE_STATUS_SOFT_UNDERRUN_XELPD ? "soft," : "",
				underruns & PIPE_STATUS_HARD_UNDERRUN_XELPD ? "hard," : "",
				underruns & PIPE_STATUS_PORT_UNDERRUN_XELPD ? "port," : "",
				underruns & PIPE_STATUS_UNDERRUN ? "transcoder," : "");
		else
			drm_err(&dev_priv->drm, "CPU pipe %c FIFO underrun\n", pipe_name(pipe));
	}

	intel_fbc_handle_fifo_underrun_irq(dev_priv);
}

 
void intel_pch_fifo_underrun_irq_handler(struct drm_i915_private *dev_priv,
					 enum pipe pch_transcoder)
{
	if (intel_set_pch_fifo_underrun_reporting(dev_priv, pch_transcoder,
						  false)) {
		trace_intel_pch_fifo_underrun(dev_priv, pch_transcoder);
		drm_err(&dev_priv->drm, "PCH transcoder %c FIFO underrun\n",
			pipe_name(pch_transcoder));
	}
}

 
void intel_check_cpu_fifo_underruns(struct drm_i915_private *dev_priv)
{
	struct intel_crtc *crtc;

	spin_lock_irq(&dev_priv->irq_lock);

	for_each_intel_crtc(&dev_priv->drm, crtc) {
		if (crtc->cpu_fifo_underrun_disabled)
			continue;

		if (HAS_GMCH(dev_priv))
			i9xx_check_fifo_underruns(crtc);
		else if (DISPLAY_VER(dev_priv) == 7)
			ivb_check_fifo_underruns(crtc);
	}

	spin_unlock_irq(&dev_priv->irq_lock);
}

 
void intel_check_pch_fifo_underruns(struct drm_i915_private *dev_priv)
{
	struct intel_crtc *crtc;

	spin_lock_irq(&dev_priv->irq_lock);

	for_each_intel_crtc(&dev_priv->drm, crtc) {
		if (crtc->pch_fifo_underrun_disabled)
			continue;

		if (HAS_PCH_CPT(dev_priv))
			cpt_check_pch_fifo_underruns(crtc);
	}

	spin_unlock_irq(&dev_priv->irq_lock);
}

void intel_init_fifo_underrun_reporting(struct drm_i915_private *i915,
					struct intel_crtc *crtc,
					bool enable)
{
	crtc->cpu_fifo_underrun_disabled = !enable;

	 
	if (intel_has_pch_trancoder(i915, crtc->pipe))
		crtc->pch_fifo_underrun_disabled = !enable;
}
