 

#ifndef __DC_COMPRESSOR_DCE110_H__
#define __DC_COMPRESSOR_DCE110_H__

#include "../inc/compressor.h"

#define TO_DCE110_COMPRESSOR(compressor)\
	container_of(compressor, struct dce110_compressor, base)

struct dce110_compressor_reg_offsets {
	uint32_t dcp_offset;
	uint32_t dmif_offset;
};

struct dce110_compressor {
	struct compressor base;
	struct dce110_compressor_reg_offsets offsets;
};

struct compressor *dce110_compressor_create(struct dc_context *ctx);

void dce110_compressor_construct(struct dce110_compressor *cp110,
	struct dc_context *ctx);

void dce110_compressor_destroy(struct compressor **cp);

 
void dce110_compressor_power_up_fbc(struct compressor *cp);

void dce110_compressor_enable_fbc(struct compressor *cp,
	struct compr_addr_and_pitch_params *params);

void dce110_compressor_disable_fbc(struct compressor *cp);

void dce110_compressor_set_fbc_invalidation_triggers(struct compressor *cp,
	uint32_t fbc_trigger);

void dce110_compressor_program_compressed_surface_address_and_pitch(
	struct compressor *cp,
	struct compr_addr_and_pitch_params *params);

bool dce110_compressor_is_fbc_enabled_in_hw(struct compressor *cp,
	uint32_t *fbc_mapped_crtc_id);

 
void dce110_compressor_enable_lpt(struct compressor *cp);

void dce110_compressor_disable_lpt(struct compressor *cp);

void dce110_compressor_program_lpt_control(struct compressor *cp,
	struct compr_addr_and_pitch_params *params);

bool dce110_compressor_is_lpt_enabled_in_hw(struct compressor *cp);

void get_max_support_fbc_buffersize(unsigned int *max_x, unsigned int *max_y);

#endif

