 
 

#ifndef _SMC_CORE_H
#define _SMC_CORE_H

#include <linux/atomic.h>
#include <linux/smc.h>
#include <linux/pci.h>
#include <rdma/ib_verbs.h>
#include <net/genetlink.h>

#include "smc.h"
#include "smc_ib.h"

#define SMC_RMBS_PER_LGR_MAX	255	 
#define SMC_CONN_PER_LGR_MIN	16	 
#define SMC_CONN_PER_LGR_MAX	255	 
#define SMC_CONN_PER_LGR_PREFER	255	 

struct smc_lgr_list {			 
	struct list_head	list;
	spinlock_t		lock;	 
	u32			num;	 
};

enum smc_lgr_role {		 
	SMC_CLNT,	 
	SMC_SERV	 
};

enum smc_link_state {			 
	SMC_LNK_UNUSED,		 
	SMC_LNK_INACTIVE,	 
	SMC_LNK_ACTIVATING,	 
	SMC_LNK_ACTIVE,		 
};

#define SMC_WR_BUF_SIZE		48	 
#define SMC_WR_BUF_V2_SIZE	8192	 

struct smc_wr_buf {
	u8	raw[SMC_WR_BUF_SIZE];
};

struct smc_wr_v2_buf {
	u8	raw[SMC_WR_BUF_V2_SIZE];
};

#define SMC_WR_REG_MR_WAIT_TIME	(5 * HZ) 

enum smc_wr_reg_state {
	POSTED,		 
	CONFIRMED,	 
	FAILED		 
};

struct smc_rdma_sge {				 
	struct ib_sge		wr_tx_rdma_sge[SMC_IB_MAX_SEND_SGE];
};

#define SMC_MAX_RDMA_WRITES	2		 

struct smc_rdma_sges {				 
	struct smc_rdma_sge	tx_rdma_sge[SMC_MAX_RDMA_WRITES];
};

struct smc_rdma_wr {				 
	struct ib_rdma_wr	wr_tx_rdma[SMC_MAX_RDMA_WRITES];
};

#define SMC_LGR_ID_SIZE		4

struct smc_link {
	struct smc_ib_device	*smcibdev;	 
	u8			ibport;		 
	struct ib_pd		*roce_pd;	 
	struct ib_qp		*roce_qp;	 
	struct ib_qp_attr	qp_attr;	 

	struct smc_wr_buf	*wr_tx_bufs;	 
	struct ib_send_wr	*wr_tx_ibs;	 
	struct ib_sge		*wr_tx_sges;	 
	struct smc_rdma_sges	*wr_tx_rdma_sges; 
	struct smc_rdma_wr	*wr_tx_rdmas;	 
	struct smc_wr_tx_pend	*wr_tx_pends;	 
	struct completion	*wr_tx_compl;	 
	 
	struct ib_send_wr	*wr_tx_v2_ib;	 
	struct ib_sge		*wr_tx_v2_sge;	 
	struct smc_wr_tx_pend	*wr_tx_v2_pend;	 
	dma_addr_t		wr_tx_dma_addr;	 
	dma_addr_t		wr_tx_v2_dma_addr;  
	atomic_long_t		wr_tx_id;	 
	unsigned long		*wr_tx_mask;	 
	u32			wr_tx_cnt;	 
	wait_queue_head_t	wr_tx_wait;	 
	struct {
		struct percpu_ref	wr_tx_refs;
	} ____cacheline_aligned_in_smp;
	struct completion	tx_ref_comp;

	struct smc_wr_buf	*wr_rx_bufs;	 
	struct ib_recv_wr	*wr_rx_ibs;	 
	struct ib_sge		*wr_rx_sges;	 
	 
	dma_addr_t		wr_rx_dma_addr;	 
	dma_addr_t		wr_rx_v2_dma_addr;  
	u64			wr_rx_id;	 
	u64			wr_rx_id_compl;  
	u32			wr_rx_cnt;	 
	unsigned long		wr_rx_tstamp;	 
	wait_queue_head_t       wr_rx_empty_wait;  

	struct ib_reg_wr	wr_reg;		 
	wait_queue_head_t	wr_reg_wait;	 
	struct {
		struct percpu_ref	wr_reg_refs;
	} ____cacheline_aligned_in_smp;
	struct completion	reg_ref_comp;
	enum smc_wr_reg_state	wr_reg_state;	 

	u8			gid[SMC_GID_SIZE]; 
	u8			sgid_index;	 
	u32			peer_qpn;	 
	enum ib_mtu		path_mtu;	 
	enum ib_mtu		peer_mtu;	 
	u32			psn_initial;	 
	u32			peer_psn;	 
	u8			peer_mac[ETH_ALEN];	 
	u8			peer_gid[SMC_GID_SIZE];	 
	u8			link_id;	 
	u8			link_uid[SMC_LGR_ID_SIZE];  
	u8			peer_link_uid[SMC_LGR_ID_SIZE];  
	u8			link_idx;	 
	u8			link_is_asym;	 
	u8			clearing : 1;	 
	refcount_t		refcnt;		 
	struct smc_link_group	*lgr;		 
	struct work_struct	link_down_wrk;	 
	char			ibname[IB_DEVICE_NAME_MAX];  
	int			ndev_ifidx;  

	enum smc_link_state	state;		 
	struct delayed_work	llc_testlink_wrk;  
	struct completion	llc_testlink_resp;  
	int			llc_testlink_time;  
	atomic_t		conn_cnt;  
};

 
#define SMC_LINKS_PER_LGR_MAX	3
#define SMC_SINGLE_LINK		0
#define SMC_LINKS_ADD_LNK_MIN	1	 
#define SMC_LINKS_ADD_LNK_MAX	2	 
#define SMC_LINKS_PER_LGR_MAX_PREFER	2	 

 
struct smc_buf_desc {
	struct list_head	list;
	void			*cpu_addr;	 
	struct page		*pages;
	int			len;		 
	u32			used;		 
	union {
		struct {  
			struct sg_table	sgt[SMC_LINKS_PER_LGR_MAX];
					 
			struct ib_mr	*mr[SMC_LINKS_PER_LGR_MAX];
					 
			u32		order;	 

			u8		is_conf_rkey;
					 
			u8		is_reg_mr[SMC_LINKS_PER_LGR_MAX];
					 
			u8		is_map_ib[SMC_LINKS_PER_LGR_MAX];
					 
			u8		is_dma_need_sync;
			u8		is_reg_err;
					 
			u8		is_vm;
					 
		};
		struct {  
			unsigned short	sba_idx;
					 
			u64		token;
					 
			dma_addr_t	dma_addr;
					 
		};
	};
};

struct smc_rtoken {				 
	u64			dma_addr;
	u32			rkey;
};

#define SMC_BUF_MIN_SIZE	16384	 
#define SMC_RMBE_SIZES		16	 
 

struct smcd_dev;

enum smc_lgr_type {				 
	SMC_LGR_NONE,			 
	SMC_LGR_SINGLE,			 
	SMC_LGR_SYMMETRIC,		 
	SMC_LGR_ASYMMETRIC_PEER,	 
	SMC_LGR_ASYMMETRIC_LOCAL,	 
};

enum smcr_buf_type {		 
	SMCR_PHYS_CONT_BUFS	= 0,
	SMCR_VIRT_CONT_BUFS	= 1,
	SMCR_MIXED_BUFS		= 2,
};

enum smc_llc_flowtype {
	SMC_LLC_FLOW_NONE	= 0,
	SMC_LLC_FLOW_ADD_LINK	= 2,
	SMC_LLC_FLOW_DEL_LINK	= 4,
	SMC_LLC_FLOW_REQ_ADD_LINK = 5,
	SMC_LLC_FLOW_RKEY	= 6,
};

struct smc_llc_qentry;

struct smc_llc_flow {
	enum smc_llc_flowtype type;
	struct smc_llc_qentry *qentry;
};

struct smc_link_group {
	struct list_head	list;
	struct rb_root		conns_all;	 
	rwlock_t		conns_lock;	 
	unsigned int		conns_num;	 
	unsigned short		vlan_id;	 

	struct list_head	sndbufs[SMC_RMBE_SIZES]; 
	struct rw_semaphore	sndbufs_lock;	 
	struct list_head	rmbs[SMC_RMBE_SIZES];	 
	struct rw_semaphore	rmbs_lock;	 

	u8			id[SMC_LGR_ID_SIZE];	 
	struct delayed_work	free_work;	 
	struct work_struct	terminate_work;	 
	struct workqueue_struct	*tx_wq;		 
	u8			sync_err : 1;	 
	u8			terminating : 1; 
	u8			freeing : 1;	 

	refcount_t		refcnt;		 
	bool			is_smcd;	 
	u8			smc_version;
	u8			negotiated_eid[SMC_MAX_EID_LEN];
	u8			peer_os;	 
	u8			peer_smc_release;
	u8			peer_hostname[SMC_MAX_HOSTNAME_LEN];
	union {
		struct {  
			enum smc_lgr_role	role;
						 
			struct smc_link		lnk[SMC_LINKS_PER_LGR_MAX];
						 
			struct smc_wr_v2_buf	*wr_rx_buf_v2;
						 
			struct smc_wr_v2_buf	*wr_tx_buf_v2;
						 
			char			peer_systemid[SMC_SYSTEMID_LEN];
						 
			struct smc_rtoken	rtokens[SMC_RMBS_PER_LGR_MAX]
						[SMC_LINKS_PER_LGR_MAX];
						 
			DECLARE_BITMAP(rtokens_used_mask, SMC_RMBS_PER_LGR_MAX);
						 
			u8			next_link_id;
			enum smc_lgr_type	type;
			enum smcr_buf_type	buf_type;
						 
			u8			pnet_id[SMC_MAX_PNETID_LEN + 1];
						 
			struct list_head	llc_event_q;
						 
			spinlock_t		llc_event_q_lock;
						 
			struct rw_semaphore	llc_conf_mutex;
						 
			struct work_struct	llc_add_link_work;
			struct work_struct	llc_del_link_work;
			struct work_struct	llc_event_work;
						 
			wait_queue_head_t	llc_flow_waiter;
						 
			wait_queue_head_t	llc_msg_waiter;
						 
			struct smc_llc_flow	llc_flow_lcl;
						 
			struct smc_llc_flow	llc_flow_rmt;
						 
			struct smc_llc_qentry	*delayed_event;
						 
			spinlock_t		llc_flow_lock;
						 
			int			llc_testlink_time;
						 
			u32			llc_termination_rsn;
						 
			u8			nexthop_mac[ETH_ALEN];
			u8			uses_gateway;
			__be32			saddr;
						 
			struct net		*net;
			u8			max_conns;
						 
			u8			max_links;
						 
		};
		struct {  
			u64			peer_gid;
						 
			struct smcd_dev		*smcd;
						 
			u8			peer_shutdown : 1;
						 
		};
	};
};

struct smc_clc_msg_local;

#define GID_LIST_SIZE	2

struct smc_gidlist {
	u8			len;
	u8			list[GID_LIST_SIZE][SMC_GID_SIZE];
};

struct smc_init_info_smcrv2 {
	 
	__be32			saddr;
	struct sock		*clc_sk;
	__be32			daddr;

	 
	struct smc_ib_device	*ib_dev_v2;
	u8			ib_port_v2;
	u8			ib_gid_v2[SMC_GID_SIZE];

	 
	u8			uses_gateway;
	u8			nexthop_mac[ETH_ALEN];

	struct smc_gidlist	gidlist;
};

struct smc_init_info {
	u8			is_smcd;
	u8			smc_type_v1;
	u8			smc_type_v2;
	u8			release_nr;
	u8			max_conns;
	u8			max_links;
	u8			first_contact_peer;
	u8			first_contact_local;
	unsigned short		vlan_id;
	u32			rc;
	u8			negotiated_eid[SMC_MAX_EID_LEN];
	 
	u8			smcr_version;
	u8			check_smcrv2;
	u8			peer_gid[SMC_GID_SIZE];
	u8			peer_mac[ETH_ALEN];
	u8			peer_systemid[SMC_SYSTEMID_LEN];
	struct smc_ib_device	*ib_dev;
	u8			ib_gid[SMC_GID_SIZE];
	u8			ib_port;
	u32			ib_clcqpn;
	struct smc_init_info_smcrv2 smcrv2;
	 
	u64			ism_peer_gid[SMC_MAX_ISM_DEVS + 1];
	struct smcd_dev		*ism_dev[SMC_MAX_ISM_DEVS + 1];
	u16			ism_chid[SMC_MAX_ISM_DEVS + 1];
	u8			ism_offered_cnt;  
	u8			ism_selected;     
	u8			smcd_version;
};

 
static inline struct smc_connection *smc_lgr_find_conn(
	u32 token, struct smc_link_group *lgr)
{
	struct smc_connection *res = NULL;
	struct rb_node *node;

	node = lgr->conns_all.rb_node;
	while (node) {
		struct smc_connection *cur = rb_entry(node,
					struct smc_connection, alert_node);

		if (cur->alert_token_local > token) {
			node = node->rb_left;
		} else {
			if (cur->alert_token_local < token) {
				node = node->rb_right;
			} else {
				res = cur;
				break;
			}
		}
	}

	return res;
}

static inline bool smc_conn_lgr_valid(struct smc_connection *conn)
{
	return conn->lgr && conn->alert_token_local;
}

 
static inline bool smc_link_usable(struct smc_link *lnk)
{
	if (lnk->state == SMC_LNK_UNUSED || lnk->state == SMC_LNK_INACTIVE)
		return false;
	return true;
}

 
static inline bool smc_link_sendable(struct smc_link *lnk)
{
	return smc_link_usable(lnk) &&
		lnk->qp_attr.cur_qp_state == IB_QPS_RTS;
}

static inline bool smc_link_active(struct smc_link *lnk)
{
	return lnk->state == SMC_LNK_ACTIVE;
}

static inline void smc_gid_be16_convert(__u8 *buf, u8 *gid_raw)
{
	sprintf(buf, "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
		be16_to_cpu(((__be16 *)gid_raw)[0]),
		be16_to_cpu(((__be16 *)gid_raw)[1]),
		be16_to_cpu(((__be16 *)gid_raw)[2]),
		be16_to_cpu(((__be16 *)gid_raw)[3]),
		be16_to_cpu(((__be16 *)gid_raw)[4]),
		be16_to_cpu(((__be16 *)gid_raw)[5]),
		be16_to_cpu(((__be16 *)gid_raw)[6]),
		be16_to_cpu(((__be16 *)gid_raw)[7]));
}

struct smc_pci_dev {
	__u32		pci_fid;
	__u16		pci_pchid;
	__u16		pci_vendor;
	__u16		pci_device;
	__u8		pci_id[SMC_PCI_ID_STR_LEN];
};

static inline void smc_set_pci_values(struct pci_dev *pci_dev,
				      struct smc_pci_dev *smc_dev)
{
	smc_dev->pci_vendor = pci_dev->vendor;
	smc_dev->pci_device = pci_dev->device;
	snprintf(smc_dev->pci_id, sizeof(smc_dev->pci_id), "%s",
		 pci_name(pci_dev));
#if IS_ENABLED(CONFIG_S390)
	{  
	struct zpci_dev *zdev;

	zdev = to_zpci(pci_dev);
	smc_dev->pci_fid = zdev->fid;
	smc_dev->pci_pchid = zdev->pchid;
	}
#endif
}

struct smc_sock;
struct smc_clc_msg_accept_confirm;

void smc_lgr_cleanup_early(struct smc_link_group *lgr);
void smc_lgr_terminate_sched(struct smc_link_group *lgr);
void smc_lgr_hold(struct smc_link_group *lgr);
void smc_lgr_put(struct smc_link_group *lgr);
void smcr_port_add(struct smc_ib_device *smcibdev, u8 ibport);
void smcr_port_err(struct smc_ib_device *smcibdev, u8 ibport);
void smc_smcd_terminate(struct smcd_dev *dev, u64 peer_gid,
			unsigned short vlan);
void smc_smcd_terminate_all(struct smcd_dev *dev);
void smc_smcr_terminate_all(struct smc_ib_device *smcibdev);
int smc_buf_create(struct smc_sock *smc, bool is_smcd);
int smc_uncompress_bufsize(u8 compressed);
int smc_rmb_rtoken_handling(struct smc_connection *conn, struct smc_link *link,
			    struct smc_clc_msg_accept_confirm *clc);
int smc_rtoken_add(struct smc_link *lnk, __be64 nw_vaddr, __be32 nw_rkey);
int smc_rtoken_delete(struct smc_link *lnk, __be32 nw_rkey);
void smc_rtoken_set(struct smc_link_group *lgr, int link_idx, int link_idx_new,
		    __be32 nw_rkey_known, __be64 nw_vaddr, __be32 nw_rkey);
void smc_rtoken_set2(struct smc_link_group *lgr, int rtok_idx, int link_id,
		     __be64 nw_vaddr, __be32 nw_rkey);
void smc_sndbuf_sync_sg_for_device(struct smc_connection *conn);
void smc_rmb_sync_sg_for_cpu(struct smc_connection *conn);
int smc_vlan_by_tcpsk(struct socket *clcsock, struct smc_init_info *ini);

void smc_conn_free(struct smc_connection *conn);
int smc_conn_create(struct smc_sock *smc, struct smc_init_info *ini);
int smc_core_init(void);
void smc_core_exit(void);

int smcr_link_init(struct smc_link_group *lgr, struct smc_link *lnk,
		   u8 link_idx, struct smc_init_info *ini);
void smcr_link_clear(struct smc_link *lnk, bool log);
void smcr_link_hold(struct smc_link *lnk);
void smcr_link_put(struct smc_link *lnk);
void smc_switch_link_and_count(struct smc_connection *conn,
			       struct smc_link *to_lnk);
int smcr_buf_map_lgr(struct smc_link *lnk);
int smcr_buf_reg_lgr(struct smc_link *lnk);
void smcr_lgr_set_type(struct smc_link_group *lgr, enum smc_lgr_type new_type);
void smcr_lgr_set_type_asym(struct smc_link_group *lgr,
			    enum smc_lgr_type new_type, int asym_lnk_idx);
int smcr_link_reg_buf(struct smc_link *link, struct smc_buf_desc *rmb_desc);
struct smc_link *smc_switch_conns(struct smc_link_group *lgr,
				  struct smc_link *from_lnk, bool is_dev_err);
void smcr_link_down_cond(struct smc_link *lnk);
void smcr_link_down_cond_sched(struct smc_link *lnk);
int smc_nl_get_sys_info(struct sk_buff *skb, struct netlink_callback *cb);
int smcr_nl_get_lgr(struct sk_buff *skb, struct netlink_callback *cb);
int smcr_nl_get_link(struct sk_buff *skb, struct netlink_callback *cb);
int smcd_nl_get_lgr(struct sk_buff *skb, struct netlink_callback *cb);

static inline struct smc_link_group *smc_get_lgr(struct smc_link *link)
{
	return link->lgr;
}
#endif
