#ifndef _VC4_HDMI_H_
#define _VC4_HDMI_H_
#include <drm/drm_connector.h>
#include <media/cec.h>
#include <sound/dmaengine_pcm.h>
#include <sound/soc.h>
#include "vc4_drv.h"
struct vc4_hdmi;
struct vc4_hdmi_register;
struct vc4_hdmi_connector_state;
enum vc4_hdmi_phy_channel {
	PHY_LANE_0 = 0,
	PHY_LANE_1,
	PHY_LANE_2,
	PHY_LANE_CK,
};
struct vc4_hdmi_variant {
	enum vc4_encoder_type encoder_type;
	const char *card_name;
	const char *debugfs_name;
	unsigned long long max_pixel_clock;
	const struct vc4_hdmi_register *registers;
	unsigned int num_registers;
	enum vc4_hdmi_phy_channel phy_lane_mapping[4];
	bool unsupported_odd_h_timings;
	bool external_irq_controller;
	int (*init_resources)(struct drm_device *drm,
			      struct vc4_hdmi *vc4_hdmi);
	void (*reset)(struct vc4_hdmi *vc4_hdmi);
	void (*csc_setup)(struct vc4_hdmi *vc4_hdmi,
			  struct drm_connector_state *state,
			  const struct drm_display_mode *mode);
	void (*set_timings)(struct vc4_hdmi *vc4_hdmi,
			    struct drm_connector_state *state,
			    const struct drm_display_mode *mode);
	void (*phy_init)(struct vc4_hdmi *vc4_hdmi,
			 struct vc4_hdmi_connector_state *vc4_conn_state);
	void (*phy_disable)(struct vc4_hdmi *vc4_hdmi);
	void (*phy_rng_enable)(struct vc4_hdmi *vc4_hdmi);
	void (*phy_rng_disable)(struct vc4_hdmi *vc4_hdmi);
	u32 (*channel_map)(struct vc4_hdmi *vc4_hdmi, u32 channel_mask);
	bool supports_hdr;
	bool (*hp_detect)(struct vc4_hdmi *vc4_hdmi);
};
struct vc4_hdmi_audio {
	struct snd_soc_card card;
	struct snd_soc_dai_link link;
	struct snd_soc_dai_link_component cpu;
	struct snd_soc_dai_link_component codec;
	struct snd_soc_dai_link_component platform;
	struct snd_dmaengine_dai_dma_data dma_data;
	struct hdmi_audio_infoframe infoframe;
	struct platform_device *codec_pdev;
	bool streaming;
};
enum vc4_hdmi_output_format {
	VC4_HDMI_OUTPUT_RGB,
	VC4_HDMI_OUTPUT_YUV422,
	VC4_HDMI_OUTPUT_YUV444,
	VC4_HDMI_OUTPUT_YUV420,
};
enum vc4_hdmi_broadcast_rgb {
	VC4_HDMI_BROADCAST_RGB_AUTO,
	VC4_HDMI_BROADCAST_RGB_FULL,
	VC4_HDMI_BROADCAST_RGB_LIMITED,
};
struct vc4_hdmi {
	struct vc4_hdmi_audio audio;
	struct platform_device *pdev;
	const struct vc4_hdmi_variant *variant;
	struct vc4_encoder encoder;
	struct drm_connector connector;
	struct delayed_work scrambling_work;
	struct drm_property *broadcast_rgb_property;
	struct i2c_adapter *ddc;
	void __iomem *hdmicore_regs;
	void __iomem *hd_regs;
	void __iomem *cec_regs;
	void __iomem *csc_regs;
	void __iomem *dvp_regs;
	void __iomem *phy_regs;
	void __iomem *ram_regs;
	void __iomem *rm_regs;
	struct gpio_desc *hpd_gpio;
	bool disable_wifi_frequencies;
	struct cec_adapter *cec_adap;
	struct cec_msg cec_rx_msg;
	bool cec_tx_ok;
	bool cec_irq_was_rx;
	struct clk *cec_clock;
	struct clk *pixel_clock;
	struct clk *hsm_clock;
	struct clk *audio_clock;
	struct clk *pixel_bvb_clock;
	struct reset_control *reset;
	struct debugfs_regset32 hdmi_regset;
	struct debugfs_regset32 hd_regset;
	struct debugfs_regset32 cec_regset;
	struct debugfs_regset32 csc_regset;
	struct debugfs_regset32 dvp_regset;
	struct debugfs_regset32 phy_regset;
	struct debugfs_regset32 ram_regset;
	struct debugfs_regset32 rm_regset;
	spinlock_t hw_lock;
	struct mutex mutex;
	struct drm_display_mode saved_adjusted_mode;
	bool packet_ram_enabled;
	bool scdc_enabled;
	unsigned int output_bpc;
	enum vc4_hdmi_output_format output_format;
};
#define connector_to_vc4_hdmi(_connector)				\
	container_of_const(_connector, struct vc4_hdmi, connector)
static inline struct vc4_hdmi *
encoder_to_vc4_hdmi(struct drm_encoder *encoder)
{
	struct vc4_encoder *_encoder = to_vc4_encoder(encoder);
	return container_of_const(_encoder, struct vc4_hdmi, encoder);
}
struct vc4_hdmi_connector_state {
	struct drm_connector_state	base;
	unsigned long long		tmds_char_rate;
	unsigned int 			output_bpc;
	enum vc4_hdmi_output_format	output_format;
	enum vc4_hdmi_broadcast_rgb	broadcast_rgb;
};
#define conn_state_to_vc4_hdmi_conn_state(_state)			\
	container_of_const(_state, struct vc4_hdmi_connector_state, base)
void vc4_hdmi_phy_init(struct vc4_hdmi *vc4_hdmi,
		       struct vc4_hdmi_connector_state *vc4_conn_state);
void vc4_hdmi_phy_disable(struct vc4_hdmi *vc4_hdmi);
void vc4_hdmi_phy_rng_enable(struct vc4_hdmi *vc4_hdmi);
void vc4_hdmi_phy_rng_disable(struct vc4_hdmi *vc4_hdmi);
void vc5_hdmi_phy_init(struct vc4_hdmi *vc4_hdmi,
		       struct vc4_hdmi_connector_state *vc4_conn_state);
void vc5_hdmi_phy_disable(struct vc4_hdmi *vc4_hdmi);
void vc5_hdmi_phy_rng_enable(struct vc4_hdmi *vc4_hdmi);
void vc5_hdmi_phy_rng_disable(struct vc4_hdmi *vc4_hdmi);
#endif  
