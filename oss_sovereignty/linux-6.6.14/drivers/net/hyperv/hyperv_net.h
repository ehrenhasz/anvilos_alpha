#ifndef _HYPERV_NET_H
#define _HYPERV_NET_H
#include <linux/list.h>
#include <linux/hyperv.h>
#include <linux/rndis.h>
#include <linux/jhash.h>
#include <net/xdp.h>
#define OID_GEN_RECEIVE_SCALE_CAPABILITIES 0x00010203   
#define OID_GEN_RECEIVE_SCALE_PARAMETERS 0x00010204   
#define NDIS_OBJECT_TYPE_RSS_CAPABILITIES 0x88
#define NDIS_OBJECT_TYPE_RSS_PARAMETERS 0x89
#define NDIS_OBJECT_TYPE_OFFLOAD	0xa7
#define NDIS_RECEIVE_SCALE_CAPABILITIES_REVISION_2 2
#define NDIS_RECEIVE_SCALE_PARAMETERS_REVISION_2 2
struct ndis_obj_header {
	u8 type;
	u8 rev;
	u16 size;
} __packed;
#define NDIS_RSS_CAPS_MESSAGE_SIGNALED_INTERRUPTS 0x01000000
#define NDIS_RSS_CAPS_CLASSIFICATION_AT_ISR       0x02000000
#define NDIS_RSS_CAPS_CLASSIFICATION_AT_DPC       0x04000000
#define NDIS_RSS_CAPS_USING_MSI_X                 0x08000000
#define NDIS_RSS_CAPS_RSS_AVAILABLE_ON_PORTS      0x10000000
#define NDIS_RSS_CAPS_SUPPORTS_MSI_X              0x20000000
#define NDIS_RSS_CAPS_HASH_TYPE_TCP_IPV4          0x00000100
#define NDIS_RSS_CAPS_HASH_TYPE_TCP_IPV6          0x00000200
#define NDIS_RSS_CAPS_HASH_TYPE_TCP_IPV6_EX       0x00000400
struct ndis_recv_scale_cap {  
	struct ndis_obj_header hdr;
	u32 cap_flag;
	u32 num_int_msg;
	u32 num_recv_que;
	u16 num_indirect_tabent;
} __packed;
#define NDIS_RSS_PARAM_FLAG_BASE_CPU_UNCHANGED     0x0001
#define NDIS_RSS_PARAM_FLAG_HASH_INFO_UNCHANGED    0x0002
#define NDIS_RSS_PARAM_FLAG_ITABLE_UNCHANGED       0x0004
#define NDIS_RSS_PARAM_FLAG_HASH_KEY_UNCHANGED     0x0008
#define NDIS_RSS_PARAM_FLAG_DISABLE_RSS            0x0010
#define NDIS_HASH_FUNC_TOEPLITZ 0x00000001
#define NDIS_HASH_IPV4          0x00000100
#define NDIS_HASH_TCP_IPV4      0x00000200
#define NDIS_HASH_IPV6          0x00000400
#define NDIS_HASH_IPV6_EX       0x00000800
#define NDIS_HASH_TCP_IPV6      0x00001000
#define NDIS_HASH_TCP_IPV6_EX   0x00002000
#define NDIS_RSS_INDIRECTION_TABLE_MAX_SIZE_REVISION_2 (128 * 4)
#define NDIS_RSS_HASH_SECRET_KEY_MAX_SIZE_REVISION_2   40
#define ITAB_NUM 128
#define ITAB_NUM_MAX 256
struct ndis_recv_scale_param {  
	struct ndis_obj_header hdr;
	u16 flag;
	u16 base_cpu_number;
	u32 hashinfo;
	u16 indirect_tabsize;
	u32 indirect_taboffset;
	u16 hashkey_size;
	u32 hashkey_offset;
	u32 processor_masks_offset;
	u32 num_processor_masks;
	u32 processor_masks_entry_size;
};
struct ndis_tcp_ip_checksum_info {
	union {
		struct {
			u32 is_ipv4:1;
			u32 is_ipv6:1;
			u32 tcp_checksum:1;
			u32 udp_checksum:1;
			u32 ip_header_checksum:1;
			u32 reserved:11;
			u32 tcp_header_offset:10;
		} transmit;
		struct {
			u32 tcp_checksum_failed:1;
			u32 udp_checksum_failed:1;
			u32 ip_checksum_failed:1;
			u32 tcp_checksum_succeeded:1;
			u32 udp_checksum_succeeded:1;
			u32 ip_checksum_succeeded:1;
			u32 loopback:1;
			u32 tcp_checksum_value_invalid:1;
			u32 ip_checksum_value_invalid:1;
		} receive;
		u32  value;
	};
};
struct ndis_pkt_8021q_info {
	union {
		struct {
			u32 pri:3;  
			u32 cfi:1;  
			u32 vlanid:12;  
			u32 reserved:16;
		};
		u32 value;
	};
};
struct hv_netvsc_packet {
	u8 cp_partial;  
	u8 rmsg_size;  
	u8 rmsg_pgcnt;  
	u8 page_buf_cnt;
	u16 q_idx;
	u16 total_packets;
	u32 total_bytes;
	u32 send_buf_index;
	u32 total_data_buflen;
	struct hv_dma_range *dma_range;
};
#define NETVSC_HASH_KEYLEN 40
struct netvsc_device_info {
	unsigned char mac_adr[ETH_ALEN];
	u32  num_chn;
	u32  send_sections;
	u32  recv_sections;
	u32  send_section_size;
	u32  recv_section_size;
	struct bpf_prog *bprog;
	u8 rss_key[NETVSC_HASH_KEYLEN];
};
enum rndis_device_state {
	RNDIS_DEV_UNINITIALIZED = 0,
	RNDIS_DEV_INITIALIZING,
	RNDIS_DEV_INITIALIZED,
	RNDIS_DEV_DATAINITIALIZED,
};
struct rndis_device {
	struct net_device *ndev;
	enum rndis_device_state state;
	atomic_t new_req_id;
	spinlock_t request_lock;
	struct list_head req_list;
	struct work_struct mcast_work;
	u32 filter;
	bool link_state;         
	u8 hw_mac_adr[ETH_ALEN];
	u8 rss_key[NETVSC_HASH_KEYLEN];
};
struct rndis_message;
struct ndis_offload_params;
struct netvsc_device;
struct netvsc_channel;
struct net_device_context;
extern u32 netvsc_ring_bytes;
struct netvsc_device *netvsc_device_add(struct hv_device *device,
					const struct netvsc_device_info *info);
int netvsc_alloc_recv_comp_ring(struct netvsc_device *net_device, u32 q_idx);
void netvsc_device_remove(struct hv_device *device);
int netvsc_send(struct net_device *net,
		struct hv_netvsc_packet *packet,
		struct rndis_message *rndis_msg,
		struct hv_page_buffer *page_buffer,
		struct sk_buff *skb,
		bool xdp_tx);
void netvsc_linkstatus_callback(struct net_device *net,
				struct rndis_message *resp,
				void *data, u32 data_buflen);
int netvsc_recv_callback(struct net_device *net,
			 struct netvsc_device *nvdev,
			 struct netvsc_channel *nvchan);
void netvsc_channel_cb(void *context);
int netvsc_poll(struct napi_struct *napi, int budget);
void netvsc_xdp_xmit(struct sk_buff *skb, struct net_device *ndev);
u32 netvsc_run_xdp(struct net_device *ndev, struct netvsc_channel *nvchan,
		   struct xdp_buff *xdp);
unsigned int netvsc_xdp_fraglen(unsigned int len);
struct bpf_prog *netvsc_xdp_get(struct netvsc_device *nvdev);
int netvsc_xdp_set(struct net_device *dev, struct bpf_prog *prog,
		   struct netlink_ext_ack *extack,
		   struct netvsc_device *nvdev);
int netvsc_vf_setxdp(struct net_device *vf_netdev, struct bpf_prog *prog);
int netvsc_bpf(struct net_device *dev, struct netdev_bpf *bpf);
int netvsc_ndoxdp_xmit(struct net_device *ndev, int n,
		       struct xdp_frame **frames, u32 flags);
int rndis_set_subchannel(struct net_device *ndev,
			 struct netvsc_device *nvdev,
			 struct netvsc_device_info *dev_info);
int rndis_filter_open(struct netvsc_device *nvdev);
int rndis_filter_close(struct netvsc_device *nvdev);
struct netvsc_device *rndis_filter_device_add(struct hv_device *dev,
					      struct netvsc_device_info *info);
void rndis_filter_update(struct netvsc_device *nvdev);
void rndis_filter_device_remove(struct hv_device *dev,
				struct netvsc_device *nvdev);
int rndis_filter_set_rss_param(struct rndis_device *rdev,
			       const u8 *key);
int rndis_filter_set_offload_params(struct net_device *ndev,
				    struct netvsc_device *nvdev,
				    struct ndis_offload_params *req_offloads);
int rndis_filter_receive(struct net_device *ndev,
			 struct netvsc_device *net_dev,
			 struct netvsc_channel *nvchan,
			 void *data, u32 buflen);
int rndis_filter_set_device_mac(struct netvsc_device *ndev,
				const char *mac);
int netvsc_switch_datapath(struct net_device *nv_dev, bool vf);
#define NVSP_INVALID_PROTOCOL_VERSION	((u32)0xFFFFFFFF)
#define NVSP_PROTOCOL_VERSION_1		2
#define NVSP_PROTOCOL_VERSION_2		0x30002
#define NVSP_PROTOCOL_VERSION_4		0x40000
#define NVSP_PROTOCOL_VERSION_5		0x50000
#define NVSP_PROTOCOL_VERSION_6		0x60000
#define NVSP_PROTOCOL_VERSION_61	0x60001
enum {
	NVSP_MSG_TYPE_NONE = 0,
	NVSP_MSG_TYPE_INIT			= 1,
	NVSP_MSG_TYPE_INIT_COMPLETE		= 2,
	NVSP_VERSION_MSG_START			= 100,
	NVSP_MSG1_TYPE_SEND_NDIS_VER		= NVSP_VERSION_MSG_START,
	NVSP_MSG1_TYPE_SEND_RECV_BUF,
	NVSP_MSG1_TYPE_SEND_RECV_BUF_COMPLETE,
	NVSP_MSG1_TYPE_REVOKE_RECV_BUF,
	NVSP_MSG1_TYPE_SEND_SEND_BUF,
	NVSP_MSG1_TYPE_SEND_SEND_BUF_COMPLETE,
	NVSP_MSG1_TYPE_REVOKE_SEND_BUF,
	NVSP_MSG1_TYPE_SEND_RNDIS_PKT,
	NVSP_MSG1_TYPE_SEND_RNDIS_PKT_COMPLETE,
	NVSP_MSG2_TYPE_SEND_CHIMNEY_DELEGATED_BUF,
	NVSP_MSG2_TYPE_SEND_CHIMNEY_DELEGATED_BUF_COMP,
	NVSP_MSG2_TYPE_REVOKE_CHIMNEY_DELEGATED_BUF,
	NVSP_MSG2_TYPE_RESUME_CHIMNEY_RX_INDICATION,
	NVSP_MSG2_TYPE_TERMINATE_CHIMNEY,
	NVSP_MSG2_TYPE_TERMINATE_CHIMNEY_COMP,
	NVSP_MSG2_TYPE_INDICATE_CHIMNEY_EVENT,
	NVSP_MSG2_TYPE_SEND_CHIMNEY_PKT,
	NVSP_MSG2_TYPE_SEND_CHIMNEY_PKT_COMP,
	NVSP_MSG2_TYPE_POST_CHIMNEY_RECV_REQ,
	NVSP_MSG2_TYPE_POST_CHIMNEY_RECV_REQ_COMP,
	NVSP_MSG2_TYPE_ALLOC_RXBUF,
	NVSP_MSG2_TYPE_ALLOC_RXBUF_COMP,
	NVSP_MSG2_TYPE_FREE_RXBUF,
	NVSP_MSG2_TYPE_SEND_VMQ_RNDIS_PKT,
	NVSP_MSG2_TYPE_SEND_VMQ_RNDIS_PKT_COMP,
	NVSP_MSG2_TYPE_SEND_NDIS_CONFIG,
	NVSP_MSG2_TYPE_ALLOC_CHIMNEY_HANDLE,
	NVSP_MSG2_TYPE_ALLOC_CHIMNEY_HANDLE_COMP,
	NVSP_MSG2_MAX = NVSP_MSG2_TYPE_ALLOC_CHIMNEY_HANDLE_COMP,
	NVSP_MSG4_TYPE_SEND_VF_ASSOCIATION,
	NVSP_MSG4_TYPE_SWITCH_DATA_PATH,
	NVSP_MSG4_TYPE_UPLINK_CONNECT_STATE_DEPRECATED,
	NVSP_MSG4_MAX = NVSP_MSG4_TYPE_UPLINK_CONNECT_STATE_DEPRECATED,
	NVSP_MSG5_TYPE_OID_QUERY_EX,
	NVSP_MSG5_TYPE_OID_QUERY_EX_COMP,
	NVSP_MSG5_TYPE_SUBCHANNEL,
	NVSP_MSG5_TYPE_SEND_INDIRECTION_TABLE,
	NVSP_MSG5_MAX = NVSP_MSG5_TYPE_SEND_INDIRECTION_TABLE,
	NVSP_MSG6_TYPE_PD_API,
	NVSP_MSG6_TYPE_PD_POST_BATCH,
	NVSP_MSG6_MAX = NVSP_MSG6_TYPE_PD_POST_BATCH
};
enum {
	NVSP_STAT_NONE = 0,
	NVSP_STAT_SUCCESS,
	NVSP_STAT_FAIL,
	NVSP_STAT_PROTOCOL_TOO_NEW,
	NVSP_STAT_PROTOCOL_TOO_OLD,
	NVSP_STAT_INVALID_RNDIS_PKT,
	NVSP_STAT_BUSY,
	NVSP_STAT_PROTOCOL_UNSUPPORTED,
	NVSP_STAT_MAX,
};
struct nvsp_message_header {
	u32 msg_type;
};
struct nvsp_message_init {
	u32 min_protocol_ver;
	u32 max_protocol_ver;
} __packed;
struct nvsp_message_init_complete {
	u32 negotiated_protocol_ver;
	u32 max_mdl_chain_len;
	u32 status;
} __packed;
union nvsp_message_init_uber {
	struct nvsp_message_init init;
	struct nvsp_message_init_complete init_complete;
} __packed;
struct nvsp_1_message_send_ndis_version {
	u32 ndis_major_ver;
	u32 ndis_minor_ver;
} __packed;
struct nvsp_1_message_send_receive_buffer {
	u32 gpadl_handle;
	u16 id;
} __packed;
struct nvsp_1_receive_buffer_section {
	u32 offset;
	u32 sub_alloc_size;
	u32 num_sub_allocs;
	u32 end_offset;
} __packed;
struct nvsp_1_message_send_receive_buffer_complete {
	u32 status;
	u32 num_sections;
	struct nvsp_1_receive_buffer_section sections[1];
} __packed;
struct nvsp_1_message_revoke_receive_buffer {
	u16 id;
};
struct nvsp_1_message_send_send_buffer {
	u32 gpadl_handle;
	u16 id;
} __packed;
struct nvsp_1_message_send_send_buffer_complete {
	u32 status;
	u32 section_size;
} __packed;
struct nvsp_1_message_revoke_send_buffer {
	u16 id;
};
struct nvsp_1_message_send_rndis_packet {
	u32 channel_type;
	u32 send_buf_section_index;
	u32 send_buf_section_size;
} __packed;
struct nvsp_1_message_send_rndis_packet_complete {
	u32 status;
};
union nvsp_1_message_uber {
	struct nvsp_1_message_send_ndis_version send_ndis_ver;
	struct nvsp_1_message_send_receive_buffer send_recv_buf;
	struct nvsp_1_message_send_receive_buffer_complete
						send_recv_buf_complete;
	struct nvsp_1_message_revoke_receive_buffer revoke_recv_buf;
	struct nvsp_1_message_send_send_buffer send_send_buf;
	struct nvsp_1_message_send_send_buffer_complete send_send_buf_complete;
	struct nvsp_1_message_revoke_send_buffer revoke_send_buf;
	struct nvsp_1_message_send_rndis_packet send_rndis_pkt;
	struct nvsp_1_message_send_rndis_packet_complete
						send_rndis_pkt_complete;
} __packed;
struct nvsp_2_vsc_capability {
	union {
		u64 data;
		struct {
			u64 vmq:1;
			u64 chimney:1;
			u64 sriov:1;
			u64 ieee8021q:1;
			u64 correlation_id:1;
			u64 teaming:1;
			u64 vsubnetid:1;
			u64 rsc:1;
		};
	};
} __packed;
struct nvsp_2_send_ndis_config {
	u32 mtu;
	u32 reserved;
	struct nvsp_2_vsc_capability capability;
} __packed;
struct nvsp_2_alloc_rxbuf {
	u32 alloc_id;
	u32 len;
} __packed;
struct nvsp_2_alloc_rxbuf_comp {
	u32 status;
	u32 alloc_id;
	u32 gpadl_handle;
	u64 recv_buf_id;
} __packed;
struct nvsp_2_free_rxbuf {
	u64 recv_buf_id;
} __packed;
union nvsp_2_message_uber {
	struct nvsp_2_send_ndis_config send_ndis_config;
	struct nvsp_2_alloc_rxbuf alloc_rxbuf;
	struct nvsp_2_alloc_rxbuf_comp alloc_rxbuf_comp;
	struct nvsp_2_free_rxbuf free_rxbuf;
} __packed;
struct nvsp_4_send_vf_association {
	u32 allocated;
	u32 serial;
} __packed;
enum nvsp_vm_datapath {
	NVSP_DATAPATH_SYNTHETIC = 0,
	NVSP_DATAPATH_VF,
	NVSP_DATAPATH_MAX
};
struct nvsp_4_sw_datapath {
	u32 active_datapath;  
} __packed;
union nvsp_4_message_uber {
	struct nvsp_4_send_vf_association vf_assoc;
	struct nvsp_4_sw_datapath active_dp;
} __packed;
enum nvsp_subchannel_operation {
	NVSP_SUBCHANNEL_NONE = 0,
	NVSP_SUBCHANNEL_ALLOCATE,
	NVSP_SUBCHANNEL_MAX
};
struct nvsp_5_subchannel_request {
	u32 op;
	u32 num_subchannels;
} __packed;
struct nvsp_5_subchannel_complete {
	u32 status;
	u32 num_subchannels;  
} __packed;
struct nvsp_5_send_indirect_table {
	u32 count;
	u32 offset;
} __packed;
union nvsp_5_message_uber {
	struct nvsp_5_subchannel_request subchn_req;
	struct nvsp_5_subchannel_complete subchn_comp;
	struct nvsp_5_send_indirect_table send_table;
} __packed;
enum nvsp_6_pd_api_op {
	PD_API_OP_CONFIG = 1,
	PD_API_OP_SW_DATAPATH,  
	PD_API_OP_OPEN_PROVIDER,
	PD_API_OP_CLOSE_PROVIDER,
	PD_API_OP_CREATE_QUEUE,
	PD_API_OP_FLUSH_QUEUE,
	PD_API_OP_FREE_QUEUE,
	PD_API_OP_ALLOC_COM_BUF,  
	PD_API_OP_FREE_COM_BUF,  
	PD_API_OP_MAX
};
struct grp_affinity {
	u64 mask;
	u16 grp;
	u16 reserved[3];
} __packed;
struct nvsp_6_pd_api_req {
	u32 op;
	union {
		struct __packed {
			u64 mmio_pa;  
			u32 mmio_len;
			u16 num_subchn;
		} config;
		struct __packed {
			u8 host_dpath_is_pd;
			u8 guest_pd_enabled;
		} sw_dpath;
		struct __packed {
			u32 prov_id;  
			u32 flag;
		} open_prov;
		struct __packed {
			u32 prov_id;
		} cls_prov;
		struct __packed {
			u32 prov_id;
			u16 q_id;
			u16 q_size;
			u8 is_recv_q;
			u8 is_rss_q;
			u32 recv_data_len;
			struct grp_affinity affy;
		} cr_q;
		struct __packed {
			u32 prov_id;
			u16 q_id;
		} del_q;
		struct __packed {
			u32 prov_id;
			u16 q_id;
		} flush_q;
		struct __packed {
			u32 len;
			u32 pf_node;  
			u16 region_id;
		} alloc_com_buf;
		struct __packed {
			u32 len;
			u64 pa;  
			u32 pf_node;  
			u16 region_id;
			u8 cache_type;
		} free_com_buf;
	} __packed;
} __packed;
struct nvsp_6_pd_api_comp {
	u32 op;
	u32 status;
	union {
		struct __packed {
			u16 num_pd_q;
			u8 num_rss_q;
			u8 is_supported;  
			u8 is_enabled;  
		} config;
		struct __packed {
			u32 prov_id;
		} open_prov;
		struct __packed {
			u32 prov_id;
			u16 q_id;
			u16 q_size;
			u32 recv_data_len;
			struct grp_affinity affy;
		} cr_q;
		struct __packed {
			u64 pa;  
			u32 len;
			u32 pf_node;  
			u16 region_id;
			u8 cache_type;
		} alloc_com_buf;
	} __packed;
} __packed;
struct nvsp_6_pd_buf {
	u32 region_offset;
	u16 region_id;
	u16 is_partial:1;
	u16 reserved:15;
} __packed;
struct nvsp_6_pd_batch_msg {
	struct nvsp_message_header hdr;
	u16 count;
	u16 guest2host:1;
	u16 is_recv:1;
	u16 reserved:14;
	struct nvsp_6_pd_buf pd_buf[0];
} __packed;
union nvsp_6_message_uber {
	struct nvsp_6_pd_api_req pd_req;
	struct nvsp_6_pd_api_comp pd_comp;
} __packed;
union nvsp_all_messages {
	union nvsp_message_init_uber init_msg;
	union nvsp_1_message_uber v1_msg;
	union nvsp_2_message_uber v2_msg;
	union nvsp_4_message_uber v4_msg;
	union nvsp_5_message_uber v5_msg;
	union nvsp_6_message_uber v6_msg;
} __packed;
struct nvsp_message {
	struct nvsp_message_header hdr;
	union nvsp_all_messages msg;
} __packed;
#define NETVSC_MTU 65535
#define NETVSC_MTU_MIN ETH_MIN_MTU
#define NETVSC_RECEIVE_BUFFER_SIZE		(1024 * 1024 * 31)  
#define NETVSC_RECEIVE_BUFFER_SIZE_LEGACY	(1024 * 1024 * 15)  
#define NETVSC_RECEIVE_BUFFER_DEFAULT		(1024 * 1024 * 16)
#define NETVSC_SEND_BUFFER_SIZE			(1024 * 1024 * 15)   
#define NETVSC_SEND_BUFFER_DEFAULT		(1024 * 1024)
#define NETVSC_INVALID_INDEX			-1
#define NETVSC_SEND_SECTION_SIZE		6144
#define NETVSC_RECV_SECTION_SIZE		1728
#define NETVSC_MIN_TX_SECTIONS	10
#define NETVSC_DEFAULT_TX	(NETVSC_SEND_BUFFER_DEFAULT \
				 / NETVSC_SEND_SECTION_SIZE)
#define NETVSC_MIN_RX_SECTIONS	10
#define NETVSC_DEFAULT_RX	(NETVSC_RECEIVE_BUFFER_DEFAULT \
				 / NETVSC_RECV_SECTION_SIZE)
#define NETVSC_RECEIVE_BUFFER_ID		0xcafe
#define NETVSC_SEND_BUFFER_ID			0
#define NETVSC_SUPPORTED_HW_FEATURES (NETIF_F_RXCSUM | NETIF_F_IP_CSUM | \
				      NETIF_F_TSO | NETIF_F_IPV6_CSUM | \
				      NETIF_F_TSO6 | NETIF_F_LRO | \
				      NETIF_F_SG | NETIF_F_RXHASH)
#define VRSS_SEND_TAB_SIZE 16   
#define VRSS_CHANNEL_MAX 64
#define VRSS_CHANNEL_DEFAULT 8
#define RNDIS_MAX_PKT_DEFAULT 8
#define RNDIS_PKT_ALIGN_DEFAULT 8
#define NETVSC_XDP_HDRM 256
#define NETVSC_MIN_OUT_MSG_SIZE (sizeof(struct vmpacket_descriptor) + \
				 sizeof(struct nvsp_message))
#define NETVSC_MIN_IN_MSG_SIZE sizeof(struct vmpacket_descriptor)
static inline u32 netvsc_rqstor_size(unsigned long ringbytes)
{
	return ringbytes / NETVSC_MIN_OUT_MSG_SIZE +
		ringbytes / NETVSC_MIN_IN_MSG_SIZE;
}
#define NETVSC_MAX_XFER_PAGE_RANGES NVSP_RSC_MAX
#define NETVSC_XFER_HEADER_SIZE(rng_cnt) \
		(offsetof(struct vmtransfer_page_packet_header, ranges) + \
		(rng_cnt) * sizeof(struct vmtransfer_page_range))
#define NETVSC_MAX_PKT_SIZE (NETVSC_XFER_HEADER_SIZE(NETVSC_MAX_XFER_PAGE_RANGES) + \
		sizeof(struct nvsp_message) + (sizeof(u32) * VRSS_SEND_TAB_SIZE))
struct multi_send_data {
	struct sk_buff *skb;  
	struct hv_netvsc_packet *pkt;  
	u32 count;  
};
struct recv_comp_data {
	u64 tid;  
	u32 status;
};
struct multi_recv_comp {
	struct recv_comp_data *slots;
	u32 first;	 
	u32 next;	 
};
#define NVSP_RSC_MAX 562  
struct nvsc_rsc {
	struct ndis_pkt_8021q_info vlan;
	struct ndis_tcp_ip_checksum_info csum_info;
	u32 hash_info;
	u8 ppi_flags;  
	u8 is_last;  
	u32 cnt;  
	u32 pktlen;  
	void *data[NVSP_RSC_MAX];
	u32 len[NVSP_RSC_MAX];
};
#define NVSC_RSC_VLAN		BIT(0)	 
#define NVSC_RSC_CSUM_INFO	BIT(1)	 
#define NVSC_RSC_HASH_INFO	BIT(2)	 
struct netvsc_stats_tx {
	u64 packets;
	u64 bytes;
	u64 xdp_xmit;
	struct u64_stats_sync syncp;
};
struct netvsc_stats_rx {
	u64 packets;
	u64 bytes;
	u64 broadcast;
	u64 multicast;
	u64 xdp_drop;
	u64 xdp_redirect;
	u64 xdp_tx;
	struct u64_stats_sync syncp;
};
struct netvsc_ethtool_stats {
	unsigned long tx_scattered;
	unsigned long tx_no_memory;
	unsigned long tx_no_space;
	unsigned long tx_too_big;
	unsigned long tx_busy;
	unsigned long tx_send_full;
	unsigned long rx_comp_busy;
	unsigned long rx_no_memory;
	unsigned long stop_queue;
	unsigned long wake_queue;
	unsigned long vlan_error;
};
struct netvsc_ethtool_pcpu_stats {
	u64     rx_packets;
	u64     rx_bytes;
	u64     tx_packets;
	u64     tx_bytes;
	u64     vf_rx_packets;
	u64     vf_rx_bytes;
	u64     vf_tx_packets;
	u64     vf_tx_bytes;
};
struct netvsc_vf_pcpu_stats {
	u64     rx_packets;
	u64     rx_bytes;
	u64     tx_packets;
	u64     tx_bytes;
	struct u64_stats_sync   syncp;
	u32	tx_dropped;
};
struct netvsc_reconfig {
	struct list_head list;
	u32 event;
};
#define HV_TCP4_L4HASH 1
#define HV_TCP6_L4HASH 2
#define HV_UDP4_L4HASH 4
#define HV_UDP6_L4HASH 8
#define HV_DEFAULT_L4HASH (HV_TCP4_L4HASH | HV_TCP6_L4HASH | HV_UDP4_L4HASH | \
			   HV_UDP6_L4HASH)
struct net_device_context {
	struct hv_device *device_ctx;
	struct netvsc_device __rcu *nvdev;
	struct list_head list;
	struct delayed_work dwork;
	unsigned long last_reconfig;
	struct list_head reconfig_events;
	spinlock_t lock;
	u32 msg_enable;  
	u32 tx_checksum_mask;
	u32 tx_table[VRSS_SEND_TAB_SIZE];
	u16 *rx_table;
	u32 rx_table_sz;
	u8 duplex;
	u32 speed;
	u32 l4_hash;  
	struct netvsc_ethtool_stats eth_stats;
	struct net_device __rcu *vf_netdev;
	struct netvsc_vf_pcpu_stats __percpu *vf_stats;
	struct delayed_work vf_takeover;
	u32 vf_alloc;
	u32 vf_serial;
	struct completion vf_add;
	bool  data_path_is_vf;
	struct netvsc_device_info *saved_netvsc_dev_info;
};
static inline u32 netvsc_get_hash(struct sk_buff *skb,
				  const struct net_device_context *ndc)
{
	struct flow_keys flow;
	u32 hash, pkt_proto = 0;
	static u32 hashrnd __read_mostly;
	net_get_random_once(&hashrnd, sizeof(hashrnd));
	if (!skb_flow_dissect_flow_keys(skb, &flow, 0))
		return 0;
	switch (flow.basic.ip_proto) {
	case IPPROTO_TCP:
		if (flow.basic.n_proto == htons(ETH_P_IP))
			pkt_proto = HV_TCP4_L4HASH;
		else if (flow.basic.n_proto == htons(ETH_P_IPV6))
			pkt_proto = HV_TCP6_L4HASH;
		break;
	case IPPROTO_UDP:
		if (flow.basic.n_proto == htons(ETH_P_IP))
			pkt_proto = HV_UDP4_L4HASH;
		else if (flow.basic.n_proto == htons(ETH_P_IPV6))
			pkt_proto = HV_UDP6_L4HASH;
		break;
	}
	if (pkt_proto & ndc->l4_hash) {
		return skb_get_hash(skb);
	} else {
		if (flow.basic.n_proto == htons(ETH_P_IP))
			hash = jhash2((u32 *)&flow.addrs.v4addrs, 2, hashrnd);
		else if (flow.basic.n_proto == htons(ETH_P_IPV6))
			hash = jhash2((u32 *)&flow.addrs.v6addrs, 8, hashrnd);
		else
			return 0;
		__skb_set_sw_hash(skb, hash, false);
	}
	return hash;
}
struct netvsc_channel {
	struct vmbus_channel *channel;
	struct netvsc_device *net_device;
	void *recv_buf;  
	const struct vmpacket_descriptor *desc;
	struct napi_struct napi;
	struct multi_send_data msd;
	struct multi_recv_comp mrc;
	atomic_t queue_sends;
	struct nvsc_rsc rsc;
	struct bpf_prog __rcu *bpf_prog;
	struct xdp_rxq_info xdp_rxq;
	bool xdp_flush;
	struct netvsc_stats_tx tx_stats;
	struct netvsc_stats_rx rx_stats;
};
struct netvsc_device {
	u32 nvsp_version;
	wait_queue_head_t wait_drain;
	bool destroy;
	bool tx_disable;  
	void *recv_buf;
	u32 recv_buf_size;  
	struct vmbus_gpadl recv_buf_gpadl_handle;
	u32 recv_section_cnt;
	u32 recv_section_size;
	u32 recv_completion_cnt;
	void *send_buf;
	u32 send_buf_size;
	struct vmbus_gpadl send_buf_gpadl_handle;
	u32 send_section_cnt;
	u32 send_section_size;
	unsigned long *send_section_map;
	struct completion channel_init_wait;
	struct nvsp_message channel_init_pkt;
	struct nvsp_message revoke_packet;
	u32 max_chn;
	u32 num_chn;
	atomic_t open_chn;
	struct work_struct subchan_work;
	wait_queue_head_t subchan_open;
	struct rndis_device *extension;
	u32 max_pkt;  
	u32 pkt_align;  
	struct netvsc_channel chan_table[VRSS_CHANNEL_MAX];
	struct rcu_head rcu;
};
struct rndis_initialize_request {
	u32 req_id;
	u32 major_ver;
	u32 minor_ver;
	u32 max_xfer_size;
};
struct rndis_initialize_complete {
	u32 req_id;
	u32 status;
	u32 major_ver;
	u32 minor_ver;
	u32 dev_flags;
	u32 medium;
	u32 max_pkt_per_msg;
	u32 max_xfer_size;
	u32 pkt_alignment_factor;
	u32 af_list_offset;
	u32 af_list_size;
};
struct rndis_co_address_family {
	u32 address_family;
	u32 major_ver;
	u32 minor_ver;
};
struct rndis_halt_request {
	u32 req_id;
};
struct rndis_query_request {
	u32 req_id;
	u32 oid;
	u32 info_buflen;
	u32 info_buf_offset;
	u32 dev_vc_handle;
};
struct rndis_query_complete {
	u32 req_id;
	u32 status;
	u32 info_buflen;
	u32 info_buf_offset;
};
struct rndis_set_request {
	u32 req_id;
	u32 oid;
	u32 info_buflen;
	u32 info_buf_offset;
	u32 dev_vc_handle;
	u8  info_buf[];
};
struct rndis_set_complete {
	u32 req_id;
	u32 status;
};
struct rndis_reset_request {
	u32 reserved;
};
struct rndis_reset_complete {
	u32 status;
	u32 addressing_reset;
};
struct rndis_indicate_status {
	u32 status;
	u32 status_buflen;
	u32 status_buf_offset;
};
struct rndis_diagnostic_info {
	u32 diag_status;
	u32 error_offset;
};
struct rndis_keepalive_request {
	u32 req_id;
};
struct rndis_keepalive_complete {
	u32 req_id;
	u32 status;
};
struct rndis_packet {
	u32 data_offset;
	u32 data_len;
	u32 oob_data_offset;
	u32 oob_data_len;
	u32 num_oob_data_elements;
	u32 per_pkt_info_offset;
	u32 per_pkt_info_len;
	u32 vc_handle;
	u32 reserved;
};
struct rndis_oobd {
	u32 size;
	u32 type;
	u32 class_info_offset;
};
struct rndis_per_packet_info {
	u32 size;
	u32 type:31;
	u32 internal:1;
	u32 ppi_offset;
};
enum ndis_per_pkt_info_type {
	TCPIP_CHKSUM_PKTINFO,
	IPSEC_PKTINFO,
	TCP_LARGESEND_PKTINFO,
	CLASSIFICATION_HANDLE_PKTINFO,
	NDIS_RESERVED,
	SG_LIST_PKTINFO,
	IEEE_8021Q_INFO,
	ORIGINAL_PKTINFO,
	PACKET_CANCEL_ID,
	NBL_HASH_VALUE = PACKET_CANCEL_ID,
	ORIGINAL_NET_BUFLIST,
	CACHED_NET_BUFLIST,
	SHORT_PKT_PADINFO,
	MAX_PER_PKT_INFO
};
enum rndis_per_pkt_info_interal_type {
	RNDIS_PKTINFO_ID = 1,
	RNDIS_PKTINFO_MAX
};
#define RNDIS_PKTINFO_SUBALLOC BIT(0)
#define RNDIS_PKTINFO_1ST_FRAG BIT(1)
#define RNDIS_PKTINFO_LAST_FRAG BIT(2)
#define RNDIS_PKTINFO_ID_V1 1
struct rndis_pktinfo_id {
	u8 ver;
	u8 flag;
	u16 pkt_id;
};
struct ndis_object_header {
	u8 type;
	u8 revision;
	u16 size;
};
#define NDIS_OBJECT_TYPE_DEFAULT	0x80
#define NDIS_OFFLOAD_PARAMETERS_REVISION_3 3
#define NDIS_OFFLOAD_PARAMETERS_REVISION_2 2
#define NDIS_OFFLOAD_PARAMETERS_REVISION_1 1
#define NDIS_OFFLOAD_PARAMETERS_NO_CHANGE 0
#define NDIS_OFFLOAD_PARAMETERS_LSOV2_DISABLED 1
#define NDIS_OFFLOAD_PARAMETERS_LSOV2_ENABLED  2
#define NDIS_OFFLOAD_PARAMETERS_LSOV1_ENABLED  2
#define NDIS_OFFLOAD_PARAMETERS_RSC_DISABLED 1
#define NDIS_OFFLOAD_PARAMETERS_RSC_ENABLED 2
#define NDIS_OFFLOAD_PARAMETERS_TX_RX_DISABLED 1
#define NDIS_OFFLOAD_PARAMETERS_TX_ENABLED_RX_DISABLED 2
#define NDIS_OFFLOAD_PARAMETERS_RX_ENABLED_TX_DISABLED 3
#define NDIS_OFFLOAD_PARAMETERS_TX_RX_ENABLED 4
#define NDIS_TCP_LARGE_SEND_OFFLOAD_V2_TYPE	1
#define NDIS_TCP_LARGE_SEND_OFFLOAD_IPV4	0
#define NDIS_TCP_LARGE_SEND_OFFLOAD_IPV6	1
#define VERSION_4_OFFLOAD_SIZE			22
#define OID_TCP_OFFLOAD_CURRENT_CONFIG 0xFC01020B  
#define OID_TCP_OFFLOAD_PARAMETERS 0xFC01020C		 
#define OID_TCP_OFFLOAD_HARDWARE_CAPABILITIES 0xFC01020D 
#define OID_TCP_CONNECTION_OFFLOAD_CURRENT_CONFIG 0xFC01020E  
#define OID_TCP_CONNECTION_OFFLOAD_HARDWARE_CAPABILITIES 0xFC01020F  
#define OID_OFFLOAD_ENCAPSULATION 0x0101010A  
#define	NDIS_OFFLOAD_ENCAP_NONE		0x0000
#define	NDIS_OFFLOAD_ENCAP_NULL		0x0001
#define	NDIS_OFFLOAD_ENCAP_8023		0x0002
#define	NDIS_OFFLOAD_ENCAP_8023PQ	0x0004
#define	NDIS_OFFLOAD_ENCAP_8023PQ_OOB	0x0008
#define	NDIS_OFFLOAD_ENCAP_RFC1483	0x0010
struct ndis_csum_offload {
	u32	ip4_txenc;
	u32	ip4_txcsum;
#define	NDIS_TXCSUM_CAP_IP4OPT		0x001
#define	NDIS_TXCSUM_CAP_TCP4OPT		0x004
#define	NDIS_TXCSUM_CAP_TCP4		0x010
#define	NDIS_TXCSUM_CAP_UDP4		0x040
#define	NDIS_TXCSUM_CAP_IP4		0x100
#define NDIS_TXCSUM_ALL_TCP4	(NDIS_TXCSUM_CAP_TCP4 | NDIS_TXCSUM_CAP_TCP4OPT)
	u32	ip4_rxenc;
	u32	ip4_rxcsum;
#define	NDIS_RXCSUM_CAP_IP4OPT		0x001
#define	NDIS_RXCSUM_CAP_TCP4OPT		0x004
#define	NDIS_RXCSUM_CAP_TCP4		0x010
#define	NDIS_RXCSUM_CAP_UDP4		0x040
#define	NDIS_RXCSUM_CAP_IP4		0x100
	u32	ip6_txenc;
	u32	ip6_txcsum;
#define	NDIS_TXCSUM_CAP_IP6EXT		0x001
#define	NDIS_TXCSUM_CAP_TCP6OPT		0x004
#define	NDIS_TXCSUM_CAP_TCP6		0x010
#define	NDIS_TXCSUM_CAP_UDP6		0x040
	u32	ip6_rxenc;
	u32	ip6_rxcsum;
#define	NDIS_RXCSUM_CAP_IP6EXT		0x001
#define	NDIS_RXCSUM_CAP_TCP6OPT		0x004
#define	NDIS_RXCSUM_CAP_TCP6		0x010
#define	NDIS_RXCSUM_CAP_UDP6		0x040
#define NDIS_TXCSUM_ALL_TCP6	(NDIS_TXCSUM_CAP_TCP6 |		\
				 NDIS_TXCSUM_CAP_TCP6OPT |	\
				 NDIS_TXCSUM_CAP_IP6EXT)
};
struct ndis_lsov1_offload {
	u32	encap;
	u32	maxsize;
	u32	minsegs;
	u32	opts;
};
struct ndis_ipsecv1_offload {
	u32	encap;
	u32	ah_esp;
	u32	xport_tun;
	u32	ip4_opts;
	u32	flags;
	u32	ip4_ah;
	u32	ip4_esp;
};
struct ndis_lsov2_offload {
	u32	ip4_encap;
	u32	ip4_maxsz;
	u32	ip4_minsg;
	u32	ip6_encap;
	u32	ip6_maxsz;
	u32	ip6_minsg;
	u32	ip6_opts;
#define	NDIS_LSOV2_CAP_IP6EXT		0x001
#define	NDIS_LSOV2_CAP_TCP6OPT		0x004
#define NDIS_LSOV2_CAP_IP6		(NDIS_LSOV2_CAP_IP6EXT | \
					 NDIS_LSOV2_CAP_TCP6OPT)
};
struct ndis_ipsecv2_offload {
	u32	encap;
	u8	ip6;
	u8	ip4opt;
	u8	ip6ext;
	u8	ah;
	u8	esp;
	u8	ah_esp;
	u8	xport;
	u8	tun;
	u8	xport_tun;
	u8	lso;
	u8	extseq;
	u32	udp_esp;
	u32	auth;
	u32	crypto;
	u32	sa_caps;
};
struct ndis_rsc_offload {
	u8	ip4;
	u8	ip6;
};
struct ndis_encap_offload {
	u32	flags;
	u32	maxhdr;
};
struct ndis_offload {
	struct ndis_object_header	header;
	struct ndis_csum_offload	csum;
	struct ndis_lsov1_offload	lsov1;
	struct ndis_ipsecv1_offload	ipsecv1;
	struct ndis_lsov2_offload	lsov2;
	u32				flags;
	struct ndis_ipsecv2_offload	ipsecv2;
	struct ndis_rsc_offload		rsc;
	struct ndis_encap_offload	encap_gre;
};
#define	NDIS_OFFLOAD_SIZE		sizeof(struct ndis_offload)
#define	NDIS_OFFLOAD_SIZE_6_0		offsetof(struct ndis_offload, ipsecv2)
#define	NDIS_OFFLOAD_SIZE_6_1		offsetof(struct ndis_offload, rsc)
struct ndis_offload_params {
	struct ndis_object_header header;
	u8 ip_v4_csum;
	u8 tcp_ip_v4_csum;
	u8 udp_ip_v4_csum;
	u8 tcp_ip_v6_csum;
	u8 udp_ip_v6_csum;
	u8 lso_v1;
	u8 ip_sec_v1;
	u8 lso_v2_ipv4;
	u8 lso_v2_ipv6;
	u8 tcp_connection_ip_v4;
	u8 tcp_connection_ip_v6;
	u32 flags;
	u8 ip_sec_v2;
	u8 ip_sec_v2_ip_v4;
	struct {
		u8 rsc_ip_v4;
		u8 rsc_ip_v6;
	};
	struct {
		u8 encapsulated_packet_task_offload;
		u8 encapsulation_types;
	};
};
struct ndis_tcp_lso_info {
	union {
		struct {
			u32 unused:30;
			u32 type:1;
			u32 reserved2:1;
		} transmit;
		struct {
			u32 mss:20;
			u32 tcp_header_offset:10;
			u32 type:1;
			u32 reserved2:1;
		} lso_v1_transmit;
		struct {
			u32 tcp_payload:30;
			u32 type:1;
			u32 reserved2:1;
		} lso_v1_transmit_complete;
		struct {
			u32 mss:20;
			u32 tcp_header_offset:10;
			u32 type:1;
			u32 ip_version:1;
		} lso_v2_transmit;
		struct {
			u32 reserved:30;
			u32 type:1;
			u32 reserved2:1;
		} lso_v2_transmit_complete;
		u32  value;
	};
};
#define NDIS_VLAN_PPI_SIZE (sizeof(struct rndis_per_packet_info) + \
		sizeof(struct ndis_pkt_8021q_info))
#define NDIS_CSUM_PPI_SIZE (sizeof(struct rndis_per_packet_info) + \
		sizeof(struct ndis_tcp_ip_checksum_info))
#define NDIS_LSO_PPI_SIZE (sizeof(struct rndis_per_packet_info) + \
		sizeof(struct ndis_tcp_lso_info))
#define NDIS_HASH_PPI_SIZE (sizeof(struct rndis_per_packet_info) + \
		sizeof(u32))
#define NDIS_ALL_PPI_SIZE (NDIS_VLAN_PPI_SIZE + NDIS_CSUM_PPI_SIZE + \
			   NDIS_LSO_PPI_SIZE + NDIS_HASH_PPI_SIZE)
struct rndis_config_parameter_info {
	u32 parameter_name_offset;
	u32 parameter_name_length;
	u32 parameter_type;
	u32 parameter_value_offset;
	u32 parameter_value_length;
};
#define RNDIS_CONFIG_PARAM_TYPE_INTEGER     0
#define RNDIS_CONFIG_PARAM_TYPE_STRING      2
struct rcondis_mp_create_vc {
	u32 req_id;
	u32 ndis_vc_handle;
};
struct rcondis_mp_create_vc_complete {
	u32 req_id;
	u32 dev_vc_handle;
	u32 status;
};
struct rcondis_mp_delete_vc {
	u32 req_id;
	u32 dev_vc_handle;
};
struct rcondis_mp_delete_vc_complete {
	u32 req_id;
	u32 status;
};
struct rcondis_mp_query_request {
	u32 req_id;
	u32 request_type;
	u32 oid;
	u32 dev_vc_handle;
	u32 info_buflen;
	u32 info_buf_offset;
};
struct rcondis_mp_set_request {
	u32 req_id;
	u32 request_type;
	u32 oid;
	u32 dev_vc_handle;
	u32 info_buflen;
	u32 info_buf_offset;
};
struct rcondis_indicate_status {
	u32 ndis_vc_handle;
	u32 status;
	u32 status_buflen;
	u32 status_buf_offset;
};
struct rcondis_specific_parameters {
	u32 parameter_type;
	u32 parameter_length;
	u32 parameter_lffset;
};
struct rcondis_media_parameters {
	u32 flags;
	u32 reserved1;
	u32 reserved2;
	struct rcondis_specific_parameters media_specific;
};
struct rndis_flowspec {
	u32 token_rate;
	u32 token_bucket_size;
	u32 peak_bandwidth;
	u32 latency;
	u32 delay_variation;
	u32 service_type;
	u32 max_sdu_size;
	u32 minimum_policed_size;
};
struct rcondis_call_manager_parameters {
	struct rndis_flowspec transmit;
	struct rndis_flowspec receive;
	struct rcondis_specific_parameters call_mgr_specific;
};
struct rcondis_mp_activate_vc_request {
	u32 req_id;
	u32 flags;
	u32 dev_vc_handle;
	u32 media_params_offset;
	u32 media_params_length;
	u32 call_mgr_params_offset;
	u32 call_mgr_params_length;
};
struct rcondis_mp_activate_vc_complete {
	u32 req_id;
	u32 status;
};
struct rcondis_mp_deactivate_vc_request {
	u32 req_id;
	u32 flags;
	u32 dev_vc_handle;
};
struct rcondis_mp_deactivate_vc_complete {
	u32 req_id;
	u32 status;
};
union rndis_message_container {
	struct rndis_packet pkt;
	struct rndis_initialize_request init_req;
	struct rndis_halt_request halt_req;
	struct rndis_query_request query_req;
	struct rndis_set_request set_req;
	struct rndis_reset_request reset_req;
	struct rndis_keepalive_request keep_alive_req;
	struct rndis_indicate_status indicate_status;
	struct rndis_initialize_complete init_complete;
	struct rndis_query_complete query_complete;
	struct rndis_set_complete set_complete;
	struct rndis_reset_complete reset_complete;
	struct rndis_keepalive_complete keep_alive_complete;
	struct rcondis_mp_create_vc co_miniport_create_vc;
	struct rcondis_mp_delete_vc co_miniport_delete_vc;
	struct rcondis_indicate_status co_indicate_status;
	struct rcondis_mp_activate_vc_request co_miniport_activate_vc;
	struct rcondis_mp_deactivate_vc_request co_miniport_deactivate_vc;
	struct rcondis_mp_create_vc_complete co_miniport_create_vc_complete;
	struct rcondis_mp_delete_vc_complete co_miniport_delete_vc_complete;
	struct rcondis_mp_activate_vc_complete co_miniport_activate_vc_complete;
	struct rcondis_mp_deactivate_vc_complete
		co_miniport_deactivate_vc_complete;
};
struct rndis_message {
	u32 ndis_msg_type;
	u32 msg_len;
	union rndis_message_container msg;
};
#define RNDIS_MESSAGE_SIZE(msg)				\
	(sizeof(msg) + (sizeof(struct rndis_message) -	\
	 sizeof(union rndis_message_container)))
#define RNDIS_HEADER_SIZE	(sizeof(struct rndis_message) - \
				 sizeof(union rndis_message_container))
#define RNDIS_AND_PPI_SIZE (sizeof(struct rndis_message) + NDIS_ALL_PPI_SIZE)
#define NDIS_PACKET_TYPE_DIRECTED	0x00000001
#define NDIS_PACKET_TYPE_MULTICAST	0x00000002
#define NDIS_PACKET_TYPE_ALL_MULTICAST	0x00000004
#define NDIS_PACKET_TYPE_BROADCAST	0x00000008
#define NDIS_PACKET_TYPE_SOURCE_ROUTING	0x00000010
#define NDIS_PACKET_TYPE_PROMISCUOUS	0x00000020
#define NDIS_PACKET_TYPE_SMT		0x00000040
#define NDIS_PACKET_TYPE_ALL_LOCAL	0x00000080
#define NDIS_PACKET_TYPE_GROUP		0x00000100
#define NDIS_PACKET_TYPE_ALL_FUNCTIONAL	0x00000200
#define NDIS_PACKET_TYPE_FUNCTIONAL	0x00000400
#define NDIS_PACKET_TYPE_MAC_FRAME	0x00000800
#define TRANSPORT_INFO_NOT_IP   0
#define TRANSPORT_INFO_IPV4_TCP 0x01
#define TRANSPORT_INFO_IPV4_UDP 0x02
#define TRANSPORT_INFO_IPV6_TCP 0x10
#define TRANSPORT_INFO_IPV6_UDP 0x20
#define RETRY_US_LO	5000
#define RETRY_US_HI	10000
#define RETRY_MAX	2000	 
void netvsc_dma_unmap(struct hv_device *hv_dev,
		      struct hv_netvsc_packet *packet);
#endif  
