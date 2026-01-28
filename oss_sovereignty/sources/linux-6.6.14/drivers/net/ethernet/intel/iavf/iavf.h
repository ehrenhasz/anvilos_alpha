


#ifndef _IAVF_H_
#define _IAVF_H_

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/sctp.h>
#include <linux/ipv6.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/gfp.h>
#include <linux/skbuff.h>
#include <linux/dma-mapping.h>
#include <linux/etherdevice.h>
#include <linux/socket.h>
#include <linux/jiffies.h>
#include <net/ip6_checksum.h>
#include <net/pkt_cls.h>
#include <net/pkt_sched.h>
#include <net/udp.h>
#include <net/tc_act/tc_gact.h>
#include <net/tc_act/tc_mirred.h>

#include "iavf_type.h"
#include <linux/avf/virtchnl.h>
#include "iavf_txrx.h"
#include "iavf_fdir.h"
#include "iavf_adv_rss.h"
#include <linux/bitmap.h>

#define DEFAULT_DEBUG_LEVEL_SHIFT 3
#define PFX "iavf: "

int iavf_status_to_errno(enum iavf_status status);
int virtchnl_status_to_errno(enum virtchnl_status_code v_status);


enum iavf_vsi_state_t {
	__IAVF_VSI_DOWN,
	
	__IAVF_VSI_STATE_SIZE__,
};


struct iavf_vsi {
	struct iavf_adapter *back;
	struct net_device *netdev;
	u16 seid;
	u16 id;
	DECLARE_BITMAP(state, __IAVF_VSI_STATE_SIZE__);
	int base_vector;
	u16 qs_handle;
	void *priv;     
};


#define IAVF_RX_BUFFER_WRITE	16	
#define IAVF_DEFAULT_TXD	512
#define IAVF_DEFAULT_RXD	512
#define IAVF_MAX_TXD		4096
#define IAVF_MIN_TXD		64
#define IAVF_MAX_RXD		4096
#define IAVF_MIN_RXD		64
#define IAVF_REQ_DESCRIPTOR_MULTIPLE	32
#define IAVF_MAX_AQ_BUF_SIZE	4096
#define IAVF_AQ_LEN		32
#define IAVF_AQ_MAX_ERR	20 

#define MAXIMUM_ETHERNET_VLAN_SIZE (VLAN_ETH_FRAME_LEN + ETH_FCS_LEN)

#define IAVF_RX_DESC(R, i) (&(((union iavf_32byte_rx_desc *)((R)->desc))[i]))
#define IAVF_TX_DESC(R, i) (&(((struct iavf_tx_desc *)((R)->desc))[i]))
#define IAVF_TX_CTXTDESC(R, i) \
	(&(((struct iavf_tx_context_desc *)((R)->desc))[i]))
#define IAVF_MAX_REQ_QUEUES 16

#define IAVF_HKEY_ARRAY_SIZE ((IAVF_VFQF_HKEY_MAX_INDEX + 1) * 4)
#define IAVF_HLUT_ARRAY_SIZE ((IAVF_VFQF_HLUT_MAX_INDEX + 1) * 4)
#define IAVF_MBPS_DIVISOR	125000 
#define IAVF_MBPS_QUANTA	50

#define IAVF_VIRTCHNL_VF_RESOURCE_SIZE					\
	virtchnl_struct_size((struct virtchnl_vf_resource *)NULL,	\
			     vsi_res, IAVF_MAX_VF_VSI)


struct iavf_q_vector {
	struct iavf_adapter *adapter;
	struct iavf_vsi *vsi;
	struct napi_struct napi;
	struct iavf_ring_container rx;
	struct iavf_ring_container tx;
	u32 ring_mask;
	u8 itr_countdown;	
	u8 num_ringpairs;	
	u16 v_idx;		
	u16 reg_idx;		
	char name[IFNAMSIZ + 15];
	bool arm_wb_state;
	cpumask_t affinity_mask;
	struct irq_affinity_notify affinity_notify;
};


#define EITR_INTS_PER_SEC_TO_REG(_eitr) \
	((_eitr) ? (1000000000 / ((_eitr) * 256)) : 8)
#define EITR_REG_TO_INTS_PER_SEC EITR_INTS_PER_SEC_TO_REG

#define IAVF_DESC_UNUSED(R) \
	((((R)->next_to_clean > (R)->next_to_use) ? 0 : (R)->count) + \
	(R)->next_to_clean - (R)->next_to_use - 1)

#define OTHER_VECTOR 1
#define NONQ_VECS (OTHER_VECTOR)

#define MIN_MSIX_Q_VECTORS 1
#define MIN_MSIX_COUNT (MIN_MSIX_Q_VECTORS + NONQ_VECS)

#define IAVF_QUEUE_END_OF_LIST 0x7FF
#define IAVF_FREE_VECTOR 0x7FFF
struct iavf_mac_filter {
	struct list_head list;
	u8 macaddr[ETH_ALEN];
	struct {
		u8 is_new_mac:1;    
		u8 remove:1;        
		u8 add:1;           
		u8 is_primary:1;    
		u8 add_handled:1;   
		u8 padding:3;
	};
};

#define IAVF_VLAN(vid, tpid) ((struct iavf_vlan){ vid, tpid })
struct iavf_vlan {
	u16 vid;
	u16 tpid;
};

enum iavf_vlan_state_t {
	IAVF_VLAN_INVALID,
	IAVF_VLAN_ADD,		
	IAVF_VLAN_IS_NEW,	
	IAVF_VLAN_ACTIVE,	
	IAVF_VLAN_DISABLE,	
	IAVF_VLAN_INACTIVE,	
	IAVF_VLAN_REMOVE,	
};

struct iavf_vlan_filter {
	struct list_head list;
	struct iavf_vlan vlan;
	enum iavf_vlan_state_t state;
};

#define IAVF_MAX_TRAFFIC_CLASS	4

enum iavf_tc_state_t {
	__IAVF_TC_INVALID, 
	__IAVF_TC_RUNNING, 
};


struct iavf_channel_config {
	struct virtchnl_channel_info ch_info[IAVF_MAX_TRAFFIC_CLASS];
	enum iavf_tc_state_t state;
	u8 total_qps;
};


enum iavf_cloud_filter_state_t {
	__IAVF_CF_INVALID,	 
	__IAVF_CF_ADD_PENDING, 
	__IAVF_CF_DEL_PENDING, 
	__IAVF_CF_ACTIVE,	 
};


enum iavf_state_t {
	__IAVF_STARTUP,		
	__IAVF_REMOVE,		
	__IAVF_INIT_VERSION_CHECK,	
	__IAVF_INIT_GET_RESOURCES,	
	__IAVF_INIT_EXTENDED_CAPS,	
	__IAVF_INIT_CONFIG_ADAPTER,
	__IAVF_INIT_SW,		
	__IAVF_INIT_FAILED,	
	__IAVF_RESETTING,		
	__IAVF_COMM_FAILED,		
	
	__IAVF_DOWN,			
	__IAVF_DOWN_PENDING,		
	__IAVF_TESTING,		
	__IAVF_RUNNING,		
};

enum iavf_critical_section_t {
	__IAVF_IN_REMOVE_TASK,	
};

#define IAVF_CLOUD_FIELD_OMAC		0x01
#define IAVF_CLOUD_FIELD_IMAC		0x02
#define IAVF_CLOUD_FIELD_IVLAN	0x04
#define IAVF_CLOUD_FIELD_TEN_ID	0x08
#define IAVF_CLOUD_FIELD_IIP		0x10

#define IAVF_CF_FLAGS_OMAC	IAVF_CLOUD_FIELD_OMAC
#define IAVF_CF_FLAGS_IMAC	IAVF_CLOUD_FIELD_IMAC
#define IAVF_CF_FLAGS_IMAC_IVLAN	(IAVF_CLOUD_FIELD_IMAC |\
					 IAVF_CLOUD_FIELD_IVLAN)
#define IAVF_CF_FLAGS_IMAC_TEN_ID	(IAVF_CLOUD_FIELD_IMAC |\
					 IAVF_CLOUD_FIELD_TEN_ID)
#define IAVF_CF_FLAGS_OMAC_TEN_ID_IMAC	(IAVF_CLOUD_FIELD_OMAC |\
						 IAVF_CLOUD_FIELD_IMAC |\
						 IAVF_CLOUD_FIELD_TEN_ID)
#define IAVF_CF_FLAGS_IMAC_IVLAN_TEN_ID	(IAVF_CLOUD_FIELD_IMAC |\
						 IAVF_CLOUD_FIELD_IVLAN |\
						 IAVF_CLOUD_FIELD_TEN_ID)
#define IAVF_CF_FLAGS_IIP	IAVF_CLOUD_FIELD_IIP


struct iavf_cloud_filter {
	enum iavf_cloud_filter_state_t state;
	struct list_head list;
	struct virtchnl_filter f;
	unsigned long cookie;
	bool del;		
	bool add;		
};

#define IAVF_RESET_WAIT_MS 10
#define IAVF_RESET_WAIT_DETECTED_COUNT 500
#define IAVF_RESET_WAIT_COMPLETE_COUNT 2000


struct iavf_adapter {
	struct workqueue_struct *wq;
	struct work_struct reset_task;
	struct work_struct adminq_task;
	struct work_struct finish_config;
	struct delayed_work client_task;
	wait_queue_head_t down_waitqueue;
	wait_queue_head_t reset_waitqueue;
	wait_queue_head_t vc_waitqueue;
	struct iavf_q_vector *q_vectors;
	struct list_head vlan_filter_list;
	int num_vlan_filters;
	struct list_head mac_filter_list;
	struct mutex crit_lock;
	struct mutex client_lock;
	
	spinlock_t mac_vlan_list_lock;
	char misc_vector_name[IFNAMSIZ + 9];
	int num_active_queues;
	int num_req_queues;

	
	struct iavf_ring *tx_rings;
	u32 tx_timeout_count;
	u32 tx_desc_count;

	
	struct iavf_ring *rx_rings;
	u64 hw_csum_rx_error;
	u32 rx_desc_count;
	int num_msix_vectors;
	int num_rdma_msix;
	int rdma_base_vector;
	u32 client_pending;
	struct iavf_client_instance *cinst;
	struct msix_entry *msix_entries;

	u32 flags;
#define IAVF_FLAG_RX_CSUM_ENABLED		BIT(0)
#define IAVF_FLAG_PF_COMMS_FAILED		BIT(3)
#define IAVF_FLAG_RESET_PENDING		BIT(4)
#define IAVF_FLAG_RESET_NEEDED		BIT(5)
#define IAVF_FLAG_WB_ON_ITR_CAPABLE		BIT(6)
#define IAVF_FLAG_SERVICE_CLIENT_REQUESTED	BIT(9)
#define IAVF_FLAG_CLIENT_NEEDS_OPEN		BIT(10)
#define IAVF_FLAG_CLIENT_NEEDS_CLOSE		BIT(11)
#define IAVF_FLAG_CLIENT_NEEDS_L2_PARAMS	BIT(12)
#define IAVF_FLAG_LEGACY_RX			BIT(15)
#define IAVF_FLAG_REINIT_ITR_NEEDED		BIT(16)
#define IAVF_FLAG_QUEUES_DISABLED		BIT(17)
#define IAVF_FLAG_SETUP_NETDEV_FEATURES		BIT(18)
#define IAVF_FLAG_REINIT_MSIX_NEEDED		BIT(20)
#define IAVF_FLAG_FDIR_ENABLED			BIT(21)

#define IAVF_FLAG_DCB_ENABLED			0
	
	u64 aq_required;
#define IAVF_FLAG_AQ_ENABLE_QUEUES		BIT_ULL(0)
#define IAVF_FLAG_AQ_DISABLE_QUEUES		BIT_ULL(1)
#define IAVF_FLAG_AQ_ADD_MAC_FILTER		BIT_ULL(2)
#define IAVF_FLAG_AQ_ADD_VLAN_FILTER		BIT_ULL(3)
#define IAVF_FLAG_AQ_DEL_MAC_FILTER		BIT_ULL(4)
#define IAVF_FLAG_AQ_DEL_VLAN_FILTER		BIT_ULL(5)
#define IAVF_FLAG_AQ_CONFIGURE_QUEUES		BIT_ULL(6)
#define IAVF_FLAG_AQ_MAP_VECTORS		BIT_ULL(7)
#define IAVF_FLAG_AQ_HANDLE_RESET		BIT_ULL(8)
#define IAVF_FLAG_AQ_CONFIGURE_RSS		BIT_ULL(9) 
#define IAVF_FLAG_AQ_GET_CONFIG			BIT_ULL(10)

#define IAVF_FLAG_AQ_GET_HENA			BIT_ULL(11)
#define IAVF_FLAG_AQ_SET_HENA			BIT_ULL(12)
#define IAVF_FLAG_AQ_SET_RSS_KEY		BIT_ULL(13)
#define IAVF_FLAG_AQ_SET_RSS_LUT		BIT_ULL(14)
#define IAVF_FLAG_AQ_CONFIGURE_PROMISC_MODE	BIT_ULL(15)
#define IAVF_FLAG_AQ_ENABLE_VLAN_STRIPPING	BIT_ULL(19)
#define IAVF_FLAG_AQ_DISABLE_VLAN_STRIPPING	BIT_ULL(20)
#define IAVF_FLAG_AQ_ENABLE_CHANNELS		BIT_ULL(21)
#define IAVF_FLAG_AQ_DISABLE_CHANNELS		BIT_ULL(22)
#define IAVF_FLAG_AQ_ADD_CLOUD_FILTER		BIT_ULL(23)
#define IAVF_FLAG_AQ_DEL_CLOUD_FILTER		BIT_ULL(24)
#define IAVF_FLAG_AQ_ADD_FDIR_FILTER		BIT_ULL(25)
#define IAVF_FLAG_AQ_DEL_FDIR_FILTER		BIT_ULL(26)
#define IAVF_FLAG_AQ_ADD_ADV_RSS_CFG		BIT_ULL(27)
#define IAVF_FLAG_AQ_DEL_ADV_RSS_CFG		BIT_ULL(28)
#define IAVF_FLAG_AQ_REQUEST_STATS		BIT_ULL(29)
#define IAVF_FLAG_AQ_GET_OFFLOAD_VLAN_V2_CAPS	BIT_ULL(30)
#define IAVF_FLAG_AQ_ENABLE_CTAG_VLAN_STRIPPING		BIT_ULL(31)
#define IAVF_FLAG_AQ_DISABLE_CTAG_VLAN_STRIPPING	BIT_ULL(32)
#define IAVF_FLAG_AQ_ENABLE_STAG_VLAN_STRIPPING		BIT_ULL(33)
#define IAVF_FLAG_AQ_DISABLE_STAG_VLAN_STRIPPING	BIT_ULL(34)
#define IAVF_FLAG_AQ_ENABLE_CTAG_VLAN_INSERTION		BIT_ULL(35)
#define IAVF_FLAG_AQ_DISABLE_CTAG_VLAN_INSERTION	BIT_ULL(36)
#define IAVF_FLAG_AQ_ENABLE_STAG_VLAN_INSERTION		BIT_ULL(37)
#define IAVF_FLAG_AQ_DISABLE_STAG_VLAN_INSERTION	BIT_ULL(38)

	
	u64 extended_caps;
#define IAVF_EXTENDED_CAP_SEND_VLAN_V2			BIT_ULL(0)
#define IAVF_EXTENDED_CAP_RECV_VLAN_V2			BIT_ULL(1)

#define IAVF_EXTENDED_CAPS				\
	(IAVF_EXTENDED_CAP_SEND_VLAN_V2 |		\
	 IAVF_EXTENDED_CAP_RECV_VLAN_V2)

	
	spinlock_t current_netdev_promisc_flags_lock;
	netdev_features_t current_netdev_promisc_flags;

	
	struct net_device *netdev;
	struct pci_dev *pdev;

	struct iavf_hw hw; 

	enum iavf_state_t state;
	enum iavf_state_t last_state;
	unsigned long crit_section;

	struct delayed_work watchdog_task;
	bool netdev_registered;
	bool link_up;
	enum virtchnl_link_speed link_speed;
	
	u32 link_speed_mbps;

	enum virtchnl_ops current_op;
#define CLIENT_ALLOWED(_a) ((_a)->vf_res ? \
			    (_a)->vf_res->vf_cap_flags & \
				VIRTCHNL_VF_OFFLOAD_RDMA : \
			    0)
#define CLIENT_ENABLED(_a) ((_a)->cinst)

#define RSS_PF(_a) ((_a)->vf_res->vf_cap_flags & \
		    VIRTCHNL_VF_OFFLOAD_RSS_PF)
#define RSS_AQ(_a) ((_a)->vf_res->vf_cap_flags & \
		    VIRTCHNL_VF_OFFLOAD_RSS_AQ)
#define RSS_REG(_a) (!((_a)->vf_res->vf_cap_flags & \
		       (VIRTCHNL_VF_OFFLOAD_RSS_AQ | \
			VIRTCHNL_VF_OFFLOAD_RSS_PF)))
#define VLAN_ALLOWED(_a) ((_a)->vf_res->vf_cap_flags & \
			  VIRTCHNL_VF_OFFLOAD_VLAN)
#define VLAN_V2_ALLOWED(_a) ((_a)->vf_res->vf_cap_flags & \
			     VIRTCHNL_VF_OFFLOAD_VLAN_V2)
#define VLAN_V2_FILTERING_ALLOWED(_a) \
	(VLAN_V2_ALLOWED((_a)) && \
	 ((_a)->vlan_v2_caps.filtering.filtering_support.outer || \
	  (_a)->vlan_v2_caps.filtering.filtering_support.inner))
#define VLAN_FILTERING_ALLOWED(_a) \
	(VLAN_ALLOWED((_a)) || VLAN_V2_FILTERING_ALLOWED((_a)))
#define ADV_LINK_SUPPORT(_a) ((_a)->vf_res->vf_cap_flags & \
			      VIRTCHNL_VF_CAP_ADV_LINK_SPEED)
#define FDIR_FLTR_SUPPORT(_a) ((_a)->vf_res->vf_cap_flags & \
			       VIRTCHNL_VF_OFFLOAD_FDIR_PF)
#define ADV_RSS_SUPPORT(_a) ((_a)->vf_res->vf_cap_flags & \
			     VIRTCHNL_VF_OFFLOAD_ADV_RSS_PF)
	struct virtchnl_vf_resource *vf_res; 
	struct virtchnl_vsi_resource *vsi_res; 
	struct virtchnl_version_info pf_version;
#define PF_IS_V11(_a) (((_a)->pf_version.major == 1) && \
		       ((_a)->pf_version.minor == 1))
	struct virtchnl_vlan_caps vlan_v2_caps;
	u16 msg_enable;
	struct iavf_eth_stats current_stats;
	struct iavf_vsi vsi;
	u32 aq_wait_count;
	
	u64 hena;
	u16 rss_key_size;
	u16 rss_lut_size;
	u8 *rss_key;
	u8 *rss_lut;
	
	struct iavf_channel_config ch_config;
	u8 num_tc;
	struct list_head cloud_filter_list;
	
	spinlock_t cloud_filter_list_lock;
	u16 num_cloud_filters;
	
	int orig_num_active_queues;

#define IAVF_MAX_FDIR_FILTERS 128	
	u16 fdir_active_fltr;
	struct list_head fdir_list_head;
	spinlock_t fdir_fltr_lock;	

	struct list_head adv_rss_list_head;
	spinlock_t adv_rss_lock;	
};





struct iavf_device {
	struct list_head list;
	struct iavf_adapter *vf;
};


extern char iavf_driver_name[];

static inline const char *iavf_state_str(enum iavf_state_t state)
{
	switch (state) {
	case __IAVF_STARTUP:
		return "__IAVF_STARTUP";
	case __IAVF_REMOVE:
		return "__IAVF_REMOVE";
	case __IAVF_INIT_VERSION_CHECK:
		return "__IAVF_INIT_VERSION_CHECK";
	case __IAVF_INIT_GET_RESOURCES:
		return "__IAVF_INIT_GET_RESOURCES";
	case __IAVF_INIT_EXTENDED_CAPS:
		return "__IAVF_INIT_EXTENDED_CAPS";
	case __IAVF_INIT_CONFIG_ADAPTER:
		return "__IAVF_INIT_CONFIG_ADAPTER";
	case __IAVF_INIT_SW:
		return "__IAVF_INIT_SW";
	case __IAVF_INIT_FAILED:
		return "__IAVF_INIT_FAILED";
	case __IAVF_RESETTING:
		return "__IAVF_RESETTING";
	case __IAVF_COMM_FAILED:
		return "__IAVF_COMM_FAILED";
	case __IAVF_DOWN:
		return "__IAVF_DOWN";
	case __IAVF_DOWN_PENDING:
		return "__IAVF_DOWN_PENDING";
	case __IAVF_TESTING:
		return "__IAVF_TESTING";
	case __IAVF_RUNNING:
		return "__IAVF_RUNNING";
	default:
		return "__IAVF_UNKNOWN_STATE";
	}
}

static inline void iavf_change_state(struct iavf_adapter *adapter,
				     enum iavf_state_t state)
{
	if (adapter->state != state) {
		adapter->last_state = adapter->state;
		adapter->state = state;
	}
	dev_dbg(&adapter->pdev->dev,
		"state transition from:%s to:%s\n",
		iavf_state_str(adapter->last_state),
		iavf_state_str(adapter->state));
}

int iavf_up(struct iavf_adapter *adapter);
void iavf_down(struct iavf_adapter *adapter);
int iavf_process_config(struct iavf_adapter *adapter);
int iavf_parse_vf_resource_msg(struct iavf_adapter *adapter);
void iavf_schedule_reset(struct iavf_adapter *adapter, u64 flags);
void iavf_schedule_aq_request(struct iavf_adapter *adapter, u64 flags);
void iavf_schedule_finish_config(struct iavf_adapter *adapter);
void iavf_reset(struct iavf_adapter *adapter);
void iavf_set_ethtool_ops(struct net_device *netdev);
void iavf_update_stats(struct iavf_adapter *adapter);
void iavf_free_all_tx_resources(struct iavf_adapter *adapter);
void iavf_free_all_rx_resources(struct iavf_adapter *adapter);

void iavf_napi_add_all(struct iavf_adapter *adapter);
void iavf_napi_del_all(struct iavf_adapter *adapter);

int iavf_send_api_ver(struct iavf_adapter *adapter);
int iavf_verify_api_ver(struct iavf_adapter *adapter);
int iavf_send_vf_config_msg(struct iavf_adapter *adapter);
int iavf_get_vf_config(struct iavf_adapter *adapter);
int iavf_get_vf_vlan_v2_caps(struct iavf_adapter *adapter);
int iavf_send_vf_offload_vlan_v2_msg(struct iavf_adapter *adapter);
void iavf_set_queue_vlan_tag_loc(struct iavf_adapter *adapter);
u16 iavf_get_num_vlans_added(struct iavf_adapter *adapter);
void iavf_irq_enable(struct iavf_adapter *adapter, bool flush);
void iavf_configure_queues(struct iavf_adapter *adapter);
void iavf_deconfigure_queues(struct iavf_adapter *adapter);
void iavf_enable_queues(struct iavf_adapter *adapter);
void iavf_disable_queues(struct iavf_adapter *adapter);
void iavf_map_queues(struct iavf_adapter *adapter);
int iavf_request_queues(struct iavf_adapter *adapter, int num);
void iavf_add_ether_addrs(struct iavf_adapter *adapter);
void iavf_del_ether_addrs(struct iavf_adapter *adapter);
void iavf_add_vlans(struct iavf_adapter *adapter);
void iavf_del_vlans(struct iavf_adapter *adapter);
void iavf_set_promiscuous(struct iavf_adapter *adapter);
bool iavf_promiscuous_mode_changed(struct iavf_adapter *adapter);
void iavf_request_stats(struct iavf_adapter *adapter);
int iavf_request_reset(struct iavf_adapter *adapter);
void iavf_get_hena(struct iavf_adapter *adapter);
void iavf_set_hena(struct iavf_adapter *adapter);
void iavf_set_rss_key(struct iavf_adapter *adapter);
void iavf_set_rss_lut(struct iavf_adapter *adapter);
void iavf_enable_vlan_stripping(struct iavf_adapter *adapter);
void iavf_disable_vlan_stripping(struct iavf_adapter *adapter);
void iavf_virtchnl_completion(struct iavf_adapter *adapter,
			      enum virtchnl_ops v_opcode,
			      enum iavf_status v_retval, u8 *msg, u16 msglen);
int iavf_config_rss(struct iavf_adapter *adapter);
int iavf_lan_add_device(struct iavf_adapter *adapter);
int iavf_lan_del_device(struct iavf_adapter *adapter);
void iavf_client_subtask(struct iavf_adapter *adapter);
void iavf_notify_client_message(struct iavf_vsi *vsi, u8 *msg, u16 len);
void iavf_notify_client_l2_params(struct iavf_vsi *vsi);
void iavf_notify_client_open(struct iavf_vsi *vsi);
void iavf_notify_client_close(struct iavf_vsi *vsi, bool reset);
void iavf_enable_channels(struct iavf_adapter *adapter);
void iavf_disable_channels(struct iavf_adapter *adapter);
void iavf_add_cloud_filter(struct iavf_adapter *adapter);
void iavf_del_cloud_filter(struct iavf_adapter *adapter);
void iavf_enable_vlan_stripping_v2(struct iavf_adapter *adapter, u16 tpid);
void iavf_disable_vlan_stripping_v2(struct iavf_adapter *adapter, u16 tpid);
void iavf_enable_vlan_insertion_v2(struct iavf_adapter *adapter, u16 tpid);
void iavf_disable_vlan_insertion_v2(struct iavf_adapter *adapter, u16 tpid);
void iavf_add_fdir_filter(struct iavf_adapter *adapter);
void iavf_del_fdir_filter(struct iavf_adapter *adapter);
void iavf_add_adv_rss_cfg(struct iavf_adapter *adapter);
void iavf_del_adv_rss_cfg(struct iavf_adapter *adapter);
struct iavf_mac_filter *iavf_add_filter(struct iavf_adapter *adapter,
					const u8 *macaddr);
int iavf_wait_for_reset(struct iavf_adapter *adapter);
#endif 
