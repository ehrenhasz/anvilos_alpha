 
 
#ifndef __iwl_fw_api_txq_h__
#define __iwl_fw_api_txq_h__

 
enum iwl_mvm_dqa_txq {
	IWL_MVM_DQA_CMD_QUEUE = 0,
	IWL_MVM_DQA_AUX_QUEUE = 1,
	IWL_MVM_DQA_P2P_DEVICE_QUEUE = 2,
	IWL_MVM_DQA_INJECT_MONITOR_QUEUE = 2,
	IWL_MVM_DQA_GCAST_QUEUE = 3,
	IWL_MVM_DQA_BSS_CLIENT_QUEUE = 4,
	IWL_MVM_DQA_MIN_MGMT_QUEUE = 5,
	IWL_MVM_DQA_MAX_MGMT_QUEUE = 8,
	IWL_MVM_DQA_AP_PROBE_RESP_QUEUE = 9,
	IWL_MVM_DQA_MIN_DATA_QUEUE = 10,
	IWL_MVM_DQA_MAX_DATA_QUEUE = 30,
};

enum iwl_mvm_tx_fifo {
	IWL_MVM_TX_FIFO_BK = 0,
	IWL_MVM_TX_FIFO_BE,
	IWL_MVM_TX_FIFO_VI,
	IWL_MVM_TX_FIFO_VO,
	IWL_MVM_TX_FIFO_MCAST = 5,
	IWL_MVM_TX_FIFO_CMD = 7,
};

enum iwl_gen2_tx_fifo {
	IWL_GEN2_TX_FIFO_CMD = 0,
	IWL_GEN2_EDCA_TX_FIFO_BK,
	IWL_GEN2_EDCA_TX_FIFO_BE,
	IWL_GEN2_EDCA_TX_FIFO_VI,
	IWL_GEN2_EDCA_TX_FIFO_VO,
	IWL_GEN2_TRIG_TX_FIFO_BK,
	IWL_GEN2_TRIG_TX_FIFO_BE,
	IWL_GEN2_TRIG_TX_FIFO_VI,
	IWL_GEN2_TRIG_TX_FIFO_VO,
};

 
enum iwl_tx_queue_cfg_actions {
	TX_QUEUE_CFG_ENABLE_QUEUE		= BIT(0),
	TX_QUEUE_CFG_TFD_SHORT_FORMAT		= BIT(1),
};

#define IWL_DEFAULT_QUEUE_SIZE_EHT (1024 * 4)
#define IWL_DEFAULT_QUEUE_SIZE_HE 1024
#define IWL_DEFAULT_QUEUE_SIZE 256
#define IWL_MGMT_QUEUE_SIZE 16
#define IWL_CMD_QUEUE_SIZE 32
 
struct iwl_tx_queue_cfg_cmd {
	u8 sta_id;
	u8 tid;
	__le16 flags;
	__le32 cb_size;
	__le64 byte_cnt_addr;
	__le64 tfdq_addr;
} __packed;  

 
struct iwl_tx_queue_cfg_rsp {
	__le16 queue_number;
	__le16 flags;
	__le16 write_pointer;
	__le16 reserved;
} __packed;  

#endif  
