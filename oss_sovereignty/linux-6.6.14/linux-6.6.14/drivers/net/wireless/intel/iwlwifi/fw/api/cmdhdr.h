#ifndef __iwl_fw_api_cmdhdr_h__
#define __iwl_fw_api_cmdhdr_h__
#define SEQ_TO_QUEUE(s)	(((s) >> 8) & 0x1f)
#define QUEUE_TO_SEQ(q)	(((q) & 0x1f) << 8)
#define SEQ_TO_INDEX(s)	((s) & 0xff)
#define INDEX_TO_SEQ(i)	((i) & 0xff)
#define SEQ_RX_FRAME	cpu_to_le16(0x8000)
static inline u8 iwl_cmd_opcode(u32 cmdid)
{
	return cmdid & 0xFF;
}
static inline u8 iwl_cmd_groupid(u32 cmdid)
{
	return ((cmdid & 0xFF00) >> 8);
}
static inline u8 iwl_cmd_version(u32 cmdid)
{
	return ((cmdid & 0xFF0000) >> 16);
}
static inline u32 iwl_cmd_id(u8 opcode, u8 groupid, u8 version)
{
	return opcode + (groupid << 8) + (version << 16);
}
#define WIDE_ID(grp, opcode) (((grp) << 8) | (opcode))
#define DEF_ID(opcode) ((1 << 8) | (opcode))
#define IWL_ALWAYS_LONG_GROUP	1
struct iwl_cmd_header {
	u8 cmd;
	u8 group_id;
	__le16 sequence;
} __packed;
struct iwl_cmd_header_wide {
	u8 cmd;
	u8 group_id;
	__le16 sequence;
	__le16 length;
	u8 reserved;
	u8 version;
} __packed;
struct iwl_calib_res_notif_phy_db {
	__le16 type;
	__le16 length;
	u8 data[];
} __packed;
struct iwl_phy_db_cmd {
	__le16 type;
	__le16 length;
	u8 data[];
} __packed;
struct iwl_cmd_response {
	__le32 status;
};
#endif  
