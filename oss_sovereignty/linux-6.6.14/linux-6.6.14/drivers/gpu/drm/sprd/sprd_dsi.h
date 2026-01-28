#ifndef __SPRD_DSI_H__
#define __SPRD_DSI_H__
#include <linux/of.h>
#include <linux/device.h>
#include <linux/regmap.h>
#include <video/videomode.h>
#include <drm/drm_bridge.h>
#include <drm/drm_connector.h>
#include <drm/drm_encoder.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_print.h>
#include <drm/drm_panel.h>
#define encoder_to_dsi(encoder) \
	container_of(encoder, struct sprd_dsi, encoder)
enum dsi_work_mode {
	DSI_MODE_CMD = 0,
	DSI_MODE_VIDEO
};
enum video_burst_mode {
	VIDEO_NON_BURST_WITH_SYNC_PULSES = 0,
	VIDEO_NON_BURST_WITH_SYNC_EVENTS,
	VIDEO_BURST_WITH_SYNC_PULSES
};
enum dsi_color_coding {
	COLOR_CODE_16BIT_CONFIG1 = 0,
	COLOR_CODE_16BIT_CONFIG2,
	COLOR_CODE_16BIT_CONFIG3,
	COLOR_CODE_18BIT_CONFIG1,
	COLOR_CODE_18BIT_CONFIG2,
	COLOR_CODE_24BIT,
	COLOR_CODE_20BIT_YCC422_LOOSELY,
	COLOR_CODE_24BIT_YCC422,
	COLOR_CODE_16BIT_YCC422,
	COLOR_CODE_30BIT,
	COLOR_CODE_36BIT,
	COLOR_CODE_12BIT_YCC420,
	COLOR_CODE_COMPRESSTION,
	COLOR_CODE_MAX
};
enum pll_timing {
	NONE,
	REQUEST_TIME,
	PREPARE_TIME,
	SETTLE_TIME,
	ZERO_TIME,
	TRAIL_TIME,
	EXIT_TIME,
	CLKPOST_TIME,
	TA_GET,
	TA_GO,
	TA_SURE,
	TA_WAIT,
};
struct dphy_pll {
	u8 refin;  
	u8 cp_s;  
	u8 fdk_s;  
	u8 sdm_en;
	u8 div;
	u8 int_n;  
	u32 ref_clk;  
	u32 freq;  
	u32 fvco;
	u32 potential_fvco;
	u32 nint;  
	u32 kint;  
	u8 lpf_sel;  
	u8 out_sel;  
	u8 vco_band;  
	u8 det_delay;
};
struct dsi_context {
	void __iomem *base;
	struct regmap *regmap;
	struct dphy_pll pll;
	struct videomode vm;
	bool enabled;
	u8 work_mode;
	u8 burst_mode;
	u32 int0_mask;
	u32 int1_mask;
	u16 data_hs2lp;
	u16 data_lp2hs;
	u16 clk_hs2lp;
	u16 clk_lp2hs;
	u16 max_rd_time;
	bool frame_ack_en;
	bool te_ack_en;
};
struct sprd_dsi {
	struct drm_device *drm;
	struct mipi_dsi_host host;
	struct mipi_dsi_device *slave;
	struct drm_encoder encoder;
	struct drm_bridge *panel_bridge;
	struct dsi_context ctx;
};
int dphy_pll_config(struct dsi_context *ctx);
void dphy_timing_config(struct dsi_context *ctx);
#endif  
