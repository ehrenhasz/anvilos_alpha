
 

#include "i915_drv.h"

#include "intel_pxp.h"
#include "intel_pxp_irq.h"
#include "intel_pxp_pm.h"
#include "intel_pxp_session.h"
#include "intel_pxp_types.h"

void intel_pxp_suspend_prepare(struct intel_pxp *pxp)
{
	if (!intel_pxp_is_enabled(pxp))
		return;

	intel_pxp_end(pxp);

	intel_pxp_invalidate(pxp);
}

void intel_pxp_suspend(struct intel_pxp *pxp)
{
	intel_wakeref_t wakeref;

	if (!intel_pxp_is_enabled(pxp))
		return;

	with_intel_runtime_pm(&pxp->ctrl_gt->i915->runtime_pm, wakeref) {
		intel_pxp_fini_hw(pxp);
		pxp->hw_state_invalidated = false;
	}
}

void intel_pxp_resume_complete(struct intel_pxp *pxp)
{
	if (!intel_pxp_is_enabled(pxp))
		return;

	 
	if (!HAS_ENGINE(pxp->ctrl_gt, GSC0) && !pxp->pxp_component)
		return;

	intel_pxp_init_hw(pxp);
}

void intel_pxp_runtime_suspend(struct intel_pxp *pxp)
{
	if (!intel_pxp_is_enabled(pxp))
		return;

	pxp->arb_is_valid = false;

	intel_pxp_fini_hw(pxp);

	pxp->hw_state_invalidated = false;
}
