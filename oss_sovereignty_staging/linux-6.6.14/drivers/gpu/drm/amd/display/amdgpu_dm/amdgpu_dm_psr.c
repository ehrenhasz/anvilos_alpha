 

#include "amdgpu_dm_psr.h"
#include "dc_dmub_srv.h"
#include "dc.h"
#include "dm_helpers.h"
#include "amdgpu_dm.h"
#include "modules/power/power_helpers.h"

static bool link_supports_psrsu(struct dc_link *link)
{
	struct dc *dc = link->ctx->dc;

	if (!dc->caps.dmcub_support)
		return false;

	if (dc->ctx->dce_version < DCN_VERSION_3_1)
		return false;

	if (!is_psr_su_specific_panel(link))
		return false;

	if (!link->dpcd_caps.alpm_caps.bits.AUX_WAKE_ALPM_CAP ||
	    !link->dpcd_caps.psr_info.psr_dpcd_caps.bits.Y_COORDINATE_REQUIRED)
		return false;

	if (link->dpcd_caps.psr_info.psr_dpcd_caps.bits.SU_GRANULARITY_REQUIRED &&
	    !link->dpcd_caps.psr_info.psr2_su_y_granularity_cap)
		return false;

	return dc_dmub_check_min_version(dc->ctx->dmub_srv->dmub);
}

 
void amdgpu_dm_set_psr_caps(struct dc_link *link)
{
	if (!(link->connector_signal & SIGNAL_TYPE_EDP)) {
		link->psr_settings.psr_feature_enabled = false;
		return;
	}

	if (link->type == dc_connection_none) {
		link->psr_settings.psr_feature_enabled = false;
		return;
	}

	if (link->dpcd_caps.psr_info.psr_version == 0) {
		link->psr_settings.psr_version = DC_PSR_VERSION_UNSUPPORTED;
		link->psr_settings.psr_feature_enabled = false;

	} else {
		if (link_supports_psrsu(link))
			link->psr_settings.psr_version = DC_PSR_VERSION_SU_1;
		else
			link->psr_settings.psr_version = DC_PSR_VERSION_1;

		link->psr_settings.psr_feature_enabled = true;
	}

	DRM_INFO("PSR support %d, DC PSR ver %d, sink PSR ver %d DPCD caps 0x%x su_y_granularity %d\n",
		link->psr_settings.psr_feature_enabled,
		link->psr_settings.psr_version,
		link->dpcd_caps.psr_info.psr_version,
		link->dpcd_caps.psr_info.psr_dpcd_caps.raw,
		link->dpcd_caps.psr_info.psr2_su_y_granularity_cap);

}

 
bool amdgpu_dm_link_setup_psr(struct dc_stream_state *stream)
{
	struct dc_link *link = NULL;
	struct psr_config psr_config = {0};
	struct psr_context psr_context = {0};
	struct dc *dc = NULL;
	bool ret = false;

	if (stream == NULL)
		return false;

	link = stream->link;
	dc = link->ctx->dc;

	if (link->psr_settings.psr_version != DC_PSR_VERSION_UNSUPPORTED) {
		mod_power_calc_psr_configs(&psr_config, link, stream);

		 
		psr_config.allow_smu_optimizations =
			(amdgpu_dc_feature_mask & DC_PSR_ALLOW_SMU_OPT) &&
			mod_power_only_edp(dc->current_state, stream);
		psr_config.allow_multi_disp_optimizations =
			(amdgpu_dc_feature_mask & DC_PSR_ALLOW_MULTI_DISP_OPT);

		if (!psr_su_set_dsc_slice_height(dc, link, stream, &psr_config))
			return false;

		ret = dc_link_setup_psr(link, stream, &psr_config, &psr_context);

	}
	DRM_DEBUG_DRIVER("PSR link: %d\n",	link->psr_settings.psr_feature_enabled);

	return ret;
}

 
bool amdgpu_dm_psr_enable(struct dc_stream_state *stream)
{
	struct dc_link *link = stream->link;
	unsigned int vsync_rate_hz = 0;
	struct dc_static_screen_params params = {0};
	 
	
	unsigned int num_frames_static = 2;
	unsigned int power_opt = 0;
	bool psr_enable = true;

	DRM_DEBUG_DRIVER("Enabling psr...\n");

	vsync_rate_hz = div64_u64(div64_u64((
			stream->timing.pix_clk_100hz * 100),
			stream->timing.v_total),
			stream->timing.h_total);

	 
	if (vsync_rate_hz != 0) {
		unsigned int frame_time_microsec = 1000000 / vsync_rate_hz;

		num_frames_static = (30000 / frame_time_microsec) + 1;
	}

	params.triggers.cursor_update = true;
	params.triggers.overlay_update = true;
	params.triggers.surface_update = true;
	params.num_frames = num_frames_static;

	dc_stream_set_static_screen_params(link->ctx->dc,
					   &stream, 1,
					   &params);

	 
	if (link->psr_settings.psr_version < DC_PSR_VERSION_SU_1)
		power_opt |= psr_power_opt_z10_static_screen;

	return dc_link_set_psr_allow_active(link, &psr_enable, false, false, &power_opt);
}

 
bool amdgpu_dm_psr_disable(struct dc_stream_state *stream)
{
	unsigned int power_opt = 0;
	bool psr_enable = false;

	DRM_DEBUG_DRIVER("Disabling psr...\n");

	return dc_link_set_psr_allow_active(stream->link, &psr_enable, true, false, &power_opt);
}

 
bool amdgpu_dm_psr_disable_all(struct amdgpu_display_manager *dm)
{
	DRM_DEBUG_DRIVER("Disabling psr if psr is enabled on any stream\n");
	return dc_set_psr_allow_active(dm->dc, false);
}

