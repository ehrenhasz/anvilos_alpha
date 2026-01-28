#ifdef CONFIG_ROCKCHIP_RGB
struct rockchip_rgb *rockchip_rgb_init(struct device *dev,
				       struct drm_crtc *crtc,
				       struct drm_device *drm_dev,
				       int video_port);
void rockchip_rgb_fini(struct rockchip_rgb *rgb);
#else
static inline struct rockchip_rgb *rockchip_rgb_init(struct device *dev,
						     struct drm_crtc *crtc,
						     struct drm_device *drm_dev,
						     int video_port)
{
	return NULL;
}
static inline void rockchip_rgb_fini(struct rockchip_rgb *rgb)
{
}
#endif
