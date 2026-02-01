 
 

#ifndef __MESON_VENC_CVBS_H
#define __MESON_VENC_CVBS_H

#include "meson_drv.h"
#include "meson_venc.h"

struct meson_cvbs_mode {
	struct meson_cvbs_enci_mode *enci;
	struct drm_display_mode mode;
};

#define MESON_CVBS_MODES_COUNT	2

 
extern struct meson_cvbs_mode meson_cvbs_modes[MESON_CVBS_MODES_COUNT];

int meson_encoder_cvbs_init(struct meson_drm *priv);
void meson_encoder_cvbs_remove(struct meson_drm *priv);

#endif  
