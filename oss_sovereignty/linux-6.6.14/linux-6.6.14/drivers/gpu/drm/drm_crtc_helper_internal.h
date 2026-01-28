#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_encoder.h>
#include <drm/drm_modes.h>
enum drm_mode_status drm_crtc_mode_valid(struct drm_crtc *crtc,
					 const struct drm_display_mode *mode);
enum drm_mode_status drm_encoder_mode_valid(struct drm_encoder *encoder,
					    const struct drm_display_mode *mode);
int
drm_connector_mode_valid(struct drm_connector *connector,
			 struct drm_display_mode *mode,
			 struct drm_modeset_acquire_ctx *ctx,
			 enum drm_mode_status *status);
struct drm_encoder *
drm_connector_get_single_encoder(struct drm_connector *connector);
