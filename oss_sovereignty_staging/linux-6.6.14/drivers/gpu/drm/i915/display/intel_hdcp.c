 
 

#include <linux/component.h>
#include <linux/i2c.h>
#include <linux/random.h>

#include <drm/display/drm_hdcp_helper.h>
#include <drm/i915_component.h>

#include "i915_drv.h"
#include "i915_reg.h"
#include "intel_connector.h"
#include "intel_de.h"
#include "intel_display_power.h"
#include "intel_display_power_well.h"
#include "intel_display_types.h"
#include "intel_hdcp.h"
#include "intel_hdcp_gsc.h"
#include "intel_hdcp_regs.h"
#include "intel_pcode.h"

#define KEY_LOAD_TRIES	5
#define HDCP2_LC_RETRY_CNT			3

static int intel_conn_to_vcpi(struct drm_atomic_state *state,
			      struct intel_connector *connector)
{
	struct drm_dp_mst_topology_mgr *mgr;
	struct drm_dp_mst_atomic_payload *payload;
	struct drm_dp_mst_topology_state *mst_state;
	int vcpi = 0;

	 
	if (!connector->port)
		return 0;
	mgr = connector->port->mgr;

	drm_modeset_lock(&mgr->base.lock, state->acquire_ctx);
	mst_state = to_drm_dp_mst_topology_state(mgr->base.state);
	payload = drm_atomic_get_mst_payload_state(mst_state, connector->port);
	if (drm_WARN_ON(mgr->dev, !payload))
		goto out;

	vcpi = payload->vcpi;
	if (drm_WARN_ON(mgr->dev, vcpi < 0)) {
		vcpi = 0;
		goto out;
	}
out:
	return vcpi;
}

 
static void
intel_hdcp_required_content_stream(struct intel_digital_port *dig_port)
{
	struct hdcp_port_data *data = &dig_port->hdcp_port_data;
	bool enforce_type0 = false;
	int k;

	if (dig_port->hdcp_auth_status)
		return;

	if (!dig_port->hdcp_mst_type1_capable)
		enforce_type0 = true;

	 
	for (k = 0; k < data->k; k++)
		data->streams[k].stream_type =
			enforce_type0 ? DRM_MODE_HDCP_CONTENT_TYPE0 : DRM_MODE_HDCP_CONTENT_TYPE1;
}

static void intel_hdcp_prepare_streams(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct hdcp_port_data *data = &dig_port->hdcp_port_data;
	struct intel_hdcp *hdcp = &connector->hdcp;

	if (!intel_encoder_is_mst(intel_attached_encoder(connector))) {
		data->streams[0].stream_type = hdcp->content_type;
	} else {
		intel_hdcp_required_content_stream(dig_port);
	}
}

static
bool intel_hdcp_is_ksv_valid(u8 *ksv)
{
	int i, ones = 0;
	 
	for (i = 0; i < DRM_HDCP_KSV_LEN; i++)
		ones += hweight8(ksv[i]);
	if (ones != 20)
		return false;

	return true;
}

static
int intel_hdcp_read_valid_bksv(struct intel_digital_port *dig_port,
			       const struct intel_hdcp_shim *shim, u8 *bksv)
{
	struct drm_i915_private *i915 = to_i915(dig_port->base.base.dev);
	int ret, i, tries = 2;

	 
	for (i = 0; i < tries; i++) {
		ret = shim->read_bksv(dig_port, bksv);
		if (ret)
			return ret;
		if (intel_hdcp_is_ksv_valid(bksv))
			break;
	}
	if (i == tries) {
		drm_dbg_kms(&i915->drm, "Bksv is invalid\n");
		return -ENODEV;
	}

	return 0;
}

 
bool intel_hdcp_capable(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	const struct intel_hdcp_shim *shim = connector->hdcp.shim;
	bool capable = false;
	u8 bksv[5];

	if (!shim)
		return capable;

	if (shim->hdcp_capable) {
		shim->hdcp_capable(dig_port, &capable);
	} else {
		if (!intel_hdcp_read_valid_bksv(dig_port, shim, bksv))
			capable = true;
	}

	return capable;
}

 
bool intel_hdcp2_capable(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct intel_hdcp *hdcp = &connector->hdcp;
	bool capable = false;

	 
	if (!hdcp->hdcp2_supported)
		return false;

	 
	if (intel_hdcp_gsc_cs_required(i915)) {
		struct intel_gt *gt = i915->media_gt;
		struct intel_gsc_uc *gsc = gt ? &gt->uc.gsc : NULL;

		if (!gsc || !intel_uc_fw_is_running(&gsc->fw)) {
			drm_dbg_kms(&i915->drm,
				    "GSC components required for HDCP2.2 are not ready\n");
			return false;
		}
	}

	 
	mutex_lock(&i915->display.hdcp.hdcp_mutex);
	if (!i915->display.hdcp.comp_added ||  !i915->display.hdcp.arbiter) {
		mutex_unlock(&i915->display.hdcp.hdcp_mutex);
		return false;
	}
	mutex_unlock(&i915->display.hdcp.hdcp_mutex);

	 
	hdcp->shim->hdcp_2_2_capable(dig_port, &capable);

	return capable;
}

static bool intel_hdcp_in_use(struct drm_i915_private *i915,
			      enum transcoder cpu_transcoder, enum port port)
{
	return intel_de_read(i915,
			     HDCP_STATUS(i915, cpu_transcoder, port)) &
		HDCP_STATUS_ENC;
}

static bool intel_hdcp2_in_use(struct drm_i915_private *i915,
			       enum transcoder cpu_transcoder, enum port port)
{
	return intel_de_read(i915,
			     HDCP2_STATUS(i915, cpu_transcoder, port)) &
		LINK_ENCRYPTION_STATUS;
}

static int intel_hdcp_poll_ksv_fifo(struct intel_digital_port *dig_port,
				    const struct intel_hdcp_shim *shim)
{
	int ret, read_ret;
	bool ksv_ready;

	 
	ret = __wait_for(read_ret = shim->read_ksv_ready(dig_port,
							 &ksv_ready),
			 read_ret || ksv_ready, 5 * 1000 * 1000, 1000,
			 100 * 1000);
	if (ret)
		return ret;
	if (read_ret)
		return read_ret;
	if (!ksv_ready)
		return -ETIMEDOUT;

	return 0;
}

static bool hdcp_key_loadable(struct drm_i915_private *i915)
{
	enum i915_power_well_id id;
	intel_wakeref_t wakeref;
	bool enabled = false;

	 
	if (IS_HASWELL(i915) || IS_BROADWELL(i915))
		id = HSW_DISP_PW_GLOBAL;
	else
		id = SKL_DISP_PW_1;

	 
	with_intel_runtime_pm(&i915->runtime_pm, wakeref)
		enabled = intel_display_power_well_is_enabled(i915, id);

	 

	return enabled;
}

static void intel_hdcp_clear_keys(struct drm_i915_private *i915)
{
	intel_de_write(i915, HDCP_KEY_CONF, HDCP_CLEAR_KEYS_TRIGGER);
	intel_de_write(i915, HDCP_KEY_STATUS,
		       HDCP_KEY_LOAD_DONE | HDCP_KEY_LOAD_STATUS | HDCP_FUSE_IN_PROGRESS | HDCP_FUSE_ERROR | HDCP_FUSE_DONE);
}

static int intel_hdcp_load_keys(struct drm_i915_private *i915)
{
	int ret;
	u32 val;

	val = intel_de_read(i915, HDCP_KEY_STATUS);
	if ((val & HDCP_KEY_LOAD_DONE) && (val & HDCP_KEY_LOAD_STATUS))
		return 0;

	 
	if (IS_HASWELL(i915) || IS_BROADWELL(i915))
		if (!(intel_de_read(i915, HDCP_KEY_STATUS) & HDCP_KEY_LOAD_DONE))
			return -ENXIO;

	 
	if (DISPLAY_VER(i915) == 9 && !IS_BROXTON(i915)) {
		ret = snb_pcode_write(&i915->uncore, SKL_PCODE_LOAD_HDCP_KEYS, 1);
		if (ret) {
			drm_err(&i915->drm,
				"Failed to initiate HDCP key load (%d)\n",
				ret);
			return ret;
		}
	} else {
		intel_de_write(i915, HDCP_KEY_CONF, HDCP_KEY_LOAD_TRIGGER);
	}

	 
	ret = __intel_wait_for_register(&i915->uncore, HDCP_KEY_STATUS,
					HDCP_KEY_LOAD_DONE, HDCP_KEY_LOAD_DONE,
					10, 1, &val);
	if (ret)
		return ret;
	else if (!(val & HDCP_KEY_LOAD_STATUS))
		return -ENXIO;

	 
	intel_de_write(i915, HDCP_KEY_CONF, HDCP_AKSV_SEND_TRIGGER);

	return 0;
}

 
static int intel_write_sha_text(struct drm_i915_private *i915, u32 sha_text)
{
	intel_de_write(i915, HDCP_SHA_TEXT, sha_text);
	if (intel_de_wait_for_set(i915, HDCP_REP_CTL, HDCP_SHA1_READY, 1)) {
		drm_err(&i915->drm, "Timed out waiting for SHA1 ready\n");
		return -ETIMEDOUT;
	}
	return 0;
}

static
u32 intel_hdcp_get_repeater_ctl(struct drm_i915_private *i915,
				enum transcoder cpu_transcoder, enum port port)
{
	if (DISPLAY_VER(i915) >= 12) {
		switch (cpu_transcoder) {
		case TRANSCODER_A:
			return HDCP_TRANSA_REP_PRESENT |
			       HDCP_TRANSA_SHA1_M0;
		case TRANSCODER_B:
			return HDCP_TRANSB_REP_PRESENT |
			       HDCP_TRANSB_SHA1_M0;
		case TRANSCODER_C:
			return HDCP_TRANSC_REP_PRESENT |
			       HDCP_TRANSC_SHA1_M0;
		case TRANSCODER_D:
			return HDCP_TRANSD_REP_PRESENT |
			       HDCP_TRANSD_SHA1_M0;
		default:
			drm_err(&i915->drm, "Unknown transcoder %d\n",
				cpu_transcoder);
			return -EINVAL;
		}
	}

	switch (port) {
	case PORT_A:
		return HDCP_DDIA_REP_PRESENT | HDCP_DDIA_SHA1_M0;
	case PORT_B:
		return HDCP_DDIB_REP_PRESENT | HDCP_DDIB_SHA1_M0;
	case PORT_C:
		return HDCP_DDIC_REP_PRESENT | HDCP_DDIC_SHA1_M0;
	case PORT_D:
		return HDCP_DDID_REP_PRESENT | HDCP_DDID_SHA1_M0;
	case PORT_E:
		return HDCP_DDIE_REP_PRESENT | HDCP_DDIE_SHA1_M0;
	default:
		drm_err(&i915->drm, "Unknown port %d\n", port);
		return -EINVAL;
	}
}

static
int intel_hdcp_validate_v_prime(struct intel_connector *connector,
				const struct intel_hdcp_shim *shim,
				u8 *ksv_fifo, u8 num_downstream, u8 *bstatus)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	enum transcoder cpu_transcoder = connector->hdcp.cpu_transcoder;
	enum port port = dig_port->base.port;
	u32 vprime, sha_text, sha_leftovers, rep_ctl;
	int ret, i, j, sha_idx;

	 
	for (i = 0; i < DRM_HDCP_V_PRIME_NUM_PARTS; i++) {
		ret = shim->read_v_prime_part(dig_port, i, &vprime);
		if (ret)
			return ret;
		intel_de_write(i915, HDCP_SHA_V_PRIME(i), vprime);
	}

	 
	sha_idx = 0;
	sha_text = 0;
	sha_leftovers = 0;
	rep_ctl = intel_hdcp_get_repeater_ctl(i915, cpu_transcoder, port);
	intel_de_write(i915, HDCP_REP_CTL, rep_ctl | HDCP_SHA1_TEXT_32);
	for (i = 0; i < num_downstream; i++) {
		unsigned int sha_empty;
		u8 *ksv = &ksv_fifo[i * DRM_HDCP_KSV_LEN];

		 
		sha_empty = sizeof(sha_text) - sha_leftovers;
		for (j = 0; j < sha_empty; j++) {
			u8 off = ((sizeof(sha_text) - j - 1 - sha_leftovers) * 8);
			sha_text |= ksv[j] << off;
		}

		ret = intel_write_sha_text(i915, sha_text);
		if (ret < 0)
			return ret;

		 
		sha_idx += sizeof(sha_text);
		if (!(sha_idx % 64))
			intel_de_write(i915, HDCP_REP_CTL,
				       rep_ctl | HDCP_SHA1_TEXT_32);

		 
		sha_leftovers = DRM_HDCP_KSV_LEN - sha_empty;
		sha_text = 0;
		for (j = 0; j < sha_leftovers; j++)
			sha_text |= ksv[sha_empty + j] <<
					((sizeof(sha_text) - j - 1) * 8);

		 
		if (sizeof(sha_text) > sha_leftovers)
			continue;

		ret = intel_write_sha_text(i915, sha_text);
		if (ret < 0)
			return ret;
		sha_leftovers = 0;
		sha_text = 0;
		sha_idx += sizeof(sha_text);
	}

	 
	if (sha_leftovers == 0) {
		 
		intel_de_write(i915, HDCP_REP_CTL,
			       rep_ctl | HDCP_SHA1_TEXT_16);
		ret = intel_write_sha_text(i915,
					   bstatus[0] << 8 | bstatus[1]);
		if (ret < 0)
			return ret;
		sha_idx += sizeof(sha_text);

		 
		intel_de_write(i915, HDCP_REP_CTL,
			       rep_ctl | HDCP_SHA1_TEXT_0);
		ret = intel_write_sha_text(i915, 0);
		if (ret < 0)
			return ret;
		sha_idx += sizeof(sha_text);

		 
		intel_de_write(i915, HDCP_REP_CTL,
			       rep_ctl | HDCP_SHA1_TEXT_16);
		ret = intel_write_sha_text(i915, 0);
		if (ret < 0)
			return ret;
		sha_idx += sizeof(sha_text);

	} else if (sha_leftovers == 1) {
		 
		intel_de_write(i915, HDCP_REP_CTL,
			       rep_ctl | HDCP_SHA1_TEXT_24);
		sha_text |= bstatus[0] << 16 | bstatus[1] << 8;
		 
		sha_text = (sha_text & 0xffffff00) >> 8;
		ret = intel_write_sha_text(i915, sha_text);
		if (ret < 0)
			return ret;
		sha_idx += sizeof(sha_text);

		 
		intel_de_write(i915, HDCP_REP_CTL,
			       rep_ctl | HDCP_SHA1_TEXT_0);
		ret = intel_write_sha_text(i915, 0);
		if (ret < 0)
			return ret;
		sha_idx += sizeof(sha_text);

		 
		intel_de_write(i915, HDCP_REP_CTL,
			       rep_ctl | HDCP_SHA1_TEXT_8);
		ret = intel_write_sha_text(i915, 0);
		if (ret < 0)
			return ret;
		sha_idx += sizeof(sha_text);

	} else if (sha_leftovers == 2) {
		 
		intel_de_write(i915, HDCP_REP_CTL,
			       rep_ctl | HDCP_SHA1_TEXT_32);
		sha_text |= bstatus[0] << 8 | bstatus[1];
		ret = intel_write_sha_text(i915, sha_text);
		if (ret < 0)
			return ret;
		sha_idx += sizeof(sha_text);

		 
		intel_de_write(i915, HDCP_REP_CTL,
			       rep_ctl | HDCP_SHA1_TEXT_0);
		for (i = 0; i < 2; i++) {
			ret = intel_write_sha_text(i915, 0);
			if (ret < 0)
				return ret;
			sha_idx += sizeof(sha_text);
		}

		 
		intel_de_write(i915, HDCP_REP_CTL,
			       rep_ctl | HDCP_SHA1_TEXT_32);
		sha_text = DRM_HDCP_SHA1_TERMINATOR << 24;
		ret = intel_write_sha_text(i915, sha_text);
		if (ret < 0)
			return ret;
		sha_idx += sizeof(sha_text);
	} else if (sha_leftovers == 3) {
		 
		intel_de_write(i915, HDCP_REP_CTL,
			       rep_ctl | HDCP_SHA1_TEXT_32);
		sha_text |= bstatus[0];
		ret = intel_write_sha_text(i915, sha_text);
		if (ret < 0)
			return ret;
		sha_idx += sizeof(sha_text);

		 
		intel_de_write(i915, HDCP_REP_CTL,
			       rep_ctl | HDCP_SHA1_TEXT_8);
		ret = intel_write_sha_text(i915, bstatus[1]);
		if (ret < 0)
			return ret;
		sha_idx += sizeof(sha_text);

		 
		intel_de_write(i915, HDCP_REP_CTL,
			       rep_ctl | HDCP_SHA1_TEXT_0);
		ret = intel_write_sha_text(i915, 0);
		if (ret < 0)
			return ret;
		sha_idx += sizeof(sha_text);

		 
		intel_de_write(i915, HDCP_REP_CTL,
			       rep_ctl | HDCP_SHA1_TEXT_24);
		ret = intel_write_sha_text(i915, 0);
		if (ret < 0)
			return ret;
		sha_idx += sizeof(sha_text);
	} else {
		drm_dbg_kms(&i915->drm, "Invalid number of leftovers %d\n",
			    sha_leftovers);
		return -EINVAL;
	}

	intel_de_write(i915, HDCP_REP_CTL, rep_ctl | HDCP_SHA1_TEXT_32);
	 
	while ((sha_idx % 64) < (64 - sizeof(sha_text))) {
		ret = intel_write_sha_text(i915, 0);
		if (ret < 0)
			return ret;
		sha_idx += sizeof(sha_text);
	}

	 
	sha_text = (num_downstream * 5 + 10) * 8;
	ret = intel_write_sha_text(i915, sha_text);
	if (ret < 0)
		return ret;

	 
	intel_de_write(i915, HDCP_REP_CTL,
		       rep_ctl | HDCP_SHA1_COMPLETE_HASH);
	if (intel_de_wait_for_set(i915, HDCP_REP_CTL,
				  HDCP_SHA1_COMPLETE, 1)) {
		drm_err(&i915->drm, "Timed out waiting for SHA1 complete\n");
		return -ETIMEDOUT;
	}
	if (!(intel_de_read(i915, HDCP_REP_CTL) & HDCP_SHA1_V_MATCH)) {
		drm_dbg_kms(&i915->drm, "SHA-1 mismatch, HDCP failed\n");
		return -ENXIO;
	}

	return 0;
}

 
static
int intel_hdcp_auth_downstream(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	const struct intel_hdcp_shim *shim = connector->hdcp.shim;
	u8 bstatus[2], num_downstream, *ksv_fifo;
	int ret, i, tries = 3;

	ret = intel_hdcp_poll_ksv_fifo(dig_port, shim);
	if (ret) {
		drm_dbg_kms(&i915->drm,
			    "KSV list failed to become ready (%d)\n", ret);
		return ret;
	}

	ret = shim->read_bstatus(dig_port, bstatus);
	if (ret)
		return ret;

	if (DRM_HDCP_MAX_DEVICE_EXCEEDED(bstatus[0]) ||
	    DRM_HDCP_MAX_CASCADE_EXCEEDED(bstatus[1])) {
		drm_dbg_kms(&i915->drm, "Max Topology Limit Exceeded\n");
		return -EPERM;
	}

	 
	num_downstream = DRM_HDCP_NUM_DOWNSTREAM(bstatus[0]);
	if (num_downstream == 0) {
		drm_dbg_kms(&i915->drm,
			    "Repeater with zero downstream devices\n");
		return -EINVAL;
	}

	ksv_fifo = kcalloc(DRM_HDCP_KSV_LEN, num_downstream, GFP_KERNEL);
	if (!ksv_fifo) {
		drm_dbg_kms(&i915->drm, "Out of mem: ksv_fifo\n");
		return -ENOMEM;
	}

	ret = shim->read_ksv_fifo(dig_port, num_downstream, ksv_fifo);
	if (ret)
		goto err;

	if (drm_hdcp_check_ksvs_revoked(&i915->drm, ksv_fifo,
					num_downstream) > 0) {
		drm_err(&i915->drm, "Revoked Ksv(s) in ksv_fifo\n");
		ret = -EPERM;
		goto err;
	}

	 
	for (i = 0; i < tries; i++) {
		ret = intel_hdcp_validate_v_prime(connector, shim,
						  ksv_fifo, num_downstream,
						  bstatus);
		if (!ret)
			break;
	}

	if (i == tries) {
		drm_dbg_kms(&i915->drm,
			    "V Prime validation failed.(%d)\n", ret);
		goto err;
	}

	drm_dbg_kms(&i915->drm, "HDCP is enabled (%d downstream devices)\n",
		    num_downstream);
	ret = 0;
err:
	kfree(ksv_fifo);
	return ret;
}

 
static int intel_hdcp_auth(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct intel_hdcp *hdcp = &connector->hdcp;
	const struct intel_hdcp_shim *shim = hdcp->shim;
	enum transcoder cpu_transcoder = connector->hdcp.cpu_transcoder;
	enum port port = dig_port->base.port;
	unsigned long r0_prime_gen_start;
	int ret, i, tries = 2;
	union {
		u32 reg[2];
		u8 shim[DRM_HDCP_AN_LEN];
	} an;
	union {
		u32 reg[2];
		u8 shim[DRM_HDCP_KSV_LEN];
	} bksv;
	union {
		u32 reg;
		u8 shim[DRM_HDCP_RI_LEN];
	} ri;
	bool repeater_present, hdcp_capable;

	 
	if (shim->hdcp_capable) {
		ret = shim->hdcp_capable(dig_port, &hdcp_capable);
		if (ret)
			return ret;
		if (!hdcp_capable) {
			drm_dbg_kms(&i915->drm,
				    "Panel is not HDCP capable\n");
			return -EINVAL;
		}
	}

	 
	for (i = 0; i < 2; i++)
		intel_de_write(i915,
			       HDCP_ANINIT(i915, cpu_transcoder, port),
			       get_random_u32());
	intel_de_write(i915, HDCP_CONF(i915, cpu_transcoder, port),
		       HDCP_CONF_CAPTURE_AN);

	 
	if (intel_de_wait_for_set(i915,
				  HDCP_STATUS(i915, cpu_transcoder, port),
				  HDCP_STATUS_AN_READY, 1)) {
		drm_err(&i915->drm, "Timed out waiting for An\n");
		return -ETIMEDOUT;
	}

	an.reg[0] = intel_de_read(i915,
				  HDCP_ANLO(i915, cpu_transcoder, port));
	an.reg[1] = intel_de_read(i915,
				  HDCP_ANHI(i915, cpu_transcoder, port));
	ret = shim->write_an_aksv(dig_port, an.shim);
	if (ret)
		return ret;

	r0_prime_gen_start = jiffies;

	memset(&bksv, 0, sizeof(bksv));

	ret = intel_hdcp_read_valid_bksv(dig_port, shim, bksv.shim);
	if (ret < 0)
		return ret;

	if (drm_hdcp_check_ksvs_revoked(&i915->drm, bksv.shim, 1) > 0) {
		drm_err(&i915->drm, "BKSV is revoked\n");
		return -EPERM;
	}

	intel_de_write(i915, HDCP_BKSVLO(i915, cpu_transcoder, port),
		       bksv.reg[0]);
	intel_de_write(i915, HDCP_BKSVHI(i915, cpu_transcoder, port),
		       bksv.reg[1]);

	ret = shim->repeater_present(dig_port, &repeater_present);
	if (ret)
		return ret;
	if (repeater_present)
		intel_de_write(i915, HDCP_REP_CTL,
			       intel_hdcp_get_repeater_ctl(i915, cpu_transcoder, port));

	ret = shim->toggle_signalling(dig_port, cpu_transcoder, true);
	if (ret)
		return ret;

	intel_de_write(i915, HDCP_CONF(i915, cpu_transcoder, port),
		       HDCP_CONF_AUTH_AND_ENC);

	 
	if (wait_for(intel_de_read(i915, HDCP_STATUS(i915, cpu_transcoder, port)) &
		     (HDCP_STATUS_R0_READY | HDCP_STATUS_ENC), 1)) {
		drm_err(&i915->drm, "Timed out waiting for R0 ready\n");
		return -ETIMEDOUT;
	}

	 
	wait_remaining_ms_from_jiffies(r0_prime_gen_start, 300);

	tries = 3;

	 
	for (i = 0; i < tries; i++) {
		ri.reg = 0;
		ret = shim->read_ri_prime(dig_port, ri.shim);
		if (ret)
			return ret;
		intel_de_write(i915,
			       HDCP_RPRIME(i915, cpu_transcoder, port),
			       ri.reg);

		 
		if (!wait_for(intel_de_read(i915, HDCP_STATUS(i915, cpu_transcoder, port)) &
			      (HDCP_STATUS_RI_MATCH | HDCP_STATUS_ENC), 1))
			break;
	}

	if (i == tries) {
		drm_dbg_kms(&i915->drm,
			    "Timed out waiting for Ri prime match (%x)\n",
			    intel_de_read(i915,
					  HDCP_STATUS(i915, cpu_transcoder, port)));
		return -ETIMEDOUT;
	}

	 
	if (intel_de_wait_for_set(i915,
				  HDCP_STATUS(i915, cpu_transcoder, port),
				  HDCP_STATUS_ENC,
				  HDCP_ENCRYPT_STATUS_CHANGE_TIMEOUT_MS)) {
		drm_err(&i915->drm, "Timed out waiting for encryption\n");
		return -ETIMEDOUT;
	}

	 
	if (shim->stream_encryption) {
		ret = shim->stream_encryption(connector, true);
		if (ret) {
			drm_err(&i915->drm, "[%s:%d] Failed to enable HDCP 1.4 stream enc\n",
				connector->base.name, connector->base.base.id);
			return ret;
		}
		drm_dbg_kms(&i915->drm, "HDCP 1.4 transcoder: %s stream encrypted\n",
			    transcoder_name(hdcp->stream_transcoder));
	}

	if (repeater_present)
		return intel_hdcp_auth_downstream(connector);

	drm_dbg_kms(&i915->drm, "HDCP is enabled (no repeater present)\n");
	return 0;
}

static int _intel_hdcp_disable(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct intel_hdcp *hdcp = &connector->hdcp;
	enum port port = dig_port->base.port;
	enum transcoder cpu_transcoder = hdcp->cpu_transcoder;
	u32 repeater_ctl;
	int ret;

	drm_dbg_kms(&i915->drm, "[%s:%d] HDCP is being disabled...\n",
		    connector->base.name, connector->base.base.id);

	if (hdcp->shim->stream_encryption) {
		ret = hdcp->shim->stream_encryption(connector, false);
		if (ret) {
			drm_err(&i915->drm, "[%s:%d] Failed to disable HDCP 1.4 stream enc\n",
				connector->base.name, connector->base.base.id);
			return ret;
		}
		drm_dbg_kms(&i915->drm, "HDCP 1.4 transcoder: %s stream encryption disabled\n",
			    transcoder_name(hdcp->stream_transcoder));
		 
		if (dig_port->num_hdcp_streams > 0)
			return 0;
	}

	hdcp->hdcp_encrypted = false;
	intel_de_write(i915, HDCP_CONF(i915, cpu_transcoder, port), 0);
	if (intel_de_wait_for_clear(i915,
				    HDCP_STATUS(i915, cpu_transcoder, port),
				    ~0, HDCP_ENCRYPT_STATUS_CHANGE_TIMEOUT_MS)) {
		drm_err(&i915->drm,
			"Failed to disable HDCP, timeout clearing status\n");
		return -ETIMEDOUT;
	}

	repeater_ctl = intel_hdcp_get_repeater_ctl(i915, cpu_transcoder,
						   port);
	intel_de_rmw(i915, HDCP_REP_CTL, repeater_ctl, 0);

	ret = hdcp->shim->toggle_signalling(dig_port, cpu_transcoder, false);
	if (ret) {
		drm_err(&i915->drm, "Failed to disable HDCP signalling\n");
		return ret;
	}

	drm_dbg_kms(&i915->drm, "HDCP is disabled\n");
	return 0;
}

static int _intel_hdcp_enable(struct intel_connector *connector)
{
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct intel_hdcp *hdcp = &connector->hdcp;
	int i, ret, tries = 3;

	drm_dbg_kms(&i915->drm, "[%s:%d] HDCP is being enabled...\n",
		    connector->base.name, connector->base.base.id);

	if (!hdcp_key_loadable(i915)) {
		drm_err(&i915->drm, "HDCP key Load is not possible\n");
		return -ENXIO;
	}

	for (i = 0; i < KEY_LOAD_TRIES; i++) {
		ret = intel_hdcp_load_keys(i915);
		if (!ret)
			break;
		intel_hdcp_clear_keys(i915);
	}
	if (ret) {
		drm_err(&i915->drm, "Could not load HDCP keys, (%d)\n",
			ret);
		return ret;
	}

	 
	for (i = 0; i < tries; i++) {
		ret = intel_hdcp_auth(connector);
		if (!ret) {
			hdcp->hdcp_encrypted = true;
			return 0;
		}

		drm_dbg_kms(&i915->drm, "HDCP Auth failure (%d)\n", ret);

		 
		_intel_hdcp_disable(connector);
	}

	drm_dbg_kms(&i915->drm,
		    "HDCP authentication failed (%d tries/%d)\n", tries, ret);
	return ret;
}

static struct intel_connector *intel_hdcp_to_connector(struct intel_hdcp *hdcp)
{
	return container_of(hdcp, struct intel_connector, hdcp);
}

static void intel_hdcp_update_value(struct intel_connector *connector,
				    u64 value, bool update_property)
{
	struct drm_device *dev = connector->base.dev;
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct intel_hdcp *hdcp = &connector->hdcp;
	struct drm_i915_private *i915 = to_i915(connector->base.dev);

	drm_WARN_ON(connector->base.dev, !mutex_is_locked(&hdcp->mutex));

	if (hdcp->value == value)
		return;

	drm_WARN_ON(dev, !mutex_is_locked(&dig_port->hdcp_mutex));

	if (hdcp->value == DRM_MODE_CONTENT_PROTECTION_ENABLED) {
		if (!drm_WARN_ON(dev, dig_port->num_hdcp_streams == 0))
			dig_port->num_hdcp_streams--;
	} else if (value == DRM_MODE_CONTENT_PROTECTION_ENABLED) {
		dig_port->num_hdcp_streams++;
	}

	hdcp->value = value;
	if (update_property) {
		drm_connector_get(&connector->base);
		queue_work(i915->unordered_wq, &hdcp->prop_work);
	}
}

 
static int intel_hdcp_check_link(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct intel_hdcp *hdcp = &connector->hdcp;
	enum port port = dig_port->base.port;
	enum transcoder cpu_transcoder;
	int ret = 0;

	mutex_lock(&hdcp->mutex);
	mutex_lock(&dig_port->hdcp_mutex);

	cpu_transcoder = hdcp->cpu_transcoder;

	 
	if (hdcp->value != DRM_MODE_CONTENT_PROTECTION_ENABLED ||
	    !hdcp->hdcp_encrypted) {
		ret = -EINVAL;
		goto out;
	}

	if (drm_WARN_ON(&i915->drm,
			!intel_hdcp_in_use(i915, cpu_transcoder, port))) {
		drm_err(&i915->drm,
			"%s:%d HDCP link stopped encryption,%x\n",
			connector->base.name, connector->base.base.id,
			intel_de_read(i915, HDCP_STATUS(i915, cpu_transcoder, port)));
		ret = -ENXIO;
		intel_hdcp_update_value(connector,
					DRM_MODE_CONTENT_PROTECTION_DESIRED,
					true);
		goto out;
	}

	if (hdcp->shim->check_link(dig_port, connector)) {
		if (hdcp->value != DRM_MODE_CONTENT_PROTECTION_UNDESIRED) {
			intel_hdcp_update_value(connector,
				DRM_MODE_CONTENT_PROTECTION_ENABLED, true);
		}
		goto out;
	}

	drm_dbg_kms(&i915->drm,
		    "[%s:%d] HDCP link failed, retrying authentication\n",
		    connector->base.name, connector->base.base.id);

	ret = _intel_hdcp_disable(connector);
	if (ret) {
		drm_err(&i915->drm, "Failed to disable hdcp (%d)\n", ret);
		intel_hdcp_update_value(connector,
					DRM_MODE_CONTENT_PROTECTION_DESIRED,
					true);
		goto out;
	}

	ret = _intel_hdcp_enable(connector);
	if (ret) {
		drm_err(&i915->drm, "Failed to enable hdcp (%d)\n", ret);
		intel_hdcp_update_value(connector,
					DRM_MODE_CONTENT_PROTECTION_DESIRED,
					true);
		goto out;
	}

out:
	mutex_unlock(&dig_port->hdcp_mutex);
	mutex_unlock(&hdcp->mutex);
	return ret;
}

static void intel_hdcp_prop_work(struct work_struct *work)
{
	struct intel_hdcp *hdcp = container_of(work, struct intel_hdcp,
					       prop_work);
	struct intel_connector *connector = intel_hdcp_to_connector(hdcp);
	struct drm_i915_private *i915 = to_i915(connector->base.dev);

	drm_modeset_lock(&i915->drm.mode_config.connection_mutex, NULL);
	mutex_lock(&hdcp->mutex);

	 
	if (hdcp->value != DRM_MODE_CONTENT_PROTECTION_UNDESIRED)
		drm_hdcp_update_content_protection(&connector->base,
						   hdcp->value);

	mutex_unlock(&hdcp->mutex);
	drm_modeset_unlock(&i915->drm.mode_config.connection_mutex);

	drm_connector_put(&connector->base);
}

bool is_hdcp_supported(struct drm_i915_private *i915, enum port port)
{
	return DISPLAY_RUNTIME_INFO(i915)->has_hdcp &&
		(DISPLAY_VER(i915) >= 12 || port < PORT_E);
}

static int
hdcp2_prepare_ake_init(struct intel_connector *connector,
		       struct hdcp2_ake_init *ake_data)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct hdcp_port_data *data = &dig_port->hdcp_port_data;
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct i915_hdcp_arbiter *arbiter;
	int ret;

	mutex_lock(&i915->display.hdcp.hdcp_mutex);
	arbiter = i915->display.hdcp.arbiter;

	if (!arbiter || !arbiter->ops) {
		mutex_unlock(&i915->display.hdcp.hdcp_mutex);
		return -EINVAL;
	}

	ret = arbiter->ops->initiate_hdcp2_session(arbiter->hdcp_dev, data, ake_data);
	if (ret)
		drm_dbg_kms(&i915->drm, "Prepare_ake_init failed. %d\n",
			    ret);
	mutex_unlock(&i915->display.hdcp.hdcp_mutex);

	return ret;
}

static int
hdcp2_verify_rx_cert_prepare_km(struct intel_connector *connector,
				struct hdcp2_ake_send_cert *rx_cert,
				bool *paired,
				struct hdcp2_ake_no_stored_km *ek_pub_km,
				size_t *msg_sz)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct hdcp_port_data *data = &dig_port->hdcp_port_data;
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct i915_hdcp_arbiter *arbiter;
	int ret;

	mutex_lock(&i915->display.hdcp.hdcp_mutex);
	arbiter = i915->display.hdcp.arbiter;

	if (!arbiter || !arbiter->ops) {
		mutex_unlock(&i915->display.hdcp.hdcp_mutex);
		return -EINVAL;
	}

	ret = arbiter->ops->verify_receiver_cert_prepare_km(arbiter->hdcp_dev, data,
							 rx_cert, paired,
							 ek_pub_km, msg_sz);
	if (ret < 0)
		drm_dbg_kms(&i915->drm, "Verify rx_cert failed. %d\n",
			    ret);
	mutex_unlock(&i915->display.hdcp.hdcp_mutex);

	return ret;
}

static int hdcp2_verify_hprime(struct intel_connector *connector,
			       struct hdcp2_ake_send_hprime *rx_hprime)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct hdcp_port_data *data = &dig_port->hdcp_port_data;
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct i915_hdcp_arbiter *arbiter;
	int ret;

	mutex_lock(&i915->display.hdcp.hdcp_mutex);
	arbiter = i915->display.hdcp.arbiter;

	if (!arbiter || !arbiter->ops) {
		mutex_unlock(&i915->display.hdcp.hdcp_mutex);
		return -EINVAL;
	}

	ret = arbiter->ops->verify_hprime(arbiter->hdcp_dev, data, rx_hprime);
	if (ret < 0)
		drm_dbg_kms(&i915->drm, "Verify hprime failed. %d\n", ret);
	mutex_unlock(&i915->display.hdcp.hdcp_mutex);

	return ret;
}

static int
hdcp2_store_pairing_info(struct intel_connector *connector,
			 struct hdcp2_ake_send_pairing_info *pairing_info)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct hdcp_port_data *data = &dig_port->hdcp_port_data;
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct i915_hdcp_arbiter *arbiter;
	int ret;

	mutex_lock(&i915->display.hdcp.hdcp_mutex);
	arbiter = i915->display.hdcp.arbiter;

	if (!arbiter || !arbiter->ops) {
		mutex_unlock(&i915->display.hdcp.hdcp_mutex);
		return -EINVAL;
	}

	ret = arbiter->ops->store_pairing_info(arbiter->hdcp_dev, data, pairing_info);
	if (ret < 0)
		drm_dbg_kms(&i915->drm, "Store pairing info failed. %d\n",
			    ret);
	mutex_unlock(&i915->display.hdcp.hdcp_mutex);

	return ret;
}

static int
hdcp2_prepare_lc_init(struct intel_connector *connector,
		      struct hdcp2_lc_init *lc_init)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct hdcp_port_data *data = &dig_port->hdcp_port_data;
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct i915_hdcp_arbiter *arbiter;
	int ret;

	mutex_lock(&i915->display.hdcp.hdcp_mutex);
	arbiter = i915->display.hdcp.arbiter;

	if (!arbiter || !arbiter->ops) {
		mutex_unlock(&i915->display.hdcp.hdcp_mutex);
		return -EINVAL;
	}

	ret = arbiter->ops->initiate_locality_check(arbiter->hdcp_dev, data, lc_init);
	if (ret < 0)
		drm_dbg_kms(&i915->drm, "Prepare lc_init failed. %d\n",
			    ret);
	mutex_unlock(&i915->display.hdcp.hdcp_mutex);

	return ret;
}

static int
hdcp2_verify_lprime(struct intel_connector *connector,
		    struct hdcp2_lc_send_lprime *rx_lprime)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct hdcp_port_data *data = &dig_port->hdcp_port_data;
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct i915_hdcp_arbiter *arbiter;
	int ret;

	mutex_lock(&i915->display.hdcp.hdcp_mutex);
	arbiter = i915->display.hdcp.arbiter;

	if (!arbiter || !arbiter->ops) {
		mutex_unlock(&i915->display.hdcp.hdcp_mutex);
		return -EINVAL;
	}

	ret = arbiter->ops->verify_lprime(arbiter->hdcp_dev, data, rx_lprime);
	if (ret < 0)
		drm_dbg_kms(&i915->drm, "Verify L_Prime failed. %d\n",
			    ret);
	mutex_unlock(&i915->display.hdcp.hdcp_mutex);

	return ret;
}

static int hdcp2_prepare_skey(struct intel_connector *connector,
			      struct hdcp2_ske_send_eks *ske_data)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct hdcp_port_data *data = &dig_port->hdcp_port_data;
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct i915_hdcp_arbiter *arbiter;
	int ret;

	mutex_lock(&i915->display.hdcp.hdcp_mutex);
	arbiter = i915->display.hdcp.arbiter;

	if (!arbiter || !arbiter->ops) {
		mutex_unlock(&i915->display.hdcp.hdcp_mutex);
		return -EINVAL;
	}

	ret = arbiter->ops->get_session_key(arbiter->hdcp_dev, data, ske_data);
	if (ret < 0)
		drm_dbg_kms(&i915->drm, "Get session key failed. %d\n",
			    ret);
	mutex_unlock(&i915->display.hdcp.hdcp_mutex);

	return ret;
}

static int
hdcp2_verify_rep_topology_prepare_ack(struct intel_connector *connector,
				      struct hdcp2_rep_send_receiverid_list
								*rep_topology,
				      struct hdcp2_rep_send_ack *rep_send_ack)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct hdcp_port_data *data = &dig_port->hdcp_port_data;
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct i915_hdcp_arbiter *arbiter;
	int ret;

	mutex_lock(&i915->display.hdcp.hdcp_mutex);
	arbiter = i915->display.hdcp.arbiter;

	if (!arbiter || !arbiter->ops) {
		mutex_unlock(&i915->display.hdcp.hdcp_mutex);
		return -EINVAL;
	}

	ret = arbiter->ops->repeater_check_flow_prepare_ack(arbiter->hdcp_dev,
							    data,
							    rep_topology,
							    rep_send_ack);
	if (ret < 0)
		drm_dbg_kms(&i915->drm,
			    "Verify rep topology failed. %d\n", ret);
	mutex_unlock(&i915->display.hdcp.hdcp_mutex);

	return ret;
}

static int
hdcp2_verify_mprime(struct intel_connector *connector,
		    struct hdcp2_rep_stream_ready *stream_ready)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct hdcp_port_data *data = &dig_port->hdcp_port_data;
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct i915_hdcp_arbiter *arbiter;
	int ret;

	mutex_lock(&i915->display.hdcp.hdcp_mutex);
	arbiter = i915->display.hdcp.arbiter;

	if (!arbiter || !arbiter->ops) {
		mutex_unlock(&i915->display.hdcp.hdcp_mutex);
		return -EINVAL;
	}

	ret = arbiter->ops->verify_mprime(arbiter->hdcp_dev, data, stream_ready);
	if (ret < 0)
		drm_dbg_kms(&i915->drm, "Verify mprime failed. %d\n", ret);
	mutex_unlock(&i915->display.hdcp.hdcp_mutex);

	return ret;
}

static int hdcp2_authenticate_port(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct hdcp_port_data *data = &dig_port->hdcp_port_data;
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct i915_hdcp_arbiter *arbiter;
	int ret;

	mutex_lock(&i915->display.hdcp.hdcp_mutex);
	arbiter = i915->display.hdcp.arbiter;

	if (!arbiter || !arbiter->ops) {
		mutex_unlock(&i915->display.hdcp.hdcp_mutex);
		return -EINVAL;
	}

	ret = arbiter->ops->enable_hdcp_authentication(arbiter->hdcp_dev, data);
	if (ret < 0)
		drm_dbg_kms(&i915->drm, "Enable hdcp auth failed. %d\n",
			    ret);
	mutex_unlock(&i915->display.hdcp.hdcp_mutex);

	return ret;
}

static int hdcp2_close_session(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct i915_hdcp_arbiter *arbiter;
	int ret;

	mutex_lock(&i915->display.hdcp.hdcp_mutex);
	arbiter = i915->display.hdcp.arbiter;

	if (!arbiter || !arbiter->ops) {
		mutex_unlock(&i915->display.hdcp.hdcp_mutex);
		return -EINVAL;
	}

	ret = arbiter->ops->close_hdcp_session(arbiter->hdcp_dev,
					     &dig_port->hdcp_port_data);
	mutex_unlock(&i915->display.hdcp.hdcp_mutex);

	return ret;
}

static int hdcp2_deauthenticate_port(struct intel_connector *connector)
{
	return hdcp2_close_session(connector);
}

 
static int hdcp2_authentication_key_exchange(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct intel_hdcp *hdcp = &connector->hdcp;
	union {
		struct hdcp2_ake_init ake_init;
		struct hdcp2_ake_send_cert send_cert;
		struct hdcp2_ake_no_stored_km no_stored_km;
		struct hdcp2_ake_send_hprime send_hprime;
		struct hdcp2_ake_send_pairing_info pairing_info;
	} msgs;
	const struct intel_hdcp_shim *shim = hdcp->shim;
	size_t size;
	int ret;

	 
	hdcp->seq_num_v = 0;
	hdcp->seq_num_m = 0;

	ret = hdcp2_prepare_ake_init(connector, &msgs.ake_init);
	if (ret < 0)
		return ret;

	ret = shim->write_2_2_msg(dig_port, &msgs.ake_init,
				  sizeof(msgs.ake_init));
	if (ret < 0)
		return ret;

	ret = shim->read_2_2_msg(dig_port, HDCP_2_2_AKE_SEND_CERT,
				 &msgs.send_cert, sizeof(msgs.send_cert));
	if (ret < 0)
		return ret;

	if (msgs.send_cert.rx_caps[0] != HDCP_2_2_RX_CAPS_VERSION_VAL) {
		drm_dbg_kms(&i915->drm, "cert.rx_caps dont claim HDCP2.2\n");
		return -EINVAL;
	}

	hdcp->is_repeater = HDCP_2_2_RX_REPEATER(msgs.send_cert.rx_caps[2]);

	if (drm_hdcp_check_ksvs_revoked(&i915->drm,
					msgs.send_cert.cert_rx.receiver_id,
					1) > 0) {
		drm_err(&i915->drm, "Receiver ID is revoked\n");
		return -EPERM;
	}

	 
	ret = hdcp2_verify_rx_cert_prepare_km(connector, &msgs.send_cert,
					      &hdcp->is_paired,
					      &msgs.no_stored_km, &size);
	if (ret < 0)
		return ret;

	ret = shim->write_2_2_msg(dig_port, &msgs.no_stored_km, size);
	if (ret < 0)
		return ret;

	ret = shim->read_2_2_msg(dig_port, HDCP_2_2_AKE_SEND_HPRIME,
				 &msgs.send_hprime, sizeof(msgs.send_hprime));
	if (ret < 0)
		return ret;

	ret = hdcp2_verify_hprime(connector, &msgs.send_hprime);
	if (ret < 0)
		return ret;

	if (!hdcp->is_paired) {
		 
		ret = shim->read_2_2_msg(dig_port,
					 HDCP_2_2_AKE_SEND_PAIRING_INFO,
					 &msgs.pairing_info,
					 sizeof(msgs.pairing_info));
		if (ret < 0)
			return ret;

		ret = hdcp2_store_pairing_info(connector, &msgs.pairing_info);
		if (ret < 0)
			return ret;
		hdcp->is_paired = true;
	}

	return 0;
}

static int hdcp2_locality_check(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct intel_hdcp *hdcp = &connector->hdcp;
	union {
		struct hdcp2_lc_init lc_init;
		struct hdcp2_lc_send_lprime send_lprime;
	} msgs;
	const struct intel_hdcp_shim *shim = hdcp->shim;
	int tries = HDCP2_LC_RETRY_CNT, ret, i;

	for (i = 0; i < tries; i++) {
		ret = hdcp2_prepare_lc_init(connector, &msgs.lc_init);
		if (ret < 0)
			continue;

		ret = shim->write_2_2_msg(dig_port, &msgs.lc_init,
				      sizeof(msgs.lc_init));
		if (ret < 0)
			continue;

		ret = shim->read_2_2_msg(dig_port,
					 HDCP_2_2_LC_SEND_LPRIME,
					 &msgs.send_lprime,
					 sizeof(msgs.send_lprime));
		if (ret < 0)
			continue;

		ret = hdcp2_verify_lprime(connector, &msgs.send_lprime);
		if (!ret)
			break;
	}

	return ret;
}

static int hdcp2_session_key_exchange(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct intel_hdcp *hdcp = &connector->hdcp;
	struct hdcp2_ske_send_eks send_eks;
	int ret;

	ret = hdcp2_prepare_skey(connector, &send_eks);
	if (ret < 0)
		return ret;

	ret = hdcp->shim->write_2_2_msg(dig_port, &send_eks,
					sizeof(send_eks));
	if (ret < 0)
		return ret;

	return 0;
}

static
int _hdcp2_propagate_stream_management_info(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct hdcp_port_data *data = &dig_port->hdcp_port_data;
	struct intel_hdcp *hdcp = &connector->hdcp;
	union {
		struct hdcp2_rep_stream_manage stream_manage;
		struct hdcp2_rep_stream_ready stream_ready;
	} msgs;
	const struct intel_hdcp_shim *shim = hdcp->shim;
	int ret, streams_size_delta, i;

	if (connector->hdcp.seq_num_m > HDCP_2_2_SEQ_NUM_MAX)
		return -ERANGE;

	 
	msgs.stream_manage.msg_id = HDCP_2_2_REP_STREAM_MANAGE;
	drm_hdcp_cpu_to_be24(msgs.stream_manage.seq_num_m, hdcp->seq_num_m);

	msgs.stream_manage.k = cpu_to_be16(data->k);

	for (i = 0; i < data->k; i++) {
		msgs.stream_manage.streams[i].stream_id = data->streams[i].stream_id;
		msgs.stream_manage.streams[i].stream_type = data->streams[i].stream_type;
	}

	streams_size_delta = (HDCP_2_2_MAX_CONTENT_STREAMS_CNT - data->k) *
				sizeof(struct hdcp2_streamid_type);
	 
	ret = shim->write_2_2_msg(dig_port, &msgs.stream_manage,
				  sizeof(msgs.stream_manage) - streams_size_delta);
	if (ret < 0)
		goto out;

	ret = shim->read_2_2_msg(dig_port, HDCP_2_2_REP_STREAM_READY,
				 &msgs.stream_ready, sizeof(msgs.stream_ready));
	if (ret < 0)
		goto out;

	data->seq_num_m = hdcp->seq_num_m;

	ret = hdcp2_verify_mprime(connector, &msgs.stream_ready);

out:
	hdcp->seq_num_m++;

	return ret;
}

static
int hdcp2_authenticate_repeater_topology(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct intel_hdcp *hdcp = &connector->hdcp;
	union {
		struct hdcp2_rep_send_receiverid_list recvid_list;
		struct hdcp2_rep_send_ack rep_ack;
	} msgs;
	const struct intel_hdcp_shim *shim = hdcp->shim;
	u32 seq_num_v, device_cnt;
	u8 *rx_info;
	int ret;

	ret = shim->read_2_2_msg(dig_port, HDCP_2_2_REP_SEND_RECVID_LIST,
				 &msgs.recvid_list, sizeof(msgs.recvid_list));
	if (ret < 0)
		return ret;

	rx_info = msgs.recvid_list.rx_info;

	if (HDCP_2_2_MAX_CASCADE_EXCEEDED(rx_info[1]) ||
	    HDCP_2_2_MAX_DEVS_EXCEEDED(rx_info[1])) {
		drm_dbg_kms(&i915->drm, "Topology Max Size Exceeded\n");
		return -EINVAL;
	}

	 
	dig_port->hdcp_mst_type1_capable =
		!HDCP_2_2_HDCP1_DEVICE_CONNECTED(rx_info[1]) &&
		!HDCP_2_2_HDCP_2_0_REP_CONNECTED(rx_info[1]);

	 
	seq_num_v =
		drm_hdcp_be24_to_cpu((const u8 *)msgs.recvid_list.seq_num_v);

	if (!hdcp->hdcp2_encrypted && seq_num_v) {
		drm_dbg_kms(&i915->drm,
			    "Non zero Seq_num_v at first RecvId_List msg\n");
		return -EINVAL;
	}

	if (seq_num_v < hdcp->seq_num_v) {
		 
		drm_dbg_kms(&i915->drm, "Seq_num_v roll over.\n");
		return -EINVAL;
	}

	device_cnt = (HDCP_2_2_DEV_COUNT_HI(rx_info[0]) << 4 |
		      HDCP_2_2_DEV_COUNT_LO(rx_info[1]));
	if (drm_hdcp_check_ksvs_revoked(&i915->drm,
					msgs.recvid_list.receiver_ids,
					device_cnt) > 0) {
		drm_err(&i915->drm, "Revoked receiver ID(s) is in list\n");
		return -EPERM;
	}

	ret = hdcp2_verify_rep_topology_prepare_ack(connector,
						    &msgs.recvid_list,
						    &msgs.rep_ack);
	if (ret < 0)
		return ret;

	hdcp->seq_num_v = seq_num_v;
	ret = shim->write_2_2_msg(dig_port, &msgs.rep_ack,
				  sizeof(msgs.rep_ack));
	if (ret < 0)
		return ret;

	return 0;
}

static int hdcp2_authenticate_sink(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct intel_hdcp *hdcp = &connector->hdcp;
	const struct intel_hdcp_shim *shim = hdcp->shim;
	int ret;

	ret = hdcp2_authentication_key_exchange(connector);
	if (ret < 0) {
		drm_dbg_kms(&i915->drm, "AKE Failed. Err : %d\n", ret);
		return ret;
	}

	ret = hdcp2_locality_check(connector);
	if (ret < 0) {
		drm_dbg_kms(&i915->drm,
			    "Locality Check failed. Err : %d\n", ret);
		return ret;
	}

	ret = hdcp2_session_key_exchange(connector);
	if (ret < 0) {
		drm_dbg_kms(&i915->drm, "SKE Failed. Err : %d\n", ret);
		return ret;
	}

	if (shim->config_stream_type) {
		ret = shim->config_stream_type(dig_port,
					       hdcp->is_repeater,
					       hdcp->content_type);
		if (ret < 0)
			return ret;
	}

	if (hdcp->is_repeater) {
		ret = hdcp2_authenticate_repeater_topology(connector);
		if (ret < 0) {
			drm_dbg_kms(&i915->drm,
				    "Repeater Auth Failed. Err: %d\n", ret);
			return ret;
		}
	}

	return ret;
}

static int hdcp2_enable_stream_encryption(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct hdcp_port_data *data = &dig_port->hdcp_port_data;
	struct intel_hdcp *hdcp = &connector->hdcp;
	enum transcoder cpu_transcoder = hdcp->cpu_transcoder;
	enum port port = dig_port->base.port;
	int ret = 0;

	if (!(intel_de_read(i915, HDCP2_STATUS(i915, cpu_transcoder, port)) &
			    LINK_ENCRYPTION_STATUS)) {
		drm_err(&i915->drm, "[%s:%d] HDCP 2.2 Link is not encrypted\n",
			connector->base.name, connector->base.base.id);
		ret = -EPERM;
		goto link_recover;
	}

	if (hdcp->shim->stream_2_2_encryption) {
		ret = hdcp->shim->stream_2_2_encryption(connector, true);
		if (ret) {
			drm_err(&i915->drm, "[%s:%d] Failed to enable HDCP 2.2 stream enc\n",
				connector->base.name, connector->base.base.id);
			return ret;
		}
		drm_dbg_kms(&i915->drm, "HDCP 2.2 transcoder: %s stream encrypted\n",
			    transcoder_name(hdcp->stream_transcoder));
	}

	return 0;

link_recover:
	if (hdcp2_deauthenticate_port(connector) < 0)
		drm_dbg_kms(&i915->drm, "Port deauth failed.\n");

	dig_port->hdcp_auth_status = false;
	data->k = 0;

	return ret;
}

static int hdcp2_enable_encryption(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct intel_hdcp *hdcp = &connector->hdcp;
	enum port port = dig_port->base.port;
	enum transcoder cpu_transcoder = hdcp->cpu_transcoder;
	int ret;

	drm_WARN_ON(&i915->drm,
		    intel_de_read(i915, HDCP2_STATUS(i915, cpu_transcoder, port)) &
		    LINK_ENCRYPTION_STATUS);
	if (hdcp->shim->toggle_signalling) {
		ret = hdcp->shim->toggle_signalling(dig_port, cpu_transcoder,
						    true);
		if (ret) {
			drm_err(&i915->drm,
				"Failed to enable HDCP signalling. %d\n",
				ret);
			return ret;
		}
	}

	if (intel_de_read(i915, HDCP2_STATUS(i915, cpu_transcoder, port)) &
	    LINK_AUTH_STATUS)
		 
		intel_de_rmw(i915, HDCP2_CTL(i915, cpu_transcoder, port),
			     0, CTL_LINK_ENCRYPTION_REQ);

	ret = intel_de_wait_for_set(i915,
				    HDCP2_STATUS(i915, cpu_transcoder,
						 port),
				    LINK_ENCRYPTION_STATUS,
				    HDCP_ENCRYPT_STATUS_CHANGE_TIMEOUT_MS);
	dig_port->hdcp_auth_status = true;

	return ret;
}

static int hdcp2_disable_encryption(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct intel_hdcp *hdcp = &connector->hdcp;
	enum port port = dig_port->base.port;
	enum transcoder cpu_transcoder = hdcp->cpu_transcoder;
	int ret;

	drm_WARN_ON(&i915->drm, !(intel_de_read(i915, HDCP2_STATUS(i915, cpu_transcoder, port)) &
				      LINK_ENCRYPTION_STATUS));

	intel_de_rmw(i915, HDCP2_CTL(i915, cpu_transcoder, port),
		     CTL_LINK_ENCRYPTION_REQ, 0);

	ret = intel_de_wait_for_clear(i915,
				      HDCP2_STATUS(i915, cpu_transcoder,
						   port),
				      LINK_ENCRYPTION_STATUS,
				      HDCP_ENCRYPT_STATUS_CHANGE_TIMEOUT_MS);
	if (ret == -ETIMEDOUT)
		drm_dbg_kms(&i915->drm, "Disable Encryption Timedout");

	if (hdcp->shim->toggle_signalling) {
		ret = hdcp->shim->toggle_signalling(dig_port, cpu_transcoder,
						    false);
		if (ret) {
			drm_err(&i915->drm,
				"Failed to disable HDCP signalling. %d\n",
				ret);
			return ret;
		}
	}

	return ret;
}

static int
hdcp2_propagate_stream_management_info(struct intel_connector *connector)
{
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	int i, tries = 3, ret;

	if (!connector->hdcp.is_repeater)
		return 0;

	for (i = 0; i < tries; i++) {
		ret = _hdcp2_propagate_stream_management_info(connector);
		if (!ret)
			break;

		 
		if (connector->hdcp.seq_num_m > HDCP_2_2_SEQ_NUM_MAX) {
			drm_dbg_kms(&i915->drm,
				    "seq_num_m roll over.(%d)\n", ret);
			break;
		}

		drm_dbg_kms(&i915->drm,
			    "HDCP2 stream management %d of %d Failed.(%d)\n",
			    i + 1, tries, ret);
	}

	return ret;
}

static int hdcp2_authenticate_and_encrypt(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	int ret = 0, i, tries = 3;

	for (i = 0; i < tries && !dig_port->hdcp_auth_status; i++) {
		ret = hdcp2_authenticate_sink(connector);
		if (!ret) {
			intel_hdcp_prepare_streams(connector);

			ret = hdcp2_propagate_stream_management_info(connector);
			if (ret) {
				drm_dbg_kms(&i915->drm,
					    "Stream management failed.(%d)\n",
					    ret);
				break;
			}

			ret = hdcp2_authenticate_port(connector);
			if (!ret)
				break;
			drm_dbg_kms(&i915->drm, "HDCP2 port auth failed.(%d)\n",
				    ret);
		}

		 
		drm_dbg_kms(&i915->drm, "HDCP2.2 Auth %d of %d Failed.(%d)\n",
			    i + 1, tries, ret);
		if (hdcp2_deauthenticate_port(connector) < 0)
			drm_dbg_kms(&i915->drm, "Port deauth failed.\n");
	}

	if (!ret && !dig_port->hdcp_auth_status) {
		 
		msleep(HDCP_2_2_DELAY_BEFORE_ENCRYPTION_EN);
		ret = hdcp2_enable_encryption(connector);
		if (ret < 0) {
			drm_dbg_kms(&i915->drm,
				    "Encryption Enable Failed.(%d)\n", ret);
			if (hdcp2_deauthenticate_port(connector) < 0)
				drm_dbg_kms(&i915->drm, "Port deauth failed.\n");
		}
	}

	if (!ret)
		ret = hdcp2_enable_stream_encryption(connector);

	return ret;
}

static int _intel_hdcp2_enable(struct intel_connector *connector)
{
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct intel_hdcp *hdcp = &connector->hdcp;
	int ret;

	drm_dbg_kms(&i915->drm, "[%s:%d] HDCP2.2 is being enabled. Type: %d\n",
		    connector->base.name, connector->base.base.id,
		    hdcp->content_type);

	ret = hdcp2_authenticate_and_encrypt(connector);
	if (ret) {
		drm_dbg_kms(&i915->drm, "HDCP2 Type%d  Enabling Failed. (%d)\n",
			    hdcp->content_type, ret);
		return ret;
	}

	drm_dbg_kms(&i915->drm, "[%s:%d] HDCP2.2 is enabled. Type %d\n",
		    connector->base.name, connector->base.base.id,
		    hdcp->content_type);

	hdcp->hdcp2_encrypted = true;
	return 0;
}

static int
_intel_hdcp2_disable(struct intel_connector *connector, bool hdcp2_link_recovery)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct hdcp_port_data *data = &dig_port->hdcp_port_data;
	struct intel_hdcp *hdcp = &connector->hdcp;
	int ret;

	drm_dbg_kms(&i915->drm, "[%s:%d] HDCP2.2 is being Disabled\n",
		    connector->base.name, connector->base.base.id);

	if (hdcp->shim->stream_2_2_encryption) {
		ret = hdcp->shim->stream_2_2_encryption(connector, false);
		if (ret) {
			drm_err(&i915->drm, "[%s:%d] Failed to disable HDCP 2.2 stream enc\n",
				connector->base.name, connector->base.base.id);
			return ret;
		}
		drm_dbg_kms(&i915->drm, "HDCP 2.2 transcoder: %s stream encryption disabled\n",
			    transcoder_name(hdcp->stream_transcoder));

		if (dig_port->num_hdcp_streams > 0 && !hdcp2_link_recovery)
			return 0;
	}

	ret = hdcp2_disable_encryption(connector);

	if (hdcp2_deauthenticate_port(connector) < 0)
		drm_dbg_kms(&i915->drm, "Port deauth failed.\n");

	connector->hdcp.hdcp2_encrypted = false;
	dig_port->hdcp_auth_status = false;
	data->k = 0;

	return ret;
}

 
static int intel_hdcp2_check_link(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct intel_hdcp *hdcp = &connector->hdcp;
	enum port port = dig_port->base.port;
	enum transcoder cpu_transcoder;
	int ret = 0;

	mutex_lock(&hdcp->mutex);
	mutex_lock(&dig_port->hdcp_mutex);
	cpu_transcoder = hdcp->cpu_transcoder;

	 
	if (hdcp->value != DRM_MODE_CONTENT_PROTECTION_ENABLED ||
	    !hdcp->hdcp2_encrypted) {
		ret = -EINVAL;
		goto out;
	}

	if (drm_WARN_ON(&i915->drm,
			!intel_hdcp2_in_use(i915, cpu_transcoder, port))) {
		drm_err(&i915->drm,
			"HDCP2.2 link stopped the encryption, %x\n",
			intel_de_read(i915, HDCP2_STATUS(i915, cpu_transcoder, port)));
		ret = -ENXIO;
		_intel_hdcp2_disable(connector, true);
		intel_hdcp_update_value(connector,
					DRM_MODE_CONTENT_PROTECTION_DESIRED,
					true);
		goto out;
	}

	ret = hdcp->shim->check_2_2_link(dig_port, connector);
	if (ret == HDCP_LINK_PROTECTED) {
		if (hdcp->value != DRM_MODE_CONTENT_PROTECTION_UNDESIRED) {
			intel_hdcp_update_value(connector,
					DRM_MODE_CONTENT_PROTECTION_ENABLED,
					true);
		}
		goto out;
	}

	if (ret == HDCP_TOPOLOGY_CHANGE) {
		if (hdcp->value == DRM_MODE_CONTENT_PROTECTION_UNDESIRED)
			goto out;

		drm_dbg_kms(&i915->drm,
			    "HDCP2.2 Downstream topology change\n");
		ret = hdcp2_authenticate_repeater_topology(connector);
		if (!ret) {
			intel_hdcp_update_value(connector,
					DRM_MODE_CONTENT_PROTECTION_ENABLED,
					true);
			goto out;
		}
		drm_dbg_kms(&i915->drm,
			    "[%s:%d] Repeater topology auth failed.(%d)\n",
			    connector->base.name, connector->base.base.id,
			    ret);
	} else {
		drm_dbg_kms(&i915->drm,
			    "[%s:%d] HDCP2.2 link failed, retrying auth\n",
			    connector->base.name, connector->base.base.id);
	}

	ret = _intel_hdcp2_disable(connector, true);
	if (ret) {
		drm_err(&i915->drm,
			"[%s:%d] Failed to disable hdcp2.2 (%d)\n",
			connector->base.name, connector->base.base.id, ret);
		intel_hdcp_update_value(connector,
				DRM_MODE_CONTENT_PROTECTION_DESIRED, true);
		goto out;
	}

	ret = _intel_hdcp2_enable(connector);
	if (ret) {
		drm_dbg_kms(&i915->drm,
			    "[%s:%d] Failed to enable hdcp2.2 (%d)\n",
			    connector->base.name, connector->base.base.id,
			    ret);
		intel_hdcp_update_value(connector,
					DRM_MODE_CONTENT_PROTECTION_DESIRED,
					true);
		goto out;
	}

out:
	mutex_unlock(&dig_port->hdcp_mutex);
	mutex_unlock(&hdcp->mutex);
	return ret;
}

static void intel_hdcp_check_work(struct work_struct *work)
{
	struct intel_hdcp *hdcp = container_of(to_delayed_work(work),
					       struct intel_hdcp,
					       check_work);
	struct intel_connector *connector = intel_hdcp_to_connector(hdcp);
	struct drm_i915_private *i915 = to_i915(connector->base.dev);

	if (drm_connector_is_unregistered(&connector->base))
		return;

	if (!intel_hdcp2_check_link(connector))
		queue_delayed_work(i915->unordered_wq, &hdcp->check_work,
				   DRM_HDCP2_CHECK_PERIOD_MS);
	else if (!intel_hdcp_check_link(connector))
		queue_delayed_work(i915->unordered_wq, &hdcp->check_work,
				   DRM_HDCP_CHECK_PERIOD_MS);
}

static int i915_hdcp_component_bind(struct device *i915_kdev,
				    struct device *mei_kdev, void *data)
{
	struct drm_i915_private *i915 = kdev_to_i915(i915_kdev);

	drm_dbg(&i915->drm, "I915 HDCP comp bind\n");
	mutex_lock(&i915->display.hdcp.hdcp_mutex);
	i915->display.hdcp.arbiter = (struct i915_hdcp_arbiter *)data;
	i915->display.hdcp.arbiter->hdcp_dev = mei_kdev;
	mutex_unlock(&i915->display.hdcp.hdcp_mutex);

	return 0;
}

static void i915_hdcp_component_unbind(struct device *i915_kdev,
				       struct device *mei_kdev, void *data)
{
	struct drm_i915_private *i915 = kdev_to_i915(i915_kdev);

	drm_dbg(&i915->drm, "I915 HDCP comp unbind\n");
	mutex_lock(&i915->display.hdcp.hdcp_mutex);
	i915->display.hdcp.arbiter = NULL;
	mutex_unlock(&i915->display.hdcp.hdcp_mutex);
}

static const struct component_ops i915_hdcp_ops = {
	.bind   = i915_hdcp_component_bind,
	.unbind = i915_hdcp_component_unbind,
};

static enum hdcp_ddi intel_get_hdcp_ddi_index(enum port port)
{
	switch (port) {
	case PORT_A:
		return HDCP_DDI_A;
	case PORT_B ... PORT_F:
		return (enum hdcp_ddi)port;
	default:
		return HDCP_DDI_INVALID_PORT;
	}
}

static enum hdcp_transcoder intel_get_hdcp_transcoder(enum transcoder cpu_transcoder)
{
	switch (cpu_transcoder) {
	case TRANSCODER_A ... TRANSCODER_D:
		return (enum hdcp_transcoder)(cpu_transcoder | 0x10);
	default:  
		return HDCP_INVALID_TRANSCODER;
	}
}

static int initialize_hdcp_port_data(struct intel_connector *connector,
				     struct intel_digital_port *dig_port,
				     const struct intel_hdcp_shim *shim)
{
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct hdcp_port_data *data = &dig_port->hdcp_port_data;
	enum port port = dig_port->base.port;

	if (DISPLAY_VER(i915) < 12)
		data->hdcp_ddi = intel_get_hdcp_ddi_index(port);
	else
		 
		data->hdcp_ddi = HDCP_DDI_INVALID_PORT;

	 
	data->hdcp_transcoder = HDCP_INVALID_TRANSCODER;

	data->port_type = (u8)HDCP_PORT_TYPE_INTEGRATED;
	data->protocol = (u8)shim->protocol;

	if (!data->streams)
		data->streams = kcalloc(INTEL_NUM_PIPES(i915),
					sizeof(struct hdcp2_streamid_type),
					GFP_KERNEL);
	if (!data->streams) {
		drm_err(&i915->drm, "Out of Memory\n");
		return -ENOMEM;
	}

	return 0;
}

static bool is_hdcp2_supported(struct drm_i915_private *i915)
{
	if (intel_hdcp_gsc_cs_required(i915))
		return true;

	if (!IS_ENABLED(CONFIG_INTEL_MEI_HDCP))
		return false;

	return (DISPLAY_VER(i915) >= 10 ||
		IS_KABYLAKE(i915) ||
		IS_COFFEELAKE(i915) ||
		IS_COMETLAKE(i915));
}

void intel_hdcp_component_init(struct drm_i915_private *i915)
{
	int ret;

	if (!is_hdcp2_supported(i915))
		return;

	mutex_lock(&i915->display.hdcp.hdcp_mutex);
	drm_WARN_ON(&i915->drm, i915->display.hdcp.comp_added);

	i915->display.hdcp.comp_added = true;
	mutex_unlock(&i915->display.hdcp.hdcp_mutex);
	if (intel_hdcp_gsc_cs_required(i915))
		ret = intel_hdcp_gsc_init(i915);
	else
		ret = component_add_typed(i915->drm.dev, &i915_hdcp_ops,
					  I915_COMPONENT_HDCP);

	if (ret < 0) {
		drm_dbg_kms(&i915->drm, "Failed at fw component add(%d)\n",
			    ret);
		mutex_lock(&i915->display.hdcp.hdcp_mutex);
		i915->display.hdcp.comp_added = false;
		mutex_unlock(&i915->display.hdcp.hdcp_mutex);
		return;
	}
}

static void intel_hdcp2_init(struct intel_connector *connector,
			     struct intel_digital_port *dig_port,
			     const struct intel_hdcp_shim *shim)
{
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct intel_hdcp *hdcp = &connector->hdcp;
	int ret;

	ret = initialize_hdcp_port_data(connector, dig_port, shim);
	if (ret) {
		drm_dbg_kms(&i915->drm, "Mei hdcp data init failed\n");
		return;
	}

	hdcp->hdcp2_supported = true;
}

int intel_hdcp_init(struct intel_connector *connector,
		    struct intel_digital_port *dig_port,
		    const struct intel_hdcp_shim *shim)
{
	struct drm_i915_private *i915 = to_i915(connector->base.dev);
	struct intel_hdcp *hdcp = &connector->hdcp;
	int ret;

	if (!shim)
		return -EINVAL;

	if (is_hdcp2_supported(i915))
		intel_hdcp2_init(connector, dig_port, shim);

	ret =
	drm_connector_attach_content_protection_property(&connector->base,
							 hdcp->hdcp2_supported);
	if (ret) {
		hdcp->hdcp2_supported = false;
		kfree(dig_port->hdcp_port_data.streams);
		return ret;
	}

	hdcp->shim = shim;
	mutex_init(&hdcp->mutex);
	INIT_DELAYED_WORK(&hdcp->check_work, intel_hdcp_check_work);
	INIT_WORK(&hdcp->prop_work, intel_hdcp_prop_work);
	init_waitqueue_head(&hdcp->cp_irq_queue);

	return 0;
}

static int
intel_hdcp_set_streams(struct intel_digital_port *dig_port,
		       struct intel_atomic_state *state)
{
	struct drm_connector_list_iter conn_iter;
	struct intel_digital_port *conn_dig_port;
	struct intel_connector *connector;
	struct drm_i915_private *i915 = to_i915(dig_port->base.base.dev);
	struct hdcp_port_data *data = &dig_port->hdcp_port_data;

	if (!intel_encoder_is_mst(&dig_port->base)) {
		data->k = 1;
		data->streams[0].stream_id = 0;
		return 0;
	}

	data->k = 0;

	drm_connector_list_iter_begin(&i915->drm, &conn_iter);
	for_each_intel_connector_iter(connector, &conn_iter) {
		if (connector->base.status == connector_status_disconnected)
			continue;

		if (!intel_encoder_is_mst(intel_attached_encoder(connector)))
			continue;

		conn_dig_port = intel_attached_dig_port(connector);
		if (conn_dig_port != dig_port)
			continue;

		data->streams[data->k].stream_id =
			intel_conn_to_vcpi(&state->base, connector);
		data->k++;

		 
		if (dig_port->dp.active_mst_links <= 1)
			break;
	}
	drm_connector_list_iter_end(&conn_iter);

	if (drm_WARN_ON(&i915->drm, data->k > INTEL_NUM_PIPES(i915) || data->k == 0))
		return -EINVAL;

	return 0;
}

int intel_hdcp_enable(struct intel_atomic_state *state,
		      struct intel_encoder *encoder,
		      const struct intel_crtc_state *pipe_config,
		      const struct drm_connector_state *conn_state)
{
	struct drm_i915_private *i915 = to_i915(encoder->base.dev);
	struct intel_connector *connector =
		to_intel_connector(conn_state->connector);
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct intel_hdcp *hdcp = &connector->hdcp;
	unsigned long check_link_interval = DRM_HDCP_CHECK_PERIOD_MS;
	int ret = -EINVAL;

	if (!hdcp->shim)
		return -ENOENT;

	if (!connector->encoder) {
		drm_err(&i915->drm, "[%s:%d] encoder is not initialized\n",
			connector->base.name, connector->base.base.id);
		return -ENODEV;
	}

	mutex_lock(&hdcp->mutex);
	mutex_lock(&dig_port->hdcp_mutex);
	drm_WARN_ON(&i915->drm,
		    hdcp->value == DRM_MODE_CONTENT_PROTECTION_ENABLED);
	hdcp->content_type = (u8)conn_state->hdcp_content_type;

	if (intel_crtc_has_type(pipe_config, INTEL_OUTPUT_DP_MST)) {
		hdcp->cpu_transcoder = pipe_config->mst_master_transcoder;
		hdcp->stream_transcoder = pipe_config->cpu_transcoder;
	} else {
		hdcp->cpu_transcoder = pipe_config->cpu_transcoder;
		hdcp->stream_transcoder = INVALID_TRANSCODER;
	}

	if (DISPLAY_VER(i915) >= 12)
		dig_port->hdcp_port_data.hdcp_transcoder =
			intel_get_hdcp_transcoder(hdcp->cpu_transcoder);

	 
	if (intel_hdcp2_capable(connector)) {
		ret = intel_hdcp_set_streams(dig_port, state);
		if (!ret) {
			ret = _intel_hdcp2_enable(connector);
			if (!ret)
				check_link_interval =
					DRM_HDCP2_CHECK_PERIOD_MS;
		} else {
			drm_dbg_kms(&i915->drm,
				    "Set content streams failed: (%d)\n",
				    ret);
		}
	}

	 
	if (ret && intel_hdcp_capable(connector) &&
	    hdcp->content_type != DRM_MODE_HDCP_CONTENT_TYPE1) {
		ret = _intel_hdcp_enable(connector);
	}

	if (!ret) {
		queue_delayed_work(i915->unordered_wq, &hdcp->check_work,
				   check_link_interval);
		intel_hdcp_update_value(connector,
					DRM_MODE_CONTENT_PROTECTION_ENABLED,
					true);
	}

	mutex_unlock(&dig_port->hdcp_mutex);
	mutex_unlock(&hdcp->mutex);
	return ret;
}

int intel_hdcp_disable(struct intel_connector *connector)
{
	struct intel_digital_port *dig_port = intel_attached_dig_port(connector);
	struct intel_hdcp *hdcp = &connector->hdcp;
	int ret = 0;

	if (!hdcp->shim)
		return -ENOENT;

	mutex_lock(&hdcp->mutex);
	mutex_lock(&dig_port->hdcp_mutex);

	if (hdcp->value == DRM_MODE_CONTENT_PROTECTION_UNDESIRED)
		goto out;

	intel_hdcp_update_value(connector,
				DRM_MODE_CONTENT_PROTECTION_UNDESIRED, false);
	if (hdcp->hdcp2_encrypted)
		ret = _intel_hdcp2_disable(connector, false);
	else if (hdcp->hdcp_encrypted)
		ret = _intel_hdcp_disable(connector);

out:
	mutex_unlock(&dig_port->hdcp_mutex);
	mutex_unlock(&hdcp->mutex);
	cancel_delayed_work_sync(&hdcp->check_work);
	return ret;
}

void intel_hdcp_update_pipe(struct intel_atomic_state *state,
			    struct intel_encoder *encoder,
			    const struct intel_crtc_state *crtc_state,
			    const struct drm_connector_state *conn_state)
{
	struct intel_connector *connector =
				to_intel_connector(conn_state->connector);
	struct intel_hdcp *hdcp = &connector->hdcp;
	bool content_protection_type_changed, desired_and_not_enabled = false;
	struct drm_i915_private *i915 = to_i915(connector->base.dev);

	if (!connector->hdcp.shim)
		return;

	content_protection_type_changed =
		(conn_state->hdcp_content_type != hdcp->content_type &&
		 conn_state->content_protection !=
		 DRM_MODE_CONTENT_PROTECTION_UNDESIRED);

	 
	if (conn_state->content_protection ==
	    DRM_MODE_CONTENT_PROTECTION_UNDESIRED ||
	    content_protection_type_changed)
		intel_hdcp_disable(connector);

	 
	if (content_protection_type_changed) {
		mutex_lock(&hdcp->mutex);
		hdcp->value = DRM_MODE_CONTENT_PROTECTION_DESIRED;
		drm_connector_get(&connector->base);
		queue_work(i915->unordered_wq, &hdcp->prop_work);
		mutex_unlock(&hdcp->mutex);
	}

	if (conn_state->content_protection ==
	    DRM_MODE_CONTENT_PROTECTION_DESIRED) {
		mutex_lock(&hdcp->mutex);
		 
		desired_and_not_enabled =
			hdcp->value != DRM_MODE_CONTENT_PROTECTION_ENABLED;
		mutex_unlock(&hdcp->mutex);
		 
		if (!desired_and_not_enabled && !content_protection_type_changed) {
			drm_connector_get(&connector->base);
			queue_work(i915->unordered_wq, &hdcp->prop_work);
		}
	}

	if (desired_and_not_enabled || content_protection_type_changed)
		intel_hdcp_enable(state, encoder, crtc_state, conn_state);
}

void intel_hdcp_component_fini(struct drm_i915_private *i915)
{
	mutex_lock(&i915->display.hdcp.hdcp_mutex);
	if (!i915->display.hdcp.comp_added) {
		mutex_unlock(&i915->display.hdcp.hdcp_mutex);
		return;
	}

	i915->display.hdcp.comp_added = false;
	mutex_unlock(&i915->display.hdcp.hdcp_mutex);

	if (intel_hdcp_gsc_cs_required(i915))
		intel_hdcp_gsc_fini(i915);
	else
		component_del(i915->drm.dev, &i915_hdcp_ops);
}

void intel_hdcp_cleanup(struct intel_connector *connector)
{
	struct intel_hdcp *hdcp = &connector->hdcp;

	if (!hdcp->shim)
		return;

	 
	drm_WARN_ON(connector->base.dev,
		connector->base.registration_state == DRM_CONNECTOR_REGISTERED);

	 
	cancel_delayed_work_sync(&hdcp->check_work);

	 
	drm_WARN_ON(connector->base.dev, work_pending(&hdcp->prop_work));

	mutex_lock(&hdcp->mutex);
	hdcp->shim = NULL;
	mutex_unlock(&hdcp->mutex);
}

void intel_hdcp_atomic_check(struct drm_connector *connector,
			     struct drm_connector_state *old_state,
			     struct drm_connector_state *new_state)
{
	u64 old_cp = old_state->content_protection;
	u64 new_cp = new_state->content_protection;
	struct drm_crtc_state *crtc_state;

	if (!new_state->crtc) {
		 
		if (old_cp == DRM_MODE_CONTENT_PROTECTION_ENABLED)
			new_state->content_protection =
				DRM_MODE_CONTENT_PROTECTION_DESIRED;
		return;
	}

	crtc_state = drm_atomic_get_new_crtc_state(new_state->state,
						   new_state->crtc);
	 
	if (drm_atomic_crtc_needs_modeset(crtc_state) &&
	    (old_cp == DRM_MODE_CONTENT_PROTECTION_ENABLED &&
	    new_cp != DRM_MODE_CONTENT_PROTECTION_UNDESIRED))
		new_state->content_protection =
			DRM_MODE_CONTENT_PROTECTION_DESIRED;

	 
	if (old_cp == new_cp ||
	    (old_cp == DRM_MODE_CONTENT_PROTECTION_DESIRED &&
	     new_cp == DRM_MODE_CONTENT_PROTECTION_ENABLED)) {
		if (old_state->hdcp_content_type ==
				new_state->hdcp_content_type)
			return;
	}

	crtc_state->mode_changed = true;
}

 
void intel_hdcp_handle_cp_irq(struct intel_connector *connector)
{
	struct intel_hdcp *hdcp = &connector->hdcp;
	struct drm_i915_private *i915 = to_i915(connector->base.dev);

	if (!hdcp->shim)
		return;

	atomic_inc(&connector->hdcp.cp_irq_count);
	wake_up_all(&connector->hdcp.cp_irq_queue);

	queue_delayed_work(i915->unordered_wq, &hdcp->check_work, 0);
}
