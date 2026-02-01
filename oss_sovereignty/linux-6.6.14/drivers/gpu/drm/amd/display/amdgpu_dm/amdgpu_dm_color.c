 
#include "amdgpu.h"
#include "amdgpu_mode.h"
#include "amdgpu_dm.h"
#include "dc.h"
#include "modules/color/color_gamma.h"
#include "basics/conversion.h"

 

#define MAX_DRM_LUT_VALUE 0xFFFF

 
void amdgpu_dm_init_color_mod(void)
{
	setup_x_points_distribution();
}

 
static const struct drm_color_lut *
__extract_blob_lut(const struct drm_property_blob *blob, uint32_t *size)
{
	*size = blob ? drm_color_lut_size(blob) : 0;
	return blob ? (struct drm_color_lut *)blob->data : NULL;
}

 
static bool __is_lut_linear(const struct drm_color_lut *lut, uint32_t size)
{
	int i;
	uint32_t expected;
	int delta;

	for (i = 0; i < size; i++) {
		 
		if ((lut[i].red != lut[i].green) || (lut[i].green != lut[i].blue))
			return false;

		expected = i * MAX_DRM_LUT_VALUE / (size-1);

		 
		delta = lut[i].red - expected;
		if (delta < -1 || 1 < delta)
			return false;
	}
	return true;
}

 
static void __drm_lut_to_dc_gamma(const struct drm_color_lut *lut,
				  struct dc_gamma *gamma, bool is_legacy)
{
	uint32_t r, g, b;
	int i;

	if (is_legacy) {
		for (i = 0; i < MAX_COLOR_LEGACY_LUT_ENTRIES; i++) {
			r = drm_color_lut_extract(lut[i].red, 16);
			g = drm_color_lut_extract(lut[i].green, 16);
			b = drm_color_lut_extract(lut[i].blue, 16);

			gamma->entries.red[i] = dc_fixpt_from_int(r);
			gamma->entries.green[i] = dc_fixpt_from_int(g);
			gamma->entries.blue[i] = dc_fixpt_from_int(b);
		}
		return;
	}

	 
	for (i = 0; i < MAX_COLOR_LUT_ENTRIES; i++) {
		r = drm_color_lut_extract(lut[i].red, 16);
		g = drm_color_lut_extract(lut[i].green, 16);
		b = drm_color_lut_extract(lut[i].blue, 16);

		gamma->entries.red[i] = dc_fixpt_from_fraction(r, MAX_DRM_LUT_VALUE);
		gamma->entries.green[i] = dc_fixpt_from_fraction(g, MAX_DRM_LUT_VALUE);
		gamma->entries.blue[i] = dc_fixpt_from_fraction(b, MAX_DRM_LUT_VALUE);
	}
}

 
static void __drm_ctm_to_dc_matrix(const struct drm_color_ctm *ctm,
				   struct fixed31_32 *matrix)
{
	int64_t val;
	int i;

	 
	for (i = 0; i < 12; i++) {
		 
		if (i % 4 == 3) {
			matrix[i] = dc_fixpt_zero;
			continue;
		}

		 
		val = ctm->matrix[i - (i / 4)];
		 
		if (val & (1ULL << 63))
			val = -(val & ~(1ULL << 63));

		matrix[i].value = val;
	}
}

 
static int __set_legacy_tf(struct dc_transfer_func *func,
			   const struct drm_color_lut *lut, uint32_t lut_size,
			   bool has_rom)
{
	struct dc_gamma *gamma = NULL;
	struct calculate_buffer cal_buffer = {0};
	bool res;

	ASSERT(lut && lut_size == MAX_COLOR_LEGACY_LUT_ENTRIES);

	cal_buffer.buffer_index = -1;

	gamma = dc_create_gamma();
	if (!gamma)
		return -ENOMEM;

	gamma->type = GAMMA_RGB_256;
	gamma->num_entries = lut_size;
	__drm_lut_to_dc_gamma(lut, gamma, true);

	res = mod_color_calculate_regamma_params(func, gamma, true, has_rom,
						 NULL, &cal_buffer);

	dc_gamma_release(&gamma);

	return res ? 0 : -ENOMEM;
}

 
static int __set_output_tf(struct dc_transfer_func *func,
			   const struct drm_color_lut *lut, uint32_t lut_size,
			   bool has_rom)
{
	struct dc_gamma *gamma = NULL;
	struct calculate_buffer cal_buffer = {0};
	bool res;

	ASSERT(lut && lut_size == MAX_COLOR_LUT_ENTRIES);

	cal_buffer.buffer_index = -1;

	gamma = dc_create_gamma();
	if (!gamma)
		return -ENOMEM;

	gamma->num_entries = lut_size;
	__drm_lut_to_dc_gamma(lut, gamma, false);

	if (func->tf == TRANSFER_FUNCTION_LINEAR) {
		 
		gamma->type = GAMMA_CUSTOM;
		res = mod_color_calculate_degamma_params(NULL, func,
							gamma, true);
	} else {
		 
		gamma->type = GAMMA_CS_TFM_1D;
		res = mod_color_calculate_regamma_params(func, gamma, false,
							 has_rom, NULL, &cal_buffer);
	}

	dc_gamma_release(&gamma);

	return res ? 0 : -ENOMEM;
}

 
static int __set_input_tf(struct dc_transfer_func *func,
			  const struct drm_color_lut *lut, uint32_t lut_size)
{
	struct dc_gamma *gamma = NULL;
	bool res;

	gamma = dc_create_gamma();
	if (!gamma)
		return -ENOMEM;

	gamma->type = GAMMA_CUSTOM;
	gamma->num_entries = lut_size;

	__drm_lut_to_dc_gamma(lut, gamma, false);

	res = mod_color_calculate_degamma_params(NULL, func, gamma, true);
	dc_gamma_release(&gamma);

	return res ? 0 : -ENOMEM;
}

 
int amdgpu_dm_verify_lut_sizes(const struct drm_crtc_state *crtc_state)
{
	const struct drm_color_lut *lut = NULL;
	uint32_t size = 0;

	lut = __extract_blob_lut(crtc_state->degamma_lut, &size);
	if (lut && size != MAX_COLOR_LUT_ENTRIES) {
		DRM_DEBUG_DRIVER(
			"Invalid Degamma LUT size. Should be %u but got %u.\n",
			MAX_COLOR_LUT_ENTRIES, size);
		return -EINVAL;
	}

	lut = __extract_blob_lut(crtc_state->gamma_lut, &size);
	if (lut && size != MAX_COLOR_LUT_ENTRIES &&
	    size != MAX_COLOR_LEGACY_LUT_ENTRIES) {
		DRM_DEBUG_DRIVER(
			"Invalid Gamma LUT size. Should be %u (or %u for legacy) but got %u.\n",
			MAX_COLOR_LUT_ENTRIES, MAX_COLOR_LEGACY_LUT_ENTRIES,
			size);
		return -EINVAL;
	}

	return 0;
}

 
int amdgpu_dm_update_crtc_color_mgmt(struct dm_crtc_state *crtc)
{
	struct dc_stream_state *stream = crtc->stream;
	struct amdgpu_device *adev = drm_to_adev(crtc->base.state->dev);
	bool has_rom = adev->asic_type <= CHIP_RAVEN;
	struct drm_color_ctm *ctm = NULL;
	const struct drm_color_lut *degamma_lut, *regamma_lut;
	uint32_t degamma_size, regamma_size;
	bool has_regamma, has_degamma;
	bool is_legacy;
	int r;

	r = amdgpu_dm_verify_lut_sizes(&crtc->base);
	if (r)
		return r;

	degamma_lut = __extract_blob_lut(crtc->base.degamma_lut, &degamma_size);
	regamma_lut = __extract_blob_lut(crtc->base.gamma_lut, &regamma_size);

	has_degamma =
		degamma_lut && !__is_lut_linear(degamma_lut, degamma_size);

	has_regamma =
		regamma_lut && !__is_lut_linear(regamma_lut, regamma_size);

	is_legacy = regamma_size == MAX_COLOR_LEGACY_LUT_ENTRIES;

	 
	crtc->cm_has_degamma = false;
	crtc->cm_is_degamma_srgb = false;

	 
	if (is_legacy) {
		 
		crtc->cm_is_degamma_srgb = true;
		stream->out_transfer_func->type = TF_TYPE_DISTRIBUTED_POINTS;
		stream->out_transfer_func->tf = TRANSFER_FUNCTION_SRGB;

		r = __set_legacy_tf(stream->out_transfer_func, regamma_lut,
				    regamma_size, has_rom);
		if (r)
			return r;
	} else if (has_regamma) {
		 
		stream->out_transfer_func->type = TF_TYPE_DISTRIBUTED_POINTS;
		stream->out_transfer_func->tf = TRANSFER_FUNCTION_LINEAR;

		r = __set_output_tf(stream->out_transfer_func, regamma_lut,
				    regamma_size, has_rom);
		if (r)
			return r;
	} else {
		 
		stream->out_transfer_func->type = TF_TYPE_BYPASS;
		stream->out_transfer_func->tf = TRANSFER_FUNCTION_LINEAR;
	}

	 
	crtc->cm_has_degamma = has_degamma;

	 
	if (crtc->base.ctm) {
		ctm = (struct drm_color_ctm *)crtc->base.ctm->data;

		 
		__drm_ctm_to_dc_matrix(ctm, stream->gamut_remap_matrix.matrix);

		stream->gamut_remap_matrix.enable_remap = true;
		stream->csc_color_matrix.enable_adjustment = false;
	} else {
		 
		stream->gamut_remap_matrix.enable_remap = false;
		stream->csc_color_matrix.enable_adjustment = false;
	}

	return 0;
}

 
int amdgpu_dm_update_plane_color_mgmt(struct dm_crtc_state *crtc,
				      struct dc_plane_state *dc_plane_state)
{
	const struct drm_color_lut *degamma_lut;
	enum dc_transfer_func_predefined tf = TRANSFER_FUNCTION_SRGB;
	uint32_t degamma_size;
	int r;

	 
	switch (dc_plane_state->format) {
	case SURFACE_PIXEL_FORMAT_VIDEO_420_YCbCr:
	case SURFACE_PIXEL_FORMAT_VIDEO_420_YCrCb:
		 
		tf = TRANSFER_FUNCTION_BT709;
		break;
	default:
		break;
	}

	if (crtc->cm_has_degamma) {
		degamma_lut = __extract_blob_lut(crtc->base.degamma_lut,
						 &degamma_size);
		ASSERT(degamma_size == MAX_COLOR_LUT_ENTRIES);

		dc_plane_state->in_transfer_func->type =
			TF_TYPE_DISTRIBUTED_POINTS;

		 
		if (crtc->cm_is_degamma_srgb)
			dc_plane_state->in_transfer_func->tf = tf;
		else
			dc_plane_state->in_transfer_func->tf =
				TRANSFER_FUNCTION_LINEAR;

		r = __set_input_tf(dc_plane_state->in_transfer_func,
				   degamma_lut, degamma_size);
		if (r)
			return r;
	} else if (crtc->cm_is_degamma_srgb) {
		 
		dc_plane_state->in_transfer_func->type = TF_TYPE_PREDEFINED;
		dc_plane_state->in_transfer_func->tf = tf;

		if (tf != TRANSFER_FUNCTION_SRGB &&
		    !mod_color_calculate_degamma_params(NULL,
			    dc_plane_state->in_transfer_func, NULL, false))
			return -ENOMEM;
	} else {
		 
		dc_plane_state->in_transfer_func->type = TF_TYPE_BYPASS;
		dc_plane_state->in_transfer_func->tf = TRANSFER_FUNCTION_LINEAR;
	}

	return 0;
}
