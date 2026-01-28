#ifndef __iwl_fw_api_coex_h__
#define __iwl_fw_api_coex_h__
#include <linux/types.h>
#include <linux/bitops.h>
#define BITS(nb) (BIT(nb) - 1)
enum iwl_bt_coex_lut_type {
	BT_COEX_TIGHT_LUT = 0,
	BT_COEX_LOOSE_LUT,
	BT_COEX_TX_DIS_LUT,
	BT_COEX_MAX_LUT,
	BT_COEX_INVALID_LUT = 0xff,
};  
#define BT_REDUCED_TX_POWER_BIT BIT(7)
enum iwl_bt_coex_mode {
	BT_COEX_DISABLE			= 0x0,
	BT_COEX_NW			= 0x1,
	BT_COEX_BT			= 0x2,
	BT_COEX_WIFI			= 0x3,
};  
enum iwl_bt_coex_enabled_modules {
	BT_COEX_MPLUT_ENABLED		= BIT(0),
	BT_COEX_MPLUT_BOOST_ENABLED	= BIT(1),
	BT_COEX_SYNC2SCO_ENABLED	= BIT(2),
	BT_COEX_CORUN_ENABLED		= BIT(3),
	BT_COEX_HIGH_BAND_RET		= BIT(4),
};  
struct iwl_bt_coex_cmd {
	__le32 mode;
	__le32 enabled_modules;
} __packed;  
struct iwl_bt_coex_reduced_txp_update_cmd {
	__le32 reduced_txp;
} __packed;  
struct iwl_bt_coex_ci_cmd {
	__le64 bt_primary_ci;
	__le32 primary_ch_phy_id;
	__le64 bt_secondary_ci;
	__le32 secondary_ch_phy_id;
} __packed;  
#define BT_MBOX(n_dw, _msg, _pos, _nbits)	\
	BT_MBOX##n_dw##_##_msg##_POS = (_pos),	\
	BT_MBOX##n_dw##_##_msg = BITS(_nbits) << BT_MBOX##n_dw##_##_msg##_POS
enum iwl_bt_mxbox_dw0 {
	BT_MBOX(0, LE_SLAVE_LAT, 0, 3),
	BT_MBOX(0, LE_PROF1, 3, 1),
	BT_MBOX(0, LE_PROF2, 4, 1),
	BT_MBOX(0, LE_PROF_OTHER, 5, 1),
	BT_MBOX(0, CHL_SEQ_N, 8, 4),
	BT_MBOX(0, INBAND_S, 13, 1),
	BT_MBOX(0, LE_MIN_RSSI, 16, 4),
	BT_MBOX(0, LE_SCAN, 20, 1),
	BT_MBOX(0, LE_ADV, 21, 1),
	BT_MBOX(0, LE_MAX_TX_POWER, 24, 4),
	BT_MBOX(0, OPEN_CON_1, 28, 2),
};
enum iwl_bt_mxbox_dw1 {
	BT_MBOX(1, BR_MAX_TX_POWER, 0, 4),
	BT_MBOX(1, IP_SR, 4, 1),
	BT_MBOX(1, LE_MSTR, 5, 1),
	BT_MBOX(1, AGGR_TRFC_LD, 8, 6),
	BT_MBOX(1, MSG_TYPE, 16, 3),
	BT_MBOX(1, SSN, 19, 2),
};
enum iwl_bt_mxbox_dw2 {
	BT_MBOX(2, SNIFF_ACT, 0, 3),
	BT_MBOX(2, PAG, 3, 1),
	BT_MBOX(2, INQUIRY, 4, 1),
	BT_MBOX(2, CONN, 5, 1),
	BT_MBOX(2, SNIFF_INTERVAL, 8, 5),
	BT_MBOX(2, DISC, 13, 1),
	BT_MBOX(2, SCO_TX_ACT, 16, 2),
	BT_MBOX(2, SCO_RX_ACT, 18, 2),
	BT_MBOX(2, ESCO_RE_TX, 20, 2),
	BT_MBOX(2, SCO_DURATION, 24, 6),
};
enum iwl_bt_mxbox_dw3 {
	BT_MBOX(3, SCO_STATE, 0, 1),
	BT_MBOX(3, SNIFF_STATE, 1, 1),
	BT_MBOX(3, A2DP_STATE, 2, 1),
	BT_MBOX(3, ACL_STATE, 3, 1),
	BT_MBOX(3, MSTR_STATE, 4, 1),
	BT_MBOX(3, OBX_STATE, 5, 1),
	BT_MBOX(3, A2DP_SRC, 6, 1),
	BT_MBOX(3, OPEN_CON_2, 8, 2),
	BT_MBOX(3, TRAFFIC_LOAD, 10, 2),
	BT_MBOX(3, CHL_SEQN_LSB, 12, 1),
	BT_MBOX(3, INBAND_P, 13, 1),
	BT_MBOX(3, MSG_TYPE_2, 16, 3),
	BT_MBOX(3, SSN_2, 19, 2),
	BT_MBOX(3, UPDATE_REQUEST, 21, 1),
};
#define BT_MBOX_MSG(_notif, _num, _field)				     \
	((le32_to_cpu((_notif)->mbox_msg[(_num)]) & BT_MBOX##_num##_##_field)\
	>> BT_MBOX##_num##_##_field##_POS)
#define BT_MBOX_PRINT(_num, _field, _end)				    \
			pos += scnprintf(buf + pos, bufsz - pos,	    \
					 "\t%s: %d%s",			    \
					 #_field,			    \
					 BT_MBOX_MSG(notif, _num, _field),  \
					 true ? "\n" : ", ")
enum iwl_bt_activity_grading {
	BT_OFF			= 0,
	BT_ON_NO_CONNECTION	= 1,
	BT_LOW_TRAFFIC		= 2,
	BT_HIGH_TRAFFIC		= 3,
	BT_VERY_HIGH_TRAFFIC	= 4,
	BT_MAX_AG,
};  
enum iwl_bt_ci_compliance {
	BT_CI_COMPLIANCE_NONE		= 0,
	BT_CI_COMPLIANCE_PRIMARY	= 1,
	BT_CI_COMPLIANCE_SECONDARY	= 2,
	BT_CI_COMPLIANCE_BOTH		= 3,
};  
struct iwl_bt_coex_profile_notif {
	__le32 mbox_msg[4];
	__le32 msg_idx;
	__le32 bt_ci_compliance;
	__le32 primary_ch_lut;
	__le32 secondary_ch_lut;
	__le32 bt_activity_grading;
	u8 ttc_status;
	u8 rrc_status;
	__le16 reserved;
} __packed;  
#endif  
