#ifndef __MESON_CRTC_H
#define __MESON_CRTC_H
#include "meson_drv.h"
int meson_crtc_create(struct meson_drm *priv);
void meson_crtc_irq(struct meson_drm *priv);
#endif  
