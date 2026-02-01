#ifndef _OPA_VNIC_INTERNAL_H
#define _OPA_VNIC_INTERNAL_H
 

 

#include <linux/bitops.h>
#include <linux/etherdevice.h>
#include <linux/hashtable.h>
#include <linux/sizes.h>
#include <rdma/opa_vnic.h>

#include "opa_vnic_encap.h"

#define OPA_VNIC_VLAN_PCP(vlan_tci)  \
			(((vlan_tci) & VLAN_PRIO_MASK) >> VLAN_PRIO_SHIFT)

 
#define OPA_VNIC_FLOW_TBL_SIZE    32

 
#define OPA_VNIC_INVALID_PORT     0xff

struct opa_vnic_adapter;

 
struct __opa_vesw_info {
	u16  fabric_id;
	u16  vesw_id;

	u8   rsvd0[6];
	u16  def_port_mask;

	u8   rsvd1[2];
	u16  pkey;

	u8   rsvd2[4];
	u32  u_mcast_dlid;
	u32  u_ucast_dlid[OPA_VESW_MAX_NUM_DEF_PORT];

	u32  rc;

	u8   rsvd3[56];
	u16  eth_mtu;
	u8   rsvd4[2];
} __packed;

 
struct __opa_per_veswport_info {
	u32  port_num;

	u8   eth_link_status;
	u8   rsvd0[3];

	u8   base_mac_addr[ETH_ALEN];
	u8   config_state;
	u8   oper_state;

	u16  max_mac_tbl_ent;
	u16  max_smac_ent;
	u32  mac_tbl_digest;
	u8   rsvd1[4];

	u32  encap_slid;

	u8   pcp_to_sc_uc[OPA_VNIC_MAX_NUM_PCP];
	u8   pcp_to_vl_uc[OPA_VNIC_MAX_NUM_PCP];
	u8   pcp_to_sc_mc[OPA_VNIC_MAX_NUM_PCP];
	u8   pcp_to_vl_mc[OPA_VNIC_MAX_NUM_PCP];

	u8   non_vlan_sc_uc;
	u8   non_vlan_vl_uc;
	u8   non_vlan_sc_mc;
	u8   non_vlan_vl_mc;

	u8   rsvd2[48];

	u16  uc_macs_gen_count;
	u16  mc_macs_gen_count;

	u8   rsvd3[8];
} __packed;

 
struct __opa_veswport_info {
	struct __opa_vesw_info            vesw;
	struct __opa_per_veswport_info    vport;
};

 
struct __opa_veswport_trap {
	u16	fabric_id;
	u16	veswid;
	u32	veswportnum;
	u16	opaportnum;
	u8	veswportindex;
	u8	opcode;
	u32	reserved;
} __packed;

 
struct opa_vnic_ctrl_port {
	struct ib_device           *ibdev;
	struct opa_vnic_ctrl_ops   *ops;
	u8                          num_ports;
};

 
struct opa_vnic_adapter {
	struct net_device             *netdev;
	struct ib_device              *ibdev;
	struct opa_vnic_ctrl_port     *cport;
	const struct net_device_ops   *rn_ops;

	u8 port_num;
	u8 vport_num;

	 
	struct mutex lock;

	struct __opa_veswport_info  info;
	u8                          vema_mac_addr[ETH_ALEN];
	u32                         umac_hash;
	u32                         mmac_hash;
	struct hlist_head  __rcu   *mactbl;

	 
	struct mutex mactbl_lock;

	 
	spinlock_t stats_lock;

	u8 flow_tbl[OPA_VNIC_FLOW_TBL_SIZE];

	unsigned long trap_timeout;
	u8            trap_count;
};

 
struct __opa_vnic_mactable_entry {
	u8  mac_addr[ETH_ALEN];
	u8  mac_addr_mask[ETH_ALEN];
	u32 dlid_sd;
} __packed;

 
struct opa_vnic_mac_tbl_node {
	struct hlist_node                    hlist;
	u16                                  index;
	struct __opa_vnic_mactable_entry     entry;
};

#define v_dbg(format, arg...) \
	netdev_dbg(adapter->netdev, format, ## arg)
#define v_err(format, arg...) \
	netdev_err(adapter->netdev, format, ## arg)
#define v_info(format, arg...) \
	netdev_info(adapter->netdev, format, ## arg)
#define v_warn(format, arg...) \
	netdev_warn(adapter->netdev, format, ## arg)

#define c_err(format, arg...) \
	dev_err(&cport->ibdev->dev, format, ## arg)
#define c_info(format, arg...) \
	dev_info(&cport->ibdev->dev, format, ## arg)
#define c_dbg(format, arg...) \
	dev_dbg(&cport->ibdev->dev, format, ## arg)

 
#define OPA_VNIC_MAC_TBL_MAX_ENTRIES  2048
 
#define OPA_VNIC_MAX_SMAC_LIMIT       256

 
#define OPA_VNIC_MAC_HASH_IDX         5

 
#define OPA_VNIC_MAC_TBL_HASH_BITS    8
#define OPA_VNIC_MAC_TBL_SIZE  BIT(OPA_VNIC_MAC_TBL_HASH_BITS)

 
#define vnic_hash_init(hashtable) __hash_init(hashtable, OPA_VNIC_MAC_TBL_SIZE)

#define vnic_hash_add(hashtable, node, key)                                   \
	hlist_add_head(node,                                                  \
		&hashtable[hash_min(key, ilog2(OPA_VNIC_MAC_TBL_SIZE))])

#define vnic_hash_for_each_safe(name, bkt, tmp, obj, member)                  \
	for ((bkt) = 0, obj = NULL;                                           \
		    !obj && (bkt) < OPA_VNIC_MAC_TBL_SIZE; (bkt)++)           \
		hlist_for_each_entry_safe(obj, tmp, &name[bkt], member)

#define vnic_hash_for_each_possible(name, obj, member, key)                   \
	hlist_for_each_entry(obj,                                             \
		&name[hash_min(key, ilog2(OPA_VNIC_MAC_TBL_SIZE))], member)

#define vnic_hash_for_each(name, bkt, obj, member)                            \
	for ((bkt) = 0, obj = NULL;                                           \
		    !obj && (bkt) < OPA_VNIC_MAC_TBL_SIZE; (bkt)++)           \
		hlist_for_each_entry(obj, &name[bkt], member)

extern char opa_vnic_driver_name[];

struct opa_vnic_adapter *opa_vnic_add_netdev(struct ib_device *ibdev,
					     u8 port_num, u8 vport_num);
void opa_vnic_rem_netdev(struct opa_vnic_adapter *adapter);
void opa_vnic_encap_skb(struct opa_vnic_adapter *adapter, struct sk_buff *skb);
u8 opa_vnic_get_vl(struct opa_vnic_adapter *adapter, struct sk_buff *skb);
u8 opa_vnic_calc_entropy(struct sk_buff *skb);
void opa_vnic_process_vema_config(struct opa_vnic_adapter *adapter);
void opa_vnic_release_mac_tbl(struct opa_vnic_adapter *adapter);
void opa_vnic_query_mac_tbl(struct opa_vnic_adapter *adapter,
			    struct opa_veswport_mactable *tbl);
int opa_vnic_update_mac_tbl(struct opa_vnic_adapter *adapter,
			    struct opa_veswport_mactable *tbl);
void opa_vnic_query_ucast_macs(struct opa_vnic_adapter *adapter,
			       struct opa_veswport_iface_macs *macs);
void opa_vnic_query_mcast_macs(struct opa_vnic_adapter *adapter,
			       struct opa_veswport_iface_macs *macs);
void opa_vnic_get_summary_counters(struct opa_vnic_adapter *adapter,
				   struct opa_veswport_summary_counters *cntrs);
void opa_vnic_get_error_counters(struct opa_vnic_adapter *adapter,
				 struct opa_veswport_error_counters *cntrs);
void opa_vnic_get_vesw_info(struct opa_vnic_adapter *adapter,
			    struct opa_vesw_info *info);
void opa_vnic_set_vesw_info(struct opa_vnic_adapter *adapter,
			    struct opa_vesw_info *info);
void opa_vnic_get_per_veswport_info(struct opa_vnic_adapter *adapter,
				    struct opa_per_veswport_info *info);
void opa_vnic_set_per_veswport_info(struct opa_vnic_adapter *adapter,
				    struct opa_per_veswport_info *info);
void opa_vnic_vema_report_event(struct opa_vnic_adapter *adapter, u8 event);
void opa_vnic_set_ethtool_ops(struct net_device *netdev);
void opa_vnic_vema_send_trap(struct opa_vnic_adapter *adapter,
			     struct __opa_veswport_trap *data, u32 lid);

#endif  
