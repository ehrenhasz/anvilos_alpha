
 
#include "sja1105.h"

 

#define SJA1105_SIZE_DYN_CMD					4

#define SJA1105ET_SIZE_VL_LOOKUP_DYN_CMD			\
	SJA1105_SIZE_DYN_CMD

#define SJA1105PQRS_SIZE_VL_LOOKUP_DYN_CMD			\
	(SJA1105_SIZE_DYN_CMD + SJA1105_SIZE_VL_LOOKUP_ENTRY)

#define SJA1110_SIZE_VL_POLICING_DYN_CMD			\
	(SJA1105_SIZE_DYN_CMD + SJA1105_SIZE_VL_POLICING_ENTRY)

#define SJA1105ET_SIZE_MAC_CONFIG_DYN_ENTRY			\
	SJA1105_SIZE_DYN_CMD

#define SJA1105ET_SIZE_L2_LOOKUP_DYN_CMD			\
	(SJA1105_SIZE_DYN_CMD + SJA1105ET_SIZE_L2_LOOKUP_ENTRY)

#define SJA1105PQRS_SIZE_L2_LOOKUP_DYN_CMD			\
	(SJA1105_SIZE_DYN_CMD + SJA1105PQRS_SIZE_L2_LOOKUP_ENTRY)

#define SJA1110_SIZE_L2_LOOKUP_DYN_CMD				\
	(SJA1105_SIZE_DYN_CMD + SJA1110_SIZE_L2_LOOKUP_ENTRY)

#define SJA1105_SIZE_VLAN_LOOKUP_DYN_CMD			\
	(SJA1105_SIZE_DYN_CMD + 4 + SJA1105_SIZE_VLAN_LOOKUP_ENTRY)

#define SJA1110_SIZE_VLAN_LOOKUP_DYN_CMD			\
	(SJA1105_SIZE_DYN_CMD + SJA1110_SIZE_VLAN_LOOKUP_ENTRY)

#define SJA1105_SIZE_L2_FORWARDING_DYN_CMD			\
	(SJA1105_SIZE_DYN_CMD + SJA1105_SIZE_L2_FORWARDING_ENTRY)

#define SJA1105ET_SIZE_MAC_CONFIG_DYN_CMD			\
	(SJA1105_SIZE_DYN_CMD + SJA1105ET_SIZE_MAC_CONFIG_DYN_ENTRY)

#define SJA1105PQRS_SIZE_MAC_CONFIG_DYN_CMD			\
	(SJA1105_SIZE_DYN_CMD + SJA1105PQRS_SIZE_MAC_CONFIG_ENTRY)

#define SJA1105ET_SIZE_L2_LOOKUP_PARAMS_DYN_CMD			\
	SJA1105_SIZE_DYN_CMD

#define SJA1105PQRS_SIZE_L2_LOOKUP_PARAMS_DYN_CMD		\
	(SJA1105_SIZE_DYN_CMD + SJA1105PQRS_SIZE_L2_LOOKUP_PARAMS_ENTRY)

#define SJA1110_SIZE_L2_LOOKUP_PARAMS_DYN_CMD		\
	(SJA1105_SIZE_DYN_CMD + SJA1110_SIZE_L2_LOOKUP_PARAMS_ENTRY)

#define SJA1105ET_SIZE_GENERAL_PARAMS_DYN_CMD			\
	SJA1105_SIZE_DYN_CMD

#define SJA1105PQRS_SIZE_GENERAL_PARAMS_DYN_CMD			\
	(SJA1105_SIZE_DYN_CMD + SJA1105PQRS_SIZE_GENERAL_PARAMS_ENTRY)

#define SJA1110_SIZE_GENERAL_PARAMS_DYN_CMD			\
	(SJA1105_SIZE_DYN_CMD + SJA1110_SIZE_GENERAL_PARAMS_ENTRY)

#define SJA1105PQRS_SIZE_AVB_PARAMS_DYN_CMD			\
	(SJA1105_SIZE_DYN_CMD + SJA1105PQRS_SIZE_AVB_PARAMS_ENTRY)

#define SJA1105_SIZE_RETAGGING_DYN_CMD				\
	(SJA1105_SIZE_DYN_CMD + SJA1105_SIZE_RETAGGING_ENTRY)

#define SJA1105ET_SIZE_CBS_DYN_CMD				\
	(SJA1105_SIZE_DYN_CMD + SJA1105ET_SIZE_CBS_ENTRY)

#define SJA1105PQRS_SIZE_CBS_DYN_CMD				\
	(SJA1105_SIZE_DYN_CMD + SJA1105PQRS_SIZE_CBS_ENTRY)

#define SJA1110_SIZE_XMII_PARAMS_DYN_CMD			\
	SJA1110_SIZE_XMII_PARAMS_ENTRY

#define SJA1110_SIZE_L2_POLICING_DYN_CMD			\
	(SJA1105_SIZE_DYN_CMD + SJA1105_SIZE_L2_POLICING_ENTRY)

#define SJA1110_SIZE_L2_FORWARDING_PARAMS_DYN_CMD		\
	SJA1105_SIZE_L2_FORWARDING_PARAMS_ENTRY

#define SJA1105_MAX_DYN_CMD_SIZE				\
	SJA1110_SIZE_GENERAL_PARAMS_DYN_CMD

struct sja1105_dyn_cmd {
	bool search;
	u64 valid;
	u64 rdwrset;
	u64 errors;
	u64 valident;
	u64 index;
};

enum sja1105_hostcmd {
	SJA1105_HOSTCMD_SEARCH = 1,
	SJA1105_HOSTCMD_READ = 2,
	SJA1105_HOSTCMD_WRITE = 3,
	SJA1105_HOSTCMD_INVALIDATE = 4,
};

 
static void
sja1105et_vl_lookup_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				enum packing_op op)
{
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(buf, &cmd->valid,   31, 31, size, op);
	sja1105_packing(buf, &cmd->errors,  30, 30, size, op);
	sja1105_packing(buf, &cmd->rdwrset, 29, 29, size, op);
	sja1105_packing(buf, &cmd->index,    9,  0, size, op);
}

 
static void
sja1105pqrs_vl_lookup_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				  enum packing_op op)
{
	u8 *p = buf + SJA1105_SIZE_VL_LOOKUP_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(p, &cmd->valid,   31, 31, size, op);
	sja1105_packing(p, &cmd->errors,  30, 30, size, op);
	sja1105_packing(p, &cmd->rdwrset, 29, 29, size, op);
	sja1105_packing(p, &cmd->index,    9,  0, size, op);
}

static void
sja1110_vl_lookup_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
			      enum packing_op op)
{
	u8 *p = buf + SJA1105PQRS_SIZE_L2_LOOKUP_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(p, &cmd->valid,   31, 31, size, op);
	sja1105_packing(p, &cmd->rdwrset, 30, 30, size, op);
	sja1105_packing(p, &cmd->errors,  29, 29, size, op);
	sja1105_packing(p, &cmd->index,   11,  0, size, op);
}

static size_t sja1105et_vl_lookup_entry_packing(void *buf, void *entry_ptr,
						enum packing_op op)
{
	struct sja1105_vl_lookup_entry *entry = entry_ptr;
	const int size = SJA1105ET_SIZE_VL_LOOKUP_DYN_CMD;

	sja1105_packing(buf, &entry->egrmirr,  21, 17, size, op);
	sja1105_packing(buf, &entry->ingrmirr, 16, 16, size, op);
	return size;
}

static void
sja1110_vl_policing_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				enum packing_op op)
{
	u8 *p = buf + SJA1105_SIZE_VL_LOOKUP_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(p, &cmd->valid,   31, 31, size, op);
	sja1105_packing(p, &cmd->rdwrset, 30, 30, size, op);
	sja1105_packing(p, &cmd->index,   11,  0, size, op);
}

static void
sja1105pqrs_common_l2_lookup_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
					 enum packing_op op, int entry_size)
{
	const int size = SJA1105_SIZE_DYN_CMD;
	u8 *p = buf + entry_size;
	u64 hostcmd;

	sja1105_packing(p, &cmd->valid,    31, 31, size, op);
	sja1105_packing(p, &cmd->rdwrset,  30, 30, size, op);
	sja1105_packing(p, &cmd->errors,   29, 29, size, op);
	sja1105_packing(p, &cmd->valident, 27, 27, size, op);

	 
	if (cmd->rdwrset == SPI_READ) {
		if (cmd->search)
			hostcmd = SJA1105_HOSTCMD_SEARCH;
		else
			hostcmd = SJA1105_HOSTCMD_READ;
	} else {
		 
		if (cmd->valident)
			hostcmd = SJA1105_HOSTCMD_WRITE;
		else
			hostcmd = SJA1105_HOSTCMD_INVALIDATE;
	}
	sja1105_packing(p, &hostcmd, 25, 23, size, op);
}

static void
sja1105pqrs_l2_lookup_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				  enum packing_op op)
{
	int entry_size = SJA1105PQRS_SIZE_L2_LOOKUP_ENTRY;

	sja1105pqrs_common_l2_lookup_cmd_packing(buf, cmd, op, entry_size);

	 
	sja1105_packing(buf, &cmd->index, 15, 6, entry_size, op);
}

static void
sja1110_l2_lookup_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
			      enum packing_op op)
{
	int entry_size = SJA1110_SIZE_L2_LOOKUP_ENTRY;

	sja1105pqrs_common_l2_lookup_cmd_packing(buf, cmd, op, entry_size);

	sja1105_packing(buf, &cmd->index, 10, 1, entry_size, op);
}

 
static size_t
sja1105pqrs_dyn_l2_lookup_entry_packing(void *buf, void *entry_ptr,
					enum packing_op op)
{
	struct sja1105_l2_lookup_entry *entry = entry_ptr;
	u8 *cmd = buf + SJA1105PQRS_SIZE_L2_LOOKUP_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(cmd, &entry->lockeds, 28, 28, size, op);

	return sja1105pqrs_l2_lookup_entry_packing(buf, entry_ptr, op);
}

static size_t sja1110_dyn_l2_lookup_entry_packing(void *buf, void *entry_ptr,
						  enum packing_op op)
{
	struct sja1105_l2_lookup_entry *entry = entry_ptr;
	u8 *cmd = buf + SJA1110_SIZE_L2_LOOKUP_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(cmd, &entry->lockeds, 28, 28, size, op);

	return sja1110_l2_lookup_entry_packing(buf, entry_ptr, op);
}

static void
sja1105et_l2_lookup_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				enum packing_op op)
{
	u8 *p = buf + SJA1105ET_SIZE_L2_LOOKUP_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(p, &cmd->valid,    31, 31, size, op);
	sja1105_packing(p, &cmd->rdwrset,  30, 30, size, op);
	sja1105_packing(p, &cmd->errors,   29, 29, size, op);
	sja1105_packing(p, &cmd->valident, 27, 27, size, op);
	 
	sja1105_packing(buf, &cmd->index, 29, 20,
			SJA1105ET_SIZE_L2_LOOKUP_ENTRY, op);
}

static size_t sja1105et_dyn_l2_lookup_entry_packing(void *buf, void *entry_ptr,
						    enum packing_op op)
{
	struct sja1105_l2_lookup_entry *entry = entry_ptr;
	u8 *cmd = buf + SJA1105ET_SIZE_L2_LOOKUP_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(cmd, &entry->lockeds, 28, 28, size, op);

	return sja1105et_l2_lookup_entry_packing(buf, entry_ptr, op);
}

static void
sja1105et_mgmt_route_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				 enum packing_op op)
{
	u8 *p = buf + SJA1105ET_SIZE_L2_LOOKUP_ENTRY;
	u64 mgmtroute = 1;

	sja1105et_l2_lookup_cmd_packing(buf, cmd, op);
	if (op == PACK)
		sja1105_pack(p, &mgmtroute, 26, 26, SJA1105_SIZE_DYN_CMD);
}

static size_t sja1105et_mgmt_route_entry_packing(void *buf, void *entry_ptr,
						 enum packing_op op)
{
	struct sja1105_mgmt_entry *entry = entry_ptr;
	const size_t size = SJA1105ET_SIZE_L2_LOOKUP_ENTRY;

	 
	sja1105_packing(buf, &entry->tsreg,     85, 85, size, op);
	sja1105_packing(buf, &entry->takets,    84, 84, size, op);
	sja1105_packing(buf, &entry->macaddr,   83, 36, size, op);
	sja1105_packing(buf, &entry->destports, 35, 31, size, op);
	sja1105_packing(buf, &entry->enfport,   30, 30, size, op);
	return size;
}

static void
sja1105pqrs_mgmt_route_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				   enum packing_op op)
{
	u8 *p = buf + SJA1105PQRS_SIZE_L2_LOOKUP_ENTRY;
	u64 mgmtroute = 1;

	sja1105pqrs_l2_lookup_cmd_packing(buf, cmd, op);
	if (op == PACK)
		sja1105_pack(p, &mgmtroute, 26, 26, SJA1105_SIZE_DYN_CMD);
}

static size_t sja1105pqrs_mgmt_route_entry_packing(void *buf, void *entry_ptr,
						   enum packing_op op)
{
	const size_t size = SJA1105PQRS_SIZE_L2_LOOKUP_ENTRY;
	struct sja1105_mgmt_entry *entry = entry_ptr;

	 
	sja1105_packing(buf, &entry->tsreg,     71, 71, size, op);
	sja1105_packing(buf, &entry->takets,    70, 70, size, op);
	sja1105_packing(buf, &entry->macaddr,   69, 22, size, op);
	sja1105_packing(buf, &entry->destports, 21, 17, size, op);
	sja1105_packing(buf, &entry->enfport,   16, 16, size, op);
	return size;
}

 
static void
sja1105_vlan_lookup_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				enum packing_op op)
{
	u8 *p = buf + SJA1105_SIZE_VLAN_LOOKUP_ENTRY + 4;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(p, &cmd->valid,    31, 31, size, op);
	sja1105_packing(p, &cmd->rdwrset,  30, 30, size, op);
	sja1105_packing(p, &cmd->valident, 27, 27, size, op);
	 
	sja1105_packing(buf, &cmd->index, 38, 27,
			SJA1105_SIZE_VLAN_LOOKUP_ENTRY, op);
}

 
static void
sja1110_vlan_lookup_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				enum packing_op op)
{
	u8 *p = buf + SJA1110_SIZE_VLAN_LOOKUP_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;
	u64 type_entry = 0;

	sja1105_packing(p, &cmd->valid,   31, 31, size, op);
	sja1105_packing(p, &cmd->rdwrset, 30, 30, size, op);
	sja1105_packing(p, &cmd->errors,  29, 29, size, op);
	 
	sja1105_packing(buf, &cmd->index, 38, 27,
			SJA1110_SIZE_VLAN_LOOKUP_ENTRY, op);

	 
	if (op == PACK && !cmd->valident) {
		sja1105_packing(buf, &type_entry, 40, 39,
				SJA1110_SIZE_VLAN_LOOKUP_ENTRY, PACK);
	} else if (op == UNPACK) {
		sja1105_packing(buf, &type_entry, 40, 39,
				SJA1110_SIZE_VLAN_LOOKUP_ENTRY, UNPACK);
		cmd->valident = !!type_entry;
	}
}

static void
sja1105_l2_forwarding_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				  enum packing_op op)
{
	u8 *p = buf + SJA1105_SIZE_L2_FORWARDING_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(p, &cmd->valid,   31, 31, size, op);
	sja1105_packing(p, &cmd->errors,  30, 30, size, op);
	sja1105_packing(p, &cmd->rdwrset, 29, 29, size, op);
	sja1105_packing(p, &cmd->index,    4,  0, size, op);
}

static void
sja1110_l2_forwarding_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				  enum packing_op op)
{
	u8 *p = buf + SJA1105_SIZE_L2_FORWARDING_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(p, &cmd->valid,   31, 31, size, op);
	sja1105_packing(p, &cmd->rdwrset, 30, 30, size, op);
	sja1105_packing(p, &cmd->errors,  29, 29, size, op);
	sja1105_packing(p, &cmd->index,    4,  0, size, op);
}

static void
sja1105et_mac_config_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				 enum packing_op op)
{
	const int size = SJA1105_SIZE_DYN_CMD;
	 
	u8 *reg1 = buf + 4;

	sja1105_packing(reg1, &cmd->valid, 31, 31, size, op);
	sja1105_packing(reg1, &cmd->index, 26, 24, size, op);
}

static size_t sja1105et_mac_config_entry_packing(void *buf, void *entry_ptr,
						 enum packing_op op)
{
	const int size = SJA1105ET_SIZE_MAC_CONFIG_DYN_ENTRY;
	struct sja1105_mac_config_entry *entry = entry_ptr;
	 
	u8 *reg1 = buf + 4;
	u8 *reg2 = buf;

	sja1105_packing(reg1, &entry->speed,     30, 29, size, op);
	sja1105_packing(reg1, &entry->drpdtag,   23, 23, size, op);
	sja1105_packing(reg1, &entry->drpuntag,  22, 22, size, op);
	sja1105_packing(reg1, &entry->retag,     21, 21, size, op);
	sja1105_packing(reg1, &entry->dyn_learn, 20, 20, size, op);
	sja1105_packing(reg1, &entry->egress,    19, 19, size, op);
	sja1105_packing(reg1, &entry->ingress,   18, 18, size, op);
	sja1105_packing(reg1, &entry->ing_mirr,  17, 17, size, op);
	sja1105_packing(reg1, &entry->egr_mirr,  16, 16, size, op);
	sja1105_packing(reg1, &entry->vlanprio,  14, 12, size, op);
	sja1105_packing(reg1, &entry->vlanid,    11,  0, size, op);
	sja1105_packing(reg2, &entry->tp_delin,  31, 16, size, op);
	sja1105_packing(reg2, &entry->tp_delout, 15,  0, size, op);
	 
	 
	return 0;
}

static void
sja1105pqrs_mac_config_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				   enum packing_op op)
{
	const int size = SJA1105ET_SIZE_MAC_CONFIG_DYN_ENTRY;
	u8 *p = buf + SJA1105PQRS_SIZE_MAC_CONFIG_ENTRY;

	sja1105_packing(p, &cmd->valid,   31, 31, size, op);
	sja1105_packing(p, &cmd->errors,  30, 30, size, op);
	sja1105_packing(p, &cmd->rdwrset, 29, 29, size, op);
	sja1105_packing(p, &cmd->index,    2,  0, size, op);
}

static void
sja1110_mac_config_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
			       enum packing_op op)
{
	u8 *p = buf + SJA1105PQRS_SIZE_MAC_CONFIG_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(p, &cmd->valid,   31, 31, size, op);
	sja1105_packing(p, &cmd->rdwrset, 30, 30, size, op);
	sja1105_packing(p, &cmd->errors,  29, 29, size, op);
	sja1105_packing(p, &cmd->index,    3,  0, size, op);
}

static void
sja1105et_l2_lookup_params_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				       enum packing_op op)
{
	sja1105_packing(buf, &cmd->valid, 31, 31,
			SJA1105ET_SIZE_L2_LOOKUP_PARAMS_DYN_CMD, op);
}

static size_t
sja1105et_l2_lookup_params_entry_packing(void *buf, void *entry_ptr,
					 enum packing_op op)
{
	struct sja1105_l2_lookup_params_entry *entry = entry_ptr;

	sja1105_packing(buf, &entry->poly, 7, 0,
			SJA1105ET_SIZE_L2_LOOKUP_PARAMS_DYN_CMD, op);
	 
	return 0;
}

static void
sja1105pqrs_l2_lookup_params_cmd_packing(void *buf,
					 struct sja1105_dyn_cmd *cmd,
					 enum packing_op op)
{
	u8 *p = buf + SJA1105PQRS_SIZE_L2_LOOKUP_PARAMS_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(p, &cmd->valid,   31, 31, size, op);
	sja1105_packing(p, &cmd->rdwrset, 30, 30, size, op);
}

static void
sja1110_l2_lookup_params_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				     enum packing_op op)
{
	u8 *p = buf + SJA1110_SIZE_L2_LOOKUP_PARAMS_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(p, &cmd->valid,   31, 31, size, op);
	sja1105_packing(p, &cmd->rdwrset, 30, 30, size, op);
	sja1105_packing(p, &cmd->errors,  29, 29, size, op);
}

static void
sja1105et_general_params_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				     enum packing_op op)
{
	const int size = SJA1105ET_SIZE_GENERAL_PARAMS_DYN_CMD;

	sja1105_packing(buf, &cmd->valid,  31, 31, size, op);
	sja1105_packing(buf, &cmd->errors, 30, 30, size, op);
}

static size_t
sja1105et_general_params_entry_packing(void *buf, void *entry_ptr,
				       enum packing_op op)
{
	struct sja1105_general_params_entry *entry = entry_ptr;
	const int size = SJA1105ET_SIZE_GENERAL_PARAMS_DYN_CMD;

	sja1105_packing(buf, &entry->mirr_port, 2, 0, size, op);
	 
	return 0;
}

static void
sja1105pqrs_general_params_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				       enum packing_op op)
{
	u8 *p = buf + SJA1105PQRS_SIZE_GENERAL_PARAMS_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(p, &cmd->valid,   31, 31, size, op);
	sja1105_packing(p, &cmd->errors,  30, 30, size, op);
	sja1105_packing(p, &cmd->rdwrset, 28, 28, size, op);
}

static void
sja1110_general_params_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				   enum packing_op op)
{
	u8 *p = buf + SJA1110_SIZE_GENERAL_PARAMS_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(p, &cmd->valid,   31, 31, size, op);
	sja1105_packing(p, &cmd->rdwrset, 30, 30, size, op);
	sja1105_packing(p, &cmd->errors,  29, 29, size, op);
}

static void
sja1105pqrs_avb_params_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				   enum packing_op op)
{
	u8 *p = buf + SJA1105PQRS_SIZE_AVB_PARAMS_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(p, &cmd->valid,   31, 31, size, op);
	sja1105_packing(p, &cmd->errors,  30, 30, size, op);
	sja1105_packing(p, &cmd->rdwrset, 29, 29, size, op);
}

static void
sja1105_retagging_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
			      enum packing_op op)
{
	u8 *p = buf + SJA1105_SIZE_RETAGGING_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(p, &cmd->valid,    31, 31, size, op);
	sja1105_packing(p, &cmd->errors,   30, 30, size, op);
	sja1105_packing(p, &cmd->valident, 29, 29, size, op);
	sja1105_packing(p, &cmd->rdwrset,  28, 28, size, op);
	sja1105_packing(p, &cmd->index,     5,  0, size, op);
}

static void
sja1110_retagging_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
			      enum packing_op op)
{
	u8 *p = buf + SJA1105_SIZE_RETAGGING_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(p, &cmd->valid,    31, 31, size, op);
	sja1105_packing(p, &cmd->rdwrset,  30, 30, size, op);
	sja1105_packing(p, &cmd->errors,   29, 29, size, op);
	sja1105_packing(p, &cmd->valident, 28, 28, size, op);
	sja1105_packing(p, &cmd->index,     4,  0, size, op);
}

static void sja1105et_cbs_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				      enum packing_op op)
{
	u8 *p = buf + SJA1105ET_SIZE_CBS_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(p, &cmd->valid, 31, 31, size, op);
	sja1105_packing(p, &cmd->index, 19, 16, size, op);
}

static size_t sja1105et_cbs_entry_packing(void *buf, void *entry_ptr,
					  enum packing_op op)
{
	const size_t size = SJA1105ET_SIZE_CBS_ENTRY;
	struct sja1105_cbs_entry *entry = entry_ptr;
	u8 *cmd = buf + size;
	u32 *p = buf;

	sja1105_packing(cmd, &entry->port, 5, 3, SJA1105_SIZE_DYN_CMD, op);
	sja1105_packing(cmd, &entry->prio, 2, 0, SJA1105_SIZE_DYN_CMD, op);
	sja1105_packing(p + 3, &entry->credit_lo,  31, 0, size, op);
	sja1105_packing(p + 2, &entry->credit_hi,  31, 0, size, op);
	sja1105_packing(p + 1, &entry->send_slope, 31, 0, size, op);
	sja1105_packing(p + 0, &entry->idle_slope, 31, 0, size, op);
	return size;
}

static void sja1105pqrs_cbs_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
					enum packing_op op)
{
	u8 *p = buf + SJA1105PQRS_SIZE_CBS_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(p, &cmd->valid,   31, 31, size, op);
	sja1105_packing(p, &cmd->rdwrset, 30, 30, size, op);
	sja1105_packing(p, &cmd->errors,  29, 29, size, op);
	sja1105_packing(p, &cmd->index,    3,  0, size, op);
}

static void sja1110_cbs_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				    enum packing_op op)
{
	u8 *p = buf + SJA1105PQRS_SIZE_CBS_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(p, &cmd->valid,   31, 31, size, op);
	sja1105_packing(p, &cmd->rdwrset, 30, 30, size, op);
	sja1105_packing(p, &cmd->errors,  29, 29, size, op);
	sja1105_packing(p, &cmd->index,    7,  0, size, op);
}

static size_t sja1105pqrs_cbs_entry_packing(void *buf, void *entry_ptr,
					    enum packing_op op)
{
	const size_t size = SJA1105PQRS_SIZE_CBS_ENTRY;
	struct sja1105_cbs_entry *entry = entry_ptr;

	sja1105_packing(buf, &entry->port,      159, 157, size, op);
	sja1105_packing(buf, &entry->prio,      156, 154, size, op);
	sja1105_packing(buf, &entry->credit_lo, 153, 122, size, op);
	sja1105_packing(buf, &entry->credit_hi, 121,  90, size, op);
	sja1105_packing(buf, &entry->send_slope, 89,  58, size, op);
	sja1105_packing(buf, &entry->idle_slope, 57,  26, size, op);
	return size;
}

static size_t sja1110_cbs_entry_packing(void *buf, void *entry_ptr,
					enum packing_op op)
{
	const size_t size = SJA1105PQRS_SIZE_CBS_ENTRY;
	struct sja1105_cbs_entry *entry = entry_ptr;
	u64 entry_type = SJA1110_CBS_SHAPER;

	sja1105_packing(buf, &entry_type,       159, 159, size, op);
	sja1105_packing(buf, &entry->credit_lo, 151, 120, size, op);
	sja1105_packing(buf, &entry->credit_hi, 119,  88, size, op);
	sja1105_packing(buf, &entry->send_slope, 87,  56, size, op);
	sja1105_packing(buf, &entry->idle_slope, 55,  24, size, op);
	return size;
}

static void sja1110_dummy_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				      enum packing_op op)
{
}

static void
sja1110_l2_policing_cmd_packing(void *buf, struct sja1105_dyn_cmd *cmd,
				enum packing_op op)
{
	u8 *p = buf + SJA1105_SIZE_L2_POLICING_ENTRY;
	const int size = SJA1105_SIZE_DYN_CMD;

	sja1105_packing(p, &cmd->valid,   31, 31, size, op);
	sja1105_packing(p, &cmd->rdwrset, 30, 30, size, op);
	sja1105_packing(p, &cmd->errors,  29, 29, size, op);
	sja1105_packing(p, &cmd->index,    6,  0, size, op);
}

#define OP_READ		BIT(0)
#define OP_WRITE	BIT(1)
#define OP_DEL		BIT(2)
#define OP_SEARCH	BIT(3)
#define OP_VALID_ANYWAY	BIT(4)

 
const struct sja1105_dynamic_table_ops sja1105et_dyn_ops[BLK_IDX_MAX_DYN] = {
	[BLK_IDX_VL_LOOKUP] = {
		.entry_packing = sja1105et_vl_lookup_entry_packing,
		.cmd_packing = sja1105et_vl_lookup_cmd_packing,
		.access = OP_WRITE,
		.max_entry_count = SJA1105_MAX_VL_LOOKUP_COUNT,
		.packed_size = SJA1105ET_SIZE_VL_LOOKUP_DYN_CMD,
		.addr = 0x35,
	},
	[BLK_IDX_L2_LOOKUP] = {
		.entry_packing = sja1105et_dyn_l2_lookup_entry_packing,
		.cmd_packing = sja1105et_l2_lookup_cmd_packing,
		.access = (OP_READ | OP_WRITE | OP_DEL),
		.max_entry_count = SJA1105_MAX_L2_LOOKUP_COUNT,
		.packed_size = SJA1105ET_SIZE_L2_LOOKUP_DYN_CMD,
		.addr = 0x20,
	},
	[BLK_IDX_MGMT_ROUTE] = {
		.entry_packing = sja1105et_mgmt_route_entry_packing,
		.cmd_packing = sja1105et_mgmt_route_cmd_packing,
		.access = (OP_READ | OP_WRITE | OP_VALID_ANYWAY),
		.max_entry_count = SJA1105_NUM_PORTS,
		.packed_size = SJA1105ET_SIZE_L2_LOOKUP_DYN_CMD,
		.addr = 0x20,
	},
	[BLK_IDX_VLAN_LOOKUP] = {
		.entry_packing = sja1105_vlan_lookup_entry_packing,
		.cmd_packing = sja1105_vlan_lookup_cmd_packing,
		.access = (OP_WRITE | OP_DEL),
		.max_entry_count = SJA1105_MAX_VLAN_LOOKUP_COUNT,
		.packed_size = SJA1105_SIZE_VLAN_LOOKUP_DYN_CMD,
		.addr = 0x27,
	},
	[BLK_IDX_L2_FORWARDING] = {
		.entry_packing = sja1105_l2_forwarding_entry_packing,
		.cmd_packing = sja1105_l2_forwarding_cmd_packing,
		.max_entry_count = SJA1105_MAX_L2_FORWARDING_COUNT,
		.access = OP_WRITE,
		.packed_size = SJA1105_SIZE_L2_FORWARDING_DYN_CMD,
		.addr = 0x24,
	},
	[BLK_IDX_MAC_CONFIG] = {
		.entry_packing = sja1105et_mac_config_entry_packing,
		.cmd_packing = sja1105et_mac_config_cmd_packing,
		.max_entry_count = SJA1105_MAX_MAC_CONFIG_COUNT,
		.access = OP_WRITE,
		.packed_size = SJA1105ET_SIZE_MAC_CONFIG_DYN_CMD,
		.addr = 0x36,
	},
	[BLK_IDX_L2_LOOKUP_PARAMS] = {
		.entry_packing = sja1105et_l2_lookup_params_entry_packing,
		.cmd_packing = sja1105et_l2_lookup_params_cmd_packing,
		.max_entry_count = SJA1105_MAX_L2_LOOKUP_PARAMS_COUNT,
		.access = OP_WRITE,
		.packed_size = SJA1105ET_SIZE_L2_LOOKUP_PARAMS_DYN_CMD,
		.addr = 0x38,
	},
	[BLK_IDX_GENERAL_PARAMS] = {
		.entry_packing = sja1105et_general_params_entry_packing,
		.cmd_packing = sja1105et_general_params_cmd_packing,
		.max_entry_count = SJA1105_MAX_GENERAL_PARAMS_COUNT,
		.access = OP_WRITE,
		.packed_size = SJA1105ET_SIZE_GENERAL_PARAMS_DYN_CMD,
		.addr = 0x34,
	},
	[BLK_IDX_RETAGGING] = {
		.entry_packing = sja1105_retagging_entry_packing,
		.cmd_packing = sja1105_retagging_cmd_packing,
		.max_entry_count = SJA1105_MAX_RETAGGING_COUNT,
		.access = (OP_WRITE | OP_DEL),
		.packed_size = SJA1105_SIZE_RETAGGING_DYN_CMD,
		.addr = 0x31,
	},
	[BLK_IDX_CBS] = {
		.entry_packing = sja1105et_cbs_entry_packing,
		.cmd_packing = sja1105et_cbs_cmd_packing,
		.max_entry_count = SJA1105ET_MAX_CBS_COUNT,
		.access = OP_WRITE,
		.packed_size = SJA1105ET_SIZE_CBS_DYN_CMD,
		.addr = 0x2c,
	},
};

 
const struct sja1105_dynamic_table_ops sja1105pqrs_dyn_ops[BLK_IDX_MAX_DYN] = {
	[BLK_IDX_VL_LOOKUP] = {
		.entry_packing = sja1105_vl_lookup_entry_packing,
		.cmd_packing = sja1105pqrs_vl_lookup_cmd_packing,
		.access = (OP_READ | OP_WRITE),
		.max_entry_count = SJA1105_MAX_VL_LOOKUP_COUNT,
		.packed_size = SJA1105PQRS_SIZE_VL_LOOKUP_DYN_CMD,
		.addr = 0x47,
	},
	[BLK_IDX_L2_LOOKUP] = {
		.entry_packing = sja1105pqrs_dyn_l2_lookup_entry_packing,
		.cmd_packing = sja1105pqrs_l2_lookup_cmd_packing,
		.access = (OP_READ | OP_WRITE | OP_DEL | OP_SEARCH),
		.max_entry_count = SJA1105_MAX_L2_LOOKUP_COUNT,
		.packed_size = SJA1105PQRS_SIZE_L2_LOOKUP_DYN_CMD,
		.addr = 0x24,
	},
	[BLK_IDX_MGMT_ROUTE] = {
		.entry_packing = sja1105pqrs_mgmt_route_entry_packing,
		.cmd_packing = sja1105pqrs_mgmt_route_cmd_packing,
		.access = (OP_READ | OP_WRITE | OP_DEL | OP_SEARCH | OP_VALID_ANYWAY),
		.max_entry_count = SJA1105_NUM_PORTS,
		.packed_size = SJA1105PQRS_SIZE_L2_LOOKUP_DYN_CMD,
		.addr = 0x24,
	},
	[BLK_IDX_VLAN_LOOKUP] = {
		.entry_packing = sja1105_vlan_lookup_entry_packing,
		.cmd_packing = sja1105_vlan_lookup_cmd_packing,
		.access = (OP_READ | OP_WRITE | OP_DEL),
		.max_entry_count = SJA1105_MAX_VLAN_LOOKUP_COUNT,
		.packed_size = SJA1105_SIZE_VLAN_LOOKUP_DYN_CMD,
		.addr = 0x2D,
	},
	[BLK_IDX_L2_FORWARDING] = {
		.entry_packing = sja1105_l2_forwarding_entry_packing,
		.cmd_packing = sja1105_l2_forwarding_cmd_packing,
		.max_entry_count = SJA1105_MAX_L2_FORWARDING_COUNT,
		.access = OP_WRITE,
		.packed_size = SJA1105_SIZE_L2_FORWARDING_DYN_CMD,
		.addr = 0x2A,
	},
	[BLK_IDX_MAC_CONFIG] = {
		.entry_packing = sja1105pqrs_mac_config_entry_packing,
		.cmd_packing = sja1105pqrs_mac_config_cmd_packing,
		.max_entry_count = SJA1105_MAX_MAC_CONFIG_COUNT,
		.access = (OP_READ | OP_WRITE),
		.packed_size = SJA1105PQRS_SIZE_MAC_CONFIG_DYN_CMD,
		.addr = 0x4B,
	},
	[BLK_IDX_L2_LOOKUP_PARAMS] = {
		.entry_packing = sja1105pqrs_l2_lookup_params_entry_packing,
		.cmd_packing = sja1105pqrs_l2_lookup_params_cmd_packing,
		.max_entry_count = SJA1105_MAX_L2_LOOKUP_PARAMS_COUNT,
		.access = (OP_READ | OP_WRITE),
		.packed_size = SJA1105PQRS_SIZE_L2_LOOKUP_PARAMS_DYN_CMD,
		.addr = 0x54,
	},
	[BLK_IDX_AVB_PARAMS] = {
		.entry_packing = sja1105pqrs_avb_params_entry_packing,
		.cmd_packing = sja1105pqrs_avb_params_cmd_packing,
		.max_entry_count = SJA1105_MAX_AVB_PARAMS_COUNT,
		.access = (OP_READ | OP_WRITE),
		.packed_size = SJA1105PQRS_SIZE_AVB_PARAMS_DYN_CMD,
		.addr = 0x8003,
	},
	[BLK_IDX_GENERAL_PARAMS] = {
		.entry_packing = sja1105pqrs_general_params_entry_packing,
		.cmd_packing = sja1105pqrs_general_params_cmd_packing,
		.max_entry_count = SJA1105_MAX_GENERAL_PARAMS_COUNT,
		.access = (OP_READ | OP_WRITE),
		.packed_size = SJA1105PQRS_SIZE_GENERAL_PARAMS_DYN_CMD,
		.addr = 0x3B,
	},
	[BLK_IDX_RETAGGING] = {
		.entry_packing = sja1105_retagging_entry_packing,
		.cmd_packing = sja1105_retagging_cmd_packing,
		.max_entry_count = SJA1105_MAX_RETAGGING_COUNT,
		.access = (OP_READ | OP_WRITE | OP_DEL),
		.packed_size = SJA1105_SIZE_RETAGGING_DYN_CMD,
		.addr = 0x38,
	},
	[BLK_IDX_CBS] = {
		.entry_packing = sja1105pqrs_cbs_entry_packing,
		.cmd_packing = sja1105pqrs_cbs_cmd_packing,
		.max_entry_count = SJA1105PQRS_MAX_CBS_COUNT,
		.access = OP_WRITE,
		.packed_size = SJA1105PQRS_SIZE_CBS_DYN_CMD,
		.addr = 0x32,
	},
};

 
const struct sja1105_dynamic_table_ops sja1110_dyn_ops[BLK_IDX_MAX_DYN] = {
	[BLK_IDX_VL_LOOKUP] = {
		.entry_packing = sja1110_vl_lookup_entry_packing,
		.cmd_packing = sja1110_vl_lookup_cmd_packing,
		.access = (OP_READ | OP_WRITE | OP_VALID_ANYWAY),
		.max_entry_count = SJA1110_MAX_VL_LOOKUP_COUNT,
		.packed_size = SJA1105PQRS_SIZE_VL_LOOKUP_DYN_CMD,
		.addr = SJA1110_SPI_ADDR(0x124),
	},
	[BLK_IDX_VL_POLICING] = {
		.entry_packing = sja1110_vl_policing_entry_packing,
		.cmd_packing = sja1110_vl_policing_cmd_packing,
		.access = (OP_READ | OP_WRITE | OP_VALID_ANYWAY),
		.max_entry_count = SJA1110_MAX_VL_POLICING_COUNT,
		.packed_size = SJA1110_SIZE_VL_POLICING_DYN_CMD,
		.addr = SJA1110_SPI_ADDR(0x310),
	},
	[BLK_IDX_L2_LOOKUP] = {
		.entry_packing = sja1110_dyn_l2_lookup_entry_packing,
		.cmd_packing = sja1110_l2_lookup_cmd_packing,
		.access = (OP_READ | OP_WRITE | OP_DEL | OP_SEARCH),
		.max_entry_count = SJA1105_MAX_L2_LOOKUP_COUNT,
		.packed_size = SJA1110_SIZE_L2_LOOKUP_DYN_CMD,
		.addr = SJA1110_SPI_ADDR(0x8c),
	},
	[BLK_IDX_VLAN_LOOKUP] = {
		.entry_packing = sja1110_vlan_lookup_entry_packing,
		.cmd_packing = sja1110_vlan_lookup_cmd_packing,
		.access = (OP_READ | OP_WRITE | OP_DEL),
		.max_entry_count = SJA1105_MAX_VLAN_LOOKUP_COUNT,
		.packed_size = SJA1110_SIZE_VLAN_LOOKUP_DYN_CMD,
		.addr = SJA1110_SPI_ADDR(0xb4),
	},
	[BLK_IDX_L2_FORWARDING] = {
		.entry_packing = sja1110_l2_forwarding_entry_packing,
		.cmd_packing = sja1110_l2_forwarding_cmd_packing,
		.max_entry_count = SJA1110_MAX_L2_FORWARDING_COUNT,
		.access = (OP_READ | OP_WRITE | OP_VALID_ANYWAY),
		.packed_size = SJA1105_SIZE_L2_FORWARDING_DYN_CMD,
		.addr = SJA1110_SPI_ADDR(0xa8),
	},
	[BLK_IDX_MAC_CONFIG] = {
		.entry_packing = sja1110_mac_config_entry_packing,
		.cmd_packing = sja1110_mac_config_cmd_packing,
		.max_entry_count = SJA1110_MAX_MAC_CONFIG_COUNT,
		.access = (OP_READ | OP_WRITE | OP_VALID_ANYWAY),
		.packed_size = SJA1105PQRS_SIZE_MAC_CONFIG_DYN_CMD,
		.addr = SJA1110_SPI_ADDR(0x134),
	},
	[BLK_IDX_L2_LOOKUP_PARAMS] = {
		.entry_packing = sja1110_l2_lookup_params_entry_packing,
		.cmd_packing = sja1110_l2_lookup_params_cmd_packing,
		.max_entry_count = SJA1105_MAX_L2_LOOKUP_PARAMS_COUNT,
		.access = (OP_READ | OP_WRITE | OP_VALID_ANYWAY),
		.packed_size = SJA1110_SIZE_L2_LOOKUP_PARAMS_DYN_CMD,
		.addr = SJA1110_SPI_ADDR(0x158),
	},
	[BLK_IDX_AVB_PARAMS] = {
		.entry_packing = sja1105pqrs_avb_params_entry_packing,
		.cmd_packing = sja1105pqrs_avb_params_cmd_packing,
		.max_entry_count = SJA1105_MAX_AVB_PARAMS_COUNT,
		.access = (OP_READ | OP_WRITE | OP_VALID_ANYWAY),
		.packed_size = SJA1105PQRS_SIZE_AVB_PARAMS_DYN_CMD,
		.addr = SJA1110_SPI_ADDR(0x2000C),
	},
	[BLK_IDX_GENERAL_PARAMS] = {
		.entry_packing = sja1110_general_params_entry_packing,
		.cmd_packing = sja1110_general_params_cmd_packing,
		.max_entry_count = SJA1105_MAX_GENERAL_PARAMS_COUNT,
		.access = (OP_READ | OP_WRITE | OP_VALID_ANYWAY),
		.packed_size = SJA1110_SIZE_GENERAL_PARAMS_DYN_CMD,
		.addr = SJA1110_SPI_ADDR(0xe8),
	},
	[BLK_IDX_RETAGGING] = {
		.entry_packing = sja1110_retagging_entry_packing,
		.cmd_packing = sja1110_retagging_cmd_packing,
		.max_entry_count = SJA1105_MAX_RETAGGING_COUNT,
		.access = (OP_READ | OP_WRITE | OP_DEL),
		.packed_size = SJA1105_SIZE_RETAGGING_DYN_CMD,
		.addr = SJA1110_SPI_ADDR(0xdc),
	},
	[BLK_IDX_CBS] = {
		.entry_packing = sja1110_cbs_entry_packing,
		.cmd_packing = sja1110_cbs_cmd_packing,
		.max_entry_count = SJA1110_MAX_CBS_COUNT,
		.access = (OP_READ | OP_WRITE | OP_VALID_ANYWAY),
		.packed_size = SJA1105PQRS_SIZE_CBS_DYN_CMD,
		.addr = SJA1110_SPI_ADDR(0xc4),
	},
	[BLK_IDX_XMII_PARAMS] = {
		.entry_packing = sja1110_xmii_params_entry_packing,
		.cmd_packing = sja1110_dummy_cmd_packing,
		.max_entry_count = SJA1105_MAX_XMII_PARAMS_COUNT,
		.access = (OP_READ | OP_VALID_ANYWAY),
		.packed_size = SJA1110_SIZE_XMII_PARAMS_DYN_CMD,
		.addr = SJA1110_SPI_ADDR(0x3c),
	},
	[BLK_IDX_L2_POLICING] = {
		.entry_packing = sja1110_l2_policing_entry_packing,
		.cmd_packing = sja1110_l2_policing_cmd_packing,
		.max_entry_count = SJA1110_MAX_L2_POLICING_COUNT,
		.access = (OP_READ | OP_WRITE | OP_VALID_ANYWAY),
		.packed_size = SJA1110_SIZE_L2_POLICING_DYN_CMD,
		.addr = SJA1110_SPI_ADDR(0x2fc),
	},
	[BLK_IDX_L2_FORWARDING_PARAMS] = {
		.entry_packing = sja1110_l2_forwarding_params_entry_packing,
		.cmd_packing = sja1110_dummy_cmd_packing,
		.max_entry_count = SJA1105_MAX_L2_FORWARDING_PARAMS_COUNT,
		.access = (OP_READ | OP_VALID_ANYWAY),
		.packed_size = SJA1110_SIZE_L2_FORWARDING_PARAMS_DYN_CMD,
		.addr = SJA1110_SPI_ADDR(0x20000),
	},
};

#define SJA1105_DYNAMIC_CONFIG_SLEEP_US		10
#define SJA1105_DYNAMIC_CONFIG_TIMEOUT_US	100000

static int
sja1105_dynamic_config_poll_valid(struct sja1105_private *priv,
				  const struct sja1105_dynamic_table_ops *ops,
				  void *entry, bool check_valident,
				  bool check_errors)
{
	u8 packed_buf[SJA1105_MAX_DYN_CMD_SIZE] = {};
	struct sja1105_dyn_cmd cmd = {};
	int rc;

	 
	rc = sja1105_xfer_buf(priv, SPI_READ, ops->addr, packed_buf,
			      ops->packed_size);
	if (rc)
		return rc;

	 
	ops->cmd_packing(packed_buf, &cmd, UNPACK);

	 
	if (cmd.valid)
		return -EAGAIN;

	if (check_valident && !cmd.valident && !(ops->access & OP_VALID_ANYWAY))
		return -ENOENT;

	if (check_errors && cmd.errors)
		return -EINVAL;

	 
	if (entry)
		ops->entry_packing(packed_buf, entry, UNPACK);

	return 0;
}

 
static int
sja1105_dynamic_config_wait_complete(struct sja1105_private *priv,
				     const struct sja1105_dynamic_table_ops *ops,
				     void *entry, bool check_valident,
				     bool check_errors)
{
	int err, rc;

	err = read_poll_timeout(sja1105_dynamic_config_poll_valid,
				rc, rc != -EAGAIN,
				SJA1105_DYNAMIC_CONFIG_SLEEP_US,
				SJA1105_DYNAMIC_CONFIG_TIMEOUT_US,
				false, priv, ops, entry, check_valident,
				check_errors);
	return err < 0 ? err : rc;
}

 
int sja1105_dynamic_config_read(struct sja1105_private *priv,
				enum sja1105_blk_idx blk_idx,
				int index, void *entry)
{
	const struct sja1105_dynamic_table_ops *ops;
	struct sja1105_dyn_cmd cmd = {0};
	 
	u8 packed_buf[SJA1105_MAX_DYN_CMD_SIZE] = {0};
	int rc;

	if (blk_idx >= BLK_IDX_MAX_DYN)
		return -ERANGE;

	ops = &priv->info->dyn_ops[blk_idx];

	if (index >= 0 && index >= ops->max_entry_count)
		return -ERANGE;
	if (index < 0 && !(ops->access & OP_SEARCH))
		return -EOPNOTSUPP;
	if (!(ops->access & OP_READ))
		return -EOPNOTSUPP;
	if (ops->packed_size > SJA1105_MAX_DYN_CMD_SIZE)
		return -ERANGE;
	if (!ops->cmd_packing)
		return -EOPNOTSUPP;
	if (!ops->entry_packing)
		return -EOPNOTSUPP;

	cmd.valid = true;  
	cmd.rdwrset = SPI_READ;  
	if (index < 0) {
		 
		cmd.index = 0;
		cmd.search = true;
	} else {
		cmd.index = index;
		cmd.search = false;
	}
	cmd.valident = true;
	ops->cmd_packing(packed_buf, &cmd, PACK);

	if (cmd.search)
		ops->entry_packing(packed_buf, entry, PACK);

	 
	mutex_lock(&priv->dynamic_config_lock);
	rc = sja1105_xfer_buf(priv, SPI_WRITE, ops->addr, packed_buf,
			      ops->packed_size);
	if (rc < 0)
		goto out;

	rc = sja1105_dynamic_config_wait_complete(priv, ops, entry, true, false);
out:
	mutex_unlock(&priv->dynamic_config_lock);

	return rc;
}

int sja1105_dynamic_config_write(struct sja1105_private *priv,
				 enum sja1105_blk_idx blk_idx,
				 int index, void *entry, bool keep)
{
	const struct sja1105_dynamic_table_ops *ops;
	struct sja1105_dyn_cmd cmd = {0};
	 
	u8 packed_buf[SJA1105_MAX_DYN_CMD_SIZE] = {0};
	int rc;

	if (blk_idx >= BLK_IDX_MAX_DYN)
		return -ERANGE;

	ops = &priv->info->dyn_ops[blk_idx];

	if (index >= ops->max_entry_count)
		return -ERANGE;
	if (index < 0)
		return -ERANGE;
	if (!(ops->access & OP_WRITE))
		return -EOPNOTSUPP;
	if (!keep && !(ops->access & OP_DEL))
		return -EOPNOTSUPP;
	if (ops->packed_size > SJA1105_MAX_DYN_CMD_SIZE)
		return -ERANGE;

	cmd.valident = keep;  
	cmd.valid = true;  
	cmd.rdwrset = SPI_WRITE;  
	cmd.index = index;

	if (!ops->cmd_packing)
		return -EOPNOTSUPP;
	ops->cmd_packing(packed_buf, &cmd, PACK);

	if (!ops->entry_packing)
		return -EOPNOTSUPP;
	 
	if (keep)
		ops->entry_packing(packed_buf, entry, PACK);

	 
	mutex_lock(&priv->dynamic_config_lock);
	rc = sja1105_xfer_buf(priv, SPI_WRITE, ops->addr, packed_buf,
			      ops->packed_size);
	if (rc < 0)
		goto out;

	rc = sja1105_dynamic_config_wait_complete(priv, ops, NULL, false, true);
out:
	mutex_unlock(&priv->dynamic_config_lock);

	return rc;
}

static u8 sja1105_crc8_add(u8 crc, u8 byte, u8 poly)
{
	int i;

	for (i = 0; i < 8; i++) {
		if ((crc ^ byte) & (1 << 7)) {
			crc <<= 1;
			crc ^= poly;
		} else {
			crc <<= 1;
		}
		byte <<= 1;
	}
	return crc;
}

 
u8 sja1105et_fdb_hash(struct sja1105_private *priv, const u8 *addr, u16 vid)
{
	struct sja1105_l2_lookup_params_entry *l2_lookup_params =
		priv->static_config.tables[BLK_IDX_L2_LOOKUP_PARAMS].entries;
	u64 input, poly_koopman = l2_lookup_params->poly;
	 
	u8 poly = (u8)(1 + (poly_koopman << 1));
	u8 crc = 0;  
	int i;

	input = ((u64)vid << 48) | ether_addr_to_u64(addr);

	 
	for (i = 56; i >= 0; i -= 8) {
		u8 byte = (input & (0xffull << i)) >> i;

		crc = sja1105_crc8_add(crc, byte, poly);
	}
	return crc;
}
