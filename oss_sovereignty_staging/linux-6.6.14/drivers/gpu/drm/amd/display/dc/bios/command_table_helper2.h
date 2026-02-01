 

#ifndef __DAL_COMMAND_TABLE_HELPER2_H__
#define __DAL_COMMAND_TABLE_HELPER2_H__

#if defined(CONFIG_DRM_AMD_DC_SI)
#include "dce60/command_table_helper_dce60.h"
#endif
#include "dce80/command_table_helper_dce80.h"
#include "dce110/command_table_helper_dce110.h"
#include "dce112/command_table_helper2_dce112.h"
#include "command_table_helper_struct.h"

bool dal_bios_parser_init_cmd_tbl_helper2(const struct command_table_helper **h,
	enum dce_version dce);

bool dal_cmd_table_helper_controller_id_to_atom2(
	enum controller_id id,
	uint8_t *atom_id);

uint32_t dal_cmd_table_helper_encoder_mode_bp_to_atom2(
	enum signal_type s,
	bool enable_dp_audio);

bool dal_cmd_table_helper_clock_source_id_to_ref_clk_src2(
	enum clock_source_id id,
	uint32_t *ref_clk_src_id);

uint8_t dal_cmd_table_helper_transmitter_bp_to_atom2(
	enum transmitter t);

uint8_t dal_cmd_table_helper_encoder_id_to_atom2(
	enum encoder_id id);
#endif
