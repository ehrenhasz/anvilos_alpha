#ifndef __DAL_BIOS_PARSER_TYPES_BIOS2_H__
#define __DAL_BIOS_PARSER_TYPES_BIOS2_H__
#include "dc_bios_types.h"
#include "bios_parser_helper.h"
struct atom_data_revision {
	uint32_t major;
	uint32_t minor;
};
struct object_info_table {
	struct atom_data_revision revision;
	union {
		struct display_object_info_table_v1_4 *v1_4;
		struct display_object_info_table_v1_5 *v1_5;
	};
};
enum spread_spectrum_id {
	SS_ID_UNKNOWN = 0,
	SS_ID_DP1 = 0xf1,
	SS_ID_DP2 = 0xf2,
	SS_ID_LVLINK_2700MHZ = 0xf3,
	SS_ID_LVLINK_1620MHZ = 0xf4
};
struct bios_parser {
	struct dc_bios base;
	struct object_info_table object_info_tbl;
	uint32_t object_info_tbl_offset;
	struct atom_master_data_table_v2_1 *master_data_tbl;
	const struct bios_parser_helper *bios_helper;
	const struct command_table_helper *cmd_helper;
	struct cmd_tbl cmd_tbl;
	bool remap_device_tags;
};
#define BP_FROM_DCB(dc_bios) \
	container_of(dc_bios, struct bios_parser, base)
#endif
