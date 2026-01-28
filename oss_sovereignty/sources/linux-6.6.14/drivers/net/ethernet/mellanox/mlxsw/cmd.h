


#ifndef _MLXSW_CMD_H
#define _MLXSW_CMD_H

#include "item.h"

#define MLXSW_CMD_MBOX_SIZE	4096

static inline char *mlxsw_cmd_mbox_alloc(void)
{
	return kzalloc(MLXSW_CMD_MBOX_SIZE, GFP_KERNEL);
}

static inline void mlxsw_cmd_mbox_free(char *mbox)
{
	kfree(mbox);
}

static inline void mlxsw_cmd_mbox_zero(char *mbox)
{
	memset(mbox, 0, MLXSW_CMD_MBOX_SIZE);
}

struct mlxsw_core;

int mlxsw_cmd_exec(struct mlxsw_core *mlxsw_core, u16 opcode, u8 opcode_mod,
		   u32 in_mod, bool out_mbox_direct, bool reset_ok,
		   char *in_mbox, size_t in_mbox_size,
		   char *out_mbox, size_t out_mbox_size);

static inline int mlxsw_cmd_exec_in(struct mlxsw_core *mlxsw_core, u16 opcode,
				    u8 opcode_mod, u32 in_mod, char *in_mbox,
				    size_t in_mbox_size)
{
	return mlxsw_cmd_exec(mlxsw_core, opcode, opcode_mod, in_mod, false,
			      false, in_mbox, in_mbox_size, NULL, 0);
}

static inline int mlxsw_cmd_exec_out(struct mlxsw_core *mlxsw_core, u16 opcode,
				     u8 opcode_mod, u32 in_mod,
				     bool out_mbox_direct,
				     char *out_mbox, size_t out_mbox_size)
{
	return mlxsw_cmd_exec(mlxsw_core, opcode, opcode_mod, in_mod,
			      out_mbox_direct, false, NULL, 0,
			      out_mbox, out_mbox_size);
}

static inline int mlxsw_cmd_exec_none(struct mlxsw_core *mlxsw_core, u16 opcode,
				      u8 opcode_mod, u32 in_mod)
{
	return mlxsw_cmd_exec(mlxsw_core, opcode, opcode_mod, in_mod, false,
			      false, NULL, 0, NULL, 0);
}

enum mlxsw_cmd_opcode {
	MLXSW_CMD_OPCODE_QUERY_FW		= 0x004,
	MLXSW_CMD_OPCODE_QUERY_BOARDINFO	= 0x006,
	MLXSW_CMD_OPCODE_QUERY_AQ_CAP		= 0x003,
	MLXSW_CMD_OPCODE_MAP_FA			= 0xFFF,
	MLXSW_CMD_OPCODE_UNMAP_FA		= 0xFFE,
	MLXSW_CMD_OPCODE_CONFIG_PROFILE		= 0x100,
	MLXSW_CMD_OPCODE_ACCESS_REG		= 0x040,
	MLXSW_CMD_OPCODE_SW2HW_DQ		= 0x201,
	MLXSW_CMD_OPCODE_HW2SW_DQ		= 0x202,
	MLXSW_CMD_OPCODE_2ERR_DQ		= 0x01E,
	MLXSW_CMD_OPCODE_QUERY_DQ		= 0x022,
	MLXSW_CMD_OPCODE_SW2HW_CQ		= 0x016,
	MLXSW_CMD_OPCODE_HW2SW_CQ		= 0x017,
	MLXSW_CMD_OPCODE_QUERY_CQ		= 0x018,
	MLXSW_CMD_OPCODE_SW2HW_EQ		= 0x013,
	MLXSW_CMD_OPCODE_HW2SW_EQ		= 0x014,
	MLXSW_CMD_OPCODE_QUERY_EQ		= 0x015,
	MLXSW_CMD_OPCODE_QUERY_RESOURCES	= 0x101,
};

static inline const char *mlxsw_cmd_opcode_str(u16 opcode)
{
	switch (opcode) {
	case MLXSW_CMD_OPCODE_QUERY_FW:
		return "QUERY_FW";
	case MLXSW_CMD_OPCODE_QUERY_BOARDINFO:
		return "QUERY_BOARDINFO";
	case MLXSW_CMD_OPCODE_QUERY_AQ_CAP:
		return "QUERY_AQ_CAP";
	case MLXSW_CMD_OPCODE_MAP_FA:
		return "MAP_FA";
	case MLXSW_CMD_OPCODE_UNMAP_FA:
		return "UNMAP_FA";
	case MLXSW_CMD_OPCODE_CONFIG_PROFILE:
		return "CONFIG_PROFILE";
	case MLXSW_CMD_OPCODE_ACCESS_REG:
		return "ACCESS_REG";
	case MLXSW_CMD_OPCODE_SW2HW_DQ:
		return "SW2HW_DQ";
	case MLXSW_CMD_OPCODE_HW2SW_DQ:
		return "HW2SW_DQ";
	case MLXSW_CMD_OPCODE_2ERR_DQ:
		return "2ERR_DQ";
	case MLXSW_CMD_OPCODE_QUERY_DQ:
		return "QUERY_DQ";
	case MLXSW_CMD_OPCODE_SW2HW_CQ:
		return "SW2HW_CQ";
	case MLXSW_CMD_OPCODE_HW2SW_CQ:
		return "HW2SW_CQ";
	case MLXSW_CMD_OPCODE_QUERY_CQ:
		return "QUERY_CQ";
	case MLXSW_CMD_OPCODE_SW2HW_EQ:
		return "SW2HW_EQ";
	case MLXSW_CMD_OPCODE_HW2SW_EQ:
		return "HW2SW_EQ";
	case MLXSW_CMD_OPCODE_QUERY_EQ:
		return "QUERY_EQ";
	case MLXSW_CMD_OPCODE_QUERY_RESOURCES:
		return "QUERY_RESOURCES";
	default:
		return "*UNKNOWN*";
	}
}

enum mlxsw_cmd_status {
	
	MLXSW_CMD_STATUS_OK		= 0x00,
	
	MLXSW_CMD_STATUS_INTERNAL_ERR	= 0x01,
	
	MLXSW_CMD_STATUS_BAD_OP		= 0x02,
	
	MLXSW_CMD_STATUS_BAD_PARAM	= 0x03,
	
	MLXSW_CMD_STATUS_BAD_SYS_STATE	= 0x04,
	
	MLXSW_CMD_STATUS_BAD_RESOURCE	= 0x05,
	
	MLXSW_CMD_STATUS_RESOURCE_BUSY	= 0x06,
	
	MLXSW_CMD_STATUS_EXCEED_LIM	= 0x08,
	
	MLXSW_CMD_STATUS_BAD_RES_STATE	= 0x09,
	
	MLXSW_CMD_STATUS_BAD_INDEX	= 0x0A,
	
	MLXSW_CMD_STATUS_BAD_NVMEM	= 0x0B,
	
	MLXSW_CMD_STATUS_RUNNING_RESET	= 0x26,
	
	MLXSW_CMD_STATUS_BAD_PKT	= 0x30,
};

static inline const char *mlxsw_cmd_status_str(u8 status)
{
	switch (status) {
	case MLXSW_CMD_STATUS_OK:
		return "OK";
	case MLXSW_CMD_STATUS_INTERNAL_ERR:
		return "INTERNAL_ERR";
	case MLXSW_CMD_STATUS_BAD_OP:
		return "BAD_OP";
	case MLXSW_CMD_STATUS_BAD_PARAM:
		return "BAD_PARAM";
	case MLXSW_CMD_STATUS_BAD_SYS_STATE:
		return "BAD_SYS_STATE";
	case MLXSW_CMD_STATUS_BAD_RESOURCE:
		return "BAD_RESOURCE";
	case MLXSW_CMD_STATUS_RESOURCE_BUSY:
		return "RESOURCE_BUSY";
	case MLXSW_CMD_STATUS_EXCEED_LIM:
		return "EXCEED_LIM";
	case MLXSW_CMD_STATUS_BAD_RES_STATE:
		return "BAD_RES_STATE";
	case MLXSW_CMD_STATUS_BAD_INDEX:
		return "BAD_INDEX";
	case MLXSW_CMD_STATUS_BAD_NVMEM:
		return "BAD_NVMEM";
	case MLXSW_CMD_STATUS_RUNNING_RESET:
		return "RUNNING_RESET";
	case MLXSW_CMD_STATUS_BAD_PKT:
		return "BAD_PKT";
	default:
		return "*UNKNOWN*";
	}
}



static inline int mlxsw_cmd_query_fw(struct mlxsw_core *mlxsw_core,
				     char *out_mbox)
{
	return mlxsw_cmd_exec_out(mlxsw_core, MLXSW_CMD_OPCODE_QUERY_FW,
				  0, 0, false, out_mbox, MLXSW_CMD_MBOX_SIZE);
}


MLXSW_ITEM32(cmd_mbox, query_fw, fw_pages, 0x00, 16, 16);


MLXSW_ITEM32(cmd_mbox, query_fw, fw_rev_major, 0x00, 0, 16);


MLXSW_ITEM32(cmd_mbox, query_fw, fw_rev_subminor, 0x04, 16, 16);


MLXSW_ITEM32(cmd_mbox, query_fw, fw_rev_minor, 0x04, 0, 16);


MLXSW_ITEM32(cmd_mbox, query_fw, core_clk, 0x08, 16, 16);


MLXSW_ITEM32(cmd_mbox, query_fw, cmd_interface_rev, 0x08, 0, 16);


MLXSW_ITEM32(cmd_mbox, query_fw, dt, 0x0C, 31, 1);


MLXSW_ITEM32(cmd_mbox, query_fw, api_version, 0x0C, 0, 16);


MLXSW_ITEM32(cmd_mbox, query_fw, fw_hour, 0x10, 24, 8);


MLXSW_ITEM32(cmd_mbox, query_fw, fw_minutes, 0x10, 16, 8);


MLXSW_ITEM32(cmd_mbox, query_fw, fw_seconds, 0x10, 8, 8);


MLXSW_ITEM32(cmd_mbox, query_fw, fw_year, 0x14, 16, 16);


MLXSW_ITEM32(cmd_mbox, query_fw, fw_month, 0x14, 8, 8);


MLXSW_ITEM32(cmd_mbox, query_fw, fw_day, 0x14, 0, 8);


MLXSW_ITEM64(cmd_mbox, query_fw, clr_int_base_offset, 0x20, 0, 64);


MLXSW_ITEM32(cmd_mbox, query_fw, clr_int_bar, 0x28, 30, 2);


MLXSW_ITEM64(cmd_mbox, query_fw, error_buf_offset, 0x30, 0, 64);


MLXSW_ITEM32(cmd_mbox, query_fw, error_buf_size, 0x38, 0, 32);


MLXSW_ITEM32(cmd_mbox, query_fw, error_int_bar, 0x3C, 30, 2);


MLXSW_ITEM64(cmd_mbox, query_fw, doorbell_page_offset, 0x40, 0, 64);


MLXSW_ITEM32(cmd_mbox, query_fw, doorbell_page_bar, 0x48, 30, 2);


MLXSW_ITEM64(cmd_mbox, query_fw, free_running_clock_offset, 0x50, 0, 64);


MLXSW_ITEM32(cmd_mbox, query_fw, fr_rn_clk_bar, 0x58, 30, 2);


MLXSW_ITEM64(cmd_mbox, query_fw, utc_sec_offset, 0x70, 0, 64);


MLXSW_ITEM32(cmd_mbox, query_fw, utc_sec_bar, 0x78, 30, 2);


MLXSW_ITEM64(cmd_mbox, query_fw, utc_nsec_offset, 0x80, 0, 64);


MLXSW_ITEM32(cmd_mbox, query_fw, utc_nsec_bar, 0x88, 30, 2);



static inline int mlxsw_cmd_boardinfo(struct mlxsw_core *mlxsw_core,
				      char *out_mbox)
{
	return mlxsw_cmd_exec_out(mlxsw_core, MLXSW_CMD_OPCODE_QUERY_BOARDINFO,
				  0, 0, false, out_mbox, MLXSW_CMD_MBOX_SIZE);
}


MLXSW_ITEM32(cmd_mbox, boardinfo, intapin, 0x10, 24, 8);


MLXSW_ITEM32(cmd_mbox, boardinfo, vsd_vendor_id, 0x1C, 0, 16);


#define MLXSW_CMD_BOARDINFO_VSD_LEN 208
MLXSW_ITEM_BUF(cmd_mbox, boardinfo, vsd, 0x20, MLXSW_CMD_BOARDINFO_VSD_LEN);


#define MLXSW_CMD_BOARDINFO_PSID_LEN 16
MLXSW_ITEM_BUF(cmd_mbox, boardinfo, psid, 0xF0, MLXSW_CMD_BOARDINFO_PSID_LEN);



static inline int mlxsw_cmd_query_aq_cap(struct mlxsw_core *mlxsw_core,
					 char *out_mbox)
{
	return mlxsw_cmd_exec_out(mlxsw_core, MLXSW_CMD_OPCODE_QUERY_AQ_CAP,
				  0, 0, false, out_mbox, MLXSW_CMD_MBOX_SIZE);
}


MLXSW_ITEM32(cmd_mbox, query_aq_cap, log_max_sdq_sz, 0x00, 24, 8);


MLXSW_ITEM32(cmd_mbox, query_aq_cap, max_num_sdqs, 0x00, 0, 8);


MLXSW_ITEM32(cmd_mbox, query_aq_cap, log_max_rdq_sz, 0x04, 24, 8);


MLXSW_ITEM32(cmd_mbox, query_aq_cap, max_num_rdqs, 0x04, 0, 8);


MLXSW_ITEM32(cmd_mbox, query_aq_cap, log_max_cq_sz, 0x08, 24, 8);


MLXSW_ITEM32(cmd_mbox, query_aq_cap, log_max_cqv2_sz, 0x08, 16, 8);


MLXSW_ITEM32(cmd_mbox, query_aq_cap, max_num_cqs, 0x08, 0, 8);


MLXSW_ITEM32(cmd_mbox, query_aq_cap, log_max_eq_sz, 0x0C, 24, 8);


MLXSW_ITEM32(cmd_mbox, query_aq_cap, max_num_eqs, 0x0C, 0, 8);


MLXSW_ITEM32(cmd_mbox, query_aq_cap, max_sg_sq, 0x10, 8, 8);


MLXSW_ITEM32(cmd_mbox, query_aq_cap, max_sg_rq, 0x10, 0, 8);



#define MLXSW_CMD_MAP_FA_VPM_ENTRIES_MAX 32

static inline int mlxsw_cmd_map_fa(struct mlxsw_core *mlxsw_core,
				   char *in_mbox, u32 vpm_entries_count)
{
	return mlxsw_cmd_exec_in(mlxsw_core, MLXSW_CMD_OPCODE_MAP_FA,
				 0, vpm_entries_count,
				 in_mbox, MLXSW_CMD_MBOX_SIZE);
}


MLXSW_ITEM64_INDEXED(cmd_mbox, map_fa, pa, 0x00, 12, 52, 0x08, 0x00, true);


MLXSW_ITEM32_INDEXED(cmd_mbox, map_fa, log2size, 0x00, 0, 5, 0x08, 0x04, false);



static inline int mlxsw_cmd_unmap_fa(struct mlxsw_core *mlxsw_core)
{
	return mlxsw_cmd_exec_none(mlxsw_core, MLXSW_CMD_OPCODE_UNMAP_FA, 0, 0);
}



#define MLXSW_CMD_QUERY_RESOURCES_TABLE_END_ID 0xffff
#define MLXSW_CMD_QUERY_RESOURCES_MAX_QUERIES 100
#define MLXSW_CMD_QUERY_RESOURCES_PER_QUERY 32

static inline int mlxsw_cmd_query_resources(struct mlxsw_core *mlxsw_core,
					    char *out_mbox, int index)
{
	return mlxsw_cmd_exec_out(mlxsw_core, MLXSW_CMD_OPCODE_QUERY_RESOURCES,
				  0, index, false, out_mbox,
				  MLXSW_CMD_MBOX_SIZE);
}


MLXSW_ITEM32_INDEXED(cmd_mbox, query_resource, id, 0x00, 16, 16, 0x8, 0, false);


MLXSW_ITEM64_INDEXED(cmd_mbox, query_resource, data,
		     0x00, 0, 40, 0x8, 0, false);



static inline int mlxsw_cmd_config_profile_set(struct mlxsw_core *mlxsw_core,
					       char *in_mbox)
{
	return mlxsw_cmd_exec_in(mlxsw_core, MLXSW_CMD_OPCODE_CONFIG_PROFILE,
				 1, 0, in_mbox, MLXSW_CMD_MBOX_SIZE);
}


MLXSW_ITEM32(cmd_mbox, config_profile, set_max_vepa_channels, 0x0C, 0, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, set_max_lag, 0x0C, 1, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, set_max_port_per_lag, 0x0C, 2, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, set_max_mid, 0x0C, 3, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, set_max_pgt, 0x0C, 4, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, set_max_system_port, 0x0C, 5, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, set_max_vlan_groups, 0x0C, 6, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, set_max_regions, 0x0C, 7, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, set_flood_mode, 0x0C, 8, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, set_flood_tables, 0x0C, 9, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, set_max_ib_mc, 0x0C, 12, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, set_max_pkey, 0x0C, 13, 1);


MLXSW_ITEM32(cmd_mbox, config_profile,
	     set_adaptive_routing_group_cap, 0x0C, 14, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, set_ar_sec, 0x0C, 15, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, set_ubridge, 0x0C, 22, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, set_kvd_linear_size, 0x0C, 24, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, set_kvd_hash_single_size, 0x0C, 25, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, set_kvd_hash_double_size, 0x0C, 26, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, set_cqe_version, 0x08, 0, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, set_cqe_time_stamp_type, 0x08, 2, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, max_vepa_channels, 0x10, 0, 8);


MLXSW_ITEM32(cmd_mbox, config_profile, max_lag, 0x14, 0, 16);


MLXSW_ITEM32(cmd_mbox, config_profile, max_port_per_lag, 0x18, 0, 16);


MLXSW_ITEM32(cmd_mbox, config_profile, max_mid, 0x1C, 0, 16);


MLXSW_ITEM32(cmd_mbox, config_profile, max_pgt, 0x20, 0, 16);


MLXSW_ITEM32(cmd_mbox, config_profile, max_system_port, 0x24, 0, 16);


MLXSW_ITEM32(cmd_mbox, config_profile, max_vlan_groups, 0x28, 0, 12);


MLXSW_ITEM32(cmd_mbox, config_profile, max_regions, 0x2C, 0, 16);


MLXSW_ITEM32(cmd_mbox, config_profile, max_flood_tables, 0x30, 16, 4);


MLXSW_ITEM32(cmd_mbox, config_profile, max_vid_flood_tables, 0x30, 8, 4);

enum mlxsw_cmd_mbox_config_profile_flood_mode {
	
	MLXSW_CMD_MBOX_CONFIG_PROFILE_FLOOD_MODE_MIXED = 3,
	
	MLXSW_CMD_MBOX_CONFIG_PROFILE_FLOOD_MODE_CONTROLLED = 4,
};


MLXSW_ITEM32(cmd_mbox, config_profile, flood_mode, 0x30, 0, 3);


MLXSW_ITEM32(cmd_mbox, config_profile,
	     max_fid_offset_flood_tables, 0x34, 24, 4);


MLXSW_ITEM32(cmd_mbox, config_profile,
	     fid_offset_flood_table_size, 0x34, 0, 16);


MLXSW_ITEM32(cmd_mbox, config_profile, max_fid_flood_tables, 0x38, 24, 4);


MLXSW_ITEM32(cmd_mbox, config_profile, fid_flood_table_size, 0x38, 0, 16);


MLXSW_ITEM32(cmd_mbox, config_profile, max_ib_mc, 0x40, 0, 15);


MLXSW_ITEM32(cmd_mbox, config_profile, max_pkey, 0x44, 0, 15);


MLXSW_ITEM32(cmd_mbox, config_profile, ar_sec, 0x4C, 24, 2);


MLXSW_ITEM32(cmd_mbox, config_profile, adaptive_routing_group_cap, 0x4C, 0, 16);


MLXSW_ITEM32(cmd_mbox, config_profile, arn, 0x50, 31, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, ubridge, 0x50, 4, 1);


MLXSW_ITEM32(cmd_mbox, config_profile, kvd_linear_size, 0x54, 0, 24);


MLXSW_ITEM32(cmd_mbox, config_profile, kvd_hash_single_size, 0x58, 0, 24);


MLXSW_ITEM32(cmd_mbox, config_profile, kvd_hash_double_size, 0x5C, 0, 24);


MLXSW_ITEM32_INDEXED(cmd_mbox, config_profile, swid_config_mask,
		     0x60, 24, 8, 0x08, 0x00, false);


MLXSW_ITEM32_INDEXED(cmd_mbox, config_profile, swid_config_type,
		     0x60, 20, 4, 0x08, 0x00, false);


MLXSW_ITEM32_INDEXED(cmd_mbox, config_profile, swid_config_properties,
		     0x60, 0, 8, 0x08, 0x00, false);

enum mlxsw_cmd_mbox_config_profile_cqe_time_stamp_type {
	
	MLXSW_CMD_MBOX_CONFIG_PROFILE_CQE_TIME_STAMP_TYPE_USEC,
	
	MLXSW_CMD_MBOX_CONFIG_PROFILE_CQE_TIME_STAMP_TYPE_FRC,
	
	MLXSW_CMD_MBOX_CONFIG_PROFILE_CQE_TIME_STAMP_TYPE_UTC,
};


MLXSW_ITEM32(cmd_mbox, config_profile, cqe_time_stamp_type, 0xB0, 8, 2);


MLXSW_ITEM32(cmd_mbox, config_profile, cqe_version, 0xB0, 0, 8);



static inline int mlxsw_cmd_access_reg(struct mlxsw_core *mlxsw_core,
				       bool reset_ok,
				       char *in_mbox, char *out_mbox)
{
	return mlxsw_cmd_exec(mlxsw_core, MLXSW_CMD_OPCODE_ACCESS_REG,
			      0, 0, false, reset_ok,
			      in_mbox, MLXSW_CMD_MBOX_SIZE,
			      out_mbox, MLXSW_CMD_MBOX_SIZE);
}



static inline int __mlxsw_cmd_sw2hw_dq(struct mlxsw_core *mlxsw_core,
				       char *in_mbox, u32 dq_number,
				       u8 opcode_mod)
{
	return mlxsw_cmd_exec_in(mlxsw_core, MLXSW_CMD_OPCODE_SW2HW_DQ,
				 opcode_mod, dq_number,
				 in_mbox, MLXSW_CMD_MBOX_SIZE);
}

enum {
	MLXSW_CMD_OPCODE_MOD_SDQ = 0,
	MLXSW_CMD_OPCODE_MOD_RDQ = 1,
};

static inline int mlxsw_cmd_sw2hw_sdq(struct mlxsw_core *mlxsw_core,
				      char *in_mbox, u32 dq_number)
{
	return __mlxsw_cmd_sw2hw_dq(mlxsw_core, in_mbox, dq_number,
				    MLXSW_CMD_OPCODE_MOD_SDQ);
}

static inline int mlxsw_cmd_sw2hw_rdq(struct mlxsw_core *mlxsw_core,
				      char *in_mbox, u32 dq_number)
{
	return __mlxsw_cmd_sw2hw_dq(mlxsw_core, in_mbox, dq_number,
				    MLXSW_CMD_OPCODE_MOD_RDQ);
}


MLXSW_ITEM32(cmd_mbox, sw2hw_dq, cq, 0x00, 24, 8);

enum mlxsw_cmd_mbox_sw2hw_dq_sdq_lp {
	MLXSW_CMD_MBOX_SW2HW_DQ_SDQ_LP_WQE,
	MLXSW_CMD_MBOX_SW2HW_DQ_SDQ_LP_IGNORE_WQE,
};


MLXSW_ITEM32(cmd_mbox, sw2hw_dq, sdq_lp, 0x00, 23, 1);


MLXSW_ITEM32(cmd_mbox, sw2hw_dq, sdq_tclass, 0x00, 16, 6);


MLXSW_ITEM32(cmd_mbox, sw2hw_dq, log2_dq_sz, 0x00, 0, 6);


MLXSW_ITEM64_INDEXED(cmd_mbox, sw2hw_dq, pa, 0x10, 12, 52, 0x08, 0x00, true);



static inline int __mlxsw_cmd_hw2sw_dq(struct mlxsw_core *mlxsw_core,
				       u32 dq_number, u8 opcode_mod)
{
	return mlxsw_cmd_exec_none(mlxsw_core, MLXSW_CMD_OPCODE_HW2SW_DQ,
				   opcode_mod, dq_number);
}

static inline int mlxsw_cmd_hw2sw_sdq(struct mlxsw_core *mlxsw_core,
				      u32 dq_number)
{
	return __mlxsw_cmd_hw2sw_dq(mlxsw_core, dq_number,
				    MLXSW_CMD_OPCODE_MOD_SDQ);
}

static inline int mlxsw_cmd_hw2sw_rdq(struct mlxsw_core *mlxsw_core,
				      u32 dq_number)
{
	return __mlxsw_cmd_hw2sw_dq(mlxsw_core, dq_number,
				    MLXSW_CMD_OPCODE_MOD_RDQ);
}



static inline int __mlxsw_cmd_2err_dq(struct mlxsw_core *mlxsw_core,
				      u32 dq_number, u8 opcode_mod)
{
	return mlxsw_cmd_exec_none(mlxsw_core, MLXSW_CMD_OPCODE_2ERR_DQ,
				   opcode_mod, dq_number);
}

static inline int mlxsw_cmd_2err_sdq(struct mlxsw_core *mlxsw_core,
				     u32 dq_number)
{
	return __mlxsw_cmd_2err_dq(mlxsw_core, dq_number,
				   MLXSW_CMD_OPCODE_MOD_SDQ);
}

static inline int mlxsw_cmd_2err_rdq(struct mlxsw_core *mlxsw_core,
				     u32 dq_number)
{
	return __mlxsw_cmd_2err_dq(mlxsw_core, dq_number,
				   MLXSW_CMD_OPCODE_MOD_RDQ);
}



static inline int __mlxsw_cmd_query_dq(struct mlxsw_core *mlxsw_core,
				       char *out_mbox, u32 dq_number,
				       u8 opcode_mod)
{
	return mlxsw_cmd_exec_out(mlxsw_core, MLXSW_CMD_OPCODE_2ERR_DQ,
				  opcode_mod, dq_number, false,
				  out_mbox, MLXSW_CMD_MBOX_SIZE);
}

static inline int mlxsw_cmd_query_sdq(struct mlxsw_core *mlxsw_core,
				      char *out_mbox, u32 dq_number)
{
	return __mlxsw_cmd_query_dq(mlxsw_core, out_mbox, dq_number,
				    MLXSW_CMD_OPCODE_MOD_SDQ);
}

static inline int mlxsw_cmd_query_rdq(struct mlxsw_core *mlxsw_core,
				      char *out_mbox, u32 dq_number)
{
	return __mlxsw_cmd_query_dq(mlxsw_core, out_mbox, dq_number,
				    MLXSW_CMD_OPCODE_MOD_RDQ);
}



static inline int mlxsw_cmd_sw2hw_cq(struct mlxsw_core *mlxsw_core,
				     char *in_mbox, u32 cq_number)
{
	return mlxsw_cmd_exec_in(mlxsw_core, MLXSW_CMD_OPCODE_SW2HW_CQ,
				 0, cq_number, in_mbox, MLXSW_CMD_MBOX_SIZE);
}

enum mlxsw_cmd_mbox_sw2hw_cq_cqe_ver {
	MLXSW_CMD_MBOX_SW2HW_CQ_CQE_VER_1,
	MLXSW_CMD_MBOX_SW2HW_CQ_CQE_VER_2,
};


MLXSW_ITEM32(cmd_mbox, sw2hw_cq, cqe_ver, 0x00, 28, 4);


MLXSW_ITEM32(cmd_mbox, sw2hw_cq, c_eqn, 0x00, 24, 1);


MLXSW_ITEM32(cmd_mbox, sw2hw_cq, st, 0x00, 8, 1);


MLXSW_ITEM32(cmd_mbox, sw2hw_cq, log_cq_size, 0x00, 0, 4);


MLXSW_ITEM32(cmd_mbox, sw2hw_cq, producer_counter, 0x04, 0, 16);


MLXSW_ITEM64_INDEXED(cmd_mbox, sw2hw_cq, pa, 0x10, 11, 53, 0x08, 0x00, true);



static inline int mlxsw_cmd_hw2sw_cq(struct mlxsw_core *mlxsw_core,
				     u32 cq_number)
{
	return mlxsw_cmd_exec_none(mlxsw_core, MLXSW_CMD_OPCODE_HW2SW_CQ,
				   0, cq_number);
}



static inline int mlxsw_cmd_query_cq(struct mlxsw_core *mlxsw_core,
				     char *out_mbox, u32 cq_number)
{
	return mlxsw_cmd_exec_out(mlxsw_core, MLXSW_CMD_OPCODE_QUERY_CQ,
				  0, cq_number, false,
				  out_mbox, MLXSW_CMD_MBOX_SIZE);
}



static inline int mlxsw_cmd_sw2hw_eq(struct mlxsw_core *mlxsw_core,
				     char *in_mbox, u32 eq_number)
{
	return mlxsw_cmd_exec_in(mlxsw_core, MLXSW_CMD_OPCODE_SW2HW_EQ,
				 0, eq_number, in_mbox, MLXSW_CMD_MBOX_SIZE);
}


MLXSW_ITEM32(cmd_mbox, sw2hw_eq, int_msix, 0x00, 24, 1);


MLXSW_ITEM32(cmd_mbox, sw2hw_eq, st, 0x00, 8, 2);


MLXSW_ITEM32(cmd_mbox, sw2hw_eq, log_eq_size, 0x00, 0, 4);


MLXSW_ITEM32(cmd_mbox, sw2hw_eq, producer_counter, 0x04, 0, 16);


MLXSW_ITEM64_INDEXED(cmd_mbox, sw2hw_eq, pa, 0x10, 11, 53, 0x08, 0x00, true);



static inline int mlxsw_cmd_hw2sw_eq(struct mlxsw_core *mlxsw_core,
				     u32 eq_number)
{
	return mlxsw_cmd_exec_none(mlxsw_core, MLXSW_CMD_OPCODE_HW2SW_EQ,
				   0, eq_number);
}



static inline int mlxsw_cmd_query_eq(struct mlxsw_core *mlxsw_core,
				     char *out_mbox, u32 eq_number)
{
	return mlxsw_cmd_exec_out(mlxsw_core, MLXSW_CMD_OPCODE_QUERY_EQ,
				  0, eq_number, false,
				  out_mbox, MLXSW_CMD_MBOX_SIZE);
}

#endif
