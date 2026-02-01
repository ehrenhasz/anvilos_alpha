 
 

#ifndef _PSB_IRQ_H_
#define _PSB_IRQ_H_

struct drm_crtc;
struct drm_device;

void gma_irq_preinstall(struct drm_device *dev);
void gma_irq_postinstall(struct drm_device *dev);
int  gma_irq_install(struct drm_device *dev);
void gma_irq_uninstall(struct drm_device *dev);

int  gma_crtc_enable_vblank(struct drm_crtc *crtc);
void gma_crtc_disable_vblank(struct drm_crtc *crtc);
u32  gma_crtc_get_vblank_counter(struct drm_crtc *crtc);
void gma_enable_pipestat(struct drm_psb_private *dev_priv, int pipe, u32 mask);
void gma_disable_pipestat(struct drm_psb_private *dev_priv, int pipe, u32 mask);

#endif  
