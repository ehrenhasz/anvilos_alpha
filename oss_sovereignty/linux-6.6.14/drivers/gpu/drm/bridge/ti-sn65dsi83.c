
 

#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/media-bus-format.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_of.h>
#include <drm/drm_panel.h>
#include <drm/drm_print.h>
#include <drm/drm_probe_helper.h>

 
#define REG_ID(n)				(0x00 + (n))
 
#define REG_RC_RESET				0x09
#define  REG_RC_RESET_SOFT_RESET		BIT(0)
#define REG_RC_LVDS_PLL				0x0a
#define  REG_RC_LVDS_PLL_PLL_EN_STAT		BIT(7)
#define  REG_RC_LVDS_PLL_LVDS_CLK_RANGE(n)	(((n) & 0x7) << 1)
#define  REG_RC_LVDS_PLL_HS_CLK_SRC_DPHY	BIT(0)
#define REG_RC_DSI_CLK				0x0b
#define  REG_RC_DSI_CLK_DSI_CLK_DIVIDER(n)	(((n) & 0x1f) << 3)
#define  REG_RC_DSI_CLK_REFCLK_MULTIPLIER(n)	((n) & 0x3)
#define REG_RC_PLL_EN				0x0d
#define  REG_RC_PLL_EN_PLL_EN			BIT(0)
 
#define REG_DSI_LANE				0x10
#define  REG_DSI_LANE_LEFT_RIGHT_PIXELS		BIT(7)	 
#define  REG_DSI_LANE_DSI_CHANNEL_MODE_DUAL	0	 
#define  REG_DSI_LANE_DSI_CHANNEL_MODE_2SINGLE	BIT(6)	 
#define  REG_DSI_LANE_DSI_CHANNEL_MODE_SINGLE	BIT(5)
#define  REG_DSI_LANE_CHA_DSI_LANES(n)		(((n) & 0x3) << 3)
#define  REG_DSI_LANE_CHB_DSI_LANES(n)		(((n) & 0x3) << 1)
#define  REG_DSI_LANE_SOT_ERR_TOL_DIS		BIT(0)
#define REG_DSI_EQ				0x11
#define  REG_DSI_EQ_CHA_DSI_DATA_EQ(n)		(((n) & 0x3) << 6)
#define  REG_DSI_EQ_CHA_DSI_CLK_EQ(n)		(((n) & 0x3) << 2)
#define REG_DSI_CLK				0x12
#define  REG_DSI_CLK_CHA_DSI_CLK_RANGE(n)	((n) & 0xff)
 
#define REG_LVDS_FMT				0x18
#define  REG_LVDS_FMT_DE_NEG_POLARITY		BIT(7)
#define  REG_LVDS_FMT_HS_NEG_POLARITY		BIT(6)
#define  REG_LVDS_FMT_VS_NEG_POLARITY		BIT(5)
#define  REG_LVDS_FMT_LVDS_LINK_CFG		BIT(4)	 
#define  REG_LVDS_FMT_CHA_24BPP_MODE		BIT(3)
#define  REG_LVDS_FMT_CHB_24BPP_MODE		BIT(2)
#define  REG_LVDS_FMT_CHA_24BPP_FORMAT1		BIT(1)
#define  REG_LVDS_FMT_CHB_24BPP_FORMAT1		BIT(0)
#define REG_LVDS_VCOM				0x19
#define  REG_LVDS_VCOM_CHA_LVDS_VOCM		BIT(6)
#define  REG_LVDS_VCOM_CHB_LVDS_VOCM		BIT(4)
#define  REG_LVDS_VCOM_CHA_LVDS_VOD_SWING(n)	(((n) & 0x3) << 2)
#define  REG_LVDS_VCOM_CHB_LVDS_VOD_SWING(n)	((n) & 0x3)
#define REG_LVDS_LANE				0x1a
#define  REG_LVDS_LANE_EVEN_ODD_SWAP		BIT(6)
#define  REG_LVDS_LANE_CHA_REVERSE_LVDS		BIT(5)
#define  REG_LVDS_LANE_CHB_REVERSE_LVDS		BIT(4)
#define  REG_LVDS_LANE_CHA_LVDS_TERM		BIT(1)
#define  REG_LVDS_LANE_CHB_LVDS_TERM		BIT(0)
#define REG_LVDS_CM				0x1b
#define  REG_LVDS_CM_CHA_LVDS_CM_ADJUST(n)	(((n) & 0x3) << 4)
#define  REG_LVDS_CM_CHB_LVDS_CM_ADJUST(n)	((n) & 0x3)
 
#define REG_VID_CHA_ACTIVE_LINE_LENGTH_LOW	0x20
#define REG_VID_CHA_ACTIVE_LINE_LENGTH_HIGH	0x21
#define REG_VID_CHA_VERTICAL_DISPLAY_SIZE_LOW	0x24
#define REG_VID_CHA_VERTICAL_DISPLAY_SIZE_HIGH	0x25
#define REG_VID_CHA_SYNC_DELAY_LOW		0x28
#define REG_VID_CHA_SYNC_DELAY_HIGH		0x29
#define REG_VID_CHA_HSYNC_PULSE_WIDTH_LOW	0x2c
#define REG_VID_CHA_HSYNC_PULSE_WIDTH_HIGH	0x2d
#define REG_VID_CHA_VSYNC_PULSE_WIDTH_LOW	0x30
#define REG_VID_CHA_VSYNC_PULSE_WIDTH_HIGH	0x31
#define REG_VID_CHA_HORIZONTAL_BACK_PORCH	0x34
#define REG_VID_CHA_VERTICAL_BACK_PORCH		0x36
#define REG_VID_CHA_HORIZONTAL_FRONT_PORCH	0x38
#define REG_VID_CHA_VERTICAL_FRONT_PORCH	0x3a
#define REG_VID_CHA_TEST_PATTERN		0x3c
 
#define REG_IRQ_GLOBAL				0xe0
#define  REG_IRQ_GLOBAL_IRQ_EN			BIT(0)
#define REG_IRQ_EN				0xe1
#define  REG_IRQ_EN_CHA_SYNCH_ERR_EN		BIT(7)
#define  REG_IRQ_EN_CHA_CRC_ERR_EN		BIT(6)
#define  REG_IRQ_EN_CHA_UNC_ECC_ERR_EN		BIT(5)
#define  REG_IRQ_EN_CHA_COR_ECC_ERR_EN		BIT(4)
#define  REG_IRQ_EN_CHA_LLP_ERR_EN		BIT(3)
#define  REG_IRQ_EN_CHA_SOT_BIT_ERR_EN		BIT(2)
#define  REG_IRQ_EN_CHA_PLL_UNLOCK_EN		BIT(0)
#define REG_IRQ_STAT				0xe5
#define  REG_IRQ_STAT_CHA_SYNCH_ERR		BIT(7)
#define  REG_IRQ_STAT_CHA_CRC_ERR		BIT(6)
#define  REG_IRQ_STAT_CHA_UNC_ECC_ERR		BIT(5)
#define  REG_IRQ_STAT_CHA_COR_ECC_ERR		BIT(4)
#define  REG_IRQ_STAT_CHA_LLP_ERR		BIT(3)
#define  REG_IRQ_STAT_CHA_SOT_BIT_ERR		BIT(2)
#define  REG_IRQ_STAT_CHA_PLL_UNLOCK		BIT(0)

enum sn65dsi83_model {
	MODEL_SN65DSI83,
	MODEL_SN65DSI84,
};

struct sn65dsi83 {
	struct drm_bridge		bridge;
	struct device			*dev;
	struct regmap			*regmap;
	struct mipi_dsi_device		*dsi;
	struct drm_bridge		*panel_bridge;
	struct gpio_desc		*enable_gpio;
	struct regulator		*vcc;
	bool				lvds_dual_link;
	bool				lvds_dual_link_even_odd_swap;
};

static const struct regmap_range sn65dsi83_readable_ranges[] = {
	regmap_reg_range(REG_ID(0), REG_ID(8)),
	regmap_reg_range(REG_RC_LVDS_PLL, REG_RC_DSI_CLK),
	regmap_reg_range(REG_RC_PLL_EN, REG_RC_PLL_EN),
	regmap_reg_range(REG_DSI_LANE, REG_DSI_CLK),
	regmap_reg_range(REG_LVDS_FMT, REG_LVDS_CM),
	regmap_reg_range(REG_VID_CHA_ACTIVE_LINE_LENGTH_LOW,
			 REG_VID_CHA_ACTIVE_LINE_LENGTH_HIGH),
	regmap_reg_range(REG_VID_CHA_VERTICAL_DISPLAY_SIZE_LOW,
			 REG_VID_CHA_VERTICAL_DISPLAY_SIZE_HIGH),
	regmap_reg_range(REG_VID_CHA_SYNC_DELAY_LOW,
			 REG_VID_CHA_SYNC_DELAY_HIGH),
	regmap_reg_range(REG_VID_CHA_HSYNC_PULSE_WIDTH_LOW,
			 REG_VID_CHA_HSYNC_PULSE_WIDTH_HIGH),
	regmap_reg_range(REG_VID_CHA_VSYNC_PULSE_WIDTH_LOW,
			 REG_VID_CHA_VSYNC_PULSE_WIDTH_HIGH),
	regmap_reg_range(REG_VID_CHA_HORIZONTAL_BACK_PORCH,
			 REG_VID_CHA_HORIZONTAL_BACK_PORCH),
	regmap_reg_range(REG_VID_CHA_VERTICAL_BACK_PORCH,
			 REG_VID_CHA_VERTICAL_BACK_PORCH),
	regmap_reg_range(REG_VID_CHA_HORIZONTAL_FRONT_PORCH,
			 REG_VID_CHA_HORIZONTAL_FRONT_PORCH),
	regmap_reg_range(REG_VID_CHA_VERTICAL_FRONT_PORCH,
			 REG_VID_CHA_VERTICAL_FRONT_PORCH),
	regmap_reg_range(REG_VID_CHA_TEST_PATTERN, REG_VID_CHA_TEST_PATTERN),
	regmap_reg_range(REG_IRQ_GLOBAL, REG_IRQ_EN),
	regmap_reg_range(REG_IRQ_STAT, REG_IRQ_STAT),
};

static const struct regmap_access_table sn65dsi83_readable_table = {
	.yes_ranges = sn65dsi83_readable_ranges,
	.n_yes_ranges = ARRAY_SIZE(sn65dsi83_readable_ranges),
};

static const struct regmap_range sn65dsi83_writeable_ranges[] = {
	regmap_reg_range(REG_RC_RESET, REG_RC_DSI_CLK),
	regmap_reg_range(REG_RC_PLL_EN, REG_RC_PLL_EN),
	regmap_reg_range(REG_DSI_LANE, REG_DSI_CLK),
	regmap_reg_range(REG_LVDS_FMT, REG_LVDS_CM),
	regmap_reg_range(REG_VID_CHA_ACTIVE_LINE_LENGTH_LOW,
			 REG_VID_CHA_ACTIVE_LINE_LENGTH_HIGH),
	regmap_reg_range(REG_VID_CHA_VERTICAL_DISPLAY_SIZE_LOW,
			 REG_VID_CHA_VERTICAL_DISPLAY_SIZE_HIGH),
	regmap_reg_range(REG_VID_CHA_SYNC_DELAY_LOW,
			 REG_VID_CHA_SYNC_DELAY_HIGH),
	regmap_reg_range(REG_VID_CHA_HSYNC_PULSE_WIDTH_LOW,
			 REG_VID_CHA_HSYNC_PULSE_WIDTH_HIGH),
	regmap_reg_range(REG_VID_CHA_VSYNC_PULSE_WIDTH_LOW,
			 REG_VID_CHA_VSYNC_PULSE_WIDTH_HIGH),
	regmap_reg_range(REG_VID_CHA_HORIZONTAL_BACK_PORCH,
			 REG_VID_CHA_HORIZONTAL_BACK_PORCH),
	regmap_reg_range(REG_VID_CHA_VERTICAL_BACK_PORCH,
			 REG_VID_CHA_VERTICAL_BACK_PORCH),
	regmap_reg_range(REG_VID_CHA_HORIZONTAL_FRONT_PORCH,
			 REG_VID_CHA_HORIZONTAL_FRONT_PORCH),
	regmap_reg_range(REG_VID_CHA_VERTICAL_FRONT_PORCH,
			 REG_VID_CHA_VERTICAL_FRONT_PORCH),
	regmap_reg_range(REG_VID_CHA_TEST_PATTERN, REG_VID_CHA_TEST_PATTERN),
	regmap_reg_range(REG_IRQ_GLOBAL, REG_IRQ_EN),
	regmap_reg_range(REG_IRQ_STAT, REG_IRQ_STAT),
};

static const struct regmap_access_table sn65dsi83_writeable_table = {
	.yes_ranges = sn65dsi83_writeable_ranges,
	.n_yes_ranges = ARRAY_SIZE(sn65dsi83_writeable_ranges),
};

static const struct regmap_range sn65dsi83_volatile_ranges[] = {
	regmap_reg_range(REG_RC_RESET, REG_RC_RESET),
	regmap_reg_range(REG_RC_LVDS_PLL, REG_RC_LVDS_PLL),
	regmap_reg_range(REG_IRQ_STAT, REG_IRQ_STAT),
};

static const struct regmap_access_table sn65dsi83_volatile_table = {
	.yes_ranges = sn65dsi83_volatile_ranges,
	.n_yes_ranges = ARRAY_SIZE(sn65dsi83_volatile_ranges),
};

static const struct regmap_config sn65dsi83_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.rd_table = &sn65dsi83_readable_table,
	.wr_table = &sn65dsi83_writeable_table,
	.volatile_table = &sn65dsi83_volatile_table,
	.cache_type = REGCACHE_RBTREE,
	.max_register = REG_IRQ_STAT,
};

static struct sn65dsi83 *bridge_to_sn65dsi83(struct drm_bridge *bridge)
{
	return container_of(bridge, struct sn65dsi83, bridge);
}

static int sn65dsi83_attach(struct drm_bridge *bridge,
			    enum drm_bridge_attach_flags flags)
{
	struct sn65dsi83 *ctx = bridge_to_sn65dsi83(bridge);

	return drm_bridge_attach(bridge->encoder, ctx->panel_bridge,
				 &ctx->bridge, flags);
}

static void sn65dsi83_detach(struct drm_bridge *bridge)
{
	struct sn65dsi83 *ctx = bridge_to_sn65dsi83(bridge);

	if (!ctx->dsi)
		return;

	ctx->dsi = NULL;
}

static u8 sn65dsi83_get_lvds_range(struct sn65dsi83 *ctx,
				   const struct drm_display_mode *mode)
{
	 
	int mode_clock = mode->clock;

	if (ctx->lvds_dual_link)
		mode_clock /= 2;

	return (mode_clock - 12500) / 25000;
}

static u8 sn65dsi83_get_dsi_range(struct sn65dsi83 *ctx,
				  const struct drm_display_mode *mode)
{
	 
	return DIV_ROUND_UP(clamp((unsigned int)mode->clock *
			    mipi_dsi_pixel_format_to_bpp(ctx->dsi->format) /
			    ctx->dsi->lanes / 2, 40000U, 500000U), 5000U);
}

static u8 sn65dsi83_get_dsi_div(struct sn65dsi83 *ctx)
{
	 
	unsigned int dsi_div = mipi_dsi_pixel_format_to_bpp(ctx->dsi->format);

	dsi_div /= ctx->dsi->lanes;

	if (!ctx->lvds_dual_link)
		dsi_div /= 2;

	return dsi_div - 1;
}

static void sn65dsi83_atomic_pre_enable(struct drm_bridge *bridge,
					struct drm_bridge_state *old_bridge_state)
{
	struct sn65dsi83 *ctx = bridge_to_sn65dsi83(bridge);
	struct drm_atomic_state *state = old_bridge_state->base.state;
	const struct drm_bridge_state *bridge_state;
	const struct drm_crtc_state *crtc_state;
	const struct drm_display_mode *mode;
	struct drm_connector *connector;
	struct drm_crtc *crtc;
	bool lvds_format_24bpp;
	bool lvds_format_jeida;
	unsigned int pval;
	__le16 le16val;
	u16 val;
	int ret;

	ret = regulator_enable(ctx->vcc);
	if (ret) {
		dev_err(ctx->dev, "Failed to enable vcc: %d\n", ret);
		return;
	}

	 
	gpiod_set_value_cansleep(ctx->enable_gpio, 1);
	usleep_range(10000, 11000);

	 
	bridge_state = drm_atomic_get_new_bridge_state(state, bridge);

	switch (bridge_state->output_bus_cfg.format) {
	case MEDIA_BUS_FMT_RGB666_1X7X3_SPWG:
		lvds_format_24bpp = false;
		lvds_format_jeida = true;
		break;
	case MEDIA_BUS_FMT_RGB888_1X7X4_JEIDA:
		lvds_format_24bpp = true;
		lvds_format_jeida = true;
		break;
	case MEDIA_BUS_FMT_RGB888_1X7X4_SPWG:
		lvds_format_24bpp = true;
		lvds_format_jeida = false;
		break;
	default:
		 
		lvds_format_24bpp = true;
		lvds_format_jeida = false;
		dev_warn(ctx->dev,
			 "Unsupported LVDS bus format 0x%04x, please check output bridge driver. Falling back to SPWG24.\n",
			 bridge_state->output_bus_cfg.format);
		break;
	}

	 
	connector = drm_atomic_get_new_connector_for_encoder(state,
							     bridge->encoder);
	crtc = drm_atomic_get_new_connector_state(state, connector)->crtc;
	crtc_state = drm_atomic_get_new_crtc_state(state, crtc);
	mode = &crtc_state->adjusted_mode;

	 
	regmap_write(ctx->regmap, REG_RC_RESET, 0x00);
	regmap_write(ctx->regmap, REG_RC_PLL_EN, 0x00);

	 
	regmap_write(ctx->regmap, REG_RC_LVDS_PLL,
		     REG_RC_LVDS_PLL_LVDS_CLK_RANGE(sn65dsi83_get_lvds_range(ctx, mode)) |
		     REG_RC_LVDS_PLL_HS_CLK_SRC_DPHY);
	regmap_write(ctx->regmap, REG_DSI_CLK,
		     REG_DSI_CLK_CHA_DSI_CLK_RANGE(sn65dsi83_get_dsi_range(ctx, mode)));
	regmap_write(ctx->regmap, REG_RC_DSI_CLK,
		     REG_RC_DSI_CLK_DSI_CLK_DIVIDER(sn65dsi83_get_dsi_div(ctx)));

	 
	regmap_write(ctx->regmap, REG_DSI_LANE,
		     REG_DSI_LANE_DSI_CHANNEL_MODE_SINGLE |
		     REG_DSI_LANE_CHA_DSI_LANES(~(ctx->dsi->lanes - 1)) |
		      
		     REG_DSI_LANE_CHB_DSI_LANES(3));
	 
	regmap_write(ctx->regmap, REG_DSI_EQ, 0x00);

	 
	val = (mode->flags & DRM_MODE_FLAG_NHSYNC ?
	       REG_LVDS_FMT_HS_NEG_POLARITY : 0) |
	      (mode->flags & DRM_MODE_FLAG_NVSYNC ?
	       REG_LVDS_FMT_VS_NEG_POLARITY : 0);

	 
	if (lvds_format_24bpp) {
		val |= REG_LVDS_FMT_CHA_24BPP_MODE;
		if (ctx->lvds_dual_link)
			val |= REG_LVDS_FMT_CHB_24BPP_MODE;
	}

	 
	if (lvds_format_jeida) {
		val |= REG_LVDS_FMT_CHA_24BPP_FORMAT1;
		if (ctx->lvds_dual_link)
			val |= REG_LVDS_FMT_CHB_24BPP_FORMAT1;
	}

	 
	if (!ctx->lvds_dual_link)
		val |= REG_LVDS_FMT_LVDS_LINK_CFG;

	regmap_write(ctx->regmap, REG_LVDS_FMT, val);
	regmap_write(ctx->regmap, REG_LVDS_VCOM, 0x05);
	regmap_write(ctx->regmap, REG_LVDS_LANE,
		     (ctx->lvds_dual_link_even_odd_swap ?
		      REG_LVDS_LANE_EVEN_ODD_SWAP : 0) |
		     REG_LVDS_LANE_CHA_LVDS_TERM |
		     REG_LVDS_LANE_CHB_LVDS_TERM);
	regmap_write(ctx->regmap, REG_LVDS_CM, 0x00);

	le16val = cpu_to_le16(mode->hdisplay);
	regmap_bulk_write(ctx->regmap, REG_VID_CHA_ACTIVE_LINE_LENGTH_LOW,
			  &le16val, 2);
	le16val = cpu_to_le16(mode->vdisplay);
	regmap_bulk_write(ctx->regmap, REG_VID_CHA_VERTICAL_DISPLAY_SIZE_LOW,
			  &le16val, 2);
	 
	le16val = cpu_to_le16(32 + 1);
	regmap_bulk_write(ctx->regmap, REG_VID_CHA_SYNC_DELAY_LOW, &le16val, 2);
	le16val = cpu_to_le16(mode->hsync_end - mode->hsync_start);
	regmap_bulk_write(ctx->regmap, REG_VID_CHA_HSYNC_PULSE_WIDTH_LOW,
			  &le16val, 2);
	le16val = cpu_to_le16(mode->vsync_end - mode->vsync_start);
	regmap_bulk_write(ctx->regmap, REG_VID_CHA_VSYNC_PULSE_WIDTH_LOW,
			  &le16val, 2);
	regmap_write(ctx->regmap, REG_VID_CHA_HORIZONTAL_BACK_PORCH,
		     mode->htotal - mode->hsync_end);
	regmap_write(ctx->regmap, REG_VID_CHA_VERTICAL_BACK_PORCH,
		     mode->vtotal - mode->vsync_end);
	regmap_write(ctx->regmap, REG_VID_CHA_HORIZONTAL_FRONT_PORCH,
		     mode->hsync_start - mode->hdisplay);
	regmap_write(ctx->regmap, REG_VID_CHA_VERTICAL_FRONT_PORCH,
		     mode->vsync_start - mode->vdisplay);
	regmap_write(ctx->regmap, REG_VID_CHA_TEST_PATTERN, 0x00);

	 
	regmap_write(ctx->regmap, REG_RC_PLL_EN, REG_RC_PLL_EN_PLL_EN);
	usleep_range(3000, 4000);
	ret = regmap_read_poll_timeout(ctx->regmap, REG_RC_LVDS_PLL, pval,
				       pval & REG_RC_LVDS_PLL_PLL_EN_STAT,
				       1000, 100000);
	if (ret) {
		dev_err(ctx->dev, "failed to lock PLL, ret=%i\n", ret);
		 
		regmap_write(ctx->regmap, REG_RC_PLL_EN, 0x00);
		regulator_disable(ctx->vcc);
		return;
	}

	 
	regmap_write(ctx->regmap, REG_RC_RESET, REG_RC_RESET_SOFT_RESET);

	 
	usleep_range(10000, 12000);
}

static void sn65dsi83_atomic_enable(struct drm_bridge *bridge,
				    struct drm_bridge_state *old_bridge_state)
{
	struct sn65dsi83 *ctx = bridge_to_sn65dsi83(bridge);
	unsigned int pval;

	 
	regmap_read(ctx->regmap, REG_IRQ_STAT, &pval);
	regmap_write(ctx->regmap, REG_IRQ_STAT, pval);

	 
	usleep_range(1000, 1100);
	regmap_read(ctx->regmap, REG_IRQ_STAT, &pval);
	if (pval)
		dev_err(ctx->dev, "Unexpected link status 0x%02x\n", pval);
}

static void sn65dsi83_atomic_disable(struct drm_bridge *bridge,
				     struct drm_bridge_state *old_bridge_state)
{
	struct sn65dsi83 *ctx = bridge_to_sn65dsi83(bridge);
	int ret;

	 
	gpiod_set_value_cansleep(ctx->enable_gpio, 0);
	usleep_range(10000, 11000);

	ret = regulator_disable(ctx->vcc);
	if (ret)
		dev_err(ctx->dev, "Failed to disable vcc: %d\n", ret);

	regcache_mark_dirty(ctx->regmap);
}

static enum drm_mode_status
sn65dsi83_mode_valid(struct drm_bridge *bridge,
		     const struct drm_display_info *info,
		     const struct drm_display_mode *mode)
{
	 
	if (mode->clock < 25000)
		return MODE_CLOCK_LOW;
	if (mode->clock > 154000)
		return MODE_CLOCK_HIGH;

	return MODE_OK;
}

#define MAX_INPUT_SEL_FORMATS	1

static u32 *
sn65dsi83_atomic_get_input_bus_fmts(struct drm_bridge *bridge,
				    struct drm_bridge_state *bridge_state,
				    struct drm_crtc_state *crtc_state,
				    struct drm_connector_state *conn_state,
				    u32 output_fmt,
				    unsigned int *num_input_fmts)
{
	u32 *input_fmts;

	*num_input_fmts = 0;

	input_fmts = kcalloc(MAX_INPUT_SEL_FORMATS, sizeof(*input_fmts),
			     GFP_KERNEL);
	if (!input_fmts)
		return NULL;

	 
	input_fmts[0] = MEDIA_BUS_FMT_RGB888_1X24;
	*num_input_fmts = 1;

	return input_fmts;
}

static const struct drm_bridge_funcs sn65dsi83_funcs = {
	.attach			= sn65dsi83_attach,
	.detach			= sn65dsi83_detach,
	.atomic_enable		= sn65dsi83_atomic_enable,
	.atomic_pre_enable	= sn65dsi83_atomic_pre_enable,
	.atomic_disable		= sn65dsi83_atomic_disable,
	.mode_valid		= sn65dsi83_mode_valid,

	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state,
	.atomic_reset = drm_atomic_helper_bridge_reset,
	.atomic_get_input_bus_fmts = sn65dsi83_atomic_get_input_bus_fmts,
};

static int sn65dsi83_parse_dt(struct sn65dsi83 *ctx, enum sn65dsi83_model model)
{
	struct drm_bridge *panel_bridge;
	struct device *dev = ctx->dev;

	ctx->lvds_dual_link = false;
	ctx->lvds_dual_link_even_odd_swap = false;
	if (model != MODEL_SN65DSI83) {
		struct device_node *port2, *port3;
		int dual_link;

		port2 = of_graph_get_port_by_id(dev->of_node, 2);
		port3 = of_graph_get_port_by_id(dev->of_node, 3);
		dual_link = drm_of_lvds_get_dual_link_pixel_order(port2, port3);
		of_node_put(port2);
		of_node_put(port3);

		if (dual_link == DRM_LVDS_DUAL_LINK_ODD_EVEN_PIXELS) {
			ctx->lvds_dual_link = true;
			 
			ctx->lvds_dual_link_even_odd_swap = false;
		} else if (dual_link == DRM_LVDS_DUAL_LINK_EVEN_ODD_PIXELS) {
			ctx->lvds_dual_link = true;
			 
			ctx->lvds_dual_link_even_odd_swap = true;
		}
	}

	panel_bridge = devm_drm_of_get_bridge(dev, dev->of_node, 2, 0);
	if (IS_ERR(panel_bridge))
		return PTR_ERR(panel_bridge);

	ctx->panel_bridge = panel_bridge;

	ctx->vcc = devm_regulator_get(dev, "vcc");
	if (IS_ERR(ctx->vcc))
		return dev_err_probe(dev, PTR_ERR(ctx->vcc),
				     "Failed to get supply 'vcc'\n");

	return 0;
}

static int sn65dsi83_host_attach(struct sn65dsi83 *ctx)
{
	struct device *dev = ctx->dev;
	struct device_node *host_node;
	struct device_node *endpoint;
	struct mipi_dsi_device *dsi;
	struct mipi_dsi_host *host;
	const struct mipi_dsi_device_info info = {
		.type = "sn65dsi83",
		.channel = 0,
		.node = NULL,
	};
	int dsi_lanes, ret;

	endpoint = of_graph_get_endpoint_by_regs(dev->of_node, 0, -1);
	dsi_lanes = drm_of_get_data_lanes_count(endpoint, 1, 4);
	host_node = of_graph_get_remote_port_parent(endpoint);
	host = of_find_mipi_dsi_host_by_node(host_node);
	of_node_put(host_node);
	of_node_put(endpoint);

	if (!host)
		return -EPROBE_DEFER;

	if (dsi_lanes < 0)
		return dsi_lanes;

	dsi = devm_mipi_dsi_device_register_full(dev, host, &info);
	if (IS_ERR(dsi))
		return dev_err_probe(dev, PTR_ERR(dsi),
				     "failed to create dsi device\n");

	ctx->dsi = dsi;

	dsi->lanes = dsi_lanes;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_VIDEO_NO_HFP | MIPI_DSI_MODE_VIDEO_NO_HBP |
			  MIPI_DSI_MODE_VIDEO_NO_HSA | MIPI_DSI_MODE_NO_EOT_PACKET;

	ret = devm_mipi_dsi_attach(dev, dsi);
	if (ret < 0) {
		dev_err(dev, "failed to attach dsi to host: %d\n", ret);
		return ret;
	}

	return 0;
}

static int sn65dsi83_probe(struct i2c_client *client)
{
	const struct i2c_device_id *id = i2c_client_get_device_id(client);
	struct device *dev = &client->dev;
	enum sn65dsi83_model model;
	struct sn65dsi83 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;

	if (dev->of_node) {
		model = (enum sn65dsi83_model)(uintptr_t)
			of_device_get_match_data(dev);
	} else {
		model = id->driver_data;
	}

	 
	ctx->enable_gpio = devm_gpiod_get_optional(ctx->dev, "enable",
						   GPIOD_OUT_LOW);
	if (IS_ERR(ctx->enable_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->enable_gpio), "failed to get enable GPIO\n");

	usleep_range(10000, 11000);

	ret = sn65dsi83_parse_dt(ctx, model);
	if (ret)
		return ret;

	ctx->regmap = devm_regmap_init_i2c(client, &sn65dsi83_regmap_config);
	if (IS_ERR(ctx->regmap))
		return dev_err_probe(dev, PTR_ERR(ctx->regmap), "failed to get regmap\n");

	dev_set_drvdata(dev, ctx);
	i2c_set_clientdata(client, ctx);

	ctx->bridge.funcs = &sn65dsi83_funcs;
	ctx->bridge.of_node = dev->of_node;
	ctx->bridge.pre_enable_prev_first = true;
	drm_bridge_add(&ctx->bridge);

	ret = sn65dsi83_host_attach(ctx);
	if (ret) {
		dev_err_probe(dev, ret, "failed to attach DSI host\n");
		goto err_remove_bridge;
	}

	return 0;

err_remove_bridge:
	drm_bridge_remove(&ctx->bridge);
	return ret;
}

static void sn65dsi83_remove(struct i2c_client *client)
{
	struct sn65dsi83 *ctx = i2c_get_clientdata(client);

	drm_bridge_remove(&ctx->bridge);
}

static struct i2c_device_id sn65dsi83_id[] = {
	{ "ti,sn65dsi83", MODEL_SN65DSI83 },
	{ "ti,sn65dsi84", MODEL_SN65DSI84 },
	{},
};
MODULE_DEVICE_TABLE(i2c, sn65dsi83_id);

static const struct of_device_id sn65dsi83_match_table[] = {
	{ .compatible = "ti,sn65dsi83", .data = (void *)MODEL_SN65DSI83 },
	{ .compatible = "ti,sn65dsi84", .data = (void *)MODEL_SN65DSI84 },
	{},
};
MODULE_DEVICE_TABLE(of, sn65dsi83_match_table);

static struct i2c_driver sn65dsi83_driver = {
	.probe = sn65dsi83_probe,
	.remove = sn65dsi83_remove,
	.id_table = sn65dsi83_id,
	.driver = {
		.name = "sn65dsi83",
		.of_match_table = sn65dsi83_match_table,
	},
};
module_i2c_driver(sn65dsi83_driver);

MODULE_AUTHOR("Marek Vasut <marex@denx.de>");
MODULE_DESCRIPTION("TI SN65DSI83 DSI to LVDS bridge driver");
MODULE_LICENSE("GPL v2");
