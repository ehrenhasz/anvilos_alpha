 
 

#ifndef _SMC_IB_H
#define _SMC_IB_H

#include <linux/interrupt.h>
#include <linux/if_ether.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <rdma/ib_verbs.h>
#include <net/smc.h>

#define SMC_MAX_PORTS			2	 
#define SMC_GID_SIZE			sizeof(union ib_gid)

#define SMC_IB_MAX_SEND_SGE		2

struct smc_ib_devices {			 
	struct list_head	list;
	struct mutex		mutex;	 
};

extern struct smc_ib_devices	smc_ib_devices;  
extern struct smc_lgr_list smc_lgr_list;  

struct smc_ib_device {				 
	struct list_head	list;
	struct ib_device	*ibdev;
	struct ib_port_attr	pattr[SMC_MAX_PORTS];	 
	struct ib_event_handler	event_handler;	 
	struct ib_cq		*roce_cq_send;	 
	struct ib_cq		*roce_cq_recv;	 
	struct tasklet_struct	send_tasklet;	 
	struct tasklet_struct	recv_tasklet;	 
	char			mac[SMC_MAX_PORTS][ETH_ALEN];
						 
	u8			pnetid[SMC_MAX_PORTS][SMC_MAX_PNETID_LEN];
						 
	bool			pnetid_by_user[SMC_MAX_PORTS];
						 
	u8			initialized : 1;  
	struct work_struct	port_event_work;
	unsigned long		port_event_mask;
	DECLARE_BITMAP(ports_going_away, SMC_MAX_PORTS);
	atomic_t		lnk_cnt;	 
	wait_queue_head_t	lnks_deleted;	 
	struct mutex		mutex;		 
	atomic_t		lnk_cnt_by_port[SMC_MAX_PORTS];
						 
	int			ndev_ifidx[SMC_MAX_PORTS];  
};

static inline __be32 smc_ib_gid_to_ipv4(u8 gid[SMC_GID_SIZE])
{
	struct in6_addr *addr6 = (struct in6_addr *)gid;

	if (ipv6_addr_v4mapped(addr6) ||
	    !(addr6->s6_addr32[0] | addr6->s6_addr32[1] | addr6->s6_addr32[2]))
		return addr6->s6_addr32[3];
	return cpu_to_be32(INADDR_NONE);
}

static inline struct net *smc_ib_net(struct smc_ib_device *smcibdev)
{
	if (smcibdev && smcibdev->ibdev)
		return read_pnet(&smcibdev->ibdev->coredev.rdma_net);
	return NULL;
}

struct smc_init_info_smcrv2;
struct smc_buf_desc;
struct smc_link;

void smc_ib_ndev_change(struct net_device *ndev, unsigned long event);
int smc_ib_register_client(void) __init;
void smc_ib_unregister_client(void);
bool smc_ib_port_active(struct smc_ib_device *smcibdev, u8 ibport);
int smc_ib_buf_map_sg(struct smc_link *lnk,
		      struct smc_buf_desc *buf_slot,
		      enum dma_data_direction data_direction);
void smc_ib_buf_unmap_sg(struct smc_link *lnk,
			 struct smc_buf_desc *buf_slot,
			 enum dma_data_direction data_direction);
void smc_ib_dealloc_protection_domain(struct smc_link *lnk);
int smc_ib_create_protection_domain(struct smc_link *lnk);
void smc_ib_destroy_queue_pair(struct smc_link *lnk);
int smc_ib_create_queue_pair(struct smc_link *lnk);
int smc_ib_ready_link(struct smc_link *lnk);
int smc_ib_modify_qp_rts(struct smc_link *lnk);
int smc_ib_modify_qp_error(struct smc_link *lnk);
long smc_ib_setup_per_ibdev(struct smc_ib_device *smcibdev);
int smc_ib_get_memory_region(struct ib_pd *pd, int access_flags,
			     struct smc_buf_desc *buf_slot, u8 link_idx);
void smc_ib_put_memory_region(struct ib_mr *mr);
bool smc_ib_is_sg_need_sync(struct smc_link *lnk,
			    struct smc_buf_desc *buf_slot);
void smc_ib_sync_sg_for_cpu(struct smc_link *lnk,
			    struct smc_buf_desc *buf_slot,
			    enum dma_data_direction data_direction);
void smc_ib_sync_sg_for_device(struct smc_link *lnk,
			       struct smc_buf_desc *buf_slot,
			       enum dma_data_direction data_direction);
int smc_ib_determine_gid(struct smc_ib_device *smcibdev, u8 ibport,
			 unsigned short vlan_id, u8 gid[], u8 *sgid_index,
			 struct smc_init_info_smcrv2 *smcrv2);
int smc_ib_find_route(struct net *net, __be32 saddr, __be32 daddr,
		      u8 nexthop_mac[], u8 *uses_gateway);
bool smc_ib_is_valid_local_systemid(void);
int smcr_nl_get_device(struct sk_buff *skb, struct netlink_callback *cb);
#endif
