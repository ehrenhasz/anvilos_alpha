 

#include <linux/slab.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_edid.h>
#include <drm/drm_mipi_dsi.h>

#include "i915_drv.h"
#include "i915_reg.h"
#include "intel_atomic.h"
#include "intel_backlight.h"
#include "intel_connector.h"
#include "intel_crtc.h"
#include "intel_de.h"
#include "intel_display_types.h"
#include "intel_dsi.h"
#include "intel_dsi_vbt.h"
#include "intel_fifo_underrun.h"
#include "intel_panel.h"
#include "skl_scaler.h"
#include "vlv_dsi.h"
#include "vlv_dsi_pll.h"
#include "vlv_dsi_regs.h"
#include "vlv_sideband.h"

 
static u16 txbyteclkhs(u16 pixels, int bpp, int lane_count,
		       u16 burst_mode_ratio)
{
	return DIV_ROUND_UP(DIV_ROUND_UP(pixels * bpp * burst_mode_ratio,
					 8 * 100), lane_count);
}

 
static u16 pixels_from_txbyteclkhs(u16 clk_hs, int bpp, int lane_count,
			u16 burst_mode_ratio)
{
	return DIV_ROUND_UP((clk_hs * lane_count * 8 * 100),
						(bpp * burst_mode_ratio));
}

enum mipi_dsi_pixel_format pixel_format_from_register_bits(u32 fmt)
{
	 
	switch (fmt) {
	case VID_MODE_FORMAT_RGB888:
		return MIPI_DSI_FMT_RGB888;
	case VID_MODE_FORMAT_RGB666:
		return MIPI_DSI_FMT_RGB666;
	case VID_MODE_FORMAT_RGB666_PACKED:
		return MIPI_DSI_FMT_RGB666_PACKED;
	case VID_MODE_FORMAT_RGB565:
		return MIPI_DSI_FMT_RGB565;
	default:
		MISSING_CASE(fmt);
		return MIPI_DSI_FMT_RGB666;
	}
}

void vlv_dsi_wait_for_fifo_empty(struct intel_dsi *intel_dsi, enum port port)
{
	struct drm_encoder *encoder = &intel_dsi->base.base;
	struct drm_device *dev = encoder->dev;
	struct drm_i915_private *dev_priv = to_i915(dev);
	u32 mask;

	mask = LP_CTRL_FIFO_EMPTY | HS_CTRL_FIFO_EMPTY |
		LP_DATA_FIFO_EMPTY | HS_DATA_FIFO_EMPTY;

	if (intel_de_wait_for_set(dev_priv, MIPI_GEN_FIFO_STAT(port),
				  mask, 100))
		drm_err(&dev_priv->drm, "DPI FIFOs are not empty\n");
}

static void write_data(struct drm_i915_private *dev_priv,
		       i915_reg_t reg,
		       const u8 *data, u32 len)
{
	u32 i, j;

	for (i = 0; i < len; i += 4) {
		u32 val = 0;

		for (j = 0; j < min_t(u32, len - i, 4); j++)
			val |= *data++ << 8 * j;

		intel_de_write(dev_priv, reg, val);
	}
}

static void read_data(struct drm_i915_private *dev_priv,
		      i915_reg_t reg,
		      u8 *data, u32 len)
{
	u32 i, j;

	for (i = 0; i < len; i += 4) {
		u32 val = intel_de_read(dev_priv, reg);

		for (j = 0; j < min_t(u32, len - i, 4); j++)
			*data++ = val >> 8 * j;
	}
}

static ssize_t intel_dsi_host_transfer(struct mipi_dsi_host *host,
				       const struct mipi_dsi_msg *msg)
{
	struct intel_dsi_host *intel_dsi_host = to_intel_dsi_host(host);
	struct drm_device *dev = intel_dsi_host->intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = to_i915(dev);
	enum port port = intel_dsi_host->port;
	struct mipi_dsi_packet packet;
	ssize_t ret;
	const u8 *header;
	i915_reg_t data_reg, ctrl_reg;
	u32 data_mask, ctrl_mask;

	ret = mipi_dsi_create_packet(&packet, msg);
	if (ret < 0)
		return ret;

	header = packet.header;

	if (msg->flags & MIPI_DSI_MSG_USE_LPM) {
		data_reg = MIPI_LP_GEN_DATA(port);
		data_mask = LP_DATA_FIFO_FULL;
		ctrl_reg = MIPI_LP_GEN_CTRL(port);
		ctrl_mask = LP_CTRL_FIFO_FULL;
	} else {
		data_reg = MIPI_HS_GEN_DATA(port);
		data_mask = HS_DATA_FIFO_FULL;
		ctrl_reg = MIPI_HS_GEN_CTRL(port);
		ctrl_mask = HS_CTRL_FIFO_FULL;
	}

	 
	if (packet.payload_length) {
		if (intel_de_wait_for_clear(dev_priv, MIPI_GEN_FIFO_STAT(port),
					    data_mask, 50))
			drm_err(&dev_priv->drm,
				"Timeout waiting for HS/LP DATA FIFO !full\n");

		write_data(dev_priv, data_reg, packet.payload,
			   packet.payload_length);
	}

	if (msg->rx_len) {
		intel_de_write(dev_priv, MIPI_INTR_STAT(port),
			       GEN_READ_DATA_AVAIL);
	}

	if (intel_de_wait_for_clear(dev_priv, MIPI_GEN_FIFO_STAT(port),
				    ctrl_mask, 50)) {
		drm_err(&dev_priv->drm,
			"Timeout waiting for HS/LP CTRL FIFO !full\n");
	}

	intel_de_write(dev_priv, ctrl_reg,
		       header[2] << 16 | header[1] << 8 | header[0]);

	 
	if (msg->rx_len) {
		data_mask = GEN_READ_DATA_AVAIL;
		if (intel_de_wait_for_set(dev_priv, MIPI_INTR_STAT(port),
					  data_mask, 50))
			drm_err(&dev_priv->drm,
				"Timeout waiting for read data.\n");

		read_data(dev_priv, data_reg, msg->rx_buf, msg->rx_len);
	}

	 
	return 4 + packet.payload_length;
}

static int intel_dsi_host_attach(struct mipi_dsi_host *host,
				 struct mipi_dsi_device *dsi)
{
	return 0;
}

static int intel_dsi_host_detach(struct mipi_dsi_host *host,
				 struct mipi_dsi_device *dsi)
{
	return 0;
}

static const struct mipi_dsi_host_ops intel_dsi_host_ops = {
	.attach = intel_dsi_host_attach,
	.detach = intel_dsi_host_detach,
	.transfer = intel_dsi_host_transfer,
};

 
static int dpi_send_cmd(struct intel_dsi *intel_dsi, u32 cmd, bool hs,
			enum port port)
{
	struct drm_encoder *encoder = &intel_dsi->base.base;
	struct drm_device *dev = encoder->dev;
	struct drm_i915_private *dev_priv = to_i915(dev);
	u32 mask;

	 
	if (hs)
		cmd &= ~DPI_LP_MODE;
	else
		cmd |= DPI_LP_MODE;

	 
	intel_de_write(dev_priv, MIPI_INTR_STAT(port), SPL_PKT_SENT_INTERRUPT);

	 
	if (cmd == intel_de_read(dev_priv, MIPI_DPI_CONTROL(port)))
		drm_dbg_kms(&dev_priv->drm,
			    "Same special packet %02x twice in a row.\n", cmd);

	intel_de_write(dev_priv, MIPI_DPI_CONTROL(port), cmd);

	mask = SPL_PKT_SENT_INTERRUPT;
	if (intel_de_wait_for_set(dev_priv, MIPI_INTR_STAT(port), mask, 100))
		drm_err(&dev_priv->drm,
			"Video mode command 0x%08x send failed.\n", cmd);

	return 0;
}

static void band_gap_reset(struct drm_i915_private *dev_priv)
{
	vlv_flisdsi_get(dev_priv);

	vlv_flisdsi_write(dev_priv, 0x08, 0x0001);
	vlv_flisdsi_write(dev_priv, 0x0F, 0x0005);
	vlv_flisdsi_write(dev_priv, 0x0F, 0x0025);
	udelay(150);
	vlv_flisdsi_write(dev_priv, 0x0F, 0x0000);
	vlv_flisdsi_write(dev_priv, 0x08, 0x0000);

	vlv_flisdsi_put(dev_priv);
}

static int intel_dsi_compute_config(struct intel_encoder *encoder,
				    struct intel_crtc_state *pipe_config,
				    struct drm_connector_state *conn_state)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct intel_dsi *intel_dsi = container_of(encoder, struct intel_dsi,
						   base);
	struct intel_connector *intel_connector = intel_dsi->attached_connector;
	struct drm_display_mode *adjusted_mode = &pipe_config->hw.adjusted_mode;
	int ret;

	drm_dbg_kms(&dev_priv->drm, "\n");
	pipe_config->sink_format = INTEL_OUTPUT_FORMAT_RGB;
	pipe_config->output_format = INTEL_OUTPUT_FORMAT_RGB;

	ret = intel_panel_compute_config(intel_connector, adjusted_mode);
	if (ret)
		return ret;

	ret = intel_panel_fitting(pipe_config, conn_state);
	if (ret)
		return ret;

	if (adjusted_mode->flags & DRM_MODE_FLAG_DBLSCAN)
		return -EINVAL;

	 
	adjusted_mode->flags = 0;

	if (intel_dsi->pixel_format == MIPI_DSI_FMT_RGB888)
		pipe_config->pipe_bpp = 24;
	else
		pipe_config->pipe_bpp = 18;

	if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv)) {
		 
		pipe_config->mode_flags |=
			I915_MODE_FLAG_GET_SCANLINE_FROM_TIMESTAMP;

		 
		if (intel_dsi->ports == BIT(PORT_C))
			pipe_config->cpu_transcoder = TRANSCODER_DSI_C;
		else
			pipe_config->cpu_transcoder = TRANSCODER_DSI_A;

		ret = bxt_dsi_pll_compute(encoder, pipe_config);
		if (ret)
			return -EINVAL;
	} else {
		ret = vlv_dsi_pll_compute(encoder, pipe_config);
		if (ret)
			return -EINVAL;
	}

	pipe_config->clock_set = true;

	return 0;
}

static bool glk_dsi_enable_io(struct intel_encoder *encoder)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(encoder);
	enum port port;
	bool cold_boot = false;

	 
	for_each_dsi_port(port, intel_dsi->ports)
		intel_de_rmw(dev_priv, MIPI_CTRL(port), 0, GLK_MIPIIO_ENABLE);

	 
	intel_de_rmw(dev_priv, MIPI_CTRL(PORT_A), GLK_MIPIIO_RESET_RELEASED, 0);

	 
	for_each_dsi_port(port, intel_dsi->ports) {
		u32 tmp = intel_de_read(dev_priv, MIPI_DEVICE_READY(port));
		intel_de_rmw(dev_priv, MIPI_CTRL(port),
			     GLK_LP_WAKE, (tmp & DEVICE_READY) ? GLK_LP_WAKE : 0);
	}

	 
	for_each_dsi_port(port, intel_dsi->ports) {
		if (intel_de_wait_for_set(dev_priv, MIPI_CTRL(port),
					  GLK_MIPIIO_PORT_POWERED, 20))
			drm_err(&dev_priv->drm, "MIPIO port is powergated\n");
	}

	 
	for_each_dsi_port(port, intel_dsi->ports) {
		cold_boot |=
			!(intel_de_read(dev_priv, MIPI_DEVICE_READY(port)) & DEVICE_READY);
	}

	return cold_boot;
}

static void glk_dsi_device_ready(struct intel_encoder *encoder)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(encoder);
	enum port port;

	 
	for_each_dsi_port(port, intel_dsi->ports) {
		if (intel_de_wait_for_set(dev_priv, MIPI_CTRL(port),
					  GLK_PHY_STATUS_PORT_READY, 20))
			drm_err(&dev_priv->drm, "PHY is not ON\n");
	}

	 
	intel_de_rmw(dev_priv, MIPI_CTRL(PORT_A), 0, GLK_MIPIIO_RESET_RELEASED);

	 
	for_each_dsi_port(port, intel_dsi->ports) {
		if (!(intel_de_read(dev_priv, MIPI_DEVICE_READY(port)) & DEVICE_READY)) {
			intel_de_rmw(dev_priv, MIPI_DEVICE_READY(port),
				     ULPS_STATE_MASK, DEVICE_READY);
			usleep_range(10, 15);
		} else {
			 
			intel_de_rmw(dev_priv, MIPI_DEVICE_READY(port),
				     ULPS_STATE_MASK, ULPS_STATE_ENTER | DEVICE_READY);

			 
			if (intel_de_wait_for_clear(dev_priv, MIPI_CTRL(port),
						    GLK_ULPS_NOT_ACTIVE, 20))
				drm_err(&dev_priv->drm, "ULPS not active\n");

			 
			intel_de_rmw(dev_priv, MIPI_DEVICE_READY(port),
				     ULPS_STATE_MASK, ULPS_STATE_EXIT | DEVICE_READY);

			 
			intel_de_rmw(dev_priv, MIPI_DEVICE_READY(port),
				     ULPS_STATE_MASK,
				     ULPS_STATE_NORMAL_OPERATION | DEVICE_READY);

			intel_de_rmw(dev_priv, MIPI_CTRL(port), GLK_LP_WAKE, 0);
		}
	}

	 
	for_each_dsi_port(port, intel_dsi->ports) {
		if (intel_de_wait_for_set(dev_priv, MIPI_CTRL(port),
					  GLK_DATA_LANE_STOP_STATE, 20))
			drm_err(&dev_priv->drm,
				"Date lane not in STOP state\n");
	}

	 
	for_each_dsi_port(port, intel_dsi->ports) {
		if (intel_de_wait_for_set(dev_priv, BXT_MIPI_PORT_CTRL(port),
					  AFE_LATCHOUT, 20))
			drm_err(&dev_priv->drm,
				"D-PHY not entering LP-11 state\n");
	}
}

static void bxt_dsi_device_ready(struct intel_encoder *encoder)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(encoder);
	enum port port;
	u32 val;

	drm_dbg_kms(&dev_priv->drm, "\n");

	 
	for_each_dsi_port(port, intel_dsi->ports) {
		intel_de_rmw(dev_priv, BXT_MIPI_PORT_CTRL(port), 0, LP_OUTPUT_HOLD);
		usleep_range(2000, 2500);
	}

	 
	for_each_dsi_port(port, intel_dsi->ports) {
		val = intel_de_read(dev_priv, MIPI_DEVICE_READY(port));
		val &= ~ULPS_STATE_MASK;
		intel_de_write(dev_priv, MIPI_DEVICE_READY(port), val);
		usleep_range(2000, 2500);
		val |= DEVICE_READY;
		intel_de_write(dev_priv, MIPI_DEVICE_READY(port), val);
	}
}

static void vlv_dsi_device_ready(struct intel_encoder *encoder)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(encoder);
	enum port port;

	drm_dbg_kms(&dev_priv->drm, "\n");

	vlv_flisdsi_get(dev_priv);
	 
	vlv_flisdsi_write(dev_priv, 0x04, 0x0004);
	vlv_flisdsi_put(dev_priv);

	 
	band_gap_reset(dev_priv);

	for_each_dsi_port(port, intel_dsi->ports) {

		intel_de_write(dev_priv, MIPI_DEVICE_READY(port),
			       ULPS_STATE_ENTER);
		usleep_range(2500, 3000);

		 
		intel_de_rmw(dev_priv, MIPI_PORT_CTRL(PORT_A), 0, LP_OUTPUT_HOLD);
		usleep_range(1000, 1500);

		intel_de_write(dev_priv, MIPI_DEVICE_READY(port),
			       ULPS_STATE_EXIT);
		usleep_range(2500, 3000);

		intel_de_write(dev_priv, MIPI_DEVICE_READY(port),
			       DEVICE_READY);
		usleep_range(2500, 3000);
	}
}

static void intel_dsi_device_ready(struct intel_encoder *encoder)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);

	if (IS_GEMINILAKE(dev_priv))
		glk_dsi_device_ready(encoder);
	else if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv))
		bxt_dsi_device_ready(encoder);
	else
		vlv_dsi_device_ready(encoder);
}

static void glk_dsi_enter_low_power_mode(struct intel_encoder *encoder)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(encoder);
	enum port port;

	 
	for_each_dsi_port(port, intel_dsi->ports)
		intel_de_rmw(dev_priv, MIPI_DEVICE_READY(port),
			     ULPS_STATE_MASK, ULPS_STATE_ENTER | DEVICE_READY);

	 
	for_each_dsi_port(port, intel_dsi->ports) {
		if (intel_de_wait_for_clear(dev_priv, MIPI_CTRL(port),
					    GLK_PHY_STATUS_PORT_READY, 20))
			drm_err(&dev_priv->drm, "PHY is not turning OFF\n");
	}

	 
	for_each_dsi_port(port, intel_dsi->ports) {
		if (intel_de_wait_for_clear(dev_priv, MIPI_CTRL(port),
					    GLK_MIPIIO_PORT_POWERED, 20))
			drm_err(&dev_priv->drm,
				"MIPI IO Port is not powergated\n");
	}
}

static void glk_dsi_disable_mipi_io(struct intel_encoder *encoder)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(encoder);
	enum port port;

	 
	intel_de_rmw(dev_priv, MIPI_CTRL(PORT_A), GLK_MIPIIO_RESET_RELEASED, 0);

	 
	for_each_dsi_port(port, intel_dsi->ports) {
		if (intel_de_wait_for_clear(dev_priv, MIPI_CTRL(port),
					    GLK_PHY_STATUS_PORT_READY, 20))
			drm_err(&dev_priv->drm, "PHY is not turning OFF\n");
	}

	 
	for_each_dsi_port(port, intel_dsi->ports)
		intel_de_rmw(dev_priv, MIPI_CTRL(port), GLK_MIPIIO_ENABLE, 0);
}

static void glk_dsi_clear_device_ready(struct intel_encoder *encoder)
{
	glk_dsi_enter_low_power_mode(encoder);
	glk_dsi_disable_mipi_io(encoder);
}

static void vlv_dsi_clear_device_ready(struct intel_encoder *encoder)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(encoder);
	enum port port;

	drm_dbg_kms(&dev_priv->drm, "\n");
	for_each_dsi_port(port, intel_dsi->ports) {
		 
		i915_reg_t port_ctrl = IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv) ?
			BXT_MIPI_PORT_CTRL(port) : MIPI_PORT_CTRL(PORT_A);

		intel_de_write(dev_priv, MIPI_DEVICE_READY(port),
			       DEVICE_READY | ULPS_STATE_ENTER);
		usleep_range(2000, 2500);

		intel_de_write(dev_priv, MIPI_DEVICE_READY(port),
			       DEVICE_READY | ULPS_STATE_EXIT);
		usleep_range(2000, 2500);

		intel_de_write(dev_priv, MIPI_DEVICE_READY(port),
			       DEVICE_READY | ULPS_STATE_ENTER);
		usleep_range(2000, 2500);

		 
		if ((IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv) || port == PORT_A) &&
		    intel_de_wait_for_clear(dev_priv, port_ctrl,
					    AFE_LATCHOUT, 30))
			drm_err(&dev_priv->drm, "DSI LP not going Low\n");

		 
		intel_de_rmw(dev_priv, port_ctrl, LP_OUTPUT_HOLD, 0);
		usleep_range(1000, 1500);

		intel_de_write(dev_priv, MIPI_DEVICE_READY(port), 0x00);
		usleep_range(2000, 2500);
	}
}

static void intel_dsi_port_enable(struct intel_encoder *encoder,
				  const struct intel_crtc_state *crtc_state)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(encoder);
	enum port port;

	if (intel_dsi->dual_link == DSI_DUAL_LINK_FRONT_BACK) {
		u32 temp = intel_dsi->pixel_overlap;

		if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv)) {
			for_each_dsi_port(port, intel_dsi->ports)
				intel_de_rmw(dev_priv, MIPI_CTRL(port),
					     BXT_PIXEL_OVERLAP_CNT_MASK,
					     temp << BXT_PIXEL_OVERLAP_CNT_SHIFT);
		} else {
			intel_de_rmw(dev_priv, VLV_CHICKEN_3,
				     PIXEL_OVERLAP_CNT_MASK,
				     temp << PIXEL_OVERLAP_CNT_SHIFT);
		}
	}

	for_each_dsi_port(port, intel_dsi->ports) {
		i915_reg_t port_ctrl = IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv) ?
			BXT_MIPI_PORT_CTRL(port) : MIPI_PORT_CTRL(port);
		u32 temp;

		temp = intel_de_read(dev_priv, port_ctrl);

		temp &= ~LANE_CONFIGURATION_MASK;
		temp &= ~DUAL_LINK_MODE_MASK;

		if (intel_dsi->ports == (BIT(PORT_A) | BIT(PORT_C))) {
			temp |= (intel_dsi->dual_link - 1)
						<< DUAL_LINK_MODE_SHIFT;
			if (IS_BROXTON(dev_priv))
				temp |= LANE_CONFIGURATION_DUAL_LINK_A;
			else
				temp |= crtc->pipe ?
					LANE_CONFIGURATION_DUAL_LINK_B :
					LANE_CONFIGURATION_DUAL_LINK_A;
		}

		if (intel_dsi->pixel_format != MIPI_DSI_FMT_RGB888)
			temp |= DITHERING_ENABLE;

		 
		intel_de_write(dev_priv, port_ctrl, temp | DPI_ENABLE);
		intel_de_posting_read(dev_priv, port_ctrl);
	}
}

static void intel_dsi_port_disable(struct intel_encoder *encoder)
{
	struct drm_device *dev = encoder->base.dev;
	struct drm_i915_private *dev_priv = to_i915(dev);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(encoder);
	enum port port;

	for_each_dsi_port(port, intel_dsi->ports) {
		i915_reg_t port_ctrl = IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv) ?
			BXT_MIPI_PORT_CTRL(port) : MIPI_PORT_CTRL(port);

		 
		intel_de_rmw(dev_priv, port_ctrl, DPI_ENABLE, 0);
		intel_de_posting_read(dev_priv, port_ctrl);
	}
}
static void intel_dsi_prepare(struct intel_encoder *intel_encoder,
			      const struct intel_crtc_state *pipe_config);
static void intel_dsi_unprepare(struct intel_encoder *encoder);

 

 
static void intel_dsi_pre_enable(struct intel_atomic_state *state,
				 struct intel_encoder *encoder,
				 const struct intel_crtc_state *pipe_config,
				 const struct drm_connector_state *conn_state)
{
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(encoder);
	struct intel_crtc *crtc = to_intel_crtc(pipe_config->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;
	enum port port;
	bool glk_cold_boot = false;

	drm_dbg_kms(&dev_priv->drm, "\n");

	intel_dsi_wait_panel_power_cycle(intel_dsi);

	intel_set_cpu_fifo_underrun_reporting(dev_priv, pipe, true);

	 
	if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv)) {
		bxt_dsi_pll_disable(encoder);
		bxt_dsi_pll_enable(encoder, pipe_config);
	} else {
		vlv_dsi_pll_disable(encoder);
		vlv_dsi_pll_enable(encoder, pipe_config);
	}

	if (IS_BROXTON(dev_priv)) {
		 
		intel_de_rmw(dev_priv, BXT_P_CR_GT_DISP_PWRON, 0, MIPIO_RST_CTRL);

		 
		intel_de_write(dev_priv, BXT_P_DSI_REGULATOR_CFG, STAP_SELECT);
		intel_de_write(dev_priv, BXT_P_DSI_REGULATOR_TX_CTRL, 0);
	}

	if (IS_VALLEYVIEW(dev_priv) || IS_CHERRYVIEW(dev_priv)) {
		 
		intel_de_rmw(dev_priv, DSPCLK_GATE_D(dev_priv),
			     0, DPOUNIT_CLOCK_GATE_DISABLE);
	}

	if (!IS_GEMINILAKE(dev_priv))
		intel_dsi_prepare(encoder, pipe_config);

	 
	intel_dsi_vbt_exec_sequence(intel_dsi, MIPI_SEQ_POWER_ON);
	msleep(intel_dsi->panel_on_delay);
	intel_dsi_vbt_exec_sequence(intel_dsi, MIPI_SEQ_DEASSERT_RESET);

	if (IS_GEMINILAKE(dev_priv)) {
		glk_cold_boot = glk_dsi_enable_io(encoder);

		 
		if (glk_cold_boot)
			intel_dsi_prepare(encoder, pipe_config);
	}

	 
	intel_dsi_device_ready(encoder);

	 
	if (IS_GEMINILAKE(dev_priv) && !glk_cold_boot)
		intel_dsi_prepare(encoder, pipe_config);

	 
	intel_dsi_vbt_exec_sequence(intel_dsi, MIPI_SEQ_INIT_OTP);

	 
	if (is_cmd_mode(intel_dsi)) {
		for_each_dsi_port(port, intel_dsi->ports)
			intel_de_write(dev_priv,
				       MIPI_MAX_RETURN_PKT_SIZE(port), 8 * 4);
		intel_dsi_vbt_exec_sequence(intel_dsi, MIPI_SEQ_TEAR_ON);
		intel_dsi_vbt_exec_sequence(intel_dsi, MIPI_SEQ_DISPLAY_ON);
	} else {
		msleep(20);  
		for_each_dsi_port(port, intel_dsi->ports)
			dpi_send_cmd(intel_dsi, TURN_ON, false, port);
		msleep(100);

		intel_dsi_vbt_exec_sequence(intel_dsi, MIPI_SEQ_DISPLAY_ON);

		intel_dsi_port_enable(encoder, pipe_config);
	}

	intel_backlight_enable(pipe_config, conn_state);
	intel_dsi_vbt_exec_sequence(intel_dsi, MIPI_SEQ_BACKLIGHT_ON);
}

static void bxt_dsi_enable(struct intel_atomic_state *state,
			   struct intel_encoder *encoder,
			   const struct intel_crtc_state *crtc_state,
			   const struct drm_connector_state *conn_state)
{
	intel_crtc_vblank_on(crtc_state);
}

 
static void intel_dsi_disable(struct intel_atomic_state *state,
			      struct intel_encoder *encoder,
			      const struct intel_crtc_state *old_crtc_state,
			      const struct drm_connector_state *old_conn_state)
{
	struct drm_i915_private *i915 = to_i915(encoder->base.dev);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(encoder);
	enum port port;

	drm_dbg_kms(&i915->drm, "\n");

	intel_dsi_vbt_exec_sequence(intel_dsi, MIPI_SEQ_BACKLIGHT_OFF);
	intel_backlight_disable(old_conn_state);

	 
	if (is_vid_mode(intel_dsi)) {
		 
		for_each_dsi_port(port, intel_dsi->ports)
			dpi_send_cmd(intel_dsi, SHUTDOWN, false, port);
		msleep(10);
	}
}

static void intel_dsi_clear_device_ready(struct intel_encoder *encoder)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);

	if (IS_GEMINILAKE(dev_priv))
		glk_dsi_clear_device_ready(encoder);
	else
		vlv_dsi_clear_device_ready(encoder);
}

static void intel_dsi_post_disable(struct intel_atomic_state *state,
				   struct intel_encoder *encoder,
				   const struct intel_crtc_state *old_crtc_state,
				   const struct drm_connector_state *old_conn_state)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(encoder);
	enum port port;

	drm_dbg_kms(&dev_priv->drm, "\n");

	if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv)) {
		intel_crtc_vblank_off(old_crtc_state);

		skl_scaler_disable(old_crtc_state);
	}

	if (is_vid_mode(intel_dsi)) {
		for_each_dsi_port(port, intel_dsi->ports)
			vlv_dsi_wait_for_fifo_empty(intel_dsi, port);

		intel_dsi_port_disable(encoder);
		usleep_range(2000, 5000);
	}

	intel_dsi_unprepare(encoder);

	 
	if (is_cmd_mode(intel_dsi))
		intel_dsi_vbt_exec_sequence(intel_dsi, MIPI_SEQ_TEAR_OFF);
	intel_dsi_vbt_exec_sequence(intel_dsi, MIPI_SEQ_DISPLAY_OFF);

	 
	intel_dsi_clear_device_ready(encoder);

	if (IS_BROXTON(dev_priv)) {
		 
		intel_de_write(dev_priv, BXT_P_DSI_REGULATOR_CFG, STAP_SELECT);
		intel_de_write(dev_priv, BXT_P_DSI_REGULATOR_TX_CTRL,
			       HS_IO_CTRL_SELECT);

		 
		intel_de_rmw(dev_priv, BXT_P_CR_GT_DISP_PWRON, MIPIO_RST_CTRL, 0);
	}

	if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv)) {
		bxt_dsi_pll_disable(encoder);
	} else {
		vlv_dsi_pll_disable(encoder);

		intel_de_rmw(dev_priv, DSPCLK_GATE_D(dev_priv),
			     DPOUNIT_CLOCK_GATE_DISABLE, 0);
	}

	 
	intel_dsi_vbt_exec_sequence(intel_dsi, MIPI_SEQ_ASSERT_RESET);

	msleep(intel_dsi->panel_off_delay);
	intel_dsi_vbt_exec_sequence(intel_dsi, MIPI_SEQ_POWER_OFF);

	intel_dsi->panel_power_off_time = ktime_get_boottime();
}

static bool intel_dsi_get_hw_state(struct intel_encoder *encoder,
				   enum pipe *pipe)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(encoder);
	intel_wakeref_t wakeref;
	enum port port;
	bool active = false;

	drm_dbg_kms(&dev_priv->drm, "\n");

	wakeref = intel_display_power_get_if_enabled(dev_priv,
						     encoder->power_domain);
	if (!wakeref)
		return false;

	 
	if ((IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv)) &&
	    !bxt_dsi_pll_is_enabled(dev_priv))
		goto out_put_power;

	 
	for_each_dsi_port(port, intel_dsi->ports) {
		i915_reg_t ctrl_reg = IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv) ?
			BXT_MIPI_PORT_CTRL(port) : MIPI_PORT_CTRL(port);
		bool enabled = intel_de_read(dev_priv, ctrl_reg) & DPI_ENABLE;

		 
		if ((IS_VALLEYVIEW(dev_priv) || IS_CHERRYVIEW(dev_priv)) &&
		    port == PORT_C)
			enabled = intel_de_read(dev_priv, TRANSCONF(PIPE_B)) & TRANSCONF_ENABLE;

		 
		if (!enabled) {
			u32 tmp = intel_de_read(dev_priv,
						MIPI_DSI_FUNC_PRG(port));
			enabled = tmp & CMD_MODE_DATA_WIDTH_MASK;
		}

		if (!enabled)
			continue;

		if (!(intel_de_read(dev_priv, MIPI_DEVICE_READY(port)) & DEVICE_READY))
			continue;

		if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv)) {
			u32 tmp = intel_de_read(dev_priv, MIPI_CTRL(port));
			tmp &= BXT_PIPE_SELECT_MASK;
			tmp >>= BXT_PIPE_SELECT_SHIFT;

			if (drm_WARN_ON(&dev_priv->drm, tmp > PIPE_C))
				continue;

			*pipe = tmp;
		} else {
			*pipe = port == PORT_A ? PIPE_A : PIPE_B;
		}

		active = true;
		break;
	}

out_put_power:
	intel_display_power_put(dev_priv, encoder->power_domain, wakeref);

	return active;
}

static void bxt_dsi_get_pipe_config(struct intel_encoder *encoder,
				    struct intel_crtc_state *pipe_config)
{
	struct drm_device *dev = encoder->base.dev;
	struct drm_i915_private *dev_priv = to_i915(dev);
	struct drm_display_mode *adjusted_mode =
					&pipe_config->hw.adjusted_mode;
	struct drm_display_mode *adjusted_mode_sw;
	struct intel_crtc *crtc = to_intel_crtc(pipe_config->uapi.crtc);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(encoder);
	unsigned int lane_count = intel_dsi->lane_count;
	unsigned int bpp, fmt;
	enum port port;
	u16 hactive, hfp, hsync, hbp, vfp, vsync;
	u16 hfp_sw, hsync_sw, hbp_sw;
	u16 crtc_htotal_sw, crtc_hsync_start_sw, crtc_hsync_end_sw,
				crtc_hblank_start_sw, crtc_hblank_end_sw;

	 
	adjusted_mode_sw = &crtc->config->hw.adjusted_mode;

	 
	for_each_dsi_port(port, intel_dsi->ports) {
		if (intel_de_read(dev_priv, BXT_MIPI_PORT_CTRL(port)) & DPI_ENABLE)
			break;
	}

	fmt = intel_de_read(dev_priv, MIPI_DSI_FUNC_PRG(port)) & VID_MODE_FORMAT_MASK;
	bpp = mipi_dsi_pixel_format_to_bpp(
			pixel_format_from_register_bits(fmt));

	pipe_config->pipe_bpp = bdw_get_pipe_misc_bpp(crtc);

	 
	pipe_config->mode_flags |=
		I915_MODE_FLAG_GET_SCANLINE_FROM_TIMESTAMP;

	 
	adjusted_mode->crtc_hdisplay =
				intel_de_read(dev_priv,
				              BXT_MIPI_TRANS_HACTIVE(port));
	adjusted_mode->crtc_vdisplay =
				intel_de_read(dev_priv,
				              BXT_MIPI_TRANS_VACTIVE(port));
	adjusted_mode->crtc_vtotal =
				intel_de_read(dev_priv,
				              BXT_MIPI_TRANS_VTOTAL(port));

	hactive = adjusted_mode->crtc_hdisplay;
	hfp = intel_de_read(dev_priv, MIPI_HFP_COUNT(port));

	 
	hsync = intel_de_read(dev_priv, MIPI_HSYNC_PADDING_COUNT(port));
	hbp = intel_de_read(dev_priv, MIPI_HBP_COUNT(port));

	 
	hfp = pixels_from_txbyteclkhs(hfp, bpp, lane_count,
						intel_dsi->burst_mode_ratio);
	hsync = pixels_from_txbyteclkhs(hsync, bpp, lane_count,
						intel_dsi->burst_mode_ratio);
	hbp = pixels_from_txbyteclkhs(hbp, bpp, lane_count,
						intel_dsi->burst_mode_ratio);

	if (intel_dsi->dual_link) {
		hfp *= 2;
		hsync *= 2;
		hbp *= 2;
	}

	 
	vfp = intel_de_read(dev_priv, MIPI_VFP_COUNT(port));
	vsync = intel_de_read(dev_priv, MIPI_VSYNC_PADDING_COUNT(port));

	adjusted_mode->crtc_htotal = hactive + hfp + hsync + hbp;
	adjusted_mode->crtc_hsync_start = hfp + adjusted_mode->crtc_hdisplay;
	adjusted_mode->crtc_hsync_end = hsync + adjusted_mode->crtc_hsync_start;
	adjusted_mode->crtc_hblank_start = adjusted_mode->crtc_hdisplay;
	adjusted_mode->crtc_hblank_end = adjusted_mode->crtc_htotal;

	adjusted_mode->crtc_vsync_start = vfp + adjusted_mode->crtc_vdisplay;
	adjusted_mode->crtc_vsync_end = vsync + adjusted_mode->crtc_vsync_start;
	adjusted_mode->crtc_vblank_start = adjusted_mode->crtc_vdisplay;
	adjusted_mode->crtc_vblank_end = adjusted_mode->crtc_vtotal;

	 
	 
	hfp_sw = adjusted_mode_sw->crtc_hsync_start -
					adjusted_mode_sw->crtc_hdisplay;
	hsync_sw = adjusted_mode_sw->crtc_hsync_end -
					adjusted_mode_sw->crtc_hsync_start;
	hbp_sw = adjusted_mode_sw->crtc_htotal -
					adjusted_mode_sw->crtc_hsync_end;

	if (intel_dsi->dual_link) {
		hfp_sw /= 2;
		hsync_sw /= 2;
		hbp_sw /= 2;
	}

	hfp_sw = txbyteclkhs(hfp_sw, bpp, lane_count,
						intel_dsi->burst_mode_ratio);
	hsync_sw = txbyteclkhs(hsync_sw, bpp, lane_count,
			    intel_dsi->burst_mode_ratio);
	hbp_sw = txbyteclkhs(hbp_sw, bpp, lane_count,
						intel_dsi->burst_mode_ratio);

	 
	hfp_sw = pixels_from_txbyteclkhs(hfp_sw, bpp, lane_count,
						intel_dsi->burst_mode_ratio);
	hsync_sw = pixels_from_txbyteclkhs(hsync_sw, bpp, lane_count,
						intel_dsi->burst_mode_ratio);
	hbp_sw = pixels_from_txbyteclkhs(hbp_sw, bpp, lane_count,
						intel_dsi->burst_mode_ratio);

	if (intel_dsi->dual_link) {
		hfp_sw *= 2;
		hsync_sw *= 2;
		hbp_sw *= 2;
	}

	crtc_htotal_sw = adjusted_mode_sw->crtc_hdisplay + hfp_sw +
							hsync_sw + hbp_sw;
	crtc_hsync_start_sw = hfp_sw + adjusted_mode_sw->crtc_hdisplay;
	crtc_hsync_end_sw = hsync_sw + crtc_hsync_start_sw;
	crtc_hblank_start_sw = adjusted_mode_sw->crtc_hdisplay;
	crtc_hblank_end_sw = crtc_htotal_sw;

	if (adjusted_mode->crtc_htotal == crtc_htotal_sw)
		adjusted_mode->crtc_htotal = adjusted_mode_sw->crtc_htotal;

	if (adjusted_mode->crtc_hsync_start == crtc_hsync_start_sw)
		adjusted_mode->crtc_hsync_start =
					adjusted_mode_sw->crtc_hsync_start;

	if (adjusted_mode->crtc_hsync_end == crtc_hsync_end_sw)
		adjusted_mode->crtc_hsync_end =
					adjusted_mode_sw->crtc_hsync_end;

	if (adjusted_mode->crtc_hblank_start == crtc_hblank_start_sw)
		adjusted_mode->crtc_hblank_start =
					adjusted_mode_sw->crtc_hblank_start;

	if (adjusted_mode->crtc_hblank_end == crtc_hblank_end_sw)
		adjusted_mode->crtc_hblank_end =
					adjusted_mode_sw->crtc_hblank_end;
}

static void intel_dsi_get_config(struct intel_encoder *encoder,
				 struct intel_crtc_state *pipe_config)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(encoder);
	u32 pclk;

	drm_dbg_kms(&dev_priv->drm, "\n");

	pipe_config->output_types |= BIT(INTEL_OUTPUT_DSI);

	if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv)) {
		bxt_dsi_get_pipe_config(encoder, pipe_config);
		pclk = bxt_dsi_get_pclk(encoder, pipe_config);
	} else {
		pclk = vlv_dsi_get_pclk(encoder, pipe_config);
	}

	pipe_config->port_clock = pclk;

	 
	pipe_config->hw.adjusted_mode.crtc_clock = pclk;
	if (intel_dsi->dual_link)
		pipe_config->hw.adjusted_mode.crtc_clock *= 2;
}

 
static u16 txclkesc(u32 divider, unsigned int us)
{
	switch (divider) {
	case ESCAPE_CLOCK_DIVIDER_1:
	default:
		return 20 * us;
	case ESCAPE_CLOCK_DIVIDER_2:
		return 10 * us;
	case ESCAPE_CLOCK_DIVIDER_4:
		return 5 * us;
	}
}

static void set_dsi_timings(struct drm_encoder *encoder,
			    const struct drm_display_mode *adjusted_mode)
{
	struct drm_device *dev = encoder->dev;
	struct drm_i915_private *dev_priv = to_i915(dev);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(to_intel_encoder(encoder));
	enum port port;
	unsigned int bpp = mipi_dsi_pixel_format_to_bpp(intel_dsi->pixel_format);
	unsigned int lane_count = intel_dsi->lane_count;

	u16 hactive, hfp, hsync, hbp, vfp, vsync, vbp;

	hactive = adjusted_mode->crtc_hdisplay;
	hfp = adjusted_mode->crtc_hsync_start - adjusted_mode->crtc_hdisplay;
	hsync = adjusted_mode->crtc_hsync_end - adjusted_mode->crtc_hsync_start;
	hbp = adjusted_mode->crtc_htotal - adjusted_mode->crtc_hsync_end;

	if (intel_dsi->dual_link) {
		hactive /= 2;
		if (intel_dsi->dual_link == DSI_DUAL_LINK_FRONT_BACK)
			hactive += intel_dsi->pixel_overlap;
		hfp /= 2;
		hsync /= 2;
		hbp /= 2;
	}

	vfp = adjusted_mode->crtc_vsync_start - adjusted_mode->crtc_vdisplay;
	vsync = adjusted_mode->crtc_vsync_end - adjusted_mode->crtc_vsync_start;
	vbp = adjusted_mode->crtc_vtotal - adjusted_mode->crtc_vsync_end;

	 
	hactive = txbyteclkhs(hactive, bpp, lane_count,
			      intel_dsi->burst_mode_ratio);
	hfp = txbyteclkhs(hfp, bpp, lane_count, intel_dsi->burst_mode_ratio);
	hsync = txbyteclkhs(hsync, bpp, lane_count,
			    intel_dsi->burst_mode_ratio);
	hbp = txbyteclkhs(hbp, bpp, lane_count, intel_dsi->burst_mode_ratio);

	for_each_dsi_port(port, intel_dsi->ports) {
		if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv)) {
			 
			intel_de_write(dev_priv, BXT_MIPI_TRANS_HACTIVE(port),
				       adjusted_mode->crtc_hdisplay);
			intel_de_write(dev_priv, BXT_MIPI_TRANS_VACTIVE(port),
				       adjusted_mode->crtc_vdisplay);
			intel_de_write(dev_priv, BXT_MIPI_TRANS_VTOTAL(port),
				       adjusted_mode->crtc_vtotal);
		}

		intel_de_write(dev_priv, MIPI_HACTIVE_AREA_COUNT(port),
			       hactive);
		intel_de_write(dev_priv, MIPI_HFP_COUNT(port), hfp);

		 
		intel_de_write(dev_priv, MIPI_HSYNC_PADDING_COUNT(port),
			       hsync);
		intel_de_write(dev_priv, MIPI_HBP_COUNT(port), hbp);

		 
		intel_de_write(dev_priv, MIPI_VFP_COUNT(port), vfp);
		intel_de_write(dev_priv, MIPI_VSYNC_PADDING_COUNT(port),
			       vsync);
		intel_de_write(dev_priv, MIPI_VBP_COUNT(port), vbp);
	}
}

static u32 pixel_format_to_reg(enum mipi_dsi_pixel_format fmt)
{
	switch (fmt) {
	case MIPI_DSI_FMT_RGB888:
		return VID_MODE_FORMAT_RGB888;
	case MIPI_DSI_FMT_RGB666:
		return VID_MODE_FORMAT_RGB666;
	case MIPI_DSI_FMT_RGB666_PACKED:
		return VID_MODE_FORMAT_RGB666_PACKED;
	case MIPI_DSI_FMT_RGB565:
		return VID_MODE_FORMAT_RGB565;
	default:
		MISSING_CASE(fmt);
		return VID_MODE_FORMAT_RGB666;
	}
}

static void intel_dsi_prepare(struct intel_encoder *intel_encoder,
			      const struct intel_crtc_state *pipe_config)
{
	struct drm_encoder *encoder = &intel_encoder->base;
	struct drm_device *dev = encoder->dev;
	struct drm_i915_private *dev_priv = to_i915(dev);
	struct intel_crtc *crtc = to_intel_crtc(pipe_config->uapi.crtc);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(to_intel_encoder(encoder));
	const struct drm_display_mode *adjusted_mode = &pipe_config->hw.adjusted_mode;
	enum port port;
	unsigned int bpp = mipi_dsi_pixel_format_to_bpp(intel_dsi->pixel_format);
	u32 val, tmp;
	u16 mode_hdisplay;

	drm_dbg_kms(&dev_priv->drm, "pipe %c\n", pipe_name(crtc->pipe));

	mode_hdisplay = adjusted_mode->crtc_hdisplay;

	if (intel_dsi->dual_link) {
		mode_hdisplay /= 2;
		if (intel_dsi->dual_link == DSI_DUAL_LINK_FRONT_BACK)
			mode_hdisplay += intel_dsi->pixel_overlap;
	}

	for_each_dsi_port(port, intel_dsi->ports) {
		if (IS_VALLEYVIEW(dev_priv) || IS_CHERRYVIEW(dev_priv)) {
			 
			tmp = intel_de_read(dev_priv, MIPI_CTRL(PORT_A));
			tmp &= ~ESCAPE_CLOCK_DIVIDER_MASK;
			intel_de_write(dev_priv, MIPI_CTRL(PORT_A),
				       tmp | ESCAPE_CLOCK_DIVIDER_1);

			 
			tmp = intel_de_read(dev_priv, MIPI_CTRL(port));
			tmp &= ~READ_REQUEST_PRIORITY_MASK;
			intel_de_write(dev_priv, MIPI_CTRL(port),
				       tmp | READ_REQUEST_PRIORITY_HIGH);
		} else if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv)) {
			enum pipe pipe = crtc->pipe;

			intel_de_rmw(dev_priv, MIPI_CTRL(port),
				     BXT_PIPE_SELECT_MASK, BXT_PIPE_SELECT(pipe));
		}

		 
		intel_de_write(dev_priv, MIPI_INTR_STAT(port), 0xffffffff);
		intel_de_write(dev_priv, MIPI_INTR_EN(port), 0xffffffff);

		intel_de_write(dev_priv, MIPI_DPHY_PARAM(port),
			       intel_dsi->dphy_reg);

		intel_de_write(dev_priv, MIPI_DPI_RESOLUTION(port),
			       adjusted_mode->crtc_vdisplay << VERTICAL_ADDRESS_SHIFT | mode_hdisplay << HORIZONTAL_ADDRESS_SHIFT);
	}

	set_dsi_timings(encoder, adjusted_mode);

	val = intel_dsi->lane_count << DATA_LANES_PRG_REG_SHIFT;
	if (is_cmd_mode(intel_dsi)) {
		val |= intel_dsi->channel << CMD_MODE_CHANNEL_NUMBER_SHIFT;
		val |= CMD_MODE_DATA_WIDTH_8_BIT;  
	} else {
		val |= intel_dsi->channel << VID_MODE_CHANNEL_NUMBER_SHIFT;
		val |= pixel_format_to_reg(intel_dsi->pixel_format);
	}

	tmp = 0;
	if (intel_dsi->eotp_pkt == 0)
		tmp |= EOT_DISABLE;
	if (intel_dsi->clock_stop)
		tmp |= CLOCKSTOP;

	if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv)) {
		tmp |= BXT_DPHY_DEFEATURE_EN;
		if (!is_cmd_mode(intel_dsi))
			tmp |= BXT_DEFEATURE_DPI_FIFO_CTR;
	}

	for_each_dsi_port(port, intel_dsi->ports) {
		intel_de_write(dev_priv, MIPI_DSI_FUNC_PRG(port), val);

		 

		 

		if (is_vid_mode(intel_dsi) &&
			intel_dsi->video_mode == BURST_MODE) {
			intel_de_write(dev_priv, MIPI_HS_TX_TIMEOUT(port),
				       txbyteclkhs(adjusted_mode->crtc_htotal, bpp, intel_dsi->lane_count, intel_dsi->burst_mode_ratio) + 1);
		} else {
			intel_de_write(dev_priv, MIPI_HS_TX_TIMEOUT(port),
				       txbyteclkhs(adjusted_mode->crtc_vtotal * adjusted_mode->crtc_htotal, bpp, intel_dsi->lane_count, intel_dsi->burst_mode_ratio) + 1);
		}
		intel_de_write(dev_priv, MIPI_LP_RX_TIMEOUT(port),
			       intel_dsi->lp_rx_timeout);
		intel_de_write(dev_priv, MIPI_TURN_AROUND_TIMEOUT(port),
			       intel_dsi->turn_arnd_val);
		intel_de_write(dev_priv, MIPI_DEVICE_RESET_TIMER(port),
			       intel_dsi->rst_timer_val);

		 

		 
		intel_de_write(dev_priv, MIPI_INIT_COUNT(port),
			       txclkesc(intel_dsi->escape_clk_div, 100));

		if ((IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv)) &&
		    !intel_dsi->dual_link) {
			 
			intel_de_write(dev_priv,
				       MIPI_INIT_COUNT(port == PORT_A ? PORT_C : PORT_A),
				       intel_dsi->init_count);
		}

		 
		intel_de_write(dev_priv, MIPI_EOT_DISABLE(port), tmp);

		 
		intel_de_write(dev_priv, MIPI_INIT_COUNT(port),
			       intel_dsi->init_count);

		 
		intel_de_write(dev_priv, MIPI_HIGH_LOW_SWITCH_COUNT(port),
			       intel_dsi->hs_to_lp_count);

		 
		intel_de_write(dev_priv, MIPI_LP_BYTECLK(port),
			       intel_dsi->lp_byte_clk);

		if (IS_GEMINILAKE(dev_priv)) {
			intel_de_write(dev_priv, MIPI_TLPX_TIME_COUNT(port),
				       intel_dsi->lp_byte_clk);
			 
			intel_de_write(dev_priv, MIPI_CLK_LANE_TIMING(port),
				       intel_dsi->dphy_reg);
		}

		 
		intel_de_write(dev_priv, MIPI_DBI_BW_CTRL(port),
			       intel_dsi->bw_timer);

		intel_de_write(dev_priv, MIPI_CLK_LANE_SWITCH_TIME_CNT(port),
			       intel_dsi->clk_lp_to_hs_count << LP_HS_SSW_CNT_SHIFT | intel_dsi->clk_hs_to_lp_count << HS_LP_PWR_SW_CNT_SHIFT);

		if (is_vid_mode(intel_dsi)) {
			u32 fmt = intel_dsi->video_frmt_cfg_bits | IP_TG_CONFIG;

			 
			fmt |= RANDOM_DPI_DISPLAY_RESOLUTION;

			switch (intel_dsi->video_mode) {
			default:
				MISSING_CASE(intel_dsi->video_mode);
				fallthrough;
			case NON_BURST_SYNC_EVENTS:
				fmt |= VIDEO_MODE_NON_BURST_WITH_SYNC_EVENTS;
				break;
			case NON_BURST_SYNC_PULSE:
				fmt |= VIDEO_MODE_NON_BURST_WITH_SYNC_PULSE;
				break;
			case BURST_MODE:
				fmt |= VIDEO_MODE_BURST;
				break;
			}

			intel_de_write(dev_priv, MIPI_VIDEO_MODE_FORMAT(port), fmt);
		}
	}
}

static void intel_dsi_unprepare(struct intel_encoder *encoder)
{
	struct drm_i915_private *dev_priv = to_i915(encoder->base.dev);
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(encoder);
	enum port port;

	if (IS_GEMINILAKE(dev_priv))
		return;

	for_each_dsi_port(port, intel_dsi->ports) {
		 
		intel_de_write(dev_priv, MIPI_DEVICE_READY(port), 0x0);

		if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv))
			bxt_dsi_reset_clocks(encoder, port);
		else
			vlv_dsi_reset_clocks(encoder, port);
		intel_de_write(dev_priv, MIPI_EOT_DISABLE(port), CLOCKSTOP);

		intel_de_rmw(dev_priv, MIPI_DSI_FUNC_PRG(port), VID_MODE_FORMAT_MASK, 0);

		intel_de_write(dev_priv, MIPI_DEVICE_READY(port), 0x1);
	}
}

static void intel_dsi_encoder_destroy(struct drm_encoder *encoder)
{
	struct intel_dsi *intel_dsi = enc_to_intel_dsi(to_intel_encoder(encoder));

	intel_dsi_vbt_gpio_cleanup(intel_dsi);
	intel_encoder_destroy(encoder);
}

static const struct drm_encoder_funcs intel_dsi_funcs = {
	.destroy = intel_dsi_encoder_destroy,
};

static enum drm_mode_status vlv_dsi_mode_valid(struct drm_connector *connector,
					       struct drm_display_mode *mode)
{
	struct drm_i915_private *i915 = to_i915(connector->dev);

	if (IS_VALLEYVIEW(i915) || IS_CHERRYVIEW(i915)) {
		enum drm_mode_status status;

		status = intel_cpu_transcoder_mode_valid(i915, mode);
		if (status != MODE_OK)
			return status;
	}

	return intel_dsi_mode_valid(connector, mode);
}

static const struct drm_connector_helper_funcs intel_dsi_connector_helper_funcs = {
	.get_modes = intel_dsi_get_modes,
	.mode_valid = vlv_dsi_mode_valid,
	.atomic_check = intel_digital_connector_atomic_check,
};

static const struct drm_connector_funcs intel_dsi_connector_funcs = {
	.detect = intel_panel_detect,
	.late_register = intel_connector_register,
	.early_unregister = intel_connector_unregister,
	.destroy = intel_connector_destroy,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.atomic_get_property = intel_digital_connector_atomic_get_property,
	.atomic_set_property = intel_digital_connector_atomic_set_property,
	.atomic_destroy_state = drm_atomic_helper_connector_destroy_state,
	.atomic_duplicate_state = intel_digital_connector_duplicate_state,
};

static void vlv_dsi_add_properties(struct intel_connector *connector)
{
	const struct drm_display_mode *fixed_mode =
		intel_panel_preferred_fixed_mode(connector);

	intel_attach_scaling_mode_property(&connector->base);

	drm_connector_set_panel_orientation_with_quirk(&connector->base,
						       intel_dsi_get_panel_orientation(connector),
						       fixed_mode->hdisplay,
						       fixed_mode->vdisplay);
}

#define NS_KHZ_RATIO		1000000

#define PREPARE_CNT_MAX		0x3F
#define EXIT_ZERO_CNT_MAX	0x3F
#define CLK_ZERO_CNT_MAX	0xFF
#define TRAIL_CNT_MAX		0x1F

static void vlv_dphy_param_init(struct intel_dsi *intel_dsi)
{
	struct drm_device *dev = intel_dsi->base.base.dev;
	struct drm_i915_private *dev_priv = to_i915(dev);
	struct intel_connector *connector = intel_dsi->attached_connector;
	struct mipi_config *mipi_config = connector->panel.vbt.dsi.config;
	u32 tlpx_ns, extra_byte_count, tlpx_ui;
	u32 ui_num, ui_den;
	u32 prepare_cnt, exit_zero_cnt, clk_zero_cnt, trail_cnt;
	u32 ths_prepare_ns, tclk_trail_ns;
	u32 tclk_prepare_clkzero, ths_prepare_hszero;
	u32 lp_to_hs_switch, hs_to_lp_switch;
	u32 mul;

	tlpx_ns = intel_dsi_tlpx_ns(intel_dsi);

	switch (intel_dsi->lane_count) {
	case 1:
	case 2:
		extra_byte_count = 2;
		break;
	case 3:
		extra_byte_count = 4;
		break;
	case 4:
	default:
		extra_byte_count = 3;
		break;
	}

	 
	ui_num = NS_KHZ_RATIO;
	ui_den = intel_dsi_bitrate(intel_dsi);

	tclk_prepare_clkzero = mipi_config->tclk_prepare_clkzero;
	ths_prepare_hszero = mipi_config->ths_prepare_hszero;

	 
	intel_dsi->lp_byte_clk = DIV_ROUND_UP(tlpx_ns * ui_den, 8 * ui_num);

	 
	mul = IS_GEMINILAKE(dev_priv) ? 8 : 2;
	ths_prepare_ns = max(mipi_config->ths_prepare,
			     mipi_config->tclk_prepare);

	 
	prepare_cnt = DIV_ROUND_UP(ths_prepare_ns * ui_den, ui_num * mul);

	if (prepare_cnt > PREPARE_CNT_MAX) {
		drm_dbg_kms(&dev_priv->drm, "prepare count too high %u\n",
			    prepare_cnt);
		prepare_cnt = PREPARE_CNT_MAX;
	}

	 
	exit_zero_cnt = DIV_ROUND_UP(
				(ths_prepare_hszero - ths_prepare_ns) * ui_den,
				ui_num * mul
				);

	 
	if (exit_zero_cnt < (55 * ui_den / ui_num) && (55 * ui_den) % ui_num)
		exit_zero_cnt += 1;

	if (exit_zero_cnt > EXIT_ZERO_CNT_MAX) {
		drm_dbg_kms(&dev_priv->drm, "exit zero count too high %u\n",
			    exit_zero_cnt);
		exit_zero_cnt = EXIT_ZERO_CNT_MAX;
	}

	 
	clk_zero_cnt = DIV_ROUND_UP(
				(tclk_prepare_clkzero -	ths_prepare_ns)
				* ui_den, ui_num * mul);

	if (clk_zero_cnt > CLK_ZERO_CNT_MAX) {
		drm_dbg_kms(&dev_priv->drm, "clock zero count too high %u\n",
			    clk_zero_cnt);
		clk_zero_cnt = CLK_ZERO_CNT_MAX;
	}

	 
	tclk_trail_ns = max(mipi_config->tclk_trail, mipi_config->ths_trail);
	trail_cnt = DIV_ROUND_UP(tclk_trail_ns * ui_den, ui_num * mul);

	if (trail_cnt > TRAIL_CNT_MAX) {
		drm_dbg_kms(&dev_priv->drm, "trail count too high %u\n",
			    trail_cnt);
		trail_cnt = TRAIL_CNT_MAX;
	}

	 
	intel_dsi->dphy_reg = exit_zero_cnt << 24 | trail_cnt << 16 |
						clk_zero_cnt << 8 | prepare_cnt;

	 
	tlpx_ui = DIV_ROUND_UP(tlpx_ns * ui_den, ui_num);

	 
	 
	lp_to_hs_switch = DIV_ROUND_UP(4 * tlpx_ui + prepare_cnt * mul +
						exit_zero_cnt * mul + 10, 8);

	hs_to_lp_switch = DIV_ROUND_UP(mipi_config->ths_trail + 2 * tlpx_ui, 8);

	intel_dsi->hs_to_lp_count = max(lp_to_hs_switch, hs_to_lp_switch);
	intel_dsi->hs_to_lp_count += extra_byte_count;

	 
	 
	intel_dsi->clk_lp_to_hs_count =
		DIV_ROUND_UP(
			4 * tlpx_ui + prepare_cnt * 2 +
			clk_zero_cnt * 2,
			8);

	intel_dsi->clk_lp_to_hs_count += extra_byte_count;

	 
	intel_dsi->clk_hs_to_lp_count =
		DIV_ROUND_UP(2 * tlpx_ui + trail_cnt * 2 + 8,
			8);
	intel_dsi->clk_hs_to_lp_count += extra_byte_count;

	intel_dsi_log_params(intel_dsi);
}

void vlv_dsi_init(struct drm_i915_private *dev_priv)
{
	struct intel_dsi *intel_dsi;
	struct intel_encoder *intel_encoder;
	struct drm_encoder *encoder;
	struct intel_connector *intel_connector;
	struct drm_connector *connector;
	struct drm_display_mode *current_mode;
	enum port port;
	enum pipe pipe;

	drm_dbg_kms(&dev_priv->drm, "\n");

	 
	if (!intel_bios_is_dsi_present(dev_priv, &port))
		return;

	if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv))
		dev_priv->display.dsi.mmio_base = BXT_MIPI_BASE;
	else
		dev_priv->display.dsi.mmio_base = VLV_MIPI_BASE;

	intel_dsi = kzalloc(sizeof(*intel_dsi), GFP_KERNEL);
	if (!intel_dsi)
		return;

	intel_connector = intel_connector_alloc();
	if (!intel_connector) {
		kfree(intel_dsi);
		return;
	}

	intel_encoder = &intel_dsi->base;
	encoder = &intel_encoder->base;
	intel_dsi->attached_connector = intel_connector;

	connector = &intel_connector->base;

	drm_encoder_init(&dev_priv->drm, encoder, &intel_dsi_funcs, DRM_MODE_ENCODER_DSI,
			 "DSI %c", port_name(port));

	intel_encoder->compute_config = intel_dsi_compute_config;
	intel_encoder->pre_enable = intel_dsi_pre_enable;
	if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv))
		intel_encoder->enable = bxt_dsi_enable;
	intel_encoder->disable = intel_dsi_disable;
	intel_encoder->post_disable = intel_dsi_post_disable;
	intel_encoder->get_hw_state = intel_dsi_get_hw_state;
	intel_encoder->get_config = intel_dsi_get_config;
	intel_encoder->update_pipe = intel_backlight_update;
	intel_encoder->shutdown = intel_dsi_shutdown;

	intel_connector->get_hw_state = intel_connector_get_hw_state;

	intel_encoder->port = port;
	intel_encoder->type = INTEL_OUTPUT_DSI;
	intel_encoder->power_domain = POWER_DOMAIN_PORT_DSI;
	intel_encoder->cloneable = 0;

	 
	if (IS_GEMINILAKE(dev_priv) || IS_BROXTON(dev_priv))
		intel_encoder->pipe_mask = ~0;
	else if (port == PORT_A)
		intel_encoder->pipe_mask = BIT(PIPE_A);
	else
		intel_encoder->pipe_mask = BIT(PIPE_B);

	intel_dsi->panel_power_off_time = ktime_get_boottime();

	intel_bios_init_panel_late(dev_priv, &intel_connector->panel, NULL, NULL);

	if (intel_connector->panel.vbt.dsi.config->dual_link)
		intel_dsi->ports = BIT(PORT_A) | BIT(PORT_C);
	else
		intel_dsi->ports = BIT(port);

	if (drm_WARN_ON(&dev_priv->drm, intel_connector->panel.vbt.dsi.bl_ports & ~intel_dsi->ports))
		intel_connector->panel.vbt.dsi.bl_ports &= intel_dsi->ports;

	if (drm_WARN_ON(&dev_priv->drm, intel_connector->panel.vbt.dsi.cabc_ports & ~intel_dsi->ports))
		intel_connector->panel.vbt.dsi.cabc_ports &= intel_dsi->ports;

	 
	for_each_dsi_port(port, intel_dsi->ports) {
		struct intel_dsi_host *host;

		host = intel_dsi_host_init(intel_dsi, &intel_dsi_host_ops,
					   port);
		if (!host)
			goto err;

		intel_dsi->dsi_hosts[port] = host;
	}

	if (!intel_dsi_vbt_init(intel_dsi, MIPI_DSI_GENERIC_PANEL_ID)) {
		drm_dbg_kms(&dev_priv->drm, "no device found\n");
		goto err;
	}

	 
	current_mode = intel_encoder_current_mode(intel_encoder);
	if (current_mode) {
		drm_dbg_kms(&dev_priv->drm, "Calculated pclk %d GOP %d\n",
			    intel_dsi->pclk, current_mode->clock);
		if (intel_fuzzy_clock_check(intel_dsi->pclk,
					    current_mode->clock)) {
			drm_dbg_kms(&dev_priv->drm, "Using GOP pclk\n");
			intel_dsi->pclk = current_mode->clock;
		}

		kfree(current_mode);
	}

	vlv_dphy_param_init(intel_dsi);

	intel_dsi_vbt_gpio_init(intel_dsi,
				intel_dsi_get_hw_state(intel_encoder, &pipe));

	drm_connector_init(&dev_priv->drm, connector, &intel_dsi_connector_funcs,
			   DRM_MODE_CONNECTOR_DSI);

	drm_connector_helper_add(connector, &intel_dsi_connector_helper_funcs);

	connector->display_info.subpixel_order = SubPixelHorizontalRGB;  

	intel_connector_attach_encoder(intel_connector, intel_encoder);

	mutex_lock(&dev_priv->drm.mode_config.mutex);
	intel_panel_add_vbt_lfp_fixed_mode(intel_connector);
	mutex_unlock(&dev_priv->drm.mode_config.mutex);

	if (!intel_panel_preferred_fixed_mode(intel_connector)) {
		drm_dbg_kms(&dev_priv->drm, "no fixed mode\n");
		goto err_cleanup_connector;
	}

	intel_panel_init(intel_connector, NULL);

	intel_backlight_setup(intel_connector, INVALID_PIPE);

	vlv_dsi_add_properties(intel_connector);

	return;

err_cleanup_connector:
	drm_connector_cleanup(&intel_connector->base);
err:
	drm_encoder_cleanup(&intel_encoder->base);
	kfree(intel_dsi);
	kfree(intel_connector);
}
