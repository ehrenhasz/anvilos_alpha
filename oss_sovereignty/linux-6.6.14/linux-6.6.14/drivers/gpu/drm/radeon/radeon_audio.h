#ifndef __RADEON_AUDIO_H__
#define __RADEON_AUDIO_H__
#include <linux/types.h>
#define RREG32_ENDPOINT(block, reg)		\
	radeon_audio_endpoint_rreg(rdev, (block), (reg))
#define WREG32_ENDPOINT(block, reg, v)	\
	radeon_audio_endpoint_wreg(rdev, (block), (reg), (v))
struct radeon_audio_basic_funcs
{
	u32  (*endpoint_rreg)(struct radeon_device *rdev, u32 offset, u32 reg);
	void (*endpoint_wreg)(struct radeon_device *rdev,
		u32 offset, u32 reg, u32 v);
	void (*enable)(struct radeon_device *rdev,
		struct r600_audio_pin *pin, u8 enable_mask);
};
struct radeon_audio_funcs
{
	void (*select_pin)(struct drm_encoder *encoder);
	struct r600_audio_pin* (*get_pin)(struct radeon_device *rdev);
	void (*write_latency_fields)(struct drm_encoder *encoder,
		struct drm_connector *connector, struct drm_display_mode *mode);
	void (*write_sad_regs)(struct drm_encoder *encoder,
		struct cea_sad *sads, int sad_count);
	void (*write_speaker_allocation)(struct drm_encoder *encoder,
		u8 *sadb, int sad_count);
	void (*set_dto)(struct radeon_device *rdev,
		struct radeon_crtc *crtc, unsigned int clock);
	void (*update_acr)(struct drm_encoder *encoder, long offset,
		const struct radeon_hdmi_acr *acr);
	void (*set_vbi_packet)(struct drm_encoder *encoder, u32 offset);
	void (*set_color_depth)(struct drm_encoder *encoder, u32 offset, int bpc);
	void (*set_avi_packet)(struct radeon_device *rdev, u32 offset,
		unsigned char *buffer, size_t size);
	void (*set_audio_packet)(struct drm_encoder *encoder, u32 offset);
	void (*set_mute)(struct drm_encoder *encoder, u32 offset, bool mute);
	void (*mode_set)(struct drm_encoder *encoder,
		struct drm_display_mode *mode);
	void (*dpms)(struct drm_encoder *encoder, bool mode);
};
int radeon_audio_init(struct radeon_device *rdev);
void radeon_audio_detect(struct drm_connector *connector,
			 struct drm_encoder *encoder,
			 enum drm_connector_status status);
u32 radeon_audio_endpoint_rreg(struct radeon_device *rdev,
	u32 offset, u32 reg);
void radeon_audio_endpoint_wreg(struct radeon_device *rdev,
	u32 offset,	u32 reg, u32 v);
struct r600_audio_pin *radeon_audio_get_pin(struct drm_encoder *encoder);
void radeon_audio_fini(struct radeon_device *rdev);
void radeon_audio_mode_set(struct drm_encoder *encoder,
	struct drm_display_mode *mode);
void radeon_audio_dpms(struct drm_encoder *encoder, int mode);
unsigned int radeon_audio_decode_dfs_div(unsigned int div);
void dce3_2_afmt_write_sad_regs(struct drm_encoder *encoder,
				struct cea_sad *sads, int sad_count);
void dce3_2_afmt_hdmi_write_speaker_allocation(struct drm_encoder *encoder,
					       u8 *sadb, int sad_count);
void dce3_2_afmt_dp_write_speaker_allocation(struct drm_encoder *encoder,
					     u8 *sadb, int sad_count);
void dce3_2_audio_set_dto(struct radeon_device *rdev,
			  struct radeon_crtc *crtc, unsigned int clock);
void dce3_2_hdmi_update_acr(struct drm_encoder *encoder, long offset,
			    const struct radeon_hdmi_acr *acr);
void dce3_2_set_audio_packet(struct drm_encoder *encoder, u32 offset);
void dce3_2_set_mute(struct drm_encoder *encoder, u32 offset, bool mute);
#endif
