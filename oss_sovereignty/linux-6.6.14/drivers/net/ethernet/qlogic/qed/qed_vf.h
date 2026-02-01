 
 

#ifndef _QED_VF_H
#define _QED_VF_H

#include "qed_l2.h"
#include "qed_mcp.h"

#define T_ETH_INDIRECTION_TABLE_SIZE 128
#define T_ETH_RSS_KEY_SIZE 10

struct vf_pf_resc_request {
	u8 num_rxqs;
	u8 num_txqs;
	u8 num_sbs;
	u8 num_mac_filters;
	u8 num_vlan_filters;
	u8 num_mc_filters;
	u8 num_cids;
	u8 padding;
};

struct hw_sb_info {
	u16 hw_sb_id;
	u8 sb_qid;
	u8 padding[5];
};

#define TLV_BUFFER_SIZE                 1024

enum {
	PFVF_STATUS_WAITING,
	PFVF_STATUS_SUCCESS,
	PFVF_STATUS_FAILURE,
	PFVF_STATUS_NOT_SUPPORTED,
	PFVF_STATUS_NO_RESOURCE,
	PFVF_STATUS_FORCED,
	PFVF_STATUS_MALICIOUS,
};

 
 
struct channel_tlv {
	u16 type;
	u16 length;
};

 
struct vfpf_first_tlv {
	struct channel_tlv tl;
	u32 padding;
	u64 reply_address;
};

 
struct pfvf_tlv {
	struct channel_tlv tl;
	u8 status;
	u8 padding[3];
};

 
struct pfvf_def_resp_tlv {
	struct pfvf_tlv hdr;
};

 
struct channel_list_end_tlv {
	struct channel_tlv tl;
	u8 padding[4];
};

#define VFPF_ACQUIRE_OS_LINUX (0)
#define VFPF_ACQUIRE_OS_WINDOWS (1)
#define VFPF_ACQUIRE_OS_ESX (2)
#define VFPF_ACQUIRE_OS_SOLARIS (3)
#define VFPF_ACQUIRE_OS_LINUX_USERSPACE (4)

struct vfpf_acquire_tlv {
	struct vfpf_first_tlv first_tlv;

	struct vf_pf_vfdev_info {
#define VFPF_ACQUIRE_CAP_PRE_FP_HSI     BIT(0)  
#define VFPF_ACQUIRE_CAP_100G		BIT(1)  
	 
#define VFPF_ACQUIRE_CAP_QUEUE_QIDS     BIT(2)

	 
#define VFPF_ACQUIRE_CAP_PHYSICAL_BAR   BIT(3)
		u64 capabilities;
		u8 fw_major;
		u8 fw_minor;
		u8 fw_revision;
		u8 fw_engineering;
		u32 driver_version;
		u16 opaque_fid;	 
		u8 os_type;	 
		u8 eth_fp_hsi_major;
		u8 eth_fp_hsi_minor;
		u8 padding[3];
	} vfdev_info;

	struct vf_pf_resc_request resc_request;

	u64 bulletin_addr;
	u32 bulletin_size;
	u32 padding;
};

 
struct vfpf_vport_update_rss_tlv {
	struct channel_tlv tl;

	u8 update_rss_flags;
#define VFPF_UPDATE_RSS_CONFIG_FLAG       BIT(0)
#define VFPF_UPDATE_RSS_CAPS_FLAG         BIT(1)
#define VFPF_UPDATE_RSS_IND_TABLE_FLAG    BIT(2)
#define VFPF_UPDATE_RSS_KEY_FLAG          BIT(3)

	u8 rss_enable;
	u8 rss_caps;
	u8 rss_table_size_log;	 
	u16 rss_ind_table[T_ETH_INDIRECTION_TABLE_SIZE];
	u32 rss_key[T_ETH_RSS_KEY_SIZE];
};

struct pfvf_storm_stats {
	u32 address;
	u32 len;
};

struct pfvf_stats_info {
	struct pfvf_storm_stats mstats;
	struct pfvf_storm_stats pstats;
	struct pfvf_storm_stats tstats;
	struct pfvf_storm_stats ustats;
};

struct pfvf_acquire_resp_tlv {
	struct pfvf_tlv hdr;

	struct pf_vf_pfdev_info {
		u32 chip_num;
		u32 mfw_ver;

		u16 fw_major;
		u16 fw_minor;
		u16 fw_rev;
		u16 fw_eng;

		u64 capabilities;
#define PFVF_ACQUIRE_CAP_DEFAULT_UNTAGGED	BIT(0)
#define PFVF_ACQUIRE_CAP_100G			BIT(1)	 
 
#define PFVF_ACQUIRE_CAP_POST_FW_OVERRIDE	BIT(2)

	 
#define PFVF_ACQUIRE_CAP_QUEUE_QIDS             BIT(3)

		u16 db_size;
		u8 indices_per_sb;
		u8 os_type;

		 
		u16 chip_rev;
		u8 dev_type;

		 
		u8 bar_size;

		struct pfvf_stats_info stats_info;

		u8 port_mac[ETH_ALEN];

		 
		u8 major_fp_hsi;
		u8 minor_fp_hsi;
	} pfdev_info;

	struct pf_vf_resc {
#define PFVF_MAX_QUEUES_PER_VF		16
#define PFVF_MAX_SBS_PER_VF		16
		struct hw_sb_info hw_sbs[PFVF_MAX_SBS_PER_VF];
		u8 hw_qid[PFVF_MAX_QUEUES_PER_VF];
		u8 cid[PFVF_MAX_QUEUES_PER_VF];

		u8 num_rxqs;
		u8 num_txqs;
		u8 num_sbs;
		u8 num_mac_filters;
		u8 num_vlan_filters;
		u8 num_mc_filters;
		u8 num_cids;
		u8 padding;
	} resc;

	u32 bulletin_size;
	u32 padding;
};

struct pfvf_start_queue_resp_tlv {
	struct pfvf_tlv hdr;
	u32 offset;		 
	u8 padding[4];
};

 
struct vfpf_qid_tlv {
	struct channel_tlv tl;
	u8 qid;
	u8 padding[3];
};

 
struct vfpf_start_rxq_tlv {
	struct vfpf_first_tlv first_tlv;

	 
	u64 rxq_addr;
	u64 deprecated_sge_addr;
	u64 cqe_pbl_addr;

	u16 cqe_pbl_size;
	u16 hw_sb;
	u16 rx_qid;
	u16 hc_rate;		 

	u16 bd_max_bytes;
	u16 stat_id;
	u8 sb_index;
	u8 padding[3];
};

struct vfpf_start_txq_tlv {
	struct vfpf_first_tlv first_tlv;

	 
	u64 pbl_addr;
	u16 pbl_size;
	u16 stat_id;
	u16 tx_qid;
	u16 hw_sb;

	u32 flags;		 
	u16 hc_rate;		 
	u8 sb_index;
	u8 padding[3];
};

 
struct vfpf_stop_rxqs_tlv {
	struct vfpf_first_tlv first_tlv;

	u16 rx_qid;

	 
	u8 num_rxqs;
	u8 cqe_completion;
	u8 padding[4];
};

 
struct vfpf_stop_txqs_tlv {
	struct vfpf_first_tlv first_tlv;

	u16 tx_qid;

	 
	u8 num_txqs;
	u8 padding[5];
};

struct vfpf_update_rxq_tlv {
	struct vfpf_first_tlv first_tlv;

	u64 deprecated_sge_addr[PFVF_MAX_QUEUES_PER_VF];

	u16 rx_qid;
	u8 num_rxqs;
	u8 flags;
#define VFPF_RXQ_UPD_INIT_SGE_DEPRECATE_FLAG    BIT(0)
#define VFPF_RXQ_UPD_COMPLETE_CQE_FLAG          BIT(1)
#define VFPF_RXQ_UPD_COMPLETE_EVENT_FLAG        BIT(2)

	u8 padding[4];
};

 
struct vfpf_q_mac_vlan_filter {
	u32 flags;
#define VFPF_Q_FILTER_DEST_MAC_VALID    0x01
#define VFPF_Q_FILTER_VLAN_TAG_VALID    0x02
#define VFPF_Q_FILTER_SET_MAC           0x100	 

	u8 mac[ETH_ALEN];
	u16 vlan_tag;

	u8 padding[4];
};

 
struct vfpf_vport_start_tlv {
	struct vfpf_first_tlv first_tlv;

	u64 sb_addr[PFVF_MAX_SBS_PER_VF];

	u32 tpa_mode;
	u16 dep1;
	u16 mtu;

	u8 vport_id;
	u8 inner_vlan_removal;

	u8 only_untagged;
	u8 max_buffers_per_cqe;

	u8 padding[4];
};

 
struct vfpf_vport_update_activate_tlv {
	struct channel_tlv tl;
	u8 update_rx;
	u8 update_tx;
	u8 active_rx;
	u8 active_tx;
};

struct vfpf_vport_update_tx_switch_tlv {
	struct channel_tlv tl;
	u8 tx_switching;
	u8 padding[3];
};

struct vfpf_vport_update_vlan_strip_tlv {
	struct channel_tlv tl;
	u8 remove_vlan;
	u8 padding[3];
};

struct vfpf_vport_update_mcast_bin_tlv {
	struct channel_tlv tl;
	u8 padding[4];

	 
	u64 bins[4];
	u64 obsolete_bins[4];
};

struct vfpf_vport_update_accept_param_tlv {
	struct channel_tlv tl;
	u8 update_rx_mode;
	u8 update_tx_mode;
	u8 rx_accept_filter;
	u8 tx_accept_filter;
};

struct vfpf_vport_update_accept_any_vlan_tlv {
	struct channel_tlv tl;
	u8 update_accept_any_vlan_flg;
	u8 accept_any_vlan;

	u8 padding[2];
};

struct vfpf_vport_update_sge_tpa_tlv {
	struct channel_tlv tl;

	u16 sge_tpa_flags;
#define VFPF_TPA_IPV4_EN_FLAG		BIT(0)
#define VFPF_TPA_IPV6_EN_FLAG		BIT(1)
#define VFPF_TPA_PKT_SPLIT_FLAG		BIT(2)
#define VFPF_TPA_HDR_DATA_SPLIT_FLAG	BIT(3)
#define VFPF_TPA_GRO_CONSIST_FLAG	BIT(4)

	u8 update_sge_tpa_flags;
#define VFPF_UPDATE_SGE_DEPRECATED_FLAG	BIT(0)
#define VFPF_UPDATE_TPA_EN_FLAG		BIT(1)
#define VFPF_UPDATE_TPA_PARAM_FLAG	BIT(2)

	u8 max_buffers_per_cqe;

	u16 deprecated_sge_buff_size;
	u16 tpa_max_size;
	u16 tpa_min_size_to_start;
	u16 tpa_min_size_to_cont;

	u8 tpa_max_aggs_num;
	u8 padding[7];
};

 
struct vfpf_vport_update_tlv {
	struct vfpf_first_tlv first_tlv;
};

struct vfpf_ucast_filter_tlv {
	struct vfpf_first_tlv first_tlv;

	u8 opcode;
	u8 type;

	u8 mac[ETH_ALEN];

	u16 vlan;
	u16 padding[3];
};

 
struct vfpf_update_tunn_param_tlv {
	struct vfpf_first_tlv first_tlv;

	u8 tun_mode_update_mask;
	u8 tunn_mode;
	u8 update_tun_cls;
	u8 vxlan_clss;
	u8 l2gre_clss;
	u8 ipgre_clss;
	u8 l2geneve_clss;
	u8 ipgeneve_clss;
	u8 update_geneve_port;
	u8 update_vxlan_port;
	u16 geneve_port;
	u16 vxlan_port;
	u8 padding[2];
};

struct pfvf_update_tunn_param_tlv {
	struct pfvf_tlv hdr;

	u16 tunn_feature_mask;
	u8 vxlan_mode;
	u8 l2geneve_mode;
	u8 ipgeneve_mode;
	u8 l2gre_mode;
	u8 ipgre_mode;
	u8 vxlan_clss;
	u8 l2gre_clss;
	u8 ipgre_clss;
	u8 l2geneve_clss;
	u8 ipgeneve_clss;
	u16 vxlan_udp_port;
	u16 geneve_udp_port;
};

struct tlv_buffer_size {
	u8 tlv_buffer[TLV_BUFFER_SIZE];
};

struct vfpf_update_coalesce {
	struct vfpf_first_tlv first_tlv;
	u16 rx_coal;
	u16 tx_coal;
	u16 qid;
	u8 padding[2];
};

struct vfpf_read_coal_req_tlv {
	struct vfpf_first_tlv first_tlv;
	u16 qid;
	u8 is_rx;
	u8 padding[5];
};

struct pfvf_read_coal_resp_tlv {
	struct pfvf_tlv hdr;
	u16 coal;
	u8 padding[6];
};

struct vfpf_bulletin_update_mac_tlv {
	struct vfpf_first_tlv first_tlv;
	u8 mac[ETH_ALEN];
	u8 padding[2];
};

union vfpf_tlvs {
	struct vfpf_first_tlv first_tlv;
	struct vfpf_acquire_tlv acquire;
	struct vfpf_start_rxq_tlv start_rxq;
	struct vfpf_start_txq_tlv start_txq;
	struct vfpf_stop_rxqs_tlv stop_rxqs;
	struct vfpf_stop_txqs_tlv stop_txqs;
	struct vfpf_update_rxq_tlv update_rxq;
	struct vfpf_vport_start_tlv start_vport;
	struct vfpf_vport_update_tlv vport_update;
	struct vfpf_ucast_filter_tlv ucast_filter;
	struct vfpf_update_tunn_param_tlv tunn_param_update;
	struct vfpf_update_coalesce update_coalesce;
	struct vfpf_read_coal_req_tlv read_coal_req;
	struct vfpf_bulletin_update_mac_tlv bulletin_update_mac;
	struct tlv_buffer_size tlv_buf_size;
};

union pfvf_tlvs {
	struct pfvf_def_resp_tlv default_resp;
	struct pfvf_acquire_resp_tlv acquire_resp;
	struct tlv_buffer_size tlv_buf_size;
	struct pfvf_start_queue_resp_tlv queue_start;
	struct pfvf_update_tunn_param_tlv tunn_param_resp;
	struct pfvf_read_coal_resp_tlv read_coal_resp;
};

enum qed_bulletin_bit {
	 
	MAC_ADDR_FORCED = 0,
	 
	VLAN_ADDR_FORCED = 2,

	 
	VFPF_BULLETIN_UNTAGGED_DEFAULT = 3,
	VFPF_BULLETIN_UNTAGGED_DEFAULT_FORCED = 4,

	 
	VFPF_BULLETIN_MAC_ADDR = 5
};

struct qed_bulletin_content {
	 
	u32 crc;

	u32 version;

	 
	u64 valid_bitmap;

	 
	u8 mac[ETH_ALEN];

	 
	u8 default_only_untagged;
	u8 padding;

	 
	u8 req_autoneg;
	u8 req_autoneg_pause;
	u8 req_forced_rx;
	u8 req_forced_tx;
	u8 padding2[4];

	u32 req_adv_speed;
	u32 req_forced_speed;
	u32 req_loopback;
	u32 padding3;

	u8 link_up;
	u8 full_duplex;
	u8 autoneg;
	u8 autoneg_complete;
	u8 parallel_detection;
	u8 pfc_enabled;
	u8 partner_tx_flow_ctrl_en;
	u8 partner_rx_flow_ctrl_en;
	u8 partner_adv_pause;
	u8 sfp_tx_fault;
	u16 vxlan_udp_port;
	u16 geneve_udp_port;
	u8 padding4[2];

	u32 speed;
	u32 partner_adv_speed;

	u32 capability_speed;

	 
	u16 pvid;
	u16 padding5;
};

struct qed_bulletin {
	dma_addr_t phys;
	struct qed_bulletin_content *p_virt;
	u32 size;
};

enum {
	CHANNEL_TLV_NONE,	 
	CHANNEL_TLV_ACQUIRE,
	CHANNEL_TLV_VPORT_START,
	CHANNEL_TLV_VPORT_UPDATE,
	CHANNEL_TLV_VPORT_TEARDOWN,
	CHANNEL_TLV_START_RXQ,
	CHANNEL_TLV_START_TXQ,
	CHANNEL_TLV_STOP_RXQS,
	CHANNEL_TLV_STOP_TXQS,
	CHANNEL_TLV_UPDATE_RXQ,
	CHANNEL_TLV_INT_CLEANUP,
	CHANNEL_TLV_CLOSE,
	CHANNEL_TLV_RELEASE,
	CHANNEL_TLV_LIST_END,
	CHANNEL_TLV_UCAST_FILTER,
	CHANNEL_TLV_VPORT_UPDATE_ACTIVATE,
	CHANNEL_TLV_VPORT_UPDATE_TX_SWITCH,
	CHANNEL_TLV_VPORT_UPDATE_VLAN_STRIP,
	CHANNEL_TLV_VPORT_UPDATE_MCAST,
	CHANNEL_TLV_VPORT_UPDATE_ACCEPT_PARAM,
	CHANNEL_TLV_VPORT_UPDATE_RSS,
	CHANNEL_TLV_VPORT_UPDATE_ACCEPT_ANY_VLAN,
	CHANNEL_TLV_VPORT_UPDATE_SGE_TPA,
	CHANNEL_TLV_UPDATE_TUNN_PARAM,
	CHANNEL_TLV_COALESCE_UPDATE,
	CHANNEL_TLV_QID,
	CHANNEL_TLV_COALESCE_READ,
	CHANNEL_TLV_BULLETIN_UPDATE_MAC,
	CHANNEL_TLV_MAX,

	 
	CHANNEL_TLV_VPORT_UPDATE_MAX = CHANNEL_TLV_VPORT_UPDATE_SGE_TPA + 1,
};

 
#define QED_ETH_VF_DEFAULT_NUM_CIDS (32)
#define QED_ETH_VF_MAX_NUM_CIDS (250)

 
struct qed_vf_iov {
	union vfpf_tlvs *vf2pf_request;
	dma_addr_t vf2pf_request_phys;
	union pfvf_tlvs *pf2vf_reply;
	dma_addr_t pf2vf_reply_phys;

	 
	struct mutex mutex;
	u8 *offset;

	 
	struct qed_bulletin bulletin;
	struct qed_bulletin_content bulletin_shadow;

	 
	struct pfvf_acquire_resp_tlv acquire_resp;

	 
	bool b_pre_fp_hsi;

	 
	struct qed_sb_info *sbs_info[PFVF_MAX_SBS_PER_VF];

	 
	bool b_doorbell_bar;
};

 
int qed_vf_pf_set_coalesce(struct qed_hwfn *p_hwfn,
			   u16 rx_coal,
			   u16 tx_coal, struct qed_queue_cid *p_cid);

 
int qed_vf_pf_get_coalesce(struct qed_hwfn *p_hwfn,
			   u16 *p_coal, struct qed_queue_cid *p_cid);

#ifdef CONFIG_QED_SRIOV
 
int qed_vf_read_bulletin(struct qed_hwfn *p_hwfn, u8 *p_change);

 
void qed_vf_get_link_params(struct qed_hwfn *p_hwfn,
			    struct qed_mcp_link_params *params);

 
void qed_vf_get_link_state(struct qed_hwfn *p_hwfn,
			   struct qed_mcp_link_state *link);

 
void qed_vf_get_link_caps(struct qed_hwfn *p_hwfn,
			  struct qed_mcp_link_capabilities *p_link_caps);

 
void qed_vf_get_num_rxqs(struct qed_hwfn *p_hwfn, u8 *num_rxqs);

 
void qed_vf_get_num_txqs(struct qed_hwfn *p_hwfn, u8 *num_txqs);

 
void qed_vf_get_num_cids(struct qed_hwfn *p_hwfn, u8 *num_cids);

 
void qed_vf_get_port_mac(struct qed_hwfn *p_hwfn, u8 *port_mac);

 
void qed_vf_get_num_vlan_filters(struct qed_hwfn *p_hwfn,
				 u8 *num_vlan_filters);

 
void qed_vf_get_num_mac_filters(struct qed_hwfn *p_hwfn, u8 *num_mac_filters);

 
bool qed_vf_check_mac(struct qed_hwfn *p_hwfn, u8 *mac);

 
void qed_vf_get_fw_version(struct qed_hwfn *p_hwfn,
			   u16 *fw_major, u16 *fw_minor,
			   u16 *fw_rev, u16 *fw_eng);

 
int qed_vf_hw_prepare(struct qed_hwfn *p_hwfn);

 
int qed_vf_pf_rxq_start(struct qed_hwfn *p_hwfn,
			struct qed_queue_cid *p_cid,
			u16 bd_max_bytes,
			dma_addr_t bd_chain_phys_addr,
			dma_addr_t cqe_pbl_addr,
			u16 cqe_pbl_size, void __iomem **pp_prod);

 
int
qed_vf_pf_txq_start(struct qed_hwfn *p_hwfn,
		    struct qed_queue_cid *p_cid,
		    dma_addr_t pbl_addr,
		    u16 pbl_size, void __iomem **pp_doorbell);

 
int qed_vf_pf_rxq_stop(struct qed_hwfn *p_hwfn,
		       struct qed_queue_cid *p_cid, bool cqe_completion);

 
int qed_vf_pf_txq_stop(struct qed_hwfn *p_hwfn, struct qed_queue_cid *p_cid);

 
int qed_vf_pf_vport_update(struct qed_hwfn *p_hwfn,
			   struct qed_sp_vport_update_params *p_params);

 
int qed_vf_pf_reset(struct qed_hwfn *p_hwfn);

 
int qed_vf_pf_release(struct qed_hwfn *p_hwfn);

 
u16 qed_vf_get_igu_sb_id(struct qed_hwfn *p_hwfn, u16 sb_id);

 
void qed_vf_set_sb_info(struct qed_hwfn *p_hwfn,
			u16 sb_id, struct qed_sb_info *p_sb);

 
int qed_vf_pf_vport_start(struct qed_hwfn *p_hwfn,
			  u8 vport_id,
			  u16 mtu,
			  u8 inner_vlan_removal,
			  enum qed_tpa_mode tpa_mode,
			  u8 max_buffers_per_cqe, u8 only_untagged);

 
int qed_vf_pf_vport_stop(struct qed_hwfn *p_hwfn);

int qed_vf_pf_filter_ucast(struct qed_hwfn *p_hwfn,
			   struct qed_filter_ucast *p_param);

void qed_vf_pf_filter_mcast(struct qed_hwfn *p_hwfn,
			    struct qed_filter_mcast *p_filter_cmd);

 
int qed_vf_pf_int_cleanup(struct qed_hwfn *p_hwfn);

 
void __qed_vf_get_link_params(struct qed_hwfn *p_hwfn,
			      struct qed_mcp_link_params *p_params,
			      struct qed_bulletin_content *p_bulletin);

 
void __qed_vf_get_link_state(struct qed_hwfn *p_hwfn,
			     struct qed_mcp_link_state *p_link,
			     struct qed_bulletin_content *p_bulletin);

 
void __qed_vf_get_link_caps(struct qed_hwfn *p_hwfn,
			    struct qed_mcp_link_capabilities *p_link_caps,
			    struct qed_bulletin_content *p_bulletin);

void qed_iov_vf_task(struct work_struct *work);
void qed_vf_set_vf_start_tunn_update_param(struct qed_tunnel_info *p_tun);
int qed_vf_pf_tunnel_param_update(struct qed_hwfn *p_hwfn,
				  struct qed_tunnel_info *p_tunn);

u32 qed_vf_hw_bar_size(struct qed_hwfn *p_hwfn, enum BAR_ID bar_id);
 
int qed_vf_pf_bulletin_update_mac(struct qed_hwfn *p_hwfn, const u8 *p_mac);

#else
static inline void qed_vf_get_link_params(struct qed_hwfn *p_hwfn,
					  struct qed_mcp_link_params *params)
{
}

static inline void qed_vf_get_link_state(struct qed_hwfn *p_hwfn,
					 struct qed_mcp_link_state *link)
{
}

static inline void
qed_vf_get_link_caps(struct qed_hwfn *p_hwfn,
		     struct qed_mcp_link_capabilities *p_link_caps)
{
}

static inline void qed_vf_get_num_rxqs(struct qed_hwfn *p_hwfn, u8 *num_rxqs)
{
}

static inline void qed_vf_get_num_txqs(struct qed_hwfn *p_hwfn, u8 *num_txqs)
{
}

static inline void qed_vf_get_num_cids(struct qed_hwfn *p_hwfn, u8 *num_cids)
{
}

static inline void qed_vf_get_port_mac(struct qed_hwfn *p_hwfn, u8 *port_mac)
{
}

static inline void qed_vf_get_num_vlan_filters(struct qed_hwfn *p_hwfn,
					       u8 *num_vlan_filters)
{
}

static inline void qed_vf_get_num_mac_filters(struct qed_hwfn *p_hwfn,
					      u8 *num_mac_filters)
{
}

static inline bool qed_vf_check_mac(struct qed_hwfn *p_hwfn, u8 *mac)
{
	return false;
}

static inline void qed_vf_get_fw_version(struct qed_hwfn *p_hwfn,
					 u16 *fw_major, u16 *fw_minor,
					 u16 *fw_rev, u16 *fw_eng)
{
}

static inline int qed_vf_hw_prepare(struct qed_hwfn *p_hwfn)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rxq_start(struct qed_hwfn *p_hwfn,
				      struct qed_queue_cid *p_cid,
				      u16 bd_max_bytes,
				      dma_addr_t bd_chain_phys_adr,
				      dma_addr_t cqe_pbl_addr,
				      u16 cqe_pbl_size, void __iomem **pp_prod)
{
	return -EINVAL;
}

static inline int qed_vf_pf_txq_start(struct qed_hwfn *p_hwfn,
				      struct qed_queue_cid *p_cid,
				      dma_addr_t pbl_addr,
				      u16 pbl_size, void __iomem **pp_doorbell)
{
	return -EINVAL;
}

static inline int qed_vf_pf_rxq_stop(struct qed_hwfn *p_hwfn,
				     struct qed_queue_cid *p_cid,
				     bool cqe_completion)
{
	return -EINVAL;
}

static inline int qed_vf_pf_txq_stop(struct qed_hwfn *p_hwfn,
				     struct qed_queue_cid *p_cid)
{
	return -EINVAL;
}

static inline int
qed_vf_pf_vport_update(struct qed_hwfn *p_hwfn,
		       struct qed_sp_vport_update_params *p_params)
{
	return -EINVAL;
}

static inline int qed_vf_pf_reset(struct qed_hwfn *p_hwfn)
{
	return -EINVAL;
}

static inline int qed_vf_pf_release(struct qed_hwfn *p_hwfn)
{
	return -EINVAL;
}

static inline u16 qed_vf_get_igu_sb_id(struct qed_hwfn *p_hwfn, u16 sb_id)
{
	return 0;
}

static inline void qed_vf_set_sb_info(struct qed_hwfn *p_hwfn, u16 sb_id,
				      struct qed_sb_info *p_sb)
{
}

static inline int qed_vf_pf_vport_start(struct qed_hwfn *p_hwfn,
					u8 vport_id,
					u16 mtu,
					u8 inner_vlan_removal,
					enum qed_tpa_mode tpa_mode,
					u8 max_buffers_per_cqe,
					u8 only_untagged)
{
	return -EINVAL;
}

static inline int qed_vf_pf_vport_stop(struct qed_hwfn *p_hwfn)
{
	return -EINVAL;
}

static inline int qed_vf_pf_filter_ucast(struct qed_hwfn *p_hwfn,
					 struct qed_filter_ucast *p_param)
{
	return -EINVAL;
}

static inline void qed_vf_pf_filter_mcast(struct qed_hwfn *p_hwfn,
					  struct qed_filter_mcast *p_filter_cmd)
{
}

static inline int qed_vf_pf_int_cleanup(struct qed_hwfn *p_hwfn)
{
	return -EINVAL;
}

static inline void __qed_vf_get_link_params(struct qed_hwfn *p_hwfn,
					    struct qed_mcp_link_params
					    *p_params,
					    struct qed_bulletin_content
					    *p_bulletin)
{
}

static inline void __qed_vf_get_link_state(struct qed_hwfn *p_hwfn,
					   struct qed_mcp_link_state *p_link,
					   struct qed_bulletin_content
					   *p_bulletin)
{
}

static inline void
__qed_vf_get_link_caps(struct qed_hwfn *p_hwfn,
		       struct qed_mcp_link_capabilities *p_link_caps,
		       struct qed_bulletin_content *p_bulletin)
{
}

static inline void qed_iov_vf_task(struct work_struct *work)
{
}

static inline void
qed_vf_set_vf_start_tunn_update_param(struct qed_tunnel_info *p_tun)
{
}

static inline int qed_vf_pf_tunnel_param_update(struct qed_hwfn *p_hwfn,
						struct qed_tunnel_info *p_tunn)
{
	return -EINVAL;
}

static inline int qed_vf_pf_bulletin_update_mac(struct qed_hwfn *p_hwfn,
						const u8 *p_mac)
{
	return -EINVAL;
}

static inline u32
qed_vf_hw_bar_size(struct qed_hwfn  *p_hwfn,
		   enum BAR_ID bar_id)
{
	return 0;
}
#endif

#endif
