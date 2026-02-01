 

#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/string_helpers.h>
#include <linux/dynamic_debug.h>

#include <drm/display/drm_dp_helper.h>
#include <drm/display/drm_dp_mst_helper.h>
#include <drm/drm_edid.h>
#include <drm/drm_print.h>
#include <drm/drm_vblank.h>
#include <drm/drm_panel.h>

#include "drm_dp_helper_internal.h"

DECLARE_DYNDBG_CLASSMAP(drm_debug_classes, DD_CLASS_TYPE_DISJOINT_BITS, 0,
			"DRM_UT_CORE",
			"DRM_UT_DRIVER",
			"DRM_UT_KMS",
			"DRM_UT_PRIME",
			"DRM_UT_ATOMIC",
			"DRM_UT_VBL",
			"DRM_UT_STATE",
			"DRM_UT_LEASE",
			"DRM_UT_DP",
			"DRM_UT_DRMRES");

struct dp_aux_backlight {
	struct backlight_device *base;
	struct drm_dp_aux *aux;
	struct drm_edp_backlight_info info;
	bool enabled;
};

 

 
static u8 dp_link_status(const u8 link_status[DP_LINK_STATUS_SIZE], int r)
{
	return link_status[r - DP_LANE0_1_STATUS];
}

static u8 dp_get_lane_status(const u8 link_status[DP_LINK_STATUS_SIZE],
			     int lane)
{
	int i = DP_LANE0_1_STATUS + (lane >> 1);
	int s = (lane & 1) * 4;
	u8 l = dp_link_status(link_status, i);

	return (l >> s) & 0xf;
}

bool drm_dp_channel_eq_ok(const u8 link_status[DP_LINK_STATUS_SIZE],
			  int lane_count)
{
	u8 lane_align;
	u8 lane_status;
	int lane;

	lane_align = dp_link_status(link_status,
				    DP_LANE_ALIGN_STATUS_UPDATED);
	if ((lane_align & DP_INTERLANE_ALIGN_DONE) == 0)
		return false;
	for (lane = 0; lane < lane_count; lane++) {
		lane_status = dp_get_lane_status(link_status, lane);
		if ((lane_status & DP_CHANNEL_EQ_BITS) != DP_CHANNEL_EQ_BITS)
			return false;
	}
	return true;
}
EXPORT_SYMBOL(drm_dp_channel_eq_ok);

bool drm_dp_clock_recovery_ok(const u8 link_status[DP_LINK_STATUS_SIZE],
			      int lane_count)
{
	int lane;
	u8 lane_status;

	for (lane = 0; lane < lane_count; lane++) {
		lane_status = dp_get_lane_status(link_status, lane);
		if ((lane_status & DP_LANE_CR_DONE) == 0)
			return false;
	}
	return true;
}
EXPORT_SYMBOL(drm_dp_clock_recovery_ok);

u8 drm_dp_get_adjust_request_voltage(const u8 link_status[DP_LINK_STATUS_SIZE],
				     int lane)
{
	int i = DP_ADJUST_REQUEST_LANE0_1 + (lane >> 1);
	int s = ((lane & 1) ?
		 DP_ADJUST_VOLTAGE_SWING_LANE1_SHIFT :
		 DP_ADJUST_VOLTAGE_SWING_LANE0_SHIFT);
	u8 l = dp_link_status(link_status, i);

	return ((l >> s) & 0x3) << DP_TRAIN_VOLTAGE_SWING_SHIFT;
}
EXPORT_SYMBOL(drm_dp_get_adjust_request_voltage);

u8 drm_dp_get_adjust_request_pre_emphasis(const u8 link_status[DP_LINK_STATUS_SIZE],
					  int lane)
{
	int i = DP_ADJUST_REQUEST_LANE0_1 + (lane >> 1);
	int s = ((lane & 1) ?
		 DP_ADJUST_PRE_EMPHASIS_LANE1_SHIFT :
		 DP_ADJUST_PRE_EMPHASIS_LANE0_SHIFT);
	u8 l = dp_link_status(link_status, i);

	return ((l >> s) & 0x3) << DP_TRAIN_PRE_EMPHASIS_SHIFT;
}
EXPORT_SYMBOL(drm_dp_get_adjust_request_pre_emphasis);

 
u8 drm_dp_get_adjust_tx_ffe_preset(const u8 link_status[DP_LINK_STATUS_SIZE],
				   int lane)
{
	int i = DP_ADJUST_REQUEST_LANE0_1 + (lane >> 1);
	int s = ((lane & 1) ?
		 DP_ADJUST_TX_FFE_PRESET_LANE1_SHIFT :
		 DP_ADJUST_TX_FFE_PRESET_LANE0_SHIFT);
	u8 l = dp_link_status(link_status, i);

	return (l >> s) & 0xf;
}
EXPORT_SYMBOL(drm_dp_get_adjust_tx_ffe_preset);

 
bool drm_dp_128b132b_lane_channel_eq_done(const u8 link_status[DP_LINK_STATUS_SIZE],
					  int lane_count)
{
	u8 lane_align, lane_status;
	int lane;

	lane_align = dp_link_status(link_status, DP_LANE_ALIGN_STATUS_UPDATED);
	if (!(lane_align & DP_INTERLANE_ALIGN_DONE))
		return false;

	for (lane = 0; lane < lane_count; lane++) {
		lane_status = dp_get_lane_status(link_status, lane);
		if (!(lane_status & DP_LANE_CHANNEL_EQ_DONE))
			return false;
	}
	return true;
}
EXPORT_SYMBOL(drm_dp_128b132b_lane_channel_eq_done);

 
bool drm_dp_128b132b_lane_symbol_locked(const u8 link_status[DP_LINK_STATUS_SIZE],
					int lane_count)
{
	u8 lane_status;
	int lane;

	for (lane = 0; lane < lane_count; lane++) {
		lane_status = dp_get_lane_status(link_status, lane);
		if (!(lane_status & DP_LANE_SYMBOL_LOCKED))
			return false;
	}
	return true;
}
EXPORT_SYMBOL(drm_dp_128b132b_lane_symbol_locked);

 
bool drm_dp_128b132b_eq_interlane_align_done(const u8 link_status[DP_LINK_STATUS_SIZE])
{
	u8 status = dp_link_status(link_status, DP_LANE_ALIGN_STATUS_UPDATED);

	return status & DP_128B132B_DPRX_EQ_INTERLANE_ALIGN_DONE;
}
EXPORT_SYMBOL(drm_dp_128b132b_eq_interlane_align_done);

 
bool drm_dp_128b132b_cds_interlane_align_done(const u8 link_status[DP_LINK_STATUS_SIZE])
{
	u8 status = dp_link_status(link_status, DP_LANE_ALIGN_STATUS_UPDATED);

	return status & DP_128B132B_DPRX_CDS_INTERLANE_ALIGN_DONE;
}
EXPORT_SYMBOL(drm_dp_128b132b_cds_interlane_align_done);

 
bool drm_dp_128b132b_link_training_failed(const u8 link_status[DP_LINK_STATUS_SIZE])
{
	u8 status = dp_link_status(link_status, DP_LANE_ALIGN_STATUS_UPDATED);

	return status & DP_128B132B_LT_FAILED;
}
EXPORT_SYMBOL(drm_dp_128b132b_link_training_failed);

static int __8b10b_clock_recovery_delay_us(const struct drm_dp_aux *aux, u8 rd_interval)
{
	if (rd_interval > 4)
		drm_dbg_kms(aux->drm_dev, "%s: invalid AUX interval 0x%02x (max 4)\n",
			    aux->name, rd_interval);

	if (rd_interval == 0)
		return 100;

	return rd_interval * 4 * USEC_PER_MSEC;
}

static int __8b10b_channel_eq_delay_us(const struct drm_dp_aux *aux, u8 rd_interval)
{
	if (rd_interval > 4)
		drm_dbg_kms(aux->drm_dev, "%s: invalid AUX interval 0x%02x (max 4)\n",
			    aux->name, rd_interval);

	if (rd_interval == 0)
		return 400;

	return rd_interval * 4 * USEC_PER_MSEC;
}

static int __128b132b_channel_eq_delay_us(const struct drm_dp_aux *aux, u8 rd_interval)
{
	switch (rd_interval) {
	default:
		drm_dbg_kms(aux->drm_dev, "%s: invalid AUX interval 0x%02x\n",
			    aux->name, rd_interval);
		fallthrough;
	case DP_128B132B_TRAINING_AUX_RD_INTERVAL_400_US:
		return 400;
	case DP_128B132B_TRAINING_AUX_RD_INTERVAL_4_MS:
		return 4000;
	case DP_128B132B_TRAINING_AUX_RD_INTERVAL_8_MS:
		return 8000;
	case DP_128B132B_TRAINING_AUX_RD_INTERVAL_12_MS:
		return 12000;
	case DP_128B132B_TRAINING_AUX_RD_INTERVAL_16_MS:
		return 16000;
	case DP_128B132B_TRAINING_AUX_RD_INTERVAL_32_MS:
		return 32000;
	case DP_128B132B_TRAINING_AUX_RD_INTERVAL_64_MS:
		return 64000;
	}
}

 
static int __read_delay(struct drm_dp_aux *aux, const u8 dpcd[DP_RECEIVER_CAP_SIZE],
			enum drm_dp_phy dp_phy, bool uhbr, bool cr)
{
	int (*parse)(const struct drm_dp_aux *aux, u8 rd_interval);
	unsigned int offset;
	u8 rd_interval, mask;

	if (dp_phy == DP_PHY_DPRX) {
		if (uhbr) {
			if (cr)
				return 100;

			offset = DP_128B132B_TRAINING_AUX_RD_INTERVAL;
			mask = DP_128B132B_TRAINING_AUX_RD_INTERVAL_MASK;
			parse = __128b132b_channel_eq_delay_us;
		} else {
			if (cr && dpcd[DP_DPCD_REV] >= DP_DPCD_REV_14)
				return 100;

			offset = DP_TRAINING_AUX_RD_INTERVAL;
			mask = DP_TRAINING_AUX_RD_MASK;
			if (cr)
				parse = __8b10b_clock_recovery_delay_us;
			else
				parse = __8b10b_channel_eq_delay_us;
		}
	} else {
		if (uhbr) {
			offset = DP_128B132B_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER(dp_phy);
			mask = DP_128B132B_TRAINING_AUX_RD_INTERVAL_MASK;
			parse = __128b132b_channel_eq_delay_us;
		} else {
			if (cr)
				return 100;

			offset = DP_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER(dp_phy);
			mask = DP_TRAINING_AUX_RD_MASK;
			parse = __8b10b_channel_eq_delay_us;
		}
	}

	if (offset < DP_RECEIVER_CAP_SIZE) {
		rd_interval = dpcd[offset];
	} else {
		if (drm_dp_dpcd_readb(aux, offset, &rd_interval) != 1) {
			drm_dbg_kms(aux->drm_dev, "%s: failed rd interval read\n",
				    aux->name);
			 
			return 400;
		}
	}

	return parse(aux, rd_interval & mask);
}

int drm_dp_read_clock_recovery_delay(struct drm_dp_aux *aux, const u8 dpcd[DP_RECEIVER_CAP_SIZE],
				     enum drm_dp_phy dp_phy, bool uhbr)
{
	return __read_delay(aux, dpcd, dp_phy, uhbr, true);
}
EXPORT_SYMBOL(drm_dp_read_clock_recovery_delay);

int drm_dp_read_channel_eq_delay(struct drm_dp_aux *aux, const u8 dpcd[DP_RECEIVER_CAP_SIZE],
				 enum drm_dp_phy dp_phy, bool uhbr)
{
	return __read_delay(aux, dpcd, dp_phy, uhbr, false);
}
EXPORT_SYMBOL(drm_dp_read_channel_eq_delay);

 
int drm_dp_128b132b_read_aux_rd_interval(struct drm_dp_aux *aux)
{
	int unit;
	u8 val;

	if (drm_dp_dpcd_readb(aux, DP_128B132B_TRAINING_AUX_RD_INTERVAL, &val) != 1) {
		drm_err(aux->drm_dev, "%s: failed rd interval read\n",
			aux->name);
		 
		val = DP_128B132B_TRAINING_AUX_RD_INTERVAL_MASK;
	}

	unit = (val & DP_128B132B_TRAINING_AUX_RD_INTERVAL_1MS_UNIT) ? 1 : 2;
	val &= DP_128B132B_TRAINING_AUX_RD_INTERVAL_MASK;

	return (val + 1) * unit * 1000;
}
EXPORT_SYMBOL(drm_dp_128b132b_read_aux_rd_interval);

void drm_dp_link_train_clock_recovery_delay(const struct drm_dp_aux *aux,
					    const u8 dpcd[DP_RECEIVER_CAP_SIZE])
{
	u8 rd_interval = dpcd[DP_TRAINING_AUX_RD_INTERVAL] &
		DP_TRAINING_AUX_RD_MASK;
	int delay_us;

	if (dpcd[DP_DPCD_REV] >= DP_DPCD_REV_14)
		delay_us = 100;
	else
		delay_us = __8b10b_clock_recovery_delay_us(aux, rd_interval);

	usleep_range(delay_us, delay_us * 2);
}
EXPORT_SYMBOL(drm_dp_link_train_clock_recovery_delay);

static void __drm_dp_link_train_channel_eq_delay(const struct drm_dp_aux *aux,
						 u8 rd_interval)
{
	int delay_us = __8b10b_channel_eq_delay_us(aux, rd_interval);

	usleep_range(delay_us, delay_us * 2);
}

void drm_dp_link_train_channel_eq_delay(const struct drm_dp_aux *aux,
					const u8 dpcd[DP_RECEIVER_CAP_SIZE])
{
	__drm_dp_link_train_channel_eq_delay(aux,
					     dpcd[DP_TRAINING_AUX_RD_INTERVAL] &
					     DP_TRAINING_AUX_RD_MASK);
}
EXPORT_SYMBOL(drm_dp_link_train_channel_eq_delay);

 
const char *drm_dp_phy_name(enum drm_dp_phy dp_phy)
{
	static const char * const phy_names[] = {
		[DP_PHY_DPRX] = "DPRX",
		[DP_PHY_LTTPR1] = "LTTPR 1",
		[DP_PHY_LTTPR2] = "LTTPR 2",
		[DP_PHY_LTTPR3] = "LTTPR 3",
		[DP_PHY_LTTPR4] = "LTTPR 4",
		[DP_PHY_LTTPR5] = "LTTPR 5",
		[DP_PHY_LTTPR6] = "LTTPR 6",
		[DP_PHY_LTTPR7] = "LTTPR 7",
		[DP_PHY_LTTPR8] = "LTTPR 8",
	};

	if (dp_phy < 0 || dp_phy >= ARRAY_SIZE(phy_names) ||
	    WARN_ON(!phy_names[dp_phy]))
		return "<INVALID DP PHY>";

	return phy_names[dp_phy];
}
EXPORT_SYMBOL(drm_dp_phy_name);

void drm_dp_lttpr_link_train_clock_recovery_delay(void)
{
	usleep_range(100, 200);
}
EXPORT_SYMBOL(drm_dp_lttpr_link_train_clock_recovery_delay);

static u8 dp_lttpr_phy_cap(const u8 phy_cap[DP_LTTPR_PHY_CAP_SIZE], int r)
{
	return phy_cap[r - DP_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER1];
}

void drm_dp_lttpr_link_train_channel_eq_delay(const struct drm_dp_aux *aux,
					      const u8 phy_cap[DP_LTTPR_PHY_CAP_SIZE])
{
	u8 interval = dp_lttpr_phy_cap(phy_cap,
				       DP_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER1) &
		      DP_TRAINING_AUX_RD_MASK;

	__drm_dp_link_train_channel_eq_delay(aux, interval);
}
EXPORT_SYMBOL(drm_dp_lttpr_link_train_channel_eq_delay);

u8 drm_dp_link_rate_to_bw_code(int link_rate)
{
	switch (link_rate) {
	case 1000000:
		return DP_LINK_BW_10;
	case 1350000:
		return DP_LINK_BW_13_5;
	case 2000000:
		return DP_LINK_BW_20;
	default:
		 
		return link_rate / 27000;
	}
}
EXPORT_SYMBOL(drm_dp_link_rate_to_bw_code);

int drm_dp_bw_code_to_link_rate(u8 link_bw)
{
	switch (link_bw) {
	case DP_LINK_BW_10:
		return 1000000;
	case DP_LINK_BW_13_5:
		return 1350000;
	case DP_LINK_BW_20:
		return 2000000;
	default:
		 
		return link_bw * 27000;
	}
}
EXPORT_SYMBOL(drm_dp_bw_code_to_link_rate);

#define AUX_RETRY_INTERVAL 500  

static inline void
drm_dp_dump_access(const struct drm_dp_aux *aux,
		   u8 request, uint offset, void *buffer, int ret)
{
	const char *arrow = request == DP_AUX_NATIVE_READ ? "->" : "<-";

	if (ret > 0)
		drm_dbg_dp(aux->drm_dev, "%s: 0x%05x AUX %s (ret=%3d) %*ph\n",
			   aux->name, offset, arrow, ret, min(ret, 20), buffer);
	else
		drm_dbg_dp(aux->drm_dev, "%s: 0x%05x AUX %s (ret=%3d)\n",
			   aux->name, offset, arrow, ret);
}

 

static int drm_dp_dpcd_access(struct drm_dp_aux *aux, u8 request,
			      unsigned int offset, void *buffer, size_t size)
{
	struct drm_dp_aux_msg msg;
	unsigned int retry, native_reply;
	int err = 0, ret = 0;

	memset(&msg, 0, sizeof(msg));
	msg.address = offset;
	msg.request = request;
	msg.buffer = buffer;
	msg.size = size;

	mutex_lock(&aux->hw_mutex);

	 
	for (retry = 0; retry < 32; retry++) {
		if (ret != 0 && ret != -ETIMEDOUT) {
			usleep_range(AUX_RETRY_INTERVAL,
				     AUX_RETRY_INTERVAL + 100);
		}

		ret = aux->transfer(aux, &msg);
		if (ret >= 0) {
			native_reply = msg.reply & DP_AUX_NATIVE_REPLY_MASK;
			if (native_reply == DP_AUX_NATIVE_REPLY_ACK) {
				if (ret == size)
					goto unlock;

				ret = -EPROTO;
			} else
				ret = -EIO;
		}

		 
		if (!err)
			err = ret;
	}

	drm_dbg_kms(aux->drm_dev, "%s: Too many retries, giving up. First error: %d\n",
		    aux->name, err);
	ret = err;

unlock:
	mutex_unlock(&aux->hw_mutex);
	return ret;
}

 
int drm_dp_dpcd_probe(struct drm_dp_aux *aux, unsigned int offset)
{
	u8 buffer;
	int ret;

	ret = drm_dp_dpcd_access(aux, DP_AUX_NATIVE_READ, offset, &buffer, 1);
	WARN_ON(ret == 0);

	drm_dp_dump_access(aux, DP_AUX_NATIVE_READ, offset, &buffer, ret);

	return ret < 0 ? ret : 0;
}
EXPORT_SYMBOL(drm_dp_dpcd_probe);

 
ssize_t drm_dp_dpcd_read(struct drm_dp_aux *aux, unsigned int offset,
			 void *buffer, size_t size)
{
	int ret;

	 
	if (!aux->is_remote) {
		ret = drm_dp_dpcd_probe(aux, DP_DPCD_REV);
		if (ret < 0)
			return ret;
	}

	if (aux->is_remote)
		ret = drm_dp_mst_dpcd_read(aux, offset, buffer, size);
	else
		ret = drm_dp_dpcd_access(aux, DP_AUX_NATIVE_READ, offset,
					 buffer, size);

	drm_dp_dump_access(aux, DP_AUX_NATIVE_READ, offset, buffer, ret);
	return ret;
}
EXPORT_SYMBOL(drm_dp_dpcd_read);

 
ssize_t drm_dp_dpcd_write(struct drm_dp_aux *aux, unsigned int offset,
			  void *buffer, size_t size)
{
	int ret;

	if (aux->is_remote)
		ret = drm_dp_mst_dpcd_write(aux, offset, buffer, size);
	else
		ret = drm_dp_dpcd_access(aux, DP_AUX_NATIVE_WRITE, offset,
					 buffer, size);

	drm_dp_dump_access(aux, DP_AUX_NATIVE_WRITE, offset, buffer, ret);
	return ret;
}
EXPORT_SYMBOL(drm_dp_dpcd_write);

 
int drm_dp_dpcd_read_link_status(struct drm_dp_aux *aux,
				 u8 status[DP_LINK_STATUS_SIZE])
{
	return drm_dp_dpcd_read(aux, DP_LANE0_1_STATUS, status,
				DP_LINK_STATUS_SIZE);
}
EXPORT_SYMBOL(drm_dp_dpcd_read_link_status);

 
int drm_dp_dpcd_read_phy_link_status(struct drm_dp_aux *aux,
				     enum drm_dp_phy dp_phy,
				     u8 link_status[DP_LINK_STATUS_SIZE])
{
	int ret;

	if (dp_phy == DP_PHY_DPRX) {
		ret = drm_dp_dpcd_read(aux,
				       DP_LANE0_1_STATUS,
				       link_status,
				       DP_LINK_STATUS_SIZE);

		if (ret < 0)
			return ret;

		WARN_ON(ret != DP_LINK_STATUS_SIZE);

		return 0;
	}

	ret = drm_dp_dpcd_read(aux,
			       DP_LANE0_1_STATUS_PHY_REPEATER(dp_phy),
			       link_status,
			       DP_LINK_STATUS_SIZE - 1);

	if (ret < 0)
		return ret;

	WARN_ON(ret != DP_LINK_STATUS_SIZE - 1);

	 
	memmove(&link_status[DP_SINK_STATUS - DP_LANE0_1_STATUS + 1],
		&link_status[DP_SINK_STATUS - DP_LANE0_1_STATUS],
		DP_LINK_STATUS_SIZE - (DP_SINK_STATUS - DP_LANE0_1_STATUS) - 1);
	link_status[DP_SINK_STATUS - DP_LANE0_1_STATUS] = 0;

	return 0;
}
EXPORT_SYMBOL(drm_dp_dpcd_read_phy_link_status);

static bool is_edid_digital_input_dp(const struct edid *edid)
{
	return edid && edid->revision >= 4 &&
		edid->input & DRM_EDID_INPUT_DIGITAL &&
		(edid->input & DRM_EDID_DIGITAL_TYPE_MASK) == DRM_EDID_DIGITAL_TYPE_DP;
}

 
bool drm_dp_downstream_is_type(const u8 dpcd[DP_RECEIVER_CAP_SIZE],
			       const u8 port_cap[4], u8 type)
{
	return drm_dp_is_branch(dpcd) &&
		dpcd[DP_DPCD_REV] >= 0x11 &&
		(port_cap[0] & DP_DS_PORT_TYPE_MASK) == type;
}
EXPORT_SYMBOL(drm_dp_downstream_is_type);

 
bool drm_dp_downstream_is_tmds(const u8 dpcd[DP_RECEIVER_CAP_SIZE],
			       const u8 port_cap[4],
			       const struct edid *edid)
{
	if (dpcd[DP_DPCD_REV] < 0x11) {
		switch (dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_TYPE_MASK) {
		case DP_DWN_STRM_PORT_TYPE_TMDS:
			return true;
		default:
			return false;
		}
	}

	switch (port_cap[0] & DP_DS_PORT_TYPE_MASK) {
	case DP_DS_PORT_TYPE_DP_DUALMODE:
		if (is_edid_digital_input_dp(edid))
			return false;
		fallthrough;
	case DP_DS_PORT_TYPE_DVI:
	case DP_DS_PORT_TYPE_HDMI:
		return true;
	default:
		return false;
	}
}
EXPORT_SYMBOL(drm_dp_downstream_is_tmds);

 
bool drm_dp_send_real_edid_checksum(struct drm_dp_aux *aux,
				    u8 real_edid_checksum)
{
	u8 link_edid_read = 0, auto_test_req = 0, test_resp = 0;

	if (drm_dp_dpcd_read(aux, DP_DEVICE_SERVICE_IRQ_VECTOR,
			     &auto_test_req, 1) < 1) {
		drm_err(aux->drm_dev, "%s: DPCD failed read at register 0x%x\n",
			aux->name, DP_DEVICE_SERVICE_IRQ_VECTOR);
		return false;
	}
	auto_test_req &= DP_AUTOMATED_TEST_REQUEST;

	if (drm_dp_dpcd_read(aux, DP_TEST_REQUEST, &link_edid_read, 1) < 1) {
		drm_err(aux->drm_dev, "%s: DPCD failed read at register 0x%x\n",
			aux->name, DP_TEST_REQUEST);
		return false;
	}
	link_edid_read &= DP_TEST_LINK_EDID_READ;

	if (!auto_test_req || !link_edid_read) {
		drm_dbg_kms(aux->drm_dev, "%s: Source DUT does not support TEST_EDID_READ\n",
			    aux->name);
		return false;
	}

	if (drm_dp_dpcd_write(aux, DP_DEVICE_SERVICE_IRQ_VECTOR,
			      &auto_test_req, 1) < 1) {
		drm_err(aux->drm_dev, "%s: DPCD failed write at register 0x%x\n",
			aux->name, DP_DEVICE_SERVICE_IRQ_VECTOR);
		return false;
	}

	 
	if (drm_dp_dpcd_write(aux, DP_TEST_EDID_CHECKSUM,
			      &real_edid_checksum, 1) < 1) {
		drm_err(aux->drm_dev, "%s: DPCD failed write at register 0x%x\n",
			aux->name, DP_TEST_EDID_CHECKSUM);
		return false;
	}

	test_resp |= DP_TEST_EDID_CHECKSUM_WRITE;
	if (drm_dp_dpcd_write(aux, DP_TEST_RESPONSE, &test_resp, 1) < 1) {
		drm_err(aux->drm_dev, "%s: DPCD failed write at register 0x%x\n",
			aux->name, DP_TEST_RESPONSE);
		return false;
	}

	return true;
}
EXPORT_SYMBOL(drm_dp_send_real_edid_checksum);

static u8 drm_dp_downstream_port_count(const u8 dpcd[DP_RECEIVER_CAP_SIZE])
{
	u8 port_count = dpcd[DP_DOWN_STREAM_PORT_COUNT] & DP_PORT_COUNT_MASK;

	if (dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DETAILED_CAP_INFO_AVAILABLE && port_count > 4)
		port_count = 4;

	return port_count;
}

static int drm_dp_read_extended_dpcd_caps(struct drm_dp_aux *aux,
					  u8 dpcd[DP_RECEIVER_CAP_SIZE])
{
	u8 dpcd_ext[DP_RECEIVER_CAP_SIZE];
	int ret;

	 
	if (!(dpcd[DP_TRAINING_AUX_RD_INTERVAL] &
	      DP_EXTENDED_RECEIVER_CAP_FIELD_PRESENT))
		return 0;

	ret = drm_dp_dpcd_read(aux, DP_DP13_DPCD_REV, &dpcd_ext,
			       sizeof(dpcd_ext));
	if (ret < 0)
		return ret;
	if (ret != sizeof(dpcd_ext))
		return -EIO;

	if (dpcd[DP_DPCD_REV] > dpcd_ext[DP_DPCD_REV]) {
		drm_dbg_kms(aux->drm_dev,
			    "%s: Extended DPCD rev less than base DPCD rev (%d > %d)\n",
			    aux->name, dpcd[DP_DPCD_REV], dpcd_ext[DP_DPCD_REV]);
		return 0;
	}

	if (!memcmp(dpcd, dpcd_ext, sizeof(dpcd_ext)))
		return 0;

	drm_dbg_kms(aux->drm_dev, "%s: Base DPCD: %*ph\n", aux->name, DP_RECEIVER_CAP_SIZE, dpcd);

	memcpy(dpcd, dpcd_ext, sizeof(dpcd_ext));

	return 0;
}

 
int drm_dp_read_dpcd_caps(struct drm_dp_aux *aux,
			  u8 dpcd[DP_RECEIVER_CAP_SIZE])
{
	int ret;

	ret = drm_dp_dpcd_read(aux, DP_DPCD_REV, dpcd, DP_RECEIVER_CAP_SIZE);
	if (ret < 0)
		return ret;
	if (ret != DP_RECEIVER_CAP_SIZE || dpcd[DP_DPCD_REV] == 0)
		return -EIO;

	ret = drm_dp_read_extended_dpcd_caps(aux, dpcd);
	if (ret < 0)
		return ret;

	drm_dbg_kms(aux->drm_dev, "%s: DPCD: %*ph\n", aux->name, DP_RECEIVER_CAP_SIZE, dpcd);

	return ret;
}
EXPORT_SYMBOL(drm_dp_read_dpcd_caps);

 
int drm_dp_read_downstream_info(struct drm_dp_aux *aux,
				const u8 dpcd[DP_RECEIVER_CAP_SIZE],
				u8 downstream_ports[DP_MAX_DOWNSTREAM_PORTS])
{
	int ret;
	u8 len;

	memset(downstream_ports, 0, DP_MAX_DOWNSTREAM_PORTS);

	 
	if (!drm_dp_is_branch(dpcd) || dpcd[DP_DPCD_REV] == DP_DPCD_REV_10)
		return 0;

	 
	len = drm_dp_downstream_port_count(dpcd);
	if (!len)
		return 0;

	if (dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DETAILED_CAP_INFO_AVAILABLE)
		len *= 4;

	ret = drm_dp_dpcd_read(aux, DP_DOWNSTREAM_PORT_0, downstream_ports, len);
	if (ret < 0)
		return ret;
	if (ret != len)
		return -EIO;

	drm_dbg_kms(aux->drm_dev, "%s: DPCD DFP: %*ph\n", aux->name, len, downstream_ports);

	return 0;
}
EXPORT_SYMBOL(drm_dp_read_downstream_info);

 
int drm_dp_downstream_max_dotclock(const u8 dpcd[DP_RECEIVER_CAP_SIZE],
				   const u8 port_cap[4])
{
	if (!drm_dp_is_branch(dpcd))
		return 0;

	if (dpcd[DP_DPCD_REV] < 0x11)
		return 0;

	switch (port_cap[0] & DP_DS_PORT_TYPE_MASK) {
	case DP_DS_PORT_TYPE_VGA:
		if ((dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DETAILED_CAP_INFO_AVAILABLE) == 0)
			return 0;
		return port_cap[1] * 8000;
	default:
		return 0;
	}
}
EXPORT_SYMBOL(drm_dp_downstream_max_dotclock);

 
int drm_dp_downstream_max_tmds_clock(const u8 dpcd[DP_RECEIVER_CAP_SIZE],
				     const u8 port_cap[4],
				     const struct edid *edid)
{
	if (!drm_dp_is_branch(dpcd))
		return 0;

	if (dpcd[DP_DPCD_REV] < 0x11) {
		switch (dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_TYPE_MASK) {
		case DP_DWN_STRM_PORT_TYPE_TMDS:
			return 165000;
		default:
			return 0;
		}
	}

	switch (port_cap[0] & DP_DS_PORT_TYPE_MASK) {
	case DP_DS_PORT_TYPE_DP_DUALMODE:
		if (is_edid_digital_input_dp(edid))
			return 0;
		 
		fallthrough;
	case DP_DS_PORT_TYPE_HDMI:
		  
		if ((dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DETAILED_CAP_INFO_AVAILABLE) == 0)
			return 300000;
		return port_cap[1] * 2500;
	case DP_DS_PORT_TYPE_DVI:
		if ((dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DETAILED_CAP_INFO_AVAILABLE) == 0)
			return 165000;
		 
		return port_cap[1] * 2500;
	default:
		return 0;
	}
}
EXPORT_SYMBOL(drm_dp_downstream_max_tmds_clock);

 
int drm_dp_downstream_min_tmds_clock(const u8 dpcd[DP_RECEIVER_CAP_SIZE],
				     const u8 port_cap[4],
				     const struct edid *edid)
{
	if (!drm_dp_is_branch(dpcd))
		return 0;

	if (dpcd[DP_DPCD_REV] < 0x11) {
		switch (dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_TYPE_MASK) {
		case DP_DWN_STRM_PORT_TYPE_TMDS:
			return 25000;
		default:
			return 0;
		}
	}

	switch (port_cap[0] & DP_DS_PORT_TYPE_MASK) {
	case DP_DS_PORT_TYPE_DP_DUALMODE:
		if (is_edid_digital_input_dp(edid))
			return 0;
		fallthrough;
	case DP_DS_PORT_TYPE_DVI:
	case DP_DS_PORT_TYPE_HDMI:
		 
		return 25000;
	default:
		return 0;
	}
}
EXPORT_SYMBOL(drm_dp_downstream_min_tmds_clock);

 
int drm_dp_downstream_max_bpc(const u8 dpcd[DP_RECEIVER_CAP_SIZE],
			      const u8 port_cap[4],
			      const struct edid *edid)
{
	if (!drm_dp_is_branch(dpcd))
		return 0;

	if (dpcd[DP_DPCD_REV] < 0x11) {
		switch (dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_TYPE_MASK) {
		case DP_DWN_STRM_PORT_TYPE_DP:
			return 0;
		default:
			return 8;
		}
	}

	switch (port_cap[0] & DP_DS_PORT_TYPE_MASK) {
	case DP_DS_PORT_TYPE_DP:
		return 0;
	case DP_DS_PORT_TYPE_DP_DUALMODE:
		if (is_edid_digital_input_dp(edid))
			return 0;
		fallthrough;
	case DP_DS_PORT_TYPE_HDMI:
	case DP_DS_PORT_TYPE_DVI:
	case DP_DS_PORT_TYPE_VGA:
		if ((dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DETAILED_CAP_INFO_AVAILABLE) == 0)
			return 8;

		switch (port_cap[2] & DP_DS_MAX_BPC_MASK) {
		case DP_DS_8BPC:
			return 8;
		case DP_DS_10BPC:
			return 10;
		case DP_DS_12BPC:
			return 12;
		case DP_DS_16BPC:
			return 16;
		default:
			return 8;
		}
		break;
	default:
		return 8;
	}
}
EXPORT_SYMBOL(drm_dp_downstream_max_bpc);

 
bool drm_dp_downstream_420_passthrough(const u8 dpcd[DP_RECEIVER_CAP_SIZE],
				       const u8 port_cap[4])
{
	if (!drm_dp_is_branch(dpcd))
		return false;

	if (dpcd[DP_DPCD_REV] < 0x13)
		return false;

	switch (port_cap[0] & DP_DS_PORT_TYPE_MASK) {
	case DP_DS_PORT_TYPE_DP:
		return true;
	case DP_DS_PORT_TYPE_HDMI:
		if ((dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DETAILED_CAP_INFO_AVAILABLE) == 0)
			return false;

		return port_cap[3] & DP_DS_HDMI_YCBCR420_PASS_THROUGH;
	default:
		return false;
	}
}
EXPORT_SYMBOL(drm_dp_downstream_420_passthrough);

 
bool drm_dp_downstream_444_to_420_conversion(const u8 dpcd[DP_RECEIVER_CAP_SIZE],
					     const u8 port_cap[4])
{
	if (!drm_dp_is_branch(dpcd))
		return false;

	if (dpcd[DP_DPCD_REV] < 0x13)
		return false;

	switch (port_cap[0] & DP_DS_PORT_TYPE_MASK) {
	case DP_DS_PORT_TYPE_HDMI:
		if ((dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DETAILED_CAP_INFO_AVAILABLE) == 0)
			return false;

		return port_cap[3] & DP_DS_HDMI_YCBCR444_TO_420_CONV;
	default:
		return false;
	}
}
EXPORT_SYMBOL(drm_dp_downstream_444_to_420_conversion);

 
bool drm_dp_downstream_rgb_to_ycbcr_conversion(const u8 dpcd[DP_RECEIVER_CAP_SIZE],
					       const u8 port_cap[4],
					       u8 color_spc)
{
	if (!drm_dp_is_branch(dpcd))
		return false;

	if (dpcd[DP_DPCD_REV] < 0x13)
		return false;

	switch (port_cap[0] & DP_DS_PORT_TYPE_MASK) {
	case DP_DS_PORT_TYPE_HDMI:
		if ((dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DETAILED_CAP_INFO_AVAILABLE) == 0)
			return false;

		return port_cap[3] & color_spc;
	default:
		return false;
	}
}
EXPORT_SYMBOL(drm_dp_downstream_rgb_to_ycbcr_conversion);

 
struct drm_display_mode *
drm_dp_downstream_mode(struct drm_device *dev,
		       const u8 dpcd[DP_RECEIVER_CAP_SIZE],
		       const u8 port_cap[4])

{
	u8 vic;

	if (!drm_dp_is_branch(dpcd))
		return NULL;

	if (dpcd[DP_DPCD_REV] < 0x11)
		return NULL;

	switch (port_cap[0] & DP_DS_PORT_TYPE_MASK) {
	case DP_DS_PORT_TYPE_NON_EDID:
		switch (port_cap[0] & DP_DS_NON_EDID_MASK) {
		case DP_DS_NON_EDID_720x480i_60:
			vic = 6;
			break;
		case DP_DS_NON_EDID_720x480i_50:
			vic = 21;
			break;
		case DP_DS_NON_EDID_1920x1080i_60:
			vic = 5;
			break;
		case DP_DS_NON_EDID_1920x1080i_50:
			vic = 20;
			break;
		case DP_DS_NON_EDID_1280x720_60:
			vic = 4;
			break;
		case DP_DS_NON_EDID_1280x720_50:
			vic = 19;
			break;
		default:
			return NULL;
		}
		return drm_display_mode_from_cea_vic(dev, vic);
	default:
		return NULL;
	}
}
EXPORT_SYMBOL(drm_dp_downstream_mode);

 
int drm_dp_downstream_id(struct drm_dp_aux *aux, char id[6])
{
	return drm_dp_dpcd_read(aux, DP_BRANCH_ID, id, 6);
}
EXPORT_SYMBOL(drm_dp_downstream_id);

 
void drm_dp_downstream_debug(struct seq_file *m,
			     const u8 dpcd[DP_RECEIVER_CAP_SIZE],
			     const u8 port_cap[4],
			     const struct edid *edid,
			     struct drm_dp_aux *aux)
{
	bool detailed_cap_info = dpcd[DP_DOWNSTREAMPORT_PRESENT] &
				 DP_DETAILED_CAP_INFO_AVAILABLE;
	int clk;
	int bpc;
	char id[7];
	int len;
	uint8_t rev[2];
	int type = port_cap[0] & DP_DS_PORT_TYPE_MASK;
	bool branch_device = drm_dp_is_branch(dpcd);

	seq_printf(m, "\tDP branch device present: %s\n",
		   str_yes_no(branch_device));

	if (!branch_device)
		return;

	switch (type) {
	case DP_DS_PORT_TYPE_DP:
		seq_puts(m, "\t\tType: DisplayPort\n");
		break;
	case DP_DS_PORT_TYPE_VGA:
		seq_puts(m, "\t\tType: VGA\n");
		break;
	case DP_DS_PORT_TYPE_DVI:
		seq_puts(m, "\t\tType: DVI\n");
		break;
	case DP_DS_PORT_TYPE_HDMI:
		seq_puts(m, "\t\tType: HDMI\n");
		break;
	case DP_DS_PORT_TYPE_NON_EDID:
		seq_puts(m, "\t\tType: others without EDID support\n");
		break;
	case DP_DS_PORT_TYPE_DP_DUALMODE:
		seq_puts(m, "\t\tType: DP++\n");
		break;
	case DP_DS_PORT_TYPE_WIRELESS:
		seq_puts(m, "\t\tType: Wireless\n");
		break;
	default:
		seq_puts(m, "\t\tType: N/A\n");
	}

	memset(id, 0, sizeof(id));
	drm_dp_downstream_id(aux, id);
	seq_printf(m, "\t\tID: %s\n", id);

	len = drm_dp_dpcd_read(aux, DP_BRANCH_HW_REV, &rev[0], 1);
	if (len > 0)
		seq_printf(m, "\t\tHW: %d.%d\n",
			   (rev[0] & 0xf0) >> 4, rev[0] & 0xf);

	len = drm_dp_dpcd_read(aux, DP_BRANCH_SW_REV, rev, 2);
	if (len > 0)
		seq_printf(m, "\t\tSW: %d.%d\n", rev[0], rev[1]);

	if (detailed_cap_info) {
		clk = drm_dp_downstream_max_dotclock(dpcd, port_cap);
		if (clk > 0)
			seq_printf(m, "\t\tMax dot clock: %d kHz\n", clk);

		clk = drm_dp_downstream_max_tmds_clock(dpcd, port_cap, edid);
		if (clk > 0)
			seq_printf(m, "\t\tMax TMDS clock: %d kHz\n", clk);

		clk = drm_dp_downstream_min_tmds_clock(dpcd, port_cap, edid);
		if (clk > 0)
			seq_printf(m, "\t\tMin TMDS clock: %d kHz\n", clk);

		bpc = drm_dp_downstream_max_bpc(dpcd, port_cap, edid);

		if (bpc > 0)
			seq_printf(m, "\t\tMax bpc: %d\n", bpc);
	}
}
EXPORT_SYMBOL(drm_dp_downstream_debug);

 
enum drm_mode_subconnector
drm_dp_subconnector_type(const u8 dpcd[DP_RECEIVER_CAP_SIZE],
			 const u8 port_cap[4])
{
	int type;
	if (!drm_dp_is_branch(dpcd))
		return DRM_MODE_SUBCONNECTOR_Native;
	 
	if (dpcd[DP_DPCD_REV] == DP_DPCD_REV_10) {
		type = dpcd[DP_DOWNSTREAMPORT_PRESENT] &
		       DP_DWN_STRM_PORT_TYPE_MASK;

		switch (type) {
		case DP_DWN_STRM_PORT_TYPE_TMDS:
			 
			return DRM_MODE_SUBCONNECTOR_DVID;
		case DP_DWN_STRM_PORT_TYPE_ANALOG:
			 
			return DRM_MODE_SUBCONNECTOR_VGA;
		case DP_DWN_STRM_PORT_TYPE_DP:
			return DRM_MODE_SUBCONNECTOR_DisplayPort;
		case DP_DWN_STRM_PORT_TYPE_OTHER:
		default:
			return DRM_MODE_SUBCONNECTOR_Unknown;
		}
	}
	type = port_cap[0] & DP_DS_PORT_TYPE_MASK;

	switch (type) {
	case DP_DS_PORT_TYPE_DP:
	case DP_DS_PORT_TYPE_DP_DUALMODE:
		return DRM_MODE_SUBCONNECTOR_DisplayPort;
	case DP_DS_PORT_TYPE_VGA:
		return DRM_MODE_SUBCONNECTOR_VGA;
	case DP_DS_PORT_TYPE_DVI:
		return DRM_MODE_SUBCONNECTOR_DVID;
	case DP_DS_PORT_TYPE_HDMI:
		return DRM_MODE_SUBCONNECTOR_HDMIA;
	case DP_DS_PORT_TYPE_WIRELESS:
		return DRM_MODE_SUBCONNECTOR_Wireless;
	case DP_DS_PORT_TYPE_NON_EDID:
	default:
		return DRM_MODE_SUBCONNECTOR_Unknown;
	}
}
EXPORT_SYMBOL(drm_dp_subconnector_type);

 
void drm_dp_set_subconnector_property(struct drm_connector *connector,
				      enum drm_connector_status status,
				      const u8 *dpcd,
				      const u8 port_cap[4])
{
	enum drm_mode_subconnector subconnector = DRM_MODE_SUBCONNECTOR_Unknown;

	if (status == connector_status_connected)
		subconnector = drm_dp_subconnector_type(dpcd, port_cap);
	drm_object_property_set_value(&connector->base,
			connector->dev->mode_config.dp_subconnector_property,
			subconnector);
}
EXPORT_SYMBOL(drm_dp_set_subconnector_property);

 
bool drm_dp_read_sink_count_cap(struct drm_connector *connector,
				const u8 dpcd[DP_RECEIVER_CAP_SIZE],
				const struct drm_dp_desc *desc)
{
	 
	return connector->connector_type != DRM_MODE_CONNECTOR_eDP &&
		dpcd[DP_DPCD_REV] >= DP_DPCD_REV_11 &&
		dpcd[DP_DOWNSTREAMPORT_PRESENT] & DP_DWN_STRM_PORT_PRESENT &&
		!drm_dp_has_quirk(desc, DP_DPCD_QUIRK_NO_SINK_COUNT);
}
EXPORT_SYMBOL(drm_dp_read_sink_count_cap);

 
int drm_dp_read_sink_count(struct drm_dp_aux *aux)
{
	u8 count;
	int ret;

	ret = drm_dp_dpcd_readb(aux, DP_SINK_COUNT, &count);
	if (ret < 0)
		return ret;
	if (ret != 1)
		return -EIO;

	return DP_GET_SINK_COUNT(count);
}
EXPORT_SYMBOL(drm_dp_read_sink_count);

 

static u32 drm_dp_i2c_functionality(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL |
	       I2C_FUNC_SMBUS_READ_BLOCK_DATA |
	       I2C_FUNC_SMBUS_BLOCK_PROC_CALL |
	       I2C_FUNC_10BIT_ADDR;
}

static void drm_dp_i2c_msg_write_status_update(struct drm_dp_aux_msg *msg)
{
	 
	if ((msg->request & ~DP_AUX_I2C_MOT) == DP_AUX_I2C_WRITE) {
		msg->request &= DP_AUX_I2C_MOT;
		msg->request |= DP_AUX_I2C_WRITE_STATUS_UPDATE;
	}
}

#define AUX_PRECHARGE_LEN 10  
#define AUX_SYNC_LEN (16 + 4)  
#define AUX_STOP_LEN 4
#define AUX_CMD_LEN 4
#define AUX_ADDRESS_LEN 20
#define AUX_REPLY_PAD_LEN 4
#define AUX_LENGTH_LEN 8

 
static int drm_dp_aux_req_duration(const struct drm_dp_aux_msg *msg)
{
	int len = AUX_PRECHARGE_LEN + AUX_SYNC_LEN + AUX_STOP_LEN +
		AUX_CMD_LEN + AUX_ADDRESS_LEN + AUX_LENGTH_LEN;

	if ((msg->request & DP_AUX_I2C_READ) == 0)
		len += msg->size * 8;

	return len;
}

static int drm_dp_aux_reply_duration(const struct drm_dp_aux_msg *msg)
{
	int len = AUX_PRECHARGE_LEN + AUX_SYNC_LEN + AUX_STOP_LEN +
		AUX_CMD_LEN + AUX_REPLY_PAD_LEN;

	 
	if (msg->request & DP_AUX_I2C_READ)
		len += msg->size * 8;

	return len;
}

#define I2C_START_LEN 1
#define I2C_STOP_LEN 1
#define I2C_ADDR_LEN 9  
#define I2C_DATA_LEN 9  

 
static int drm_dp_i2c_msg_duration(const struct drm_dp_aux_msg *msg,
				   int i2c_speed_khz)
{
	 
	return DIV_ROUND_UP((I2C_START_LEN + I2C_ADDR_LEN +
			     msg->size * I2C_DATA_LEN +
			     I2C_STOP_LEN) * 1000, i2c_speed_khz);
}

 
static int drm_dp_i2c_retry_count(const struct drm_dp_aux_msg *msg,
			      int i2c_speed_khz)
{
	int aux_time_us = drm_dp_aux_req_duration(msg) +
		drm_dp_aux_reply_duration(msg);
	int i2c_time_us = drm_dp_i2c_msg_duration(msg, i2c_speed_khz);

	return DIV_ROUND_UP(i2c_time_us, aux_time_us + AUX_RETRY_INTERVAL);
}

 
static int dp_aux_i2c_speed_khz __read_mostly = 10;
module_param_unsafe(dp_aux_i2c_speed_khz, int, 0644);
MODULE_PARM_DESC(dp_aux_i2c_speed_khz,
		 "Assumed speed of the i2c bus in kHz, (1-400, default 10)");

 
static int drm_dp_i2c_do_msg(struct drm_dp_aux *aux, struct drm_dp_aux_msg *msg)
{
	unsigned int retry, defer_i2c;
	int ret;
	 
	int max_retries = max(7, drm_dp_i2c_retry_count(msg, dp_aux_i2c_speed_khz));

	for (retry = 0, defer_i2c = 0; retry < (max_retries + defer_i2c); retry++) {
		ret = aux->transfer(aux, msg);
		if (ret < 0) {
			if (ret == -EBUSY)
				continue;

			 
			if (ret == -ETIMEDOUT)
				drm_dbg_kms_ratelimited(aux->drm_dev, "%s: transaction timed out\n",
							aux->name);
			else
				drm_dbg_kms(aux->drm_dev, "%s: transaction failed: %d\n",
					    aux->name, ret);
			return ret;
		}


		switch (msg->reply & DP_AUX_NATIVE_REPLY_MASK) {
		case DP_AUX_NATIVE_REPLY_ACK:
			 
			break;

		case DP_AUX_NATIVE_REPLY_NACK:
			drm_dbg_kms(aux->drm_dev, "%s: native nack (result=%d, size=%zu)\n",
				    aux->name, ret, msg->size);
			return -EREMOTEIO;

		case DP_AUX_NATIVE_REPLY_DEFER:
			drm_dbg_kms(aux->drm_dev, "%s: native defer\n", aux->name);
			 
			usleep_range(AUX_RETRY_INTERVAL, AUX_RETRY_INTERVAL + 100);
			continue;

		default:
			drm_err(aux->drm_dev, "%s: invalid native reply %#04x\n",
				aux->name, msg->reply);
			return -EREMOTEIO;
		}

		switch (msg->reply & DP_AUX_I2C_REPLY_MASK) {
		case DP_AUX_I2C_REPLY_ACK:
			 
			if (ret != msg->size)
				drm_dp_i2c_msg_write_status_update(msg);
			return ret;

		case DP_AUX_I2C_REPLY_NACK:
			drm_dbg_kms(aux->drm_dev, "%s: I2C nack (result=%d, size=%zu)\n",
				    aux->name, ret, msg->size);
			aux->i2c_nack_count++;
			return -EREMOTEIO;

		case DP_AUX_I2C_REPLY_DEFER:
			drm_dbg_kms(aux->drm_dev, "%s: I2C defer\n", aux->name);
			 
			aux->i2c_defer_count++;
			if (defer_i2c < 7)
				defer_i2c++;
			usleep_range(AUX_RETRY_INTERVAL, AUX_RETRY_INTERVAL + 100);
			drm_dp_i2c_msg_write_status_update(msg);

			continue;

		default:
			drm_err(aux->drm_dev, "%s: invalid I2C reply %#04x\n",
				aux->name, msg->reply);
			return -EREMOTEIO;
		}
	}

	drm_dbg_kms(aux->drm_dev, "%s: Too many retries, giving up\n", aux->name);
	return -EREMOTEIO;
}

static void drm_dp_i2c_msg_set_request(struct drm_dp_aux_msg *msg,
				       const struct i2c_msg *i2c_msg)
{
	msg->request = (i2c_msg->flags & I2C_M_RD) ?
		DP_AUX_I2C_READ : DP_AUX_I2C_WRITE;
	if (!(i2c_msg->flags & I2C_M_STOP))
		msg->request |= DP_AUX_I2C_MOT;
}

 
static int drm_dp_i2c_drain_msg(struct drm_dp_aux *aux, struct drm_dp_aux_msg *orig_msg)
{
	int err, ret = orig_msg->size;
	struct drm_dp_aux_msg msg = *orig_msg;

	while (msg.size > 0) {
		err = drm_dp_i2c_do_msg(aux, &msg);
		if (err <= 0)
			return err == 0 ? -EPROTO : err;

		if (err < msg.size && err < ret) {
			drm_dbg_kms(aux->drm_dev,
				    "%s: Partial I2C reply: requested %zu bytes got %d bytes\n",
				    aux->name, msg.size, err);
			ret = err;
		}

		msg.size -= err;
		msg.buffer += err;
	}

	return ret;
}

 
static int dp_aux_i2c_transfer_size __read_mostly = DP_AUX_MAX_PAYLOAD_BYTES;
module_param_unsafe(dp_aux_i2c_transfer_size, int, 0644);
MODULE_PARM_DESC(dp_aux_i2c_transfer_size,
		 "Number of bytes to transfer in a single I2C over DP AUX CH message, (1-16, default 16)");

static int drm_dp_i2c_xfer(struct i2c_adapter *adapter, struct i2c_msg *msgs,
			   int num)
{
	struct drm_dp_aux *aux = adapter->algo_data;
	unsigned int i, j;
	unsigned transfer_size;
	struct drm_dp_aux_msg msg;
	int err = 0;

	dp_aux_i2c_transfer_size = clamp(dp_aux_i2c_transfer_size, 1, DP_AUX_MAX_PAYLOAD_BYTES);

	memset(&msg, 0, sizeof(msg));

	for (i = 0; i < num; i++) {
		msg.address = msgs[i].addr;
		drm_dp_i2c_msg_set_request(&msg, &msgs[i]);
		 
		msg.buffer = NULL;
		msg.size = 0;
		err = drm_dp_i2c_do_msg(aux, &msg);

		 
		drm_dp_i2c_msg_set_request(&msg, &msgs[i]);

		if (err < 0)
			break;
		 
		transfer_size = dp_aux_i2c_transfer_size;
		for (j = 0; j < msgs[i].len; j += msg.size) {
			msg.buffer = msgs[i].buf + j;
			msg.size = min(transfer_size, msgs[i].len - j);

			err = drm_dp_i2c_drain_msg(aux, &msg);

			 
			drm_dp_i2c_msg_set_request(&msg, &msgs[i]);

			if (err < 0)
				break;
			transfer_size = err;
		}
		if (err < 0)
			break;
	}
	if (err >= 0)
		err = num;
	 
	msg.request &= ~DP_AUX_I2C_MOT;
	msg.buffer = NULL;
	msg.size = 0;
	(void)drm_dp_i2c_do_msg(aux, &msg);

	return err;
}

static const struct i2c_algorithm drm_dp_i2c_algo = {
	.functionality = drm_dp_i2c_functionality,
	.master_xfer = drm_dp_i2c_xfer,
};

static struct drm_dp_aux *i2c_to_aux(struct i2c_adapter *i2c)
{
	return container_of(i2c, struct drm_dp_aux, ddc);
}

static void lock_bus(struct i2c_adapter *i2c, unsigned int flags)
{
	mutex_lock(&i2c_to_aux(i2c)->hw_mutex);
}

static int trylock_bus(struct i2c_adapter *i2c, unsigned int flags)
{
	return mutex_trylock(&i2c_to_aux(i2c)->hw_mutex);
}

static void unlock_bus(struct i2c_adapter *i2c, unsigned int flags)
{
	mutex_unlock(&i2c_to_aux(i2c)->hw_mutex);
}

static const struct i2c_lock_operations drm_dp_i2c_lock_ops = {
	.lock_bus = lock_bus,
	.trylock_bus = trylock_bus,
	.unlock_bus = unlock_bus,
};

static int drm_dp_aux_get_crc(struct drm_dp_aux *aux, u8 *crc)
{
	u8 buf, count;
	int ret;

	ret = drm_dp_dpcd_readb(aux, DP_TEST_SINK, &buf);
	if (ret < 0)
		return ret;

	WARN_ON(!(buf & DP_TEST_SINK_START));

	ret = drm_dp_dpcd_readb(aux, DP_TEST_SINK_MISC, &buf);
	if (ret < 0)
		return ret;

	count = buf & DP_TEST_COUNT_MASK;
	if (count == aux->crc_count)
		return -EAGAIN;  

	aux->crc_count = count;

	 
	ret = drm_dp_dpcd_read(aux, DP_TEST_CRC_R_CR, crc, 6);
	if (ret < 0)
		return ret;

	return 0;
}

static void drm_dp_aux_crc_work(struct work_struct *work)
{
	struct drm_dp_aux *aux = container_of(work, struct drm_dp_aux,
					      crc_work);
	struct drm_crtc *crtc;
	u8 crc_bytes[6];
	uint32_t crcs[3];
	int ret;

	if (WARN_ON(!aux->crtc))
		return;

	crtc = aux->crtc;
	while (crtc->crc.opened) {
		drm_crtc_wait_one_vblank(crtc);
		if (!crtc->crc.opened)
			break;

		ret = drm_dp_aux_get_crc(aux, crc_bytes);
		if (ret == -EAGAIN) {
			usleep_range(1000, 2000);
			ret = drm_dp_aux_get_crc(aux, crc_bytes);
		}

		if (ret == -EAGAIN) {
			drm_dbg_kms(aux->drm_dev, "%s: Get CRC failed after retrying: %d\n",
				    aux->name, ret);
			continue;
		} else if (ret) {
			drm_dbg_kms(aux->drm_dev, "%s: Failed to get a CRC: %d\n", aux->name, ret);
			continue;
		}

		crcs[0] = crc_bytes[0] | crc_bytes[1] << 8;
		crcs[1] = crc_bytes[2] | crc_bytes[3] << 8;
		crcs[2] = crc_bytes[4] | crc_bytes[5] << 8;
		drm_crtc_add_crc_entry(crtc, false, 0, crcs);
	}
}

 
void drm_dp_remote_aux_init(struct drm_dp_aux *aux)
{
	INIT_WORK(&aux->crc_work, drm_dp_aux_crc_work);
}
EXPORT_SYMBOL(drm_dp_remote_aux_init);

 
void drm_dp_aux_init(struct drm_dp_aux *aux)
{
	mutex_init(&aux->hw_mutex);
	mutex_init(&aux->cec.lock);
	INIT_WORK(&aux->crc_work, drm_dp_aux_crc_work);

	aux->ddc.algo = &drm_dp_i2c_algo;
	aux->ddc.algo_data = aux;
	aux->ddc.retries = 3;

	aux->ddc.lock_ops = &drm_dp_i2c_lock_ops;
}
EXPORT_SYMBOL(drm_dp_aux_init);

 
int drm_dp_aux_register(struct drm_dp_aux *aux)
{
	int ret;

	WARN_ON_ONCE(!aux->drm_dev);

	if (!aux->ddc.algo)
		drm_dp_aux_init(aux);

	aux->ddc.class = I2C_CLASS_DDC;
	aux->ddc.owner = THIS_MODULE;
	aux->ddc.dev.parent = aux->dev;

	strscpy(aux->ddc.name, aux->name ? aux->name : dev_name(aux->dev),
		sizeof(aux->ddc.name));

	ret = drm_dp_aux_register_devnode(aux);
	if (ret)
		return ret;

	ret = i2c_add_adapter(&aux->ddc);
	if (ret) {
		drm_dp_aux_unregister_devnode(aux);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(drm_dp_aux_register);

 
void drm_dp_aux_unregister(struct drm_dp_aux *aux)
{
	drm_dp_aux_unregister_devnode(aux);
	i2c_del_adapter(&aux->ddc);
}
EXPORT_SYMBOL(drm_dp_aux_unregister);

#define PSR_SETUP_TIME(x) [DP_PSR_SETUP_TIME_ ## x >> DP_PSR_SETUP_TIME_SHIFT] = (x)

 
int drm_dp_psr_setup_time(const u8 psr_cap[EDP_PSR_RECEIVER_CAP_SIZE])
{
	static const u16 psr_setup_time_us[] = {
		PSR_SETUP_TIME(330),
		PSR_SETUP_TIME(275),
		PSR_SETUP_TIME(220),
		PSR_SETUP_TIME(165),
		PSR_SETUP_TIME(110),
		PSR_SETUP_TIME(55),
		PSR_SETUP_TIME(0),
	};
	int i;

	i = (psr_cap[1] & DP_PSR_SETUP_TIME_MASK) >> DP_PSR_SETUP_TIME_SHIFT;
	if (i >= ARRAY_SIZE(psr_setup_time_us))
		return -EINVAL;

	return psr_setup_time_us[i];
}
EXPORT_SYMBOL(drm_dp_psr_setup_time);

#undef PSR_SETUP_TIME

 
int drm_dp_start_crc(struct drm_dp_aux *aux, struct drm_crtc *crtc)
{
	u8 buf;
	int ret;

	ret = drm_dp_dpcd_readb(aux, DP_TEST_SINK, &buf);
	if (ret < 0)
		return ret;

	ret = drm_dp_dpcd_writeb(aux, DP_TEST_SINK, buf | DP_TEST_SINK_START);
	if (ret < 0)
		return ret;

	aux->crc_count = 0;
	aux->crtc = crtc;
	schedule_work(&aux->crc_work);

	return 0;
}
EXPORT_SYMBOL(drm_dp_start_crc);

 
int drm_dp_stop_crc(struct drm_dp_aux *aux)
{
	u8 buf;
	int ret;

	ret = drm_dp_dpcd_readb(aux, DP_TEST_SINK, &buf);
	if (ret < 0)
		return ret;

	ret = drm_dp_dpcd_writeb(aux, DP_TEST_SINK, buf & ~DP_TEST_SINK_START);
	if (ret < 0)
		return ret;

	flush_work(&aux->crc_work);
	aux->crtc = NULL;

	return 0;
}
EXPORT_SYMBOL(drm_dp_stop_crc);

struct dpcd_quirk {
	u8 oui[3];
	u8 device_id[6];
	bool is_branch;
	u32 quirks;
};

#define OUI(first, second, third) { (first), (second), (third) }
#define DEVICE_ID(first, second, third, fourth, fifth, sixth) \
	{ (first), (second), (third), (fourth), (fifth), (sixth) }

#define DEVICE_ID_ANY	DEVICE_ID(0, 0, 0, 0, 0, 0)

static const struct dpcd_quirk dpcd_quirk_list[] = {
	 
	{ OUI(0x00, 0x22, 0xb9), DEVICE_ID_ANY, true, BIT(DP_DPCD_QUIRK_CONSTANT_N) },
	 
	{ OUI(0x00, 0x22, 0xb9), DEVICE_ID('s', 'i', 'v', 'a', 'r', 'T'), false, BIT(DP_DPCD_QUIRK_CONSTANT_N) },
	 
	{ OUI(0x00, 0x10, 0xfa), DEVICE_ID_ANY, false, BIT(DP_DPCD_QUIRK_NO_PSR) },
	 
	{ OUI(0x00, 0x00, 0x00), DEVICE_ID('C', 'H', '7', '5', '1', '1'), false, BIT(DP_DPCD_QUIRK_NO_SINK_COUNT) },
	 
	{ OUI(0x90, 0xCC, 0x24), DEVICE_ID_ANY, true, BIT(DP_DPCD_QUIRK_DSC_WITHOUT_VIRTUAL_DPCD) },
	 
	{ OUI(0x00, 0x10, 0xfa), DEVICE_ID(101, 68, 21, 101, 98, 97), false, BIT(DP_DPCD_QUIRK_CAN_DO_MAX_LINK_RATE_3_24_GBPS) },
};

#undef OUI

 
static u32
drm_dp_get_quirks(const struct drm_dp_dpcd_ident *ident, bool is_branch)
{
	const struct dpcd_quirk *quirk;
	u32 quirks = 0;
	int i;
	u8 any_device[] = DEVICE_ID_ANY;

	for (i = 0; i < ARRAY_SIZE(dpcd_quirk_list); i++) {
		quirk = &dpcd_quirk_list[i];

		if (quirk->is_branch != is_branch)
			continue;

		if (memcmp(quirk->oui, ident->oui, sizeof(ident->oui)) != 0)
			continue;

		if (memcmp(quirk->device_id, any_device, sizeof(any_device)) != 0 &&
		    memcmp(quirk->device_id, ident->device_id, sizeof(ident->device_id)) != 0)
			continue;

		quirks |= quirk->quirks;
	}

	return quirks;
}

#undef DEVICE_ID_ANY
#undef DEVICE_ID

 
int drm_dp_read_desc(struct drm_dp_aux *aux, struct drm_dp_desc *desc,
		     bool is_branch)
{
	struct drm_dp_dpcd_ident *ident = &desc->ident;
	unsigned int offset = is_branch ? DP_BRANCH_OUI : DP_SINK_OUI;
	int ret, dev_id_len;

	ret = drm_dp_dpcd_read(aux, offset, ident, sizeof(*ident));
	if (ret < 0)
		return ret;

	desc->quirks = drm_dp_get_quirks(ident, is_branch);

	dev_id_len = strnlen(ident->device_id, sizeof(ident->device_id));

	drm_dbg_kms(aux->drm_dev,
		    "%s: DP %s: OUI %*phD dev-ID %*pE HW-rev %d.%d SW-rev %d.%d quirks 0x%04x\n",
		    aux->name, is_branch ? "branch" : "sink",
		    (int)sizeof(ident->oui), ident->oui, dev_id_len,
		    ident->device_id, ident->hw_rev >> 4, ident->hw_rev & 0xf,
		    ident->sw_major_rev, ident->sw_minor_rev, desc->quirks);

	return 0;
}
EXPORT_SYMBOL(drm_dp_read_desc);

 
u8 drm_dp_dsc_sink_max_slice_count(const u8 dsc_dpcd[DP_DSC_RECEIVER_CAP_SIZE],
				   bool is_edp)
{
	u8 slice_cap1 = dsc_dpcd[DP_DSC_SLICE_CAP_1 - DP_DSC_SUPPORT];

	if (is_edp) {
		 
		if (slice_cap1 & DP_DSC_4_PER_DP_DSC_SINK)
			return 4;
		if (slice_cap1 & DP_DSC_2_PER_DP_DSC_SINK)
			return 2;
		if (slice_cap1 & DP_DSC_1_PER_DP_DSC_SINK)
			return 1;
	} else {
		 
		u8 slice_cap2 = dsc_dpcd[DP_DSC_SLICE_CAP_2 - DP_DSC_SUPPORT];

		if (slice_cap2 & DP_DSC_24_PER_DP_DSC_SINK)
			return 24;
		if (slice_cap2 & DP_DSC_20_PER_DP_DSC_SINK)
			return 20;
		if (slice_cap2 & DP_DSC_16_PER_DP_DSC_SINK)
			return 16;
		if (slice_cap1 & DP_DSC_12_PER_DP_DSC_SINK)
			return 12;
		if (slice_cap1 & DP_DSC_10_PER_DP_DSC_SINK)
			return 10;
		if (slice_cap1 & DP_DSC_8_PER_DP_DSC_SINK)
			return 8;
		if (slice_cap1 & DP_DSC_6_PER_DP_DSC_SINK)
			return 6;
		if (slice_cap1 & DP_DSC_4_PER_DP_DSC_SINK)
			return 4;
		if (slice_cap1 & DP_DSC_2_PER_DP_DSC_SINK)
			return 2;
		if (slice_cap1 & DP_DSC_1_PER_DP_DSC_SINK)
			return 1;
	}

	return 0;
}
EXPORT_SYMBOL(drm_dp_dsc_sink_max_slice_count);

 
u8 drm_dp_dsc_sink_line_buf_depth(const u8 dsc_dpcd[DP_DSC_RECEIVER_CAP_SIZE])
{
	u8 line_buf_depth = dsc_dpcd[DP_DSC_LINE_BUF_BIT_DEPTH - DP_DSC_SUPPORT];

	switch (line_buf_depth & DP_DSC_LINE_BUF_BIT_DEPTH_MASK) {
	case DP_DSC_LINE_BUF_BIT_DEPTH_9:
		return 9;
	case DP_DSC_LINE_BUF_BIT_DEPTH_10:
		return 10;
	case DP_DSC_LINE_BUF_BIT_DEPTH_11:
		return 11;
	case DP_DSC_LINE_BUF_BIT_DEPTH_12:
		return 12;
	case DP_DSC_LINE_BUF_BIT_DEPTH_13:
		return 13;
	case DP_DSC_LINE_BUF_BIT_DEPTH_14:
		return 14;
	case DP_DSC_LINE_BUF_BIT_DEPTH_15:
		return 15;
	case DP_DSC_LINE_BUF_BIT_DEPTH_16:
		return 16;
	case DP_DSC_LINE_BUF_BIT_DEPTH_8:
		return 8;
	}

	return 0;
}
EXPORT_SYMBOL(drm_dp_dsc_sink_line_buf_depth);

 
int drm_dp_dsc_sink_supported_input_bpcs(const u8 dsc_dpcd[DP_DSC_RECEIVER_CAP_SIZE],
					 u8 dsc_bpc[3])
{
	int num_bpc = 0;
	u8 color_depth = dsc_dpcd[DP_DSC_DEC_COLOR_DEPTH_CAP - DP_DSC_SUPPORT];

	if (color_depth & DP_DSC_12_BPC)
		dsc_bpc[num_bpc++] = 12;
	if (color_depth & DP_DSC_10_BPC)
		dsc_bpc[num_bpc++] = 10;
	if (color_depth & DP_DSC_8_BPC)
		dsc_bpc[num_bpc++] = 8;

	return num_bpc;
}
EXPORT_SYMBOL(drm_dp_dsc_sink_supported_input_bpcs);

static int drm_dp_read_lttpr_regs(struct drm_dp_aux *aux,
				  const u8 dpcd[DP_RECEIVER_CAP_SIZE], int address,
				  u8 *buf, int buf_size)
{
	 
	int block_size = dpcd[DP_DPCD_REV] < 0x14 ? 1 : buf_size;
	int offset;
	int ret;

	for (offset = 0; offset < buf_size; offset += block_size) {
		ret = drm_dp_dpcd_read(aux,
				       address + offset,
				       &buf[offset], block_size);
		if (ret < 0)
			return ret;

		WARN_ON(ret != block_size);
	}

	return 0;
}

 
int drm_dp_read_lttpr_common_caps(struct drm_dp_aux *aux,
				  const u8 dpcd[DP_RECEIVER_CAP_SIZE],
				  u8 caps[DP_LTTPR_COMMON_CAP_SIZE])
{
	return drm_dp_read_lttpr_regs(aux, dpcd,
				      DP_LT_TUNABLE_PHY_REPEATER_FIELD_DATA_STRUCTURE_REV,
				      caps, DP_LTTPR_COMMON_CAP_SIZE);
}
EXPORT_SYMBOL(drm_dp_read_lttpr_common_caps);

 
int drm_dp_read_lttpr_phy_caps(struct drm_dp_aux *aux,
			       const u8 dpcd[DP_RECEIVER_CAP_SIZE],
			       enum drm_dp_phy dp_phy,
			       u8 caps[DP_LTTPR_PHY_CAP_SIZE])
{
	return drm_dp_read_lttpr_regs(aux, dpcd,
				      DP_TRAINING_AUX_RD_INTERVAL_PHY_REPEATER(dp_phy),
				      caps, DP_LTTPR_PHY_CAP_SIZE);
}
EXPORT_SYMBOL(drm_dp_read_lttpr_phy_caps);

static u8 dp_lttpr_common_cap(const u8 caps[DP_LTTPR_COMMON_CAP_SIZE], int r)
{
	return caps[r - DP_LT_TUNABLE_PHY_REPEATER_FIELD_DATA_STRUCTURE_REV];
}

 
int drm_dp_lttpr_count(const u8 caps[DP_LTTPR_COMMON_CAP_SIZE])
{
	u8 count = dp_lttpr_common_cap(caps, DP_PHY_REPEATER_CNT);

	switch (hweight8(count)) {
	case 0:
		return 0;
	case 1:
		return 8 - ilog2(count);
	case 8:
		return -ERANGE;
	default:
		return -EINVAL;
	}
}
EXPORT_SYMBOL(drm_dp_lttpr_count);

 
int drm_dp_lttpr_max_link_rate(const u8 caps[DP_LTTPR_COMMON_CAP_SIZE])
{
	u8 rate = dp_lttpr_common_cap(caps, DP_MAX_LINK_RATE_PHY_REPEATER);

	return drm_dp_bw_code_to_link_rate(rate);
}
EXPORT_SYMBOL(drm_dp_lttpr_max_link_rate);

 
int drm_dp_lttpr_max_lane_count(const u8 caps[DP_LTTPR_COMMON_CAP_SIZE])
{
	u8 max_lanes = dp_lttpr_common_cap(caps, DP_MAX_LANE_COUNT_PHY_REPEATER);

	return max_lanes & DP_MAX_LANE_COUNT_MASK;
}
EXPORT_SYMBOL(drm_dp_lttpr_max_lane_count);

 
bool
drm_dp_lttpr_voltage_swing_level_3_supported(const u8 caps[DP_LTTPR_PHY_CAP_SIZE])
{
	u8 txcap = dp_lttpr_phy_cap(caps, DP_TRANSMITTER_CAPABILITY_PHY_REPEATER1);

	return txcap & DP_VOLTAGE_SWING_LEVEL_3_SUPPORTED;
}
EXPORT_SYMBOL(drm_dp_lttpr_voltage_swing_level_3_supported);

 
bool
drm_dp_lttpr_pre_emphasis_level_3_supported(const u8 caps[DP_LTTPR_PHY_CAP_SIZE])
{
	u8 txcap = dp_lttpr_phy_cap(caps, DP_TRANSMITTER_CAPABILITY_PHY_REPEATER1);

	return txcap & DP_PRE_EMPHASIS_LEVEL_3_SUPPORTED;
}
EXPORT_SYMBOL(drm_dp_lttpr_pre_emphasis_level_3_supported);

 
int drm_dp_get_phy_test_pattern(struct drm_dp_aux *aux,
				struct drm_dp_phy_test_params *data)
{
	int err;
	u8 rate, lanes;

	err = drm_dp_dpcd_readb(aux, DP_TEST_LINK_RATE, &rate);
	if (err < 0)
		return err;
	data->link_rate = drm_dp_bw_code_to_link_rate(rate);

	err = drm_dp_dpcd_readb(aux, DP_TEST_LANE_COUNT, &lanes);
	if (err < 0)
		return err;
	data->num_lanes = lanes & DP_MAX_LANE_COUNT_MASK;

	if (lanes & DP_ENHANCED_FRAME_CAP)
		data->enhanced_frame_cap = true;

	err = drm_dp_dpcd_readb(aux, DP_PHY_TEST_PATTERN, &data->phy_pattern);
	if (err < 0)
		return err;

	switch (data->phy_pattern) {
	case DP_PHY_TEST_PATTERN_80BIT_CUSTOM:
		err = drm_dp_dpcd_read(aux, DP_TEST_80BIT_CUSTOM_PATTERN_7_0,
				       &data->custom80, sizeof(data->custom80));
		if (err < 0)
			return err;

		break;
	case DP_PHY_TEST_PATTERN_CP2520:
		err = drm_dp_dpcd_read(aux, DP_TEST_HBR2_SCRAMBLER_RESET,
				       &data->hbr2_reset,
				       sizeof(data->hbr2_reset));
		if (err < 0)
			return err;
	}

	return 0;
}
EXPORT_SYMBOL(drm_dp_get_phy_test_pattern);

 
int drm_dp_set_phy_test_pattern(struct drm_dp_aux *aux,
				struct drm_dp_phy_test_params *data, u8 dp_rev)
{
	int err, i;
	u8 test_pattern;

	test_pattern = data->phy_pattern;
	if (dp_rev < 0x12) {
		test_pattern = (test_pattern << 2) &
			       DP_LINK_QUAL_PATTERN_11_MASK;
		err = drm_dp_dpcd_writeb(aux, DP_TRAINING_PATTERN_SET,
					 test_pattern);
		if (err < 0)
			return err;
	} else {
		for (i = 0; i < data->num_lanes; i++) {
			err = drm_dp_dpcd_writeb(aux,
						 DP_LINK_QUAL_LANE0_SET + i,
						 test_pattern);
			if (err < 0)
				return err;
		}
	}

	return 0;
}
EXPORT_SYMBOL(drm_dp_set_phy_test_pattern);

static const char *dp_pixelformat_get_name(enum dp_pixelformat pixelformat)
{
	if (pixelformat < 0 || pixelformat > DP_PIXELFORMAT_RESERVED)
		return "Invalid";

	switch (pixelformat) {
	case DP_PIXELFORMAT_RGB:
		return "RGB";
	case DP_PIXELFORMAT_YUV444:
		return "YUV444";
	case DP_PIXELFORMAT_YUV422:
		return "YUV422";
	case DP_PIXELFORMAT_YUV420:
		return "YUV420";
	case DP_PIXELFORMAT_Y_ONLY:
		return "Y_ONLY";
	case DP_PIXELFORMAT_RAW:
		return "RAW";
	default:
		return "Reserved";
	}
}

static const char *dp_colorimetry_get_name(enum dp_pixelformat pixelformat,
					   enum dp_colorimetry colorimetry)
{
	if (pixelformat < 0 || pixelformat > DP_PIXELFORMAT_RESERVED)
		return "Invalid";

	switch (colorimetry) {
	case DP_COLORIMETRY_DEFAULT:
		switch (pixelformat) {
		case DP_PIXELFORMAT_RGB:
			return "sRGB";
		case DP_PIXELFORMAT_YUV444:
		case DP_PIXELFORMAT_YUV422:
		case DP_PIXELFORMAT_YUV420:
			return "BT.601";
		case DP_PIXELFORMAT_Y_ONLY:
			return "DICOM PS3.14";
		case DP_PIXELFORMAT_RAW:
			return "Custom Color Profile";
		default:
			return "Reserved";
		}
	case DP_COLORIMETRY_RGB_WIDE_FIXED:  
		switch (pixelformat) {
		case DP_PIXELFORMAT_RGB:
			return "Wide Fixed";
		case DP_PIXELFORMAT_YUV444:
		case DP_PIXELFORMAT_YUV422:
		case DP_PIXELFORMAT_YUV420:
			return "BT.709";
		default:
			return "Reserved";
		}
	case DP_COLORIMETRY_RGB_WIDE_FLOAT:  
		switch (pixelformat) {
		case DP_PIXELFORMAT_RGB:
			return "Wide Float";
		case DP_PIXELFORMAT_YUV444:
		case DP_PIXELFORMAT_YUV422:
		case DP_PIXELFORMAT_YUV420:
			return "xvYCC 601";
		default:
			return "Reserved";
		}
	case DP_COLORIMETRY_OPRGB:  
		switch (pixelformat) {
		case DP_PIXELFORMAT_RGB:
			return "OpRGB";
		case DP_PIXELFORMAT_YUV444:
		case DP_PIXELFORMAT_YUV422:
		case DP_PIXELFORMAT_YUV420:
			return "xvYCC 709";
		default:
			return "Reserved";
		}
	case DP_COLORIMETRY_DCI_P3_RGB:  
		switch (pixelformat) {
		case DP_PIXELFORMAT_RGB:
			return "DCI-P3";
		case DP_PIXELFORMAT_YUV444:
		case DP_PIXELFORMAT_YUV422:
		case DP_PIXELFORMAT_YUV420:
			return "sYCC 601";
		default:
			return "Reserved";
		}
	case DP_COLORIMETRY_RGB_CUSTOM:  
		switch (pixelformat) {
		case DP_PIXELFORMAT_RGB:
			return "Custom Profile";
		case DP_PIXELFORMAT_YUV444:
		case DP_PIXELFORMAT_YUV422:
		case DP_PIXELFORMAT_YUV420:
			return "OpYCC 601";
		default:
			return "Reserved";
		}
	case DP_COLORIMETRY_BT2020_RGB:  
		switch (pixelformat) {
		case DP_PIXELFORMAT_RGB:
			return "BT.2020 RGB";
		case DP_PIXELFORMAT_YUV444:
		case DP_PIXELFORMAT_YUV422:
		case DP_PIXELFORMAT_YUV420:
			return "BT.2020 CYCC";
		default:
			return "Reserved";
		}
	case DP_COLORIMETRY_BT2020_YCC:
		switch (pixelformat) {
		case DP_PIXELFORMAT_YUV444:
		case DP_PIXELFORMAT_YUV422:
		case DP_PIXELFORMAT_YUV420:
			return "BT.2020 YCC";
		default:
			return "Reserved";
		}
	default:
		return "Invalid";
	}
}

static const char *dp_dynamic_range_get_name(enum dp_dynamic_range dynamic_range)
{
	switch (dynamic_range) {
	case DP_DYNAMIC_RANGE_VESA:
		return "VESA range";
	case DP_DYNAMIC_RANGE_CTA:
		return "CTA range";
	default:
		return "Invalid";
	}
}

static const char *dp_content_type_get_name(enum dp_content_type content_type)
{
	switch (content_type) {
	case DP_CONTENT_TYPE_NOT_DEFINED:
		return "Not defined";
	case DP_CONTENT_TYPE_GRAPHICS:
		return "Graphics";
	case DP_CONTENT_TYPE_PHOTO:
		return "Photo";
	case DP_CONTENT_TYPE_VIDEO:
		return "Video";
	case DP_CONTENT_TYPE_GAME:
		return "Game";
	default:
		return "Reserved";
	}
}

void drm_dp_vsc_sdp_log(const char *level, struct device *dev,
			const struct drm_dp_vsc_sdp *vsc)
{
#define DP_SDP_LOG(fmt, ...) dev_printk(level, dev, fmt, ##__VA_ARGS__)
	DP_SDP_LOG("DP SDP: %s, revision %u, length %u\n", "VSC",
		   vsc->revision, vsc->length);
	DP_SDP_LOG("    pixelformat: %s\n",
		   dp_pixelformat_get_name(vsc->pixelformat));
	DP_SDP_LOG("    colorimetry: %s\n",
		   dp_colorimetry_get_name(vsc->pixelformat, vsc->colorimetry));
	DP_SDP_LOG("    bpc: %u\n", vsc->bpc);
	DP_SDP_LOG("    dynamic range: %s\n",
		   dp_dynamic_range_get_name(vsc->dynamic_range));
	DP_SDP_LOG("    content type: %s\n",
		   dp_content_type_get_name(vsc->content_type));
#undef DP_SDP_LOG
}
EXPORT_SYMBOL(drm_dp_vsc_sdp_log);

 
int drm_dp_get_pcon_max_frl_bw(const u8 dpcd[DP_RECEIVER_CAP_SIZE],
			       const u8 port_cap[4])
{
	int bw;
	u8 buf;

	buf = port_cap[2];
	bw = buf & DP_PCON_MAX_FRL_BW;

	switch (bw) {
	case DP_PCON_MAX_9GBPS:
		return 9;
	case DP_PCON_MAX_18GBPS:
		return 18;
	case DP_PCON_MAX_24GBPS:
		return 24;
	case DP_PCON_MAX_32GBPS:
		return 32;
	case DP_PCON_MAX_40GBPS:
		return 40;
	case DP_PCON_MAX_48GBPS:
		return 48;
	case DP_PCON_MAX_0GBPS:
	default:
		return 0;
	}

	return 0;
}
EXPORT_SYMBOL(drm_dp_get_pcon_max_frl_bw);

 
int drm_dp_pcon_frl_prepare(struct drm_dp_aux *aux, bool enable_frl_ready_hpd)
{
	int ret;
	u8 buf = DP_PCON_ENABLE_SOURCE_CTL_MODE |
		 DP_PCON_ENABLE_LINK_FRL_MODE;

	if (enable_frl_ready_hpd)
		buf |= DP_PCON_ENABLE_HPD_READY;

	ret = drm_dp_dpcd_writeb(aux, DP_PCON_HDMI_LINK_CONFIG_1, buf);

	return ret;
}
EXPORT_SYMBOL(drm_dp_pcon_frl_prepare);

 
bool drm_dp_pcon_is_frl_ready(struct drm_dp_aux *aux)
{
	int ret;
	u8 buf;

	ret = drm_dp_dpcd_readb(aux, DP_PCON_HDMI_TX_LINK_STATUS, &buf);
	if (ret < 0)
		return false;

	if (buf & DP_PCON_FRL_READY)
		return true;

	return false;
}
EXPORT_SYMBOL(drm_dp_pcon_is_frl_ready);

 

int drm_dp_pcon_frl_configure_1(struct drm_dp_aux *aux, int max_frl_gbps,
				u8 frl_mode)
{
	int ret;
	u8 buf;

	ret = drm_dp_dpcd_readb(aux, DP_PCON_HDMI_LINK_CONFIG_1, &buf);
	if (ret < 0)
		return ret;

	if (frl_mode == DP_PCON_ENABLE_CONCURRENT_LINK)
		buf |= DP_PCON_ENABLE_CONCURRENT_LINK;
	else
		buf &= ~DP_PCON_ENABLE_CONCURRENT_LINK;

	switch (max_frl_gbps) {
	case 9:
		buf |=  DP_PCON_ENABLE_MAX_BW_9GBPS;
		break;
	case 18:
		buf |=  DP_PCON_ENABLE_MAX_BW_18GBPS;
		break;
	case 24:
		buf |=  DP_PCON_ENABLE_MAX_BW_24GBPS;
		break;
	case 32:
		buf |=  DP_PCON_ENABLE_MAX_BW_32GBPS;
		break;
	case 40:
		buf |=  DP_PCON_ENABLE_MAX_BW_40GBPS;
		break;
	case 48:
		buf |=  DP_PCON_ENABLE_MAX_BW_48GBPS;
		break;
	case 0:
		buf |=  DP_PCON_ENABLE_MAX_BW_0GBPS;
		break;
	default:
		return -EINVAL;
	}

	ret = drm_dp_dpcd_writeb(aux, DP_PCON_HDMI_LINK_CONFIG_1, buf);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL(drm_dp_pcon_frl_configure_1);

 
int drm_dp_pcon_frl_configure_2(struct drm_dp_aux *aux, int max_frl_mask,
				u8 frl_type)
{
	int ret;
	u8 buf = max_frl_mask;

	if (frl_type == DP_PCON_FRL_LINK_TRAIN_EXTENDED)
		buf |= DP_PCON_FRL_LINK_TRAIN_EXTENDED;
	else
		buf &= ~DP_PCON_FRL_LINK_TRAIN_EXTENDED;

	ret = drm_dp_dpcd_writeb(aux, DP_PCON_HDMI_LINK_CONFIG_2, buf);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL(drm_dp_pcon_frl_configure_2);

 
int drm_dp_pcon_reset_frl_config(struct drm_dp_aux *aux)
{
	int ret;

	ret = drm_dp_dpcd_writeb(aux, DP_PCON_HDMI_LINK_CONFIG_1, 0x0);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL(drm_dp_pcon_reset_frl_config);

 
int drm_dp_pcon_frl_enable(struct drm_dp_aux *aux)
{
	int ret;
	u8 buf = 0;

	ret = drm_dp_dpcd_readb(aux, DP_PCON_HDMI_LINK_CONFIG_1, &buf);
	if (ret < 0)
		return ret;
	if (!(buf & DP_PCON_ENABLE_SOURCE_CTL_MODE)) {
		drm_dbg_kms(aux->drm_dev, "%s: PCON in Autonomous mode, can't enable FRL\n",
			    aux->name);
		return -EINVAL;
	}
	buf |= DP_PCON_ENABLE_HDMI_LINK;
	ret = drm_dp_dpcd_writeb(aux, DP_PCON_HDMI_LINK_CONFIG_1, buf);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL(drm_dp_pcon_frl_enable);

 
bool drm_dp_pcon_hdmi_link_active(struct drm_dp_aux *aux)
{
	u8 buf;
	int ret;

	ret = drm_dp_dpcd_readb(aux, DP_PCON_HDMI_TX_LINK_STATUS, &buf);
	if (ret < 0)
		return false;

	return buf & DP_PCON_HDMI_TX_LINK_ACTIVE;
}
EXPORT_SYMBOL(drm_dp_pcon_hdmi_link_active);

 
int drm_dp_pcon_hdmi_link_mode(struct drm_dp_aux *aux, u8 *frl_trained_mask)
{
	u8 buf;
	int mode;
	int ret;

	ret = drm_dp_dpcd_readb(aux, DP_PCON_HDMI_POST_FRL_STATUS, &buf);
	if (ret < 0)
		return ret;

	mode = buf & DP_PCON_HDMI_LINK_MODE;

	if (frl_trained_mask && DP_PCON_HDMI_MODE_FRL == mode)
		*frl_trained_mask = (buf & DP_PCON_HDMI_FRL_TRAINED_BW) >> 1;

	return mode;
}
EXPORT_SYMBOL(drm_dp_pcon_hdmi_link_mode);

 

void drm_dp_pcon_hdmi_frl_link_error_count(struct drm_dp_aux *aux,
					   struct drm_connector *connector)
{
	u8 buf, error_count;
	int i, num_error;
	struct drm_hdmi_info *hdmi = &connector->display_info.hdmi;

	for (i = 0; i < hdmi->max_lanes; i++) {
		if (drm_dp_dpcd_readb(aux, DP_PCON_HDMI_ERROR_STATUS_LN0 + i, &buf) < 0)
			return;

		error_count = buf & DP_PCON_HDMI_ERROR_COUNT_MASK;
		switch (error_count) {
		case DP_PCON_HDMI_ERROR_COUNT_HUNDRED_PLUS:
			num_error = 100;
			break;
		case DP_PCON_HDMI_ERROR_COUNT_TEN_PLUS:
			num_error = 10;
			break;
		case DP_PCON_HDMI_ERROR_COUNT_THREE_PLUS:
			num_error = 3;
			break;
		default:
			num_error = 0;
		}

		drm_err(aux->drm_dev, "%s: More than %d errors since the last read for lane %d",
			aux->name, num_error, i);
	}
}
EXPORT_SYMBOL(drm_dp_pcon_hdmi_frl_link_error_count);

 
bool drm_dp_pcon_enc_is_dsc_1_2(const u8 pcon_dsc_dpcd[DP_PCON_DSC_ENCODER_CAP_SIZE])
{
	u8 buf;
	u8 major_v, minor_v;

	buf = pcon_dsc_dpcd[DP_PCON_DSC_VERSION - DP_PCON_DSC_ENCODER];
	major_v = (buf & DP_PCON_DSC_MAJOR_MASK) >> DP_PCON_DSC_MAJOR_SHIFT;
	minor_v = (buf & DP_PCON_DSC_MINOR_MASK) >> DP_PCON_DSC_MINOR_SHIFT;

	if (major_v == 1 && minor_v == 2)
		return true;

	return false;
}
EXPORT_SYMBOL(drm_dp_pcon_enc_is_dsc_1_2);

 
int drm_dp_pcon_dsc_max_slices(const u8 pcon_dsc_dpcd[DP_PCON_DSC_ENCODER_CAP_SIZE])
{
	u8 slice_cap1, slice_cap2;

	slice_cap1 = pcon_dsc_dpcd[DP_PCON_DSC_SLICE_CAP_1 - DP_PCON_DSC_ENCODER];
	slice_cap2 = pcon_dsc_dpcd[DP_PCON_DSC_SLICE_CAP_2 - DP_PCON_DSC_ENCODER];

	if (slice_cap2 & DP_PCON_DSC_24_PER_DSC_ENC)
		return 24;
	if (slice_cap2 & DP_PCON_DSC_20_PER_DSC_ENC)
		return 20;
	if (slice_cap2 & DP_PCON_DSC_16_PER_DSC_ENC)
		return 16;
	if (slice_cap1 & DP_PCON_DSC_12_PER_DSC_ENC)
		return 12;
	if (slice_cap1 & DP_PCON_DSC_10_PER_DSC_ENC)
		return 10;
	if (slice_cap1 & DP_PCON_DSC_8_PER_DSC_ENC)
		return 8;
	if (slice_cap1 & DP_PCON_DSC_6_PER_DSC_ENC)
		return 6;
	if (slice_cap1 & DP_PCON_DSC_4_PER_DSC_ENC)
		return 4;
	if (slice_cap1 & DP_PCON_DSC_2_PER_DSC_ENC)
		return 2;
	if (slice_cap1 & DP_PCON_DSC_1_PER_DSC_ENC)
		return 1;

	return 0;
}
EXPORT_SYMBOL(drm_dp_pcon_dsc_max_slices);

 
int drm_dp_pcon_dsc_max_slice_width(const u8 pcon_dsc_dpcd[DP_PCON_DSC_ENCODER_CAP_SIZE])
{
	u8 buf;

	buf = pcon_dsc_dpcd[DP_PCON_DSC_MAX_SLICE_WIDTH - DP_PCON_DSC_ENCODER];

	return buf * DP_DSC_SLICE_WIDTH_MULTIPLIER;
}
EXPORT_SYMBOL(drm_dp_pcon_dsc_max_slice_width);

 
int drm_dp_pcon_dsc_bpp_incr(const u8 pcon_dsc_dpcd[DP_PCON_DSC_ENCODER_CAP_SIZE])
{
	u8 buf;

	buf = pcon_dsc_dpcd[DP_PCON_DSC_BPP_INCR - DP_PCON_DSC_ENCODER];

	switch (buf & DP_PCON_DSC_BPP_INCR_MASK) {
	case DP_PCON_DSC_ONE_16TH_BPP:
		return 16;
	case DP_PCON_DSC_ONE_8TH_BPP:
		return 8;
	case DP_PCON_DSC_ONE_4TH_BPP:
		return 4;
	case DP_PCON_DSC_ONE_HALF_BPP:
		return 2;
	case DP_PCON_DSC_ONE_BPP:
		return 1;
	}

	return 0;
}
EXPORT_SYMBOL(drm_dp_pcon_dsc_bpp_incr);

static
int drm_dp_pcon_configure_dsc_enc(struct drm_dp_aux *aux, u8 pps_buf_config)
{
	u8 buf;
	int ret;

	ret = drm_dp_dpcd_readb(aux, DP_PROTOCOL_CONVERTER_CONTROL_2, &buf);
	if (ret < 0)
		return ret;

	buf |= DP_PCON_ENABLE_DSC_ENCODER;

	if (pps_buf_config <= DP_PCON_ENC_PPS_OVERRIDE_EN_BUFFER) {
		buf &= ~DP_PCON_ENCODER_PPS_OVERRIDE_MASK;
		buf |= pps_buf_config << 2;
	}

	ret = drm_dp_dpcd_writeb(aux, DP_PROTOCOL_CONVERTER_CONTROL_2, buf);
	if (ret < 0)
		return ret;

	return 0;
}

 
int drm_dp_pcon_pps_default(struct drm_dp_aux *aux)
{
	int ret;

	ret = drm_dp_pcon_configure_dsc_enc(aux, DP_PCON_ENC_PPS_OVERRIDE_DISABLED);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL(drm_dp_pcon_pps_default);

 
int drm_dp_pcon_pps_override_buf(struct drm_dp_aux *aux, u8 pps_buf[128])
{
	int ret;

	ret = drm_dp_dpcd_write(aux, DP_PCON_HDMI_PPS_OVERRIDE_BASE, &pps_buf, 128);
	if (ret < 0)
		return ret;

	ret = drm_dp_pcon_configure_dsc_enc(aux, DP_PCON_ENC_PPS_OVERRIDE_EN_BUFFER);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL(drm_dp_pcon_pps_override_buf);

 
int drm_dp_pcon_pps_override_param(struct drm_dp_aux *aux, u8 pps_param[6])
{
	int ret;

	ret = drm_dp_dpcd_write(aux, DP_PCON_HDMI_PPS_OVRD_SLICE_HEIGHT, &pps_param[0], 2);
	if (ret < 0)
		return ret;
	ret = drm_dp_dpcd_write(aux, DP_PCON_HDMI_PPS_OVRD_SLICE_WIDTH, &pps_param[2], 2);
	if (ret < 0)
		return ret;
	ret = drm_dp_dpcd_write(aux, DP_PCON_HDMI_PPS_OVRD_BPP, &pps_param[4], 2);
	if (ret < 0)
		return ret;

	ret = drm_dp_pcon_configure_dsc_enc(aux, DP_PCON_ENC_PPS_OVERRIDE_EN_BUFFER);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL(drm_dp_pcon_pps_override_param);

 
int drm_dp_pcon_convert_rgb_to_ycbcr(struct drm_dp_aux *aux, u8 color_spc)
{
	int ret;
	u8 buf;

	ret = drm_dp_dpcd_readb(aux, DP_PROTOCOL_CONVERTER_CONTROL_2, &buf);
	if (ret < 0)
		return ret;

	if (color_spc & DP_CONVERSION_RGB_YCBCR_MASK)
		buf |= (color_spc & DP_CONVERSION_RGB_YCBCR_MASK);
	else
		buf &= ~DP_CONVERSION_RGB_YCBCR_MASK;

	ret = drm_dp_dpcd_writeb(aux, DP_PROTOCOL_CONVERTER_CONTROL_2, buf);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL(drm_dp_pcon_convert_rgb_to_ycbcr);

 
int drm_edp_backlight_set_level(struct drm_dp_aux *aux, const struct drm_edp_backlight_info *bl,
				u16 level)
{
	int ret;
	u8 buf[2] = { 0 };

	 
	if (!bl->aux_set)
		return 0;

	if (bl->lsb_reg_used) {
		buf[0] = (level & 0xff00) >> 8;
		buf[1] = (level & 0x00ff);
	} else {
		buf[0] = level;
	}

	ret = drm_dp_dpcd_write(aux, DP_EDP_BACKLIGHT_BRIGHTNESS_MSB, buf, sizeof(buf));
	if (ret != sizeof(buf)) {
		drm_err(aux->drm_dev,
			"%s: Failed to write aux backlight level: %d\n",
			aux->name, ret);
		return ret < 0 ? ret : -EIO;
	}

	return 0;
}
EXPORT_SYMBOL(drm_edp_backlight_set_level);

static int
drm_edp_backlight_set_enable(struct drm_dp_aux *aux, const struct drm_edp_backlight_info *bl,
			     bool enable)
{
	int ret;
	u8 buf;

	 
	if (!bl->aux_enable)
		return 0;

	ret = drm_dp_dpcd_readb(aux, DP_EDP_DISPLAY_CONTROL_REGISTER, &buf);
	if (ret != 1) {
		drm_err(aux->drm_dev, "%s: Failed to read eDP display control register: %d\n",
			aux->name, ret);
		return ret < 0 ? ret : -EIO;
	}
	if (enable)
		buf |= DP_EDP_BACKLIGHT_ENABLE;
	else
		buf &= ~DP_EDP_BACKLIGHT_ENABLE;

	ret = drm_dp_dpcd_writeb(aux, DP_EDP_DISPLAY_CONTROL_REGISTER, buf);
	if (ret != 1) {
		drm_err(aux->drm_dev, "%s: Failed to write eDP display control register: %d\n",
			aux->name, ret);
		return ret < 0 ? ret : -EIO;
	}

	return 0;
}

 
int drm_edp_backlight_enable(struct drm_dp_aux *aux, const struct drm_edp_backlight_info *bl,
			     const u16 level)
{
	int ret;
	u8 dpcd_buf;

	if (bl->aux_set)
		dpcd_buf = DP_EDP_BACKLIGHT_CONTROL_MODE_DPCD;
	else
		dpcd_buf = DP_EDP_BACKLIGHT_CONTROL_MODE_PWM;

	if (bl->pwmgen_bit_count) {
		ret = drm_dp_dpcd_writeb(aux, DP_EDP_PWMGEN_BIT_COUNT, bl->pwmgen_bit_count);
		if (ret != 1)
			drm_dbg_kms(aux->drm_dev, "%s: Failed to write aux pwmgen bit count: %d\n",
				    aux->name, ret);
	}

	if (bl->pwm_freq_pre_divider) {
		ret = drm_dp_dpcd_writeb(aux, DP_EDP_BACKLIGHT_FREQ_SET, bl->pwm_freq_pre_divider);
		if (ret != 1)
			drm_dbg_kms(aux->drm_dev,
				    "%s: Failed to write aux backlight frequency: %d\n",
				    aux->name, ret);
		else
			dpcd_buf |= DP_EDP_BACKLIGHT_FREQ_AUX_SET_ENABLE;
	}

	ret = drm_dp_dpcd_writeb(aux, DP_EDP_BACKLIGHT_MODE_SET_REGISTER, dpcd_buf);
	if (ret != 1) {
		drm_dbg_kms(aux->drm_dev, "%s: Failed to write aux backlight mode: %d\n",
			    aux->name, ret);
		return ret < 0 ? ret : -EIO;
	}

	ret = drm_edp_backlight_set_level(aux, bl, level);
	if (ret < 0)
		return ret;
	ret = drm_edp_backlight_set_enable(aux, bl, true);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL(drm_edp_backlight_enable);

 
int drm_edp_backlight_disable(struct drm_dp_aux *aux, const struct drm_edp_backlight_info *bl)
{
	int ret;

	ret = drm_edp_backlight_set_enable(aux, bl, false);
	if (ret < 0)
		return ret;

	return 0;
}
EXPORT_SYMBOL(drm_edp_backlight_disable);

static inline int
drm_edp_backlight_probe_max(struct drm_dp_aux *aux, struct drm_edp_backlight_info *bl,
			    u16 driver_pwm_freq_hz, const u8 edp_dpcd[EDP_DISPLAY_CTL_CAP_SIZE])
{
	int fxp, fxp_min, fxp_max, fxp_actual, f = 1;
	int ret;
	u8 pn, pn_min, pn_max;

	if (!bl->aux_set)
		return 0;

	ret = drm_dp_dpcd_readb(aux, DP_EDP_PWMGEN_BIT_COUNT, &pn);
	if (ret != 1) {
		drm_dbg_kms(aux->drm_dev, "%s: Failed to read pwmgen bit count cap: %d\n",
			    aux->name, ret);
		return -ENODEV;
	}

	pn &= DP_EDP_PWMGEN_BIT_COUNT_MASK;
	bl->max = (1 << pn) - 1;
	if (!driver_pwm_freq_hz)
		return 0;

	 

	 
	fxp = DIV_ROUND_CLOSEST(1000 * DP_EDP_BACKLIGHT_FREQ_BASE_KHZ, driver_pwm_freq_hz);

	 
	ret = drm_dp_dpcd_readb(aux, DP_EDP_PWMGEN_BIT_COUNT_CAP_MIN, &pn_min);
	if (ret != 1) {
		drm_dbg_kms(aux->drm_dev, "%s: Failed to read pwmgen bit count cap min: %d\n",
			    aux->name, ret);
		return 0;
	}
	ret = drm_dp_dpcd_readb(aux, DP_EDP_PWMGEN_BIT_COUNT_CAP_MAX, &pn_max);
	if (ret != 1) {
		drm_dbg_kms(aux->drm_dev, "%s: Failed to read pwmgen bit count cap max: %d\n",
			    aux->name, ret);
		return 0;
	}
	pn_min &= DP_EDP_PWMGEN_BIT_COUNT_MASK;
	pn_max &= DP_EDP_PWMGEN_BIT_COUNT_MASK;

	 
	fxp_min = DIV_ROUND_CLOSEST(fxp * 3, 4);
	fxp_max = DIV_ROUND_CLOSEST(fxp * 5, 4);
	if (fxp_min < (1 << pn_min) || (255 << pn_max) < fxp_max) {
		drm_dbg_kms(aux->drm_dev,
			    "%s: Driver defined backlight frequency (%d) out of range\n",
			    aux->name, driver_pwm_freq_hz);
		return 0;
	}

	for (pn = pn_max; pn >= pn_min; pn--) {
		f = clamp(DIV_ROUND_CLOSEST(fxp, 1 << pn), 1, 255);
		fxp_actual = f << pn;
		if (fxp_min <= fxp_actual && fxp_actual <= fxp_max)
			break;
	}

	ret = drm_dp_dpcd_writeb(aux, DP_EDP_PWMGEN_BIT_COUNT, pn);
	if (ret != 1) {
		drm_dbg_kms(aux->drm_dev, "%s: Failed to write aux pwmgen bit count: %d\n",
			    aux->name, ret);
		return 0;
	}
	bl->pwmgen_bit_count = pn;
	bl->max = (1 << pn) - 1;

	if (edp_dpcd[2] & DP_EDP_BACKLIGHT_FREQ_AUX_SET_CAP) {
		bl->pwm_freq_pre_divider = f;
		drm_dbg_kms(aux->drm_dev, "%s: Using backlight frequency from driver (%dHz)\n",
			    aux->name, driver_pwm_freq_hz);
	}

	return 0;
}

static inline int
drm_edp_backlight_probe_state(struct drm_dp_aux *aux, struct drm_edp_backlight_info *bl,
			      u8 *current_mode)
{
	int ret;
	u8 buf[2];
	u8 mode_reg;

	ret = drm_dp_dpcd_readb(aux, DP_EDP_BACKLIGHT_MODE_SET_REGISTER, &mode_reg);
	if (ret != 1) {
		drm_dbg_kms(aux->drm_dev, "%s: Failed to read backlight mode: %d\n",
			    aux->name, ret);
		return ret < 0 ? ret : -EIO;
	}

	*current_mode = (mode_reg & DP_EDP_BACKLIGHT_CONTROL_MODE_MASK);
	if (!bl->aux_set)
		return 0;

	if (*current_mode == DP_EDP_BACKLIGHT_CONTROL_MODE_DPCD) {
		int size = 1 + bl->lsb_reg_used;

		ret = drm_dp_dpcd_read(aux, DP_EDP_BACKLIGHT_BRIGHTNESS_MSB, buf, size);
		if (ret != size) {
			drm_dbg_kms(aux->drm_dev, "%s: Failed to read backlight level: %d\n",
				    aux->name, ret);
			return ret < 0 ? ret : -EIO;
		}

		if (bl->lsb_reg_used)
			return (buf[0] << 8) | buf[1];
		else
			return buf[0];
	}

	 
	return bl->max;
}

 
int
drm_edp_backlight_init(struct drm_dp_aux *aux, struct drm_edp_backlight_info *bl,
		       u16 driver_pwm_freq_hz, const u8 edp_dpcd[EDP_DISPLAY_CTL_CAP_SIZE],
		       u16 *current_level, u8 *current_mode)
{
	int ret;

	if (edp_dpcd[1] & DP_EDP_BACKLIGHT_AUX_ENABLE_CAP)
		bl->aux_enable = true;
	if (edp_dpcd[2] & DP_EDP_BACKLIGHT_BRIGHTNESS_AUX_SET_CAP)
		bl->aux_set = true;
	if (edp_dpcd[2] & DP_EDP_BACKLIGHT_BRIGHTNESS_BYTE_COUNT)
		bl->lsb_reg_used = true;

	 
	if (!bl->aux_set && !(edp_dpcd[2] & DP_EDP_BACKLIGHT_BRIGHTNESS_PWM_PIN_CAP)) {
		drm_dbg_kms(aux->drm_dev,
			    "%s: Panel supports neither AUX or PWM brightness control? Aborting\n",
			    aux->name);
		return -EINVAL;
	}

	ret = drm_edp_backlight_probe_max(aux, bl, driver_pwm_freq_hz, edp_dpcd);
	if (ret < 0)
		return ret;

	ret = drm_edp_backlight_probe_state(aux, bl, current_mode);
	if (ret < 0)
		return ret;
	*current_level = ret;

	drm_dbg_kms(aux->drm_dev,
		    "%s: Found backlight: aux_set=%d aux_enable=%d mode=%d\n",
		    aux->name, bl->aux_set, bl->aux_enable, *current_mode);
	if (bl->aux_set) {
		drm_dbg_kms(aux->drm_dev,
			    "%s: Backlight caps: level=%d/%d pwm_freq_pre_divider=%d lsb_reg_used=%d\n",
			    aux->name, *current_level, bl->max, bl->pwm_freq_pre_divider,
			    bl->lsb_reg_used);
	}

	return 0;
}
EXPORT_SYMBOL(drm_edp_backlight_init);

#if IS_BUILTIN(CONFIG_BACKLIGHT_CLASS_DEVICE) || \
	(IS_MODULE(CONFIG_DRM_KMS_HELPER) && IS_MODULE(CONFIG_BACKLIGHT_CLASS_DEVICE))

static int dp_aux_backlight_update_status(struct backlight_device *bd)
{
	struct dp_aux_backlight *bl = bl_get_data(bd);
	u16 brightness = backlight_get_brightness(bd);
	int ret = 0;

	if (!backlight_is_blank(bd)) {
		if (!bl->enabled) {
			drm_edp_backlight_enable(bl->aux, &bl->info, brightness);
			bl->enabled = true;
			return 0;
		}
		ret = drm_edp_backlight_set_level(bl->aux, &bl->info, brightness);
	} else {
		if (bl->enabled) {
			drm_edp_backlight_disable(bl->aux, &bl->info);
			bl->enabled = false;
		}
	}

	return ret;
}

static const struct backlight_ops dp_aux_bl_ops = {
	.update_status = dp_aux_backlight_update_status,
};

 
int drm_panel_dp_aux_backlight(struct drm_panel *panel, struct drm_dp_aux *aux)
{
	struct dp_aux_backlight *bl;
	struct backlight_properties props = { 0 };
	u16 current_level;
	u8 current_mode;
	u8 edp_dpcd[EDP_DISPLAY_CTL_CAP_SIZE];
	int ret;

	if (!panel || !panel->dev || !aux)
		return -EINVAL;

	ret = drm_dp_dpcd_read(aux, DP_EDP_DPCD_REV, edp_dpcd,
			       EDP_DISPLAY_CTL_CAP_SIZE);
	if (ret < 0)
		return ret;

	if (!drm_edp_backlight_supported(edp_dpcd)) {
		DRM_DEV_INFO(panel->dev, "DP AUX backlight is not supported\n");
		return 0;
	}

	bl = devm_kzalloc(panel->dev, sizeof(*bl), GFP_KERNEL);
	if (!bl)
		return -ENOMEM;

	bl->aux = aux;

	ret = drm_edp_backlight_init(aux, &bl->info, 0, edp_dpcd,
				     &current_level, &current_mode);
	if (ret < 0)
		return ret;

	props.type = BACKLIGHT_RAW;
	props.brightness = current_level;
	props.max_brightness = bl->info.max;

	bl->base = devm_backlight_device_register(panel->dev, "dp_aux_backlight",
						  panel->dev, bl,
						  &dp_aux_bl_ops, &props);
	if (IS_ERR(bl->base))
		return PTR_ERR(bl->base);

	backlight_disable(bl->base);

	panel->backlight = bl->base;

	return 0;
}
EXPORT_SYMBOL(drm_panel_dp_aux_backlight);

#endif
