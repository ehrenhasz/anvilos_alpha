

#ifndef IOSM_IPC_MUX_CODEC_H
#define IOSM_IPC_MUX_CODEC_H

#include "iosm_ipc_mux.h"


#define MUX_QUEUE_LEVEL 1


#define IOSM_AGGR_MUX_ADB_FINISH_TIMEOUT_NSEC (500 * 1000)


#define IOSM_AGGR_MUX_CMD_FLOW_CTL_ENABLE 5


#define IOSM_AGGR_MUX_CMD_FLOW_CTL_DISABLE 6


#define IOSM_AGGR_MUX_CMD_FLOW_CTL_ACK 7


#define IOSM_AGGR_MUX_CMD_LINK_STATUS_REPORT 8


#define IOSM_AGGR_MUX_CMD_LINK_STATUS_REPORT_RESP 9


#define IOSM_AGGR_MUX_SIG_ACBH 0x48424341


#define IOSM_AGGR_MUX_SIG_ADTH 0x48544441


#define IOSM_AGGR_MUX_SIG_ADBH 0x48424441


#define IOSM_AGGR_MUX_SIG_ADGH 0x48474441


#define MUX_MAX_UL_ACB_BUF_SIZE 256


#define MUX_MAX_UL_DG_ENTRIES 100


#define MUX_SIG_ADGH 0x48474441


#define MUX_SIG_CMDH 0x48444D43


#define MUX_SIG_QLTH 0x48544C51


#define MUX_SIG_FCTH 0x48544346


#define IPC_MEM_MUX_UL_SESS_FCOFF_THRESHOLD_FACTOR (4)


#define IPC_MEM_MAX_DL_MUX_LITE_BUF_SIZE (2 * 1024)


#define IPC_MEM_MUX_UL_SESS_FCON_THRESHOLD (64)


#define IPC_MUX_CMD_RUN_DEFAULT_TIMEOUT 1000 


#define IPC_MEM_MUX_UL_FLOWCTRL_LOW_B 10240 


#define IPC_MEM_MUX_UL_FLOWCTRL_HIGH_B (110 * 1024)


struct mux_cmdh {
	__le32 signature;
	__le16 cmd_len;
	u8 if_id;
	u8 reserved;
	__le32 next_cmd_index;
	__le32 command_type;
	__le32 transaction_id;
	union mux_cmd_param param;
};


struct mux_acbh {
	__le32 signature;
	__le16 reserved;
	__le16 sequence_nr;
	__le32 block_length;
	__le32 first_cmd_index;
};


struct mux_adbh {
	__le32 signature;
	__le16 reserved;
	__le16 sequence_nr;
	__le32 block_length;
	__le32 first_table_index;
};


struct mux_adth {
	__le32 signature;
	__le16 table_length;
	u8 if_id;
	u8 opt_ipv4v6;
	__le32 next_table_index;
	__le32 reserved2;
	struct mux_adth_dg dg[];
};


struct mux_adgh {
	__le32 signature;
	__le16 length;
	u8 if_id;
	u8 opt_ipv4v6;
	u8 service_class;
	u8 next_count;
	u8 reserved1[6];
};


struct mux_lite_cmdh {
	__le32 signature;
	__le16 cmd_len;
	u8 if_id;
	u8 reserved;
	__le32 command_type;
	__le32 transaction_id;
	union mux_cmd_param param;
};


struct mux_lite_vfl {
	__le32 nr_of_bytes;
};


struct ipc_mem_lite_gen_tbl {
	__le32 signature;
	__le16 length;
	u8 if_id;
	u8 vfl_length;
	u32 reserved[2];
	struct mux_lite_vfl vfl;
};


union mux_type_cmdh {
	struct mux_lite_cmdh *ack_lite;
	struct mux_cmdh *ack_aggr;
};


union mux_type_header {
	struct mux_adgh *adgh;
	struct mux_adbh *adbh;
};

void ipc_mux_dl_decode(struct iosm_mux *ipc_mux, struct sk_buff *skb);


int ipc_mux_dl_acb_send_cmds(struct iosm_mux *ipc_mux, u32 cmd_type, u8 if_id,
			     u32 transaction_id, union mux_cmd_param *param,
			     size_t res_size, bool blocking, bool respond);


void ipc_mux_netif_tx_flowctrl(struct mux_session *session, int idx, bool on);


int ipc_mux_ul_trigger_encode(struct iosm_mux *ipc_mux, int if_id,
			      struct sk_buff *skb);

bool ipc_mux_ul_data_encode(struct iosm_mux *ipc_mux);


void ipc_mux_ul_encoded_process(struct iosm_mux *ipc_mux, struct sk_buff *skb);

void ipc_mux_ul_adb_finish(struct iosm_mux *ipc_mux);

void ipc_mux_ul_adb_update_ql(struct iosm_mux *ipc_mux, struct mux_adb *p_adb,
			      int session_id, int qlth_n_ql_size,
			      struct sk_buff_head *ul_list);

#endif
