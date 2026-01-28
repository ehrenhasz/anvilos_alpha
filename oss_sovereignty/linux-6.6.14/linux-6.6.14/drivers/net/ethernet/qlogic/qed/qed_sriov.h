#ifndef _QED_SRIOV_H
#define _QED_SRIOV_H
#include <linux/types.h>
#include "qed_vf.h"
#define QED_ETH_VF_NUM_MAC_FILTERS 1
#define QED_ETH_VF_NUM_VLAN_FILTERS 2
#define QED_VF_ARRAY_LENGTH (3)
#ifdef CONFIG_QED_SRIOV
#define IS_VF(cdev)             ((cdev)->b_is_vf)
#define IS_PF(cdev)             (!((cdev)->b_is_vf))
#define IS_PF_SRIOV(p_hwfn)     (!!((p_hwfn)->cdev->p_iov_info))
#else
#define IS_VF(cdev)             (0)
#define IS_PF(cdev)             (1)
#define IS_PF_SRIOV(p_hwfn)     (0)
#endif
#define IS_PF_SRIOV_ALLOC(p_hwfn)       (!!((p_hwfn)->pf_iov_info))
#define QED_MAX_VF_CHAINS_PER_PF 16
#define QED_ETH_MAX_VF_NUM_VLAN_FILTERS	\
	(MAX_NUM_VFS * QED_ETH_VF_NUM_VLAN_FILTERS)
enum qed_iov_vport_update_flag {
	QED_IOV_VP_UPDATE_ACTIVATE,
	QED_IOV_VP_UPDATE_VLAN_STRIP,
	QED_IOV_VP_UPDATE_TX_SWITCH,
	QED_IOV_VP_UPDATE_MCAST,
	QED_IOV_VP_UPDATE_ACCEPT_PARAM,
	QED_IOV_VP_UPDATE_RSS,
	QED_IOV_VP_UPDATE_ACCEPT_ANY_VLAN,
	QED_IOV_VP_UPDATE_SGE_TPA,
	QED_IOV_VP_UPDATE_MAX,
};
struct qed_public_vf_info {
	u8 forced_mac[ETH_ALEN];
	u16 forced_vlan;
	u8 mac[ETH_ALEN];
	int link_state;
	int tx_rate;
	bool is_trusted_configured;
	bool is_trusted_request;
	u8 rx_accept_mode;
	u8 tx_accept_mode;
	bool accept_any_vlan;
};
struct qed_iov_vf_init_params {
	u16 rel_vf_id;
	u16 num_queues;
	u16 req_rx_queue[QED_MAX_VF_CHAINS_PER_PF];
	u16 req_tx_queue[QED_MAX_VF_CHAINS_PER_PF];
};
struct qed_hw_sriov_info {
	int pos;		 
	int nres;		 
	u32 cap;		 
	u16 ctrl;		 
	u16 total_vfs;		 
	u16 num_vfs;		 
	u16 initial_vfs;	 
	u16 nr_virtfn;		 
	u16 offset;		 
	u16 stride;		 
	u16 vf_device_id;	 
	u32 pgsz;		 
	u8 link;		 
	u32 first_vf_in_pf;
};
struct qed_iov_vf_mbx {
	union vfpf_tlvs *req_virt;
	dma_addr_t req_phys;
	union pfvf_tlvs *reply_virt;
	dma_addr_t reply_phys;
	dma_addr_t pending_req;
	bool b_pending_msg;
	u8 *offset;
	struct vfpf_first_tlv first_tlv;
};
#define QED_IOV_LEGACY_QID_RX (0)
#define QED_IOV_LEGACY_QID_TX (1)
#define QED_IOV_QID_INVALID (0xFE)
struct qed_vf_queue_cid {
	bool b_is_tx;
	struct qed_queue_cid *p_cid;
};
struct qed_vf_queue {
	u16 fw_rx_qid;
	u16 fw_tx_qid;
	struct qed_vf_queue_cid cids[MAX_QUEUES_PER_QZONE];
};
enum vf_state {
	VF_FREE = 0,		 
	VF_ACQUIRED,		 
	VF_ENABLED,		 
	VF_RESET,		 
	VF_STOPPED		 
};
struct qed_vf_vlan_shadow {
	bool used;
	u16 vid;
};
struct qed_vf_shadow_config {
	struct qed_vf_vlan_shadow vlans[QED_ETH_VF_NUM_VLAN_FILTERS + 1];
	u8 macs[QED_ETH_VF_NUM_MAC_FILTERS][ETH_ALEN];
	u8 inner_vlan_removal;
};
struct qed_vf_info {
	struct qed_iov_vf_mbx vf_mbx;
	enum vf_state state;
	bool b_init;
	bool b_malicious;
	u8 to_disable;
	struct qed_bulletin bulletin;
	dma_addr_t vf_bulletin;
	struct vfpf_acquire_tlv acquire;
	u32 concrete_fid;
	u16 opaque_fid;
	u16 mtu;
	u8 vport_id;
	u8 relative_vf_id;
	u8 abs_vf_id;
#define QED_VF_ABS_ID(p_hwfn, p_vf)	(QED_PATH_ID(p_hwfn) ?		      \
					 (p_vf)->abs_vf_id + MAX_NUM_VFS_BB : \
					 (p_vf)->abs_vf_id)
	u8 vport_instance;
	u8 num_rxqs;
	u8 num_txqs;
	u16 rx_coal;
	u16 tx_coal;
	u8 num_sbs;
	u8 num_mac_filters;
	u8 num_vlan_filters;
	struct qed_vf_queue vf_queues[QED_MAX_VF_CHAINS_PER_PF];
	u16 igu_sbs[QED_MAX_VF_CHAINS_PER_PF];
	u8 num_active_rxqs;
	struct qed_public_vf_info p_vf_info;
	bool spoof_chk;
	bool req_spoofchk_val;
	struct qed_vf_shadow_config shadow_config;
	u64 configured_features;
#define QED_IOV_CONFIGURED_FEATURES_MASK        ((1 << MAC_ADDR_FORCED) | \
						 (1 << VLAN_ADDR_FORCED))
};
struct qed_pf_iov {
	struct qed_vf_info vfs_array[MAX_NUM_VFS];
	u64 pending_flr[QED_VF_ARRAY_LENGTH];
	void *mbx_msg_virt_addr;
	dma_addr_t mbx_msg_phys_addr;
	u32 mbx_msg_size;
	void *mbx_reply_virt_addr;
	dma_addr_t mbx_reply_phys_addr;
	u32 mbx_reply_size;
	void *p_bulletins;
	dma_addr_t bulletins_phys;
	u32 bulletins_size;
};
enum qed_iov_wq_flag {
	QED_IOV_WQ_MSG_FLAG,
	QED_IOV_WQ_SET_UNICAST_FILTER_FLAG,
	QED_IOV_WQ_BULLETIN_UPDATE_FLAG,
	QED_IOV_WQ_STOP_WQ_FLAG,
	QED_IOV_WQ_FLR_FLAG,
	QED_IOV_WQ_TRUST_FLAG,
	QED_IOV_WQ_VF_FORCE_LINK_QUERY_FLAG,
};
extern const struct qed_iov_hv_ops qed_iov_ops_pass;
#ifdef CONFIG_QED_SRIOV
bool qed_iov_is_valid_vfid(struct qed_hwfn *p_hwfn,
			   int rel_vf_id,
			   bool b_enabled_only, bool b_non_malicious);
u16 qed_iov_get_next_active_vf(struct qed_hwfn *p_hwfn, u16 rel_vf_id);
void qed_iov_bulletin_set_udp_ports(struct qed_hwfn *p_hwfn,
				    int vfid, u16 vxlan_port, u16 geneve_port);
int qed_iov_hw_info(struct qed_hwfn *p_hwfn);
void *qed_add_tlv(struct qed_hwfn *p_hwfn, u8 **offset, u16 type, u16 length);
void qed_dp_tlv_list(struct qed_hwfn *p_hwfn, void *tlvs_list);
void qed_sriov_vfpf_malicious(struct qed_hwfn *p_hwfn,
			      struct fw_err_data *p_data);
int qed_sriov_eqe_event(struct qed_hwfn *p_hwfn, u8 opcode, __le16 echo,
			union event_ring_data *data, u8  fw_return_code);
int qed_iov_alloc(struct qed_hwfn *p_hwfn);
void qed_iov_setup(struct qed_hwfn *p_hwfn);
void qed_iov_free(struct qed_hwfn *p_hwfn);
void qed_iov_free_hw_info(struct qed_dev *cdev);
bool qed_iov_mark_vf_flr(struct qed_hwfn *p_hwfn, u32 *disabled_vfs);
void *qed_iov_search_list_tlvs(struct qed_hwfn *p_hwfn,
			       void *p_tlvs_list, u16 req_type);
void qed_iov_wq_stop(struct qed_dev *cdev, bool schedule_first);
int qed_iov_wq_start(struct qed_dev *cdev);
void qed_schedule_iov(struct qed_hwfn *hwfn, enum qed_iov_wq_flag flag);
void qed_vf_start_iov_wq(struct qed_dev *cdev);
int qed_sriov_disable(struct qed_dev *cdev, bool pci_enabled);
void qed_inform_vf_link_state(struct qed_hwfn *hwfn);
#else
static inline bool
qed_iov_is_valid_vfid(struct qed_hwfn *p_hwfn,
		      int rel_vf_id, bool b_enabled_only, bool b_non_malicious)
{
	return false;
}
static inline u16 qed_iov_get_next_active_vf(struct qed_hwfn *p_hwfn,
					     u16 rel_vf_id)
{
	return MAX_NUM_VFS;
}
static inline void
qed_iov_bulletin_set_udp_ports(struct qed_hwfn *p_hwfn, int vfid,
			       u16 vxlan_port, u16 geneve_port)
{
}
static inline int qed_iov_hw_info(struct qed_hwfn *p_hwfn)
{
	return 0;
}
static inline int qed_iov_alloc(struct qed_hwfn *p_hwfn)
{
	return 0;
}
static inline void qed_iov_setup(struct qed_hwfn *p_hwfn)
{
}
static inline void qed_iov_free(struct qed_hwfn *p_hwfn)
{
}
static inline void qed_iov_free_hw_info(struct qed_dev *cdev)
{
}
static inline bool qed_iov_mark_vf_flr(struct qed_hwfn *p_hwfn,
				       u32 *disabled_vfs)
{
	return false;
}
static inline void qed_iov_wq_stop(struct qed_dev *cdev, bool schedule_first)
{
}
static inline int qed_iov_wq_start(struct qed_dev *cdev)
{
	return 0;
}
static inline void qed_schedule_iov(struct qed_hwfn *hwfn,
				    enum qed_iov_wq_flag flag)
{
}
static inline void qed_vf_start_iov_wq(struct qed_dev *cdev)
{
}
static inline int qed_sriov_disable(struct qed_dev *cdev, bool pci_enabled)
{
	return 0;
}
static inline void qed_inform_vf_link_state(struct qed_hwfn *hwfn)
{
}
static inline void qed_sriov_vfpf_malicious(struct qed_hwfn *p_hwfn,
					    struct fw_err_data *p_data)
{
}
static inline int qed_sriov_eqe_event(struct qed_hwfn *p_hwfn, u8 opcode,
				      __le16 echo, union event_ring_data *data,
				      u8  fw_return_code)
{
	return 0;
}
#endif
#define qed_for_each_vf(_p_hwfn, _i)			  \
	for (_i = qed_iov_get_next_active_vf(_p_hwfn, 0); \
	     _i < MAX_NUM_VFS;				  \
	     _i = qed_iov_get_next_active_vf(_p_hwfn, _i + 1))
#endif
