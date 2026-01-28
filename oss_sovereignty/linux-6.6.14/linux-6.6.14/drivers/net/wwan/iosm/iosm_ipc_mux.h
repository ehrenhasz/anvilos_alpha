#ifndef IOSM_IPC_MUX_H
#define IOSM_IPC_MUX_H
#include "iosm_ipc_protocol.h"
#define IPC_MEM_MAX_UL_DG_ENTRIES	100
#define IPC_MEM_MAX_TDS_MUX_AGGR_UL	60
#define IPC_MEM_MAX_TDS_MUX_AGGR_DL	60
#define IPC_MEM_MAX_ADB_BUF_SIZE (16 * 1024)
#define IPC_MEM_MAX_UL_ADB_BUF_SIZE IPC_MEM_MAX_ADB_BUF_SIZE
#define IPC_MEM_MAX_DL_ADB_BUF_SIZE IPC_MEM_MAX_ADB_BUF_SIZE
#define IPC_MEM_MAX_DL_MUX_LITE_BUF_SIZE (2 * 1024)
#define IPC_MEM_MAX_TDS_MUX_LITE_UL 800
#define IPC_MEM_MAX_TDS_MUX_LITE_DL 1200
#define MUX_CMD_OPEN_SESSION 1
#define MUX_CMD_OPEN_SESSION_RESP 2
#define MUX_CMD_CLOSE_SESSION 3
#define MUX_CMD_CLOSE_SESSION_RESP 4
#define MUX_LITE_CMD_FLOW_CTL 5
#define MUX_LITE_CMD_FLOW_CTL_ACK 6
#define MUX_LITE_CMD_LINK_STATUS_REPORT 7
#define MUX_LITE_CMD_LINK_STATUS_REPORT_RESP 8
#define MUX_CMD_INVALID 255
#define MUX_CMD_RESP_SUCCESS 0
#define IPC_MEM_WWAN_MUX BIT(0)
enum mux_event {
	MUX_E_INACTIVE,  
	MUX_E_MUX_SESSION_OPEN,  
	MUX_E_MUX_SESSION_CLOSE,  
	MUX_E_MUX_CHANNEL_CLOSE,  
	MUX_E_NO_ORDERS,  
	MUX_E_NOT_APPLICABLE,  
};
struct mux_session_open {
	enum mux_event event;
	__le32 if_id;
};
struct mux_session_close {
	enum mux_event event;
	__le32 if_id;
};
struct mux_channel_close {
	enum mux_event event;
};
struct mux_common {
	enum mux_event event;
};
union mux_msg {
	struct mux_session_open session_open;
	struct mux_session_close session_close;
	struct mux_channel_close channel_close;
	struct mux_common common;
};
struct mux_cmd_open_session {
	u8 flow_ctrl;  
	u8 ipv4v6_hints;  
	__le16 reserved2;  
	__le32 dl_head_pad_len;  
};
struct mux_cmd_open_session_resp {
	__le32 response;  
	u8 flow_ctrl;  
	u8 ipv4v6_hints;  
	__le16 reserved2;  
	__le32 ul_head_pad_len;  
};
struct mux_cmd_close_session_resp {
	__le32 response;
};
struct mux_cmd_flow_ctl {
	__le32 mask;  
};
struct mux_cmd_link_status_report {
	u8 payload;
};
struct mux_cmd_link_status_report_resp {
	__le32 response;
};
union mux_cmd_param {
	struct mux_cmd_open_session open_session;
	struct mux_cmd_open_session_resp open_session_resp;
	struct mux_cmd_close_session_resp close_session_resp;
	struct mux_cmd_flow_ctl flow_ctl;
	struct mux_cmd_link_status_report link_status;
	struct mux_cmd_link_status_report_resp link_status_resp;
};
enum mux_state {
	MUX_S_INACTIVE,  
	MUX_S_ACTIVE,  
	MUX_S_ERROR,  
};
enum ipc_mux_protocol {
	MUX_UNKNOWN,
	MUX_LITE,
	MUX_AGGREGATION,
};
enum ipc_mux_ul_flow {
	MUX_UL_UNKNOWN,
	MUX_UL,  
	MUX_UL_ON_CREDITS,  
};
struct mux_session {
	struct iosm_wwan *wwan;  
	int if_id;  
	u32 flags;
	u32 ul_head_pad_len;  
	u32 dl_head_pad_len;  
	struct sk_buff_head ul_list;  
	u32 flow_ctl_mask;  
	u32 flow_ctl_en_cnt;  
	u32 flow_ctl_dis_cnt;  
	int ul_flow_credits;  
	u8 net_tx_stop:1,
	   flush:1;  
};
struct mux_adth_dg {
	__le32 datagram_index;
	__le16 datagram_length;
	u8 service_class;
	u8 reserved;
};
struct mux_qlth_ql {
	__le32 nr_of_bytes;
};
struct mux_qlth {
	__le32 signature;
	__le16 table_length;
	u8 if_id;
	u8 reserved;
	__le32 next_table_index;
	__le32 reserved2;
	struct mux_qlth_ql ql;
};
struct mux_adb {
	struct sk_buff *dest_skb;
	u8 *buf;
	struct mux_adgh *adgh;
	struct sk_buff *qlth_skb;
	u32 *next_table_index;
	struct sk_buff_head free_list;
	int size;
	u32 if_cnt;
	u32 dg_cnt_total;
	u32 payload_size;
	struct mux_adth_dg
		dg[IPC_MEM_MUX_IP_SESSION_ENTRIES][IPC_MEM_MAX_UL_DG_ENTRIES];
	struct mux_qlth *pp_qlt[IPC_MEM_MUX_IP_SESSION_ENTRIES];
	struct mux_adbh *adbh;
	u32 qlt_updated[IPC_MEM_MUX_IP_SESSION_ENTRIES];
	u32 dg_count[IPC_MEM_MUX_IP_SESSION_ENTRIES];
};
struct mux_acb {
	struct sk_buff *skb;  
	int if_id;  
	u8 *buf_p;
	u32 wanted_response;
	u32 got_response;
	u32 cmd;
	union mux_cmd_param got_param;  
};
struct iosm_mux {
	struct device *dev;
	struct mux_session session[IPC_MEM_MUX_IP_SESSION_ENTRIES];
	struct ipc_mem_channel *channel;
	struct iosm_pcie *pcie;
	struct iosm_imem *imem;
	struct iosm_wwan *wwan;
	struct iosm_protocol *ipc_protocol;
	int channel_id;
	enum ipc_mux_protocol protocol;
	enum ipc_mux_ul_flow ul_flow;
	int nr_sessions;
	int instance_id;
	enum mux_state state;
	enum mux_event event;
	u32 tx_transaction_id;
	int rr_next_session;
	struct mux_adb ul_adb;
	int size_needed;
	long long ul_data_pend_bytes;
	struct mux_acb acb;
	int wwan_q_offset;
	u16 acb_tx_sequence_nr;
	u16 adb_tx_sequence_nr;
	unsigned long long acc_adb_size;
	unsigned long long acc_payload_size;
	u8 initialized:1,
	   ev_mux_net_transmit_pending:1,
	   adb_prep_ongoing;
} __packed;
struct ipc_mux_config {
	enum ipc_mux_protocol protocol;
	enum ipc_mux_ul_flow ul_flow;
	int instance_id;
};
struct iosm_mux *ipc_mux_init(struct ipc_mux_config *mux_cfg,
			      struct iosm_imem *ipc_imem);
void ipc_mux_deinit(struct iosm_mux *ipc_mux);
void ipc_mux_check_n_restart_tx(struct iosm_mux *ipc_mux);
enum ipc_mux_protocol ipc_mux_get_active_protocol(struct iosm_mux *ipc_mux);
int ipc_mux_open_session(struct iosm_mux *ipc_mux, int session_nr);
int ipc_mux_close_session(struct iosm_mux *ipc_mux, int session_nr);
int ipc_mux_get_max_sessions(struct iosm_mux *ipc_mux);
#endif
