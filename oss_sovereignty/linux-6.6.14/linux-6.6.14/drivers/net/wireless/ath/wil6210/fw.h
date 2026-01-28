#ifndef __WIL_FW_H__
#define __WIL_FW_H__
#define WIL_FW_SIGNATURE (0x36323130)  
#define WIL_FW_FMT_VERSION (1)  
enum wil_fw_record_type {
	wil_fw_type_comment = 1,
	wil_fw_type_data = 2,
	wil_fw_type_fill = 3,
	wil_fw_type_action = 4,
	wil_fw_type_verify = 5,
	wil_fw_type_file_header = 6,
	wil_fw_type_direct_write = 7,
	wil_fw_type_gateway_data = 8,
	wil_fw_type_gateway_data4 = 9,
};
struct wil_fw_record_head {
	__le16 type;  
	__le16 flags;  
	__le32 size;  
} __packed;
struct wil_fw_record_data {  
	__le32 addr;
	__le32 data[];  
} __packed;
struct wil_fw_record_fill {  
	__le32 addr;
	__le32 value;
	__le32 size;
} __packed;
struct wil_fw_record_comment {  
	DECLARE_FLEX_ARRAY(u8, data);  
} __packed;
struct wil_fw_record_comment_hdr {
	__le32 magic;
};
#define WIL_FW_CAPABILITIES_MAGIC (0xabcddcba)
struct wil_fw_record_capabilities {  
	struct wil_fw_record_comment_hdr hdr;
	u8 capabilities[];
} __packed;
#define WIL_FW_CONCURRENCY_MAGIC (0xfedccdef)
#define WIL_FW_CONCURRENCY_REC_VER	1
struct wil_fw_concurrency_limit {
	__le16 max;  
	__le16 types;  
} __packed;
struct wil_fw_concurrency_combo {
	u8 n_limits;  
	u8 max_interfaces;  
	u8 n_diff_channels;  
	u8 same_bi;  
	struct wil_fw_concurrency_limit limits[];
} __packed;
struct wil_fw_record_concurrency {  
	__le32 magic;
	u8 version;
	u8 n_mids;
	__le16 n_combos;
	struct wil_fw_concurrency_combo combos[];
} __packed;
#define WIL_BRD_FILE_MAGIC (0xabcddcbb)
struct brd_info {
	__le32 base_addr;
	__le32 max_size_bytes;
} __packed;
struct wil_fw_record_brd_file {  
	struct wil_fw_record_comment_hdr hdr;
	__le32 version;
	struct brd_info brd_info[];
} __packed;
struct wil_fw_record_action {  
	__le32 action;  
	__le32 data[];  
} __packed;
struct wil_fw_data_dwrite {
	__le32 addr;
	__le32 value;
	__le32 mask;
} __packed;
struct wil_fw_record_direct_write {  
	DECLARE_FLEX_ARRAY(struct wil_fw_data_dwrite, data);
} __packed;
struct wil_fw_record_verify {  
	__le32 addr;  
	__le32 value;  
	__le32 mask;  
} __packed;
#define WIL_FW_VERSION_PREFIX "FW version: "
#define WIL_FW_VERSION_PREFIX_LEN (sizeof(WIL_FW_VERSION_PREFIX) - 1)
struct wil_fw_record_file_header {
	__le32 signature ;  
	__le32 reserved;
	__le32 crc;  
	__le32 version;  
	__le32 data_len;  
	u8 comment[32];  
} __packed;
struct wil_fw_data_gw {
	__le32 addr;
	__le32 value;
} __packed;
struct wil_fw_record_gateway_data {  
	__le32 gateway_addr_addr;
	__le32 gateway_value_addr;
	__le32 gateway_cmd_addr;
	__le32 gateway_ctrl_address;
#define WIL_FW_GW_CTL_BUSY	BIT(29)  
#define WIL_FW_GW_CTL_RUN	BIT(30)  
	__le32 command;
	struct wil_fw_data_gw data[];  
} __packed;
struct wil_fw_data_gw4 {
	__le32 addr;
	__le32 value[4];
} __packed;
struct wil_fw_record_gateway_data4 {  
	__le32 gateway_addr_addr;
	__le32 gateway_value_addr[4];
	__le32 gateway_cmd_addr;
	__le32 gateway_ctrl_address;  
	__le32 command;
	struct wil_fw_data_gw4 data[];  
} __packed;
#endif  
