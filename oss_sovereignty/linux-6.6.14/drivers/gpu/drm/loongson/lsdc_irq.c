
 

#include <drm/drm_vblank.h>

#include "lsdc_irq.h"

 

irqreturn_t ls7a2000_dc_irq_handler(int irq, void *arg)
{
	struct drm_device *ddev = arg;
	struct lsdc_device *ldev = to_lsdc(ddev);
	u32 val;

	 
	val = lsdc_rreg32(ldev, LSDC_INT_REG);
	if ((val & INT_STATUS_MASK) == 0) {
		drm_warn(ddev, "no interrupt occurs\n");
		return IRQ_NONE;
	}

	ldev->irq_status = val;

	 
	lsdc_wreg32(ldev, LSDC_INT_REG, val);

	if (ldev->irq_status & INT_CRTC0_VSYNC)
		drm_handle_vblank(ddev, 0);

	if (ldev->irq_status & INT_CRTC1_VSYNC)
		drm_handle_vblank(ddev, 1);

	return IRQ_HANDLED;
}

 
irqreturn_t ls7a1000_dc_irq_handler(int irq, void *arg)
{
	struct drm_device *ddev = arg;
	struct lsdc_device *ldev = to_lsdc(ddev);
	u32 val;

	 
	val = lsdc_rreg32(ldev, LSDC_INT_REG);
	if ((val & INT_STATUS_MASK) == 0) {
		drm_warn(ddev, "no interrupt occurs\n");
		return IRQ_NONE;
	}

	ldev->irq_status = val;

	 
	val &= ~(INT_CRTC0_VSYNC | INT_CRTC1_VSYNC);
	lsdc_wreg32(ldev, LSDC_INT_REG, val);

	if (ldev->irq_status & INT_CRTC0_VSYNC)
		drm_handle_vblank(ddev, 0);

	if (ldev->irq_status & INT_CRTC1_VSYNC)
		drm_handle_vblank(ddev, 1);

	return IRQ_HANDLED;
}
