#ifndef __TILCDC_DRV_H__
#define __TILCDC_DRV_H__
#include <linux/cpufreq.h>
#include <linux/irqreturn.h>
#include <drm/drm_print.h>
struct clk;
struct workqueue_struct;
struct drm_connector;
struct drm_connector_helper_funcs;
struct drm_crtc;
struct drm_device;
struct drm_display_mode;
struct drm_encoder;
struct drm_framebuffer;
struct drm_minor;
struct drm_pending_vblank_event;
struct drm_plane;
#define TILCDC_DEFAULT_MAX_PIXELCLOCK  126000
#define TILCDC_DEFAULT_MAX_WIDTH_V1  1024
#define TILCDC_DEFAULT_MAX_WIDTH_V2  2048
#define TILCDC_DEFAULT_MAX_BANDWIDTH  (1280*1024*60)
struct tilcdc_drm_private {
	void __iomem *mmio;
	struct clk *clk;          
	int rev;                  
	unsigned int irq;
	uint32_t max_bandwidth;
	uint32_t max_pixelclock;
	uint32_t max_width;
	const uint32_t *pixelformats;
	uint32_t num_pixelformats;
#ifdef CONFIG_CPU_FREQ
	struct notifier_block freq_transition;
#endif
	struct workqueue_struct *wq;
	struct drm_crtc *crtc;
	unsigned int num_encoders;
	struct drm_encoder *encoders[8];
	unsigned int num_connectors;
	struct drm_connector *connectors[8];
	struct drm_encoder *external_encoder;
	struct drm_connector *external_connector;
	bool is_registered;
	bool is_componentized;
	bool irq_enabled;
};
struct tilcdc_module;
struct tilcdc_module_ops {
	int (*modeset_init)(struct tilcdc_module *mod, struct drm_device *dev);
#ifdef CONFIG_DEBUG_FS
	int (*debugfs_init)(struct tilcdc_module *mod, struct drm_minor *minor);
#endif
};
struct tilcdc_module {
	const char *name;
	struct list_head list;
	const struct tilcdc_module_ops *funcs;
};
void tilcdc_module_init(struct tilcdc_module *mod, const char *name,
		const struct tilcdc_module_ops *funcs);
void tilcdc_module_cleanup(struct tilcdc_module *mod);
struct tilcdc_panel_info {
	uint32_t ac_bias;
	uint32_t ac_bias_intrpt;
	uint32_t dma_burst_sz;
	uint32_t bpp;
	uint32_t fdd;
	bool tft_alt_mode;
	bool invert_pxl_clk;
	uint32_t sync_edge;
	uint32_t sync_ctrl;
	uint32_t raster_order;
	uint32_t fifo_th;
};
#define DBG(fmt, ...) DRM_DEBUG(fmt"\n", ##__VA_ARGS__)
int tilcdc_crtc_create(struct drm_device *dev);
irqreturn_t tilcdc_crtc_irq(struct drm_crtc *crtc);
void tilcdc_crtc_update_clk(struct drm_crtc *crtc);
void tilcdc_crtc_set_panel_info(struct drm_crtc *crtc,
		const struct tilcdc_panel_info *info);
void tilcdc_crtc_set_simulate_vesa_sync(struct drm_crtc *crtc,
					bool simulate_vesa_sync);
void tilcdc_crtc_shutdown(struct drm_crtc *crtc);
int tilcdc_crtc_update_fb(struct drm_crtc *crtc,
		struct drm_framebuffer *fb,
		struct drm_pending_vblank_event *event);
int tilcdc_plane_init(struct drm_device *dev, struct drm_plane *plane);
#endif  
