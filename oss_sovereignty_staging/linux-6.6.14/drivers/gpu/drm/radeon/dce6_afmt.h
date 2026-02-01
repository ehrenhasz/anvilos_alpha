 

#ifndef __DCE6_AFMT_H__
#define __DCE6_AFMT_H__

struct cea_sad;
struct drm_connector;
struct drm_display_mode;
struct drm_encoder;
struct radeon_crtc;
struct radeon_device;

u32 dce6_endpoint_rreg(struct radeon_device *rdev, u32 offset, u32 reg);
void dce6_endpoint_wreg(struct radeon_device *rdev, u32 offset, u32 reg, u32 v);
void dce6_afmt_write_sad_regs(struct drm_encoder *encoder,
			      struct cea_sad *sads, int sad_count);
void dce6_afmt_hdmi_write_speaker_allocation(struct drm_encoder *encoder,
					     u8 *sadb, int sad_count);
void dce6_afmt_dp_write_speaker_allocation(struct drm_encoder *encoder,
					   u8 *sadb, int sad_count);
void dce6_afmt_write_latency_fields(struct drm_encoder *encoder,
				    struct drm_connector *connector,
				    struct drm_display_mode *mode);
void dce6_afmt_select_pin(struct drm_encoder *encoder);
void dce6_hdmi_audio_set_dto(struct radeon_device *rdev,
			     struct radeon_crtc *crtc, unsigned int clock);
void dce6_dp_audio_set_dto(struct radeon_device *rdev,
			   struct radeon_crtc *crtc, unsigned int clock);

#endif                          
