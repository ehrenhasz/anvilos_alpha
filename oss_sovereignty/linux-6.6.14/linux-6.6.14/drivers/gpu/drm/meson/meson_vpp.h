#ifndef __MESON_VPP_H
#define __MESON_VPP_H
struct drm_rect;
struct meson_drm;
#define MESON_VIU_VPP_MUX_ENCL	0x0
#define MESON_VIU_VPP_MUX_ENCI	0x5
#define MESON_VIU_VPP_MUX_ENCP	0xA
void meson_vpp_setup_mux(struct meson_drm *priv, unsigned int mux);
void meson_vpp_setup_interlace_vscaler_osd1(struct meson_drm *priv,
					    struct drm_rect *input);
void meson_vpp_disable_interlace_vscaler_osd1(struct meson_drm *priv);
void meson_vpp_init(struct meson_drm *priv);
#endif  
