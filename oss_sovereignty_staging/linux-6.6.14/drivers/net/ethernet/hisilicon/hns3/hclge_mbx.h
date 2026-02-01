 
 

#ifndef __HCLGE_MBX_H
#define __HCLGE_MBX_H
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/types.h>

enum HCLGE_MBX_OPCODE {
	HCLGE_MBX_RESET = 0x01,		 
	HCLGE_MBX_ASSERTING_RESET,	 
	HCLGE_MBX_SET_UNICAST,		 
	HCLGE_MBX_SET_MULTICAST,	 
	HCLGE_MBX_SET_VLAN,		 
	HCLGE_MBX_MAP_RING_TO_VECTOR,	 
	HCLGE_MBX_UNMAP_RING_TO_VECTOR,	 
	HCLGE_MBX_SET_PROMISC_MODE,	 
	HCLGE_MBX_SET_MACVLAN,		 
	HCLGE_MBX_API_NEGOTIATE,	 
	HCLGE_MBX_GET_QINFO,		 
	HCLGE_MBX_GET_QDEPTH,		 
	HCLGE_MBX_GET_BASIC_INFO,	 
	HCLGE_MBX_GET_RETA,		 
	HCLGE_MBX_GET_RSS_KEY,		 
	HCLGE_MBX_GET_MAC_ADDR,		 
	HCLGE_MBX_PF_VF_RESP,		 
	HCLGE_MBX_GET_BDNUM,		 
	HCLGE_MBX_GET_BUFSIZE,		 
	HCLGE_MBX_GET_STREAMID,		 
	HCLGE_MBX_SET_AESTART,		 
	HCLGE_MBX_SET_TSOSTATS,		 
	HCLGE_MBX_LINK_STAT_CHANGE,	 
	HCLGE_MBX_GET_BASE_CONFIG,	 
	HCLGE_MBX_BIND_FUNC_QUEUE,	 
	HCLGE_MBX_GET_LINK_STATUS,	 
	HCLGE_MBX_QUEUE_RESET,		 
	HCLGE_MBX_KEEP_ALIVE,		 
	HCLGE_MBX_SET_ALIVE,		 
	HCLGE_MBX_SET_MTU,		 
	HCLGE_MBX_GET_QID_IN_PF,	 
	HCLGE_MBX_LINK_STAT_MODE,	 
	HCLGE_MBX_GET_LINK_MODE,	 
	HCLGE_MBX_PUSH_VLAN_INFO,	 
	HCLGE_MBX_GET_MEDIA_TYPE,        
	HCLGE_MBX_PUSH_PROMISC_INFO,	 
	HCLGE_MBX_VF_UNINIT,             
	HCLGE_MBX_HANDLE_VF_TBL,	 
	HCLGE_MBX_GET_RING_VECTOR_MAP,	 

	HCLGE_MBX_GET_VF_FLR_STATUS = 200,  
	HCLGE_MBX_PUSH_LINK_STATUS,	 
	HCLGE_MBX_NCSI_ERROR,		 
};

 
enum hclge_mbx_mac_vlan_subcode {
	HCLGE_MBX_MAC_VLAN_UC_MODIFY = 0,	 
	HCLGE_MBX_MAC_VLAN_UC_ADD,		 
	HCLGE_MBX_MAC_VLAN_UC_REMOVE,		 
	HCLGE_MBX_MAC_VLAN_MC_MODIFY,		 
	HCLGE_MBX_MAC_VLAN_MC_ADD,		 
	HCLGE_MBX_MAC_VLAN_MC_REMOVE,		 
};

 
enum hclge_mbx_vlan_cfg_subcode {
	HCLGE_MBX_VLAN_FILTER = 0,	 
	HCLGE_MBX_VLAN_TX_OFF_CFG,	 
	HCLGE_MBX_VLAN_RX_OFF_CFG,	 
	HCLGE_MBX_PORT_BASE_VLAN_CFG,	 
	HCLGE_MBX_GET_PORT_BASE_VLAN_STATE,	 
	HCLGE_MBX_ENABLE_VLAN_FILTER,
};

enum hclge_mbx_tbl_cfg_subcode {
	HCLGE_MBX_VPORT_LIST_CLEAR,
};

#define HCLGE_MBX_MAX_MSG_SIZE	14
#define HCLGE_MBX_MAX_RESP_DATA_SIZE	8U
#define HCLGE_MBX_MAX_RING_CHAIN_PARAM_NUM	4

#define HCLGE_RESET_SCHED_TIMEOUT	(3 * HZ)
#define HCLGE_MBX_SCHED_TIMEOUT	(HZ / 2)

struct hclge_ring_chain_param {
	u8 ring_type;
	u8 tqp_index;
	u8 int_gl_index;
};

struct hclge_basic_info {
	u8 hw_tc_map;
	u8 rsv;
	__le16 mbx_api_version;
	__le32 pf_caps;
};

struct hclgevf_mbx_resp_status {
	struct mutex mbx_mutex;  
	u32 origin_mbx_msg;
	bool received_resp;
	int resp_status;
	u16 match_id;
	u8 additional_info[HCLGE_MBX_MAX_RESP_DATA_SIZE];
};

struct hclge_respond_to_vf_msg {
	int status;
	u8 data[HCLGE_MBX_MAX_RESP_DATA_SIZE];
	u16 len;
};

struct hclge_vf_to_pf_msg {
	u8 code;
	union {
		struct {
			u8 subcode;
			u8 data[HCLGE_MBX_MAX_MSG_SIZE];
		};
		struct {
			u8 en_bc;
			u8 en_uc;
			u8 en_mc;
			u8 en_limit_promisc;
		};
		struct {
			u8 vector_id;
			u8 ring_num;
			struct hclge_ring_chain_param
				param[HCLGE_MBX_MAX_RING_CHAIN_PARAM_NUM];
		};
	};
};

struct hclge_pf_to_vf_msg {
	__le16 code;
	union {
		 
		struct {
			__le16 vf_mbx_msg_code;
			__le16 vf_mbx_msg_subcode;
			__le16 resp_status;
			u8 resp_data[HCLGE_MBX_MAX_RESP_DATA_SIZE];
		};
		 
		struct {
			u8 msg_data[HCLGE_MBX_MAX_MSG_SIZE];
		};
	};
};

struct hclge_mbx_vf_to_pf_cmd {
	u8 rsv;
	u8 mbx_src_vfid;  
	u8 mbx_need_resp;
	u8 rsv1[1];
	u8 msg_len;
	u8 rsv2;
	__le16 match_id;
	struct hclge_vf_to_pf_msg msg;
};

#define HCLGE_MBX_NEED_RESP_B		0

struct hclge_mbx_pf_to_vf_cmd {
	u8 dest_vfid;
	u8 rsv[3];
	u8 msg_len;
	u8 rsv1;
	__le16 match_id;
	struct hclge_pf_to_vf_msg msg;
};

struct hclge_vf_rst_cmd {
	u8 dest_vfid;
	u8 vf_rst;
	u8 rsv[22];
};

#pragma pack(1)
struct hclge_mbx_link_status {
	__le16 link_status;
	__le32 speed;
	__le16 duplex;
	u8 flag;
};

struct hclge_mbx_link_mode {
	__le16 idx;
	__le64 link_mode;
};

struct hclge_mbx_port_base_vlan {
	__le16 state;
	__le16 vlan_proto;
	__le16 qos;
	__le16 vlan_tag;
};

struct hclge_mbx_vf_queue_info {
	__le16 num_tqps;
	__le16 rss_size;
	__le16 rx_buf_len;
};

struct hclge_mbx_vf_queue_depth {
	__le16 num_tx_desc;
	__le16 num_rx_desc;
};

struct hclge_mbx_vlan_filter {
	u8 is_kill;
	__le16 vlan_id;
	__le16 proto;
};

struct hclge_mbx_mtu_info {
	__le32 mtu;
};

#pragma pack()

 
struct hclgevf_mbx_arq_ring {
#define HCLGE_MBX_MAX_ARQ_MSG_SIZE	8
#define HCLGE_MBX_MAX_ARQ_MSG_NUM	1024
	struct hclgevf_dev *hdev;
	u32 head;
	u32 tail;
	atomic_t count;
	__le16 msg_q[HCLGE_MBX_MAX_ARQ_MSG_NUM][HCLGE_MBX_MAX_ARQ_MSG_SIZE];
};

struct hclge_dev;

#define HCLGE_MBX_OPCODE_MAX 256
struct hclge_mbx_ops_param {
	struct hclge_vport *vport;
	struct hclge_mbx_vf_to_pf_cmd *req;
	struct hclge_respond_to_vf_msg *resp_msg;
};

typedef int (*hclge_mbx_ops_fn)(struct hclge_mbx_ops_param *param);

#define hclge_mbx_ring_ptr_move_crq(crq) \
	(crq->next_to_use = (crq->next_to_use + 1) % crq->desc_num)
#define hclge_mbx_tail_ptr_move_arq(arq) \
		(arq.tail = (arq.tail + 1) % HCLGE_MBX_MAX_ARQ_MSG_NUM)
#define hclge_mbx_head_ptr_move_arq(arq) \
		(arq.head = (arq.head + 1) % HCLGE_MBX_MAX_ARQ_MSG_NUM)

 
#define HCLGE_MBX_PUSH_LINK_STATUS_EN			BIT(0)
#endif
