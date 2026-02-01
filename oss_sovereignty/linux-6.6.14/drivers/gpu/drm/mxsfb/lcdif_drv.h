 
 

#ifndef __LCDIF_DRV_H__
#define __LCDIF_DRV_H__

#include <drm/drm_bridge.h>
#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_plane.h>

struct clk;

struct lcdif_drm_private {
	void __iomem			*base;	 
	struct clk			*clk;
	struct clk			*clk_axi;
	struct clk			*clk_disp_axi;

	unsigned int			irq;

	struct drm_device		*drm;
	struct {
		struct drm_plane	primary;
		 
	} planes;
	struct drm_crtc			crtc;
};

static inline struct lcdif_drm_private *
to_lcdif_drm_private(struct drm_device *drm)
{
	return drm->dev_private;
}

int lcdif_kms_init(struct lcdif_drm_private *lcdif);

#endif  
