#ifndef _OPA_VNIC_ENCAP_H
#define _OPA_VNIC_ENCAP_H
 

 

#include <linux/types.h>
#include <rdma/ib_mad.h>

 
#define OPA_EMA_CLASS_VERSION               0x80

 
#define OPA_MGMT_CLASS_INTEL_EMA            0x34

 
#define OPA_EM_ATTR_CLASS_PORT_INFO                 0x0001
#define OPA_EM_ATTR_VESWPORT_INFO                   0x0011
#define OPA_EM_ATTR_VESWPORT_MAC_ENTRIES            0x0012
#define OPA_EM_ATTR_IFACE_UCAST_MACS                0x0013
#define OPA_EM_ATTR_IFACE_MCAST_MACS                0x0014
#define OPA_EM_ATTR_DELETE_VESW                     0x0015
#define OPA_EM_ATTR_VESWPORT_SUMMARY_COUNTERS       0x0020
#define OPA_EM_ATTR_VESWPORT_ERROR_COUNTERS         0x0022

 
#define OPA_VNIC_STATE_DROP_ALL        0x1
#define OPA_VNIC_STATE_FORWARDING      0x3

#define OPA_VESW_MAX_NUM_DEF_PORT   16
#define OPA_VNIC_MAX_NUM_PCP        8

#define OPA_VNIC_EMA_DATA    (OPA_MGMT_MAD_SIZE - IB_MGMT_VENDOR_HDR)

 
#define OPA_INTEL_EMA_NOTICE_TYPE_INFO 0x04

 
#define INTEL_OUI_1 0x00
#define INTEL_OUI_2 0x06
#define INTEL_OUI_3 0x6a

 
#define OPA_VESWPORT_TRAP_IFACE_UCAST_MAC_CHANGE 0x1
#define OPA_VESWPORT_TRAP_IFACE_MCAST_MAC_CHANGE 0x2
#define OPA_VESWPORT_TRAP_ETH_LINK_STATUS_CHANGE 0x3

#define OPA_VNIC_DLID_SD_IS_SRC_MAC(dlid_sd)  (!!((dlid_sd) & 0x20))
#define OPA_VNIC_DLID_SD_GET_DLID(dlid_sd)    ((dlid_sd) >> 8)

 
#define OPA_VNIC_ETH_LINK_UP     1
#define OPA_VNIC_ETH_LINK_DOWN   2

 
#define OPA_VNIC_ENCAP_RC_DEFAULT   0
#define OPA_VNIC_ENCAP_RC_IPV4      4
#define OPA_VNIC_ENCAP_RC_IPV4_UDP  8
#define OPA_VNIC_ENCAP_RC_IPV4_TCP  12
#define OPA_VNIC_ENCAP_RC_IPV6      16
#define OPA_VNIC_ENCAP_RC_IPV6_TCP  20
#define OPA_VNIC_ENCAP_RC_IPV6_UDP  24

#define OPA_VNIC_ENCAP_RC_EXT(w, b) (((w) >> OPA_VNIC_ENCAP_RC_ ## b) & 0x7)

 
struct opa_vesw_info {
	__be16  fabric_id;
	__be16  vesw_id;

	u8      rsvd0[6];
	__be16  def_port_mask;

	u8      rsvd1[2];
	__be16  pkey;

	u8      rsvd2[4];
	__be32  u_mcast_dlid;
	__be32  u_ucast_dlid[OPA_VESW_MAX_NUM_DEF_PORT];

	__be32  rc;

	u8      rsvd3[56];
	__be16  eth_mtu;
	u8      rsvd4[2];
} __packed;

 
struct opa_per_veswport_info {
	__be32  port_num;

	u8      eth_link_status;
	u8      rsvd0[3];

	u8      base_mac_addr[ETH_ALEN];
	u8      config_state;
	u8      oper_state;

	__be16  max_mac_tbl_ent;
	__be16  max_smac_ent;
	__be32  mac_tbl_digest;
	u8      rsvd1[4];

	__be32  encap_slid;

	u8      pcp_to_sc_uc[OPA_VNIC_MAX_NUM_PCP];
	u8      pcp_to_vl_uc[OPA_VNIC_MAX_NUM_PCP];
	u8      pcp_to_sc_mc[OPA_VNIC_MAX_NUM_PCP];
	u8      pcp_to_vl_mc[OPA_VNIC_MAX_NUM_PCP];

	u8      non_vlan_sc_uc;
	u8      non_vlan_vl_uc;
	u8      non_vlan_sc_mc;
	u8      non_vlan_vl_mc;

	u8      rsvd2[48];

	__be16  uc_macs_gen_count;
	__be16  mc_macs_gen_count;

	u8      rsvd3[8];
} __packed;

 
struct opa_veswport_info {
	struct opa_vesw_info          vesw;
	struct opa_per_veswport_info  vport;
};

 
struct opa_veswport_mactable_entry {
	u8      mac_addr[ETH_ALEN];
	u8      mac_addr_mask[ETH_ALEN];
	__be32  dlid_sd;
} __packed;

 
struct opa_veswport_mactable {
	__be16                              offset;
	__be16                              num_entries;
	__be32                              mac_tbl_digest;
	struct opa_veswport_mactable_entry  tbl_entries[];
} __packed;

 
struct opa_veswport_summary_counters {
	__be16  vp_instance;
	__be16  vesw_id;
	__be32  veswport_num;

	__be64  tx_errors;
	__be64  rx_errors;
	__be64  tx_packets;
	__be64  rx_packets;
	__be64  tx_bytes;
	__be64  rx_bytes;

	__be64  tx_unicast;
	__be64  tx_mcastbcast;

	__be64  tx_untagged;
	__be64  tx_vlan;

	__be64  tx_64_size;
	__be64  tx_65_127;
	__be64  tx_128_255;
	__be64  tx_256_511;
	__be64  tx_512_1023;
	__be64  tx_1024_1518;
	__be64  tx_1519_max;

	__be64  rx_unicast;
	__be64  rx_mcastbcast;

	__be64  rx_untagged;
	__be64  rx_vlan;

	__be64  rx_64_size;
	__be64  rx_65_127;
	__be64  rx_128_255;
	__be64  rx_256_511;
	__be64  rx_512_1023;
	__be64  rx_1024_1518;
	__be64  rx_1519_max;

	__be64  reserved[16];
} __packed;

 
struct opa_veswport_error_counters {
	__be16  vp_instance;
	__be16  vesw_id;
	__be32  veswport_num;

	__be64  tx_errors;
	__be64  rx_errors;

	__be64  rsvd0;
	__be64  tx_smac_filt;
	__be64  rsvd1;
	__be64  rsvd2;
	__be64  rsvd3;
	__be64  tx_dlid_zero;
	__be64  rsvd4;
	__be64  tx_logic;
	__be64  rsvd5;
	__be64  tx_drop_state;

	__be64  rx_bad_veswid;
	__be64  rsvd6;
	__be64  rx_runt;
	__be64  rx_oversize;
	__be64  rsvd7;
	__be64  rx_eth_down;
	__be64  rx_drop_state;
	__be64  rx_logic;
	__be64  rsvd8;

	__be64  rsvd9[16];
} __packed;

 
struct opa_veswport_trap {
	__be16  fabric_id;
	__be16  veswid;
	__be32  veswportnum;
	__be16  opaportnum;
	u8      veswportindex;
	u8      opcode;
	__be32  reserved;
} __packed;

 
struct opa_vnic_iface_mac_entry {
	u8 mac_addr[ETH_ALEN];
};

 
struct opa_veswport_iface_macs {
	__be16 start_idx;
	__be16 num_macs_in_msg;
	__be16 tot_macs_in_lst;
	__be16 gen_count;
	struct opa_vnic_iface_mac_entry entry[];
} __packed;

 
struct opa_vnic_vema_mad {
	struct ib_mad_hdr  mad_hdr;
	struct ib_rmpp_hdr rmpp_hdr;
	u8                 reserved;
	u8                 oui[3];
	u8                 data[OPA_VNIC_EMA_DATA];
};

 
struct opa_vnic_notice_attr {
	u8     gen_type;
	u8     oui_1;
	u8     oui_2;
	u8     oui_3;
	__be16 trap_num;
	__be16 toggle_count;
	__be32 issuer_lid;
	__be32 reserved;
	u8     issuer_gid[16];
	u8     raw_data[64];
} __packed;

 
struct opa_vnic_vema_mad_trap {
	struct ib_mad_hdr            mad_hdr;
	struct ib_rmpp_hdr           rmpp_hdr;
	u8                           reserved;
	u8                           oui[3];
	struct opa_vnic_notice_attr  notice;
};

#endif  
