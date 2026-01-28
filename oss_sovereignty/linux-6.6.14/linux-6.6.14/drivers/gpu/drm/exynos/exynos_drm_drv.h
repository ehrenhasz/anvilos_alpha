#ifndef _EXYNOS_DRM_DRV_H_
#define _EXYNOS_DRM_DRV_H_
#include <linux/module.h>
#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_plane.h>
#define MAX_CRTC	3
#define MAX_PLANE	5
#define MAX_FB_BUFFER	4
#define DEFAULT_WIN	0
struct drm_crtc_state;
struct drm_display_mode;
#define to_exynos_crtc(x)	container_of(x, struct exynos_drm_crtc, base)
#define to_exynos_plane(x)	container_of(x, struct exynos_drm_plane, base)
enum exynos_drm_output_type {
	EXYNOS_DISPLAY_TYPE_NONE,
	EXYNOS_DISPLAY_TYPE_LCD,
	EXYNOS_DISPLAY_TYPE_HDMI,
	EXYNOS_DISPLAY_TYPE_VIDI,
};
struct exynos_drm_rect {
	unsigned int x, y;
	unsigned int w, h;
};
struct exynos_drm_plane_state {
	struct drm_plane_state base;
	struct exynos_drm_rect crtc;
	struct exynos_drm_rect src;
	unsigned int h_ratio;
	unsigned int v_ratio;
};
static inline struct exynos_drm_plane_state *
to_exynos_plane_state(struct drm_plane_state *state)
{
	return container_of(state, struct exynos_drm_plane_state, base);
}
struct exynos_drm_plane {
	struct drm_plane base;
	const struct exynos_drm_plane_config *config;
	unsigned int index;
};
#define EXYNOS_DRM_PLANE_CAP_DOUBLE	(1 << 0)
#define EXYNOS_DRM_PLANE_CAP_SCALE	(1 << 1)
#define EXYNOS_DRM_PLANE_CAP_ZPOS	(1 << 2)
#define EXYNOS_DRM_PLANE_CAP_TILE	(1 << 3)
#define EXYNOS_DRM_PLANE_CAP_PIX_BLEND	(1 << 4)
#define EXYNOS_DRM_PLANE_CAP_WIN_BLEND	(1 << 5)
struct exynos_drm_plane_config {
	unsigned int zpos;
	enum drm_plane_type type;
	const uint32_t *pixel_formats;
	unsigned int num_pixel_formats;
	unsigned int capabilities;
};
struct exynos_drm_crtc;
struct exynos_drm_crtc_ops {
	void (*atomic_enable)(struct exynos_drm_crtc *crtc);
	void (*atomic_disable)(struct exynos_drm_crtc *crtc);
	int (*enable_vblank)(struct exynos_drm_crtc *crtc);
	void (*disable_vblank)(struct exynos_drm_crtc *crtc);
	enum drm_mode_status (*mode_valid)(struct exynos_drm_crtc *crtc,
		const struct drm_display_mode *mode);
	bool (*mode_fixup)(struct exynos_drm_crtc *crtc,
			   const struct drm_display_mode *mode,
			   struct drm_display_mode *adjusted_mode);
	int (*atomic_check)(struct exynos_drm_crtc *crtc,
			    struct drm_crtc_state *state);
	void (*atomic_begin)(struct exynos_drm_crtc *crtc);
	void (*update_plane)(struct exynos_drm_crtc *crtc,
			     struct exynos_drm_plane *plane);
	void (*disable_plane)(struct exynos_drm_crtc *crtc,
			      struct exynos_drm_plane *plane);
	void (*atomic_flush)(struct exynos_drm_crtc *crtc);
	void (*te_handler)(struct exynos_drm_crtc *crtc);
};
struct exynos_drm_clk {
	void (*enable)(struct exynos_drm_clk *clk, bool enable);
};
struct exynos_drm_crtc {
	struct drm_crtc			base;
	enum exynos_drm_output_type	type;
	const struct exynos_drm_crtc_ops	*ops;
	void				*ctx;
	struct exynos_drm_clk		*pipe_clk;
	bool				i80_mode : 1;
};
static inline void exynos_drm_pipe_clk_enable(struct exynos_drm_crtc *crtc,
					      bool enable)
{
	if (crtc->pipe_clk)
		crtc->pipe_clk->enable(crtc->pipe_clk, enable);
}
struct drm_exynos_file_private {
	struct list_head	inuse_cmdlist;
	struct list_head	event_list;
	struct list_head	userptr_list;
};
struct exynos_drm_private {
	struct device *g2d_dev;
	struct device *dma_dev;
	void *mapping;
	u32			pending;
	spinlock_t		lock;
	wait_queue_head_t	wait;
};
static inline struct device *to_dma_dev(struct drm_device *dev)
{
	struct exynos_drm_private *priv = dev->dev_private;
	return priv->dma_dev;
}
static inline bool is_drm_iommu_supported(struct drm_device *drm_dev)
{
	struct exynos_drm_private *priv = drm_dev->dev_private;
	return priv->mapping ? true : false;
}
int exynos_drm_register_dma(struct drm_device *drm, struct device *dev,
			    void **dma_priv);
void exynos_drm_unregister_dma(struct drm_device *drm, struct device *dev,
			       void **dma_priv);
void exynos_drm_cleanup_dma(struct drm_device *drm);
#ifdef CONFIG_DRM_EXYNOS_DPI
struct drm_encoder *exynos_dpi_probe(struct device *dev);
int exynos_dpi_remove(struct drm_encoder *encoder);
int exynos_dpi_bind(struct drm_device *dev, struct drm_encoder *encoder);
#else
static inline struct drm_encoder *
exynos_dpi_probe(struct device *dev) { return NULL; }
static inline int exynos_dpi_remove(struct drm_encoder *encoder)
{
	return 0;
}
static inline int exynos_dpi_bind(struct drm_device *dev,
				  struct drm_encoder *encoder)
{
	return 0;
}
#endif
#ifdef CONFIG_DRM_EXYNOS_FIMC
int exynos_drm_check_fimc_device(struct device *dev);
#else
static inline int exynos_drm_check_fimc_device(struct device *dev)
{
	return 0;
}
#endif
int exynos_atomic_commit(struct drm_device *dev, struct drm_atomic_state *state,
			 bool nonblock);
extern struct platform_driver fimd_driver;
extern struct platform_driver exynos5433_decon_driver;
extern struct platform_driver decon_driver;
extern struct platform_driver dp_driver;
extern struct platform_driver dsi_driver;
extern struct platform_driver mixer_driver;
extern struct platform_driver hdmi_driver;
extern struct platform_driver vidi_driver;
extern struct platform_driver g2d_driver;
extern struct platform_driver fimc_driver;
extern struct platform_driver rotator_driver;
extern struct platform_driver scaler_driver;
extern struct platform_driver gsc_driver;
extern struct platform_driver mic_driver;
#endif
