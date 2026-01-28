#ifndef __SHMOB_DRM_BACKLIGHT_H__
#define __SHMOB_DRM_BACKLIGHT_H__
struct shmob_drm_connector;
void shmob_drm_backlight_dpms(struct shmob_drm_connector *scon, int mode);
int shmob_drm_backlight_init(struct shmob_drm_connector *scon);
void shmob_drm_backlight_exit(struct shmob_drm_connector *scon);
#endif  
