 

#include <drm/display/drm_dp_helper.h>

#include "nouveau_drv.h"
#include "nouveau_connector.h"
#include "nouveau_encoder.h"
#include "nouveau_crtc.h"

#include <nvif/if0011.h>

MODULE_PARM_DESC(mst, "Enable DisplayPort multi-stream (default: enabled)");
static int nouveau_mst = 1;
module_param_named(mst, nouveau_mst, int, 0400);

static bool
nouveau_dp_has_sink_count(struct drm_connector *connector,
			  struct nouveau_encoder *outp)
{
	return drm_dp_read_sink_count_cap(connector, outp->dp.dpcd, &outp->dp.desc);
}

static enum drm_connector_status
nouveau_dp_probe_dpcd(struct nouveau_connector *nv_connector,
		      struct nouveau_encoder *outp)
{
	struct drm_connector *connector = &nv_connector->base;
	struct drm_dp_aux *aux = &nv_connector->aux;
	struct nv50_mstm *mstm = NULL;
	enum drm_connector_status status = connector_status_disconnected;
	int ret;
	u8 *dpcd = outp->dp.dpcd;

	ret = drm_dp_read_dpcd_caps(aux, dpcd);
	if (ret < 0)
		goto out;

	ret = drm_dp_read_desc(aux, &outp->dp.desc, drm_dp_is_branch(dpcd));
	if (ret < 0)
		goto out;

	if (nouveau_mst) {
		mstm = outp->dp.mstm;
		if (mstm)
			mstm->can_mst = drm_dp_read_mst_cap(aux, dpcd);
	}

	if (nouveau_dp_has_sink_count(connector, outp)) {
		ret = drm_dp_read_sink_count(aux);
		if (ret < 0)
			goto out;

		outp->dp.sink_count = ret;

		 
		if (!outp->dp.sink_count)
			return connector_status_disconnected;
	}

	ret = drm_dp_read_downstream_info(aux, dpcd,
					  outp->dp.downstream_ports);
	if (ret < 0)
		goto out;

	status = connector_status_connected;
out:
	if (status != connector_status_connected) {
		 
		outp->dp.sink_count = 0;
	}
	return status;
}

int
nouveau_dp_detect(struct nouveau_connector *nv_connector,
		  struct nouveau_encoder *nv_encoder)
{
	struct drm_device *dev = nv_encoder->base.base.dev;
	struct nouveau_drm *drm = nouveau_drm(dev);
	struct drm_connector *connector = &nv_connector->base;
	struct nv50_mstm *mstm = nv_encoder->dp.mstm;
	enum drm_connector_status status;
	u8 *dpcd = nv_encoder->dp.dpcd;
	int ret = NOUVEAU_DP_NONE, hpd;

	 
	if (connector->connector_type == DRM_MODE_CONNECTOR_eDP &&
	    dpcd[DP_DPCD_REV] != 0)
		return NOUVEAU_DP_SST;

	mutex_lock(&nv_encoder->dp.hpd_irq_lock);
	if (mstm) {
		 
		if (mstm->suspended) {
			if (mstm->is_mst)
				ret = NOUVEAU_DP_MST;
			else if (connector->status ==
				 connector_status_connected)
				ret = NOUVEAU_DP_SST;

			goto out;
		}
	}

	 
	hpd = nvif_conn_hpd_status(&nv_connector->conn);
	if (hpd == NVIF_CONN_HPD_STATUS_NOT_PRESENT) {
		nvif_outp_dp_aux_pwr(&nv_encoder->outp, false);
		goto out;
	}
	nvif_outp_dp_aux_pwr(&nv_encoder->outp, true);

	status = nouveau_dp_probe_dpcd(nv_connector, nv_encoder);
	if (status == connector_status_disconnected) {
		nvif_outp_dp_aux_pwr(&nv_encoder->outp, false);
		goto out;
	}

	 
	if (mstm && mstm->can_mst && mstm->is_mst) {
		ret = NOUVEAU_DP_MST;
		goto out;
	}

	nv_encoder->dp.link_bw = 27000 * dpcd[DP_MAX_LINK_RATE];
	nv_encoder->dp.link_nr =
		dpcd[DP_MAX_LANE_COUNT] & DP_MAX_LANE_COUNT_MASK;

	if (connector->connector_type == DRM_MODE_CONNECTOR_eDP && dpcd[DP_DPCD_REV] >= 0x13) {
		struct drm_dp_aux *aux = &nv_connector->aux;
		int ret, i;
		u8 sink_rates[16];

		ret = drm_dp_dpcd_read(aux, DP_SUPPORTED_LINK_RATES, sink_rates, sizeof(sink_rates));
		if (ret == sizeof(sink_rates)) {
			for (i = 0; i < ARRAY_SIZE(sink_rates); i += 2) {
				int val = ((sink_rates[i + 1] << 8) | sink_rates[i]) * 200 / 10;
				if (val && (i == 0 || val > nv_encoder->dp.link_bw))
					nv_encoder->dp.link_bw = val;
			}
		}
	}

	NV_DEBUG(drm, "display: %dx%d dpcd 0x%02x\n",
		 nv_encoder->dp.link_nr, nv_encoder->dp.link_bw,
		 dpcd[DP_DPCD_REV]);
	NV_DEBUG(drm, "encoder: %dx%d\n",
		 nv_encoder->dcb->dpconf.link_nr,
		 nv_encoder->dcb->dpconf.link_bw);

	if (nv_encoder->dcb->dpconf.link_nr < nv_encoder->dp.link_nr)
		nv_encoder->dp.link_nr = nv_encoder->dcb->dpconf.link_nr;
	if (nv_encoder->dcb->dpconf.link_bw < nv_encoder->dp.link_bw)
		nv_encoder->dp.link_bw = nv_encoder->dcb->dpconf.link_bw;

	NV_DEBUG(drm, "maximum: %dx%d\n",
		 nv_encoder->dp.link_nr, nv_encoder->dp.link_bw);

	if (mstm && mstm->can_mst) {
		ret = nv50_mstm_detect(nv_encoder);
		if (ret == 1) {
			ret = NOUVEAU_DP_MST;
			goto out;
		} else if (ret != 0) {
			nvif_outp_dp_aux_pwr(&nv_encoder->outp, false);
			goto out;
		}
	}
	ret = NOUVEAU_DP_SST;

out:
	if (mstm && !mstm->suspended && ret != NOUVEAU_DP_MST)
		nv50_mstm_remove(mstm);

	mutex_unlock(&nv_encoder->dp.hpd_irq_lock);
	return ret;
}

bool
nouveau_dp_link_check(struct nouveau_connector *nv_connector)
{
	struct nouveau_encoder *nv_encoder = find_encoder(&nv_connector->base, DCB_OUTPUT_DP);

	if (!nv_encoder || nv_encoder->outp.or.id < 0)
		return true;

	return nvif_outp_dp_retrain(&nv_encoder->outp) == 0;
}

void
nouveau_dp_irq(struct work_struct *work)
{
	struct nouveau_connector *nv_connector =
		container_of(work, typeof(*nv_connector), irq_work);
	struct drm_connector *connector = &nv_connector->base;
	struct nouveau_encoder *outp = find_encoder(connector, DCB_OUTPUT_DP);
	struct nouveau_drm *drm = nouveau_drm(outp->base.base.dev);
	struct nv50_mstm *mstm;
	u64 hpd = 0;
	int ret;

	if (!outp)
		return;

	mstm = outp->dp.mstm;
	NV_DEBUG(drm, "service %s\n", connector->name);

	mutex_lock(&outp->dp.hpd_irq_lock);

	if (mstm && mstm->is_mst) {
		if (!nv50_mstm_service(drm, nv_connector, mstm))
			hpd |= NVIF_CONN_EVENT_V0_UNPLUG;
	} else {
		drm_dp_cec_irq(&nv_connector->aux);

		if (nouveau_dp_has_sink_count(connector, outp)) {
			ret = drm_dp_read_sink_count(&nv_connector->aux);
			if (ret != outp->dp.sink_count)
				hpd |= NVIF_CONN_EVENT_V0_PLUG;
			if (ret >= 0)
				outp->dp.sink_count = ret;
		}
	}

	mutex_unlock(&outp->dp.hpd_irq_lock);

	nouveau_connector_hpd(nv_connector, NVIF_CONN_EVENT_V0_IRQ | hpd);
}

 
enum drm_mode_status
nv50_dp_mode_valid(struct nouveau_encoder *outp,
		   const struct drm_display_mode *mode,
		   unsigned *out_clock)
{
	const unsigned int min_clock = 25000;
	unsigned int max_rate, mode_rate, ds_max_dotclock, clock = mode->clock;
	 
	const u8 bpp = 6 * 3;

	if (mode->flags & DRM_MODE_FLAG_INTERLACE && !outp->caps.dp_interlace)
		return MODE_NO_INTERLACE;

	if ((mode->flags & DRM_MODE_FLAG_3D_MASK) == DRM_MODE_FLAG_3D_FRAME_PACKING)
		clock *= 2;

	max_rate = outp->dp.link_nr * outp->dp.link_bw;
	mode_rate = DIV_ROUND_UP(clock * bpp, 8);
	if (mode_rate > max_rate)
		return MODE_CLOCK_HIGH;

	ds_max_dotclock = drm_dp_downstream_max_dotclock(outp->dp.dpcd, outp->dp.downstream_ports);
	if (ds_max_dotclock && clock > ds_max_dotclock)
		return MODE_CLOCK_HIGH;

	if (clock < min_clock)
		return MODE_CLOCK_LOW;

	if (out_clock)
		*out_clock = clock;

	return MODE_OK;
}
