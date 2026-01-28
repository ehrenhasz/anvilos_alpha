#ifndef NVM_ISCSI_CFG_H
#define NVM_ISCSI_CFG_H
#define NUM_OF_ISCSI_TARGET_PER_PF    4    
#define NUM_OF_ISCSI_PF_SUPPORTED     4    
#define NVM_ISCSI_CFG_DHCP_NAME_MAX_LEN  256
union nvm_iscsi_dhcp_vendor_id {
	u32 value[NVM_ISCSI_CFG_DHCP_NAME_MAX_LEN / 4];
	u8  byte[NVM_ISCSI_CFG_DHCP_NAME_MAX_LEN];
};
#define NVM_ISCSI_IPV4_ADDR_BYTE_LEN 4
union nvm_iscsi_ipv4_addr {
	u32 addr;
	u8  byte[NVM_ISCSI_IPV4_ADDR_BYTE_LEN];
};
#define NVM_ISCSI_IPV6_ADDR_BYTE_LEN 16
union nvm_iscsi_ipv6_addr {
	u32 addr[4];
	u8  byte[NVM_ISCSI_IPV6_ADDR_BYTE_LEN];
};
struct nvm_iscsi_initiator_ipv4 {
	union nvm_iscsi_ipv4_addr addr;				 
	union nvm_iscsi_ipv4_addr subnet_mask;			 
	union nvm_iscsi_ipv4_addr gateway;			 
	union nvm_iscsi_ipv4_addr primary_dns;			 
	union nvm_iscsi_ipv4_addr secondary_dns;		 
	union nvm_iscsi_ipv4_addr dhcp_addr;			 
	union nvm_iscsi_ipv4_addr isns_server;			 
	union nvm_iscsi_ipv4_addr slp_server;			 
	union nvm_iscsi_ipv4_addr primay_radius_server;		 
	union nvm_iscsi_ipv4_addr secondary_radius_server;	 
	union nvm_iscsi_ipv4_addr rsvd[4];			 
};
struct nvm_iscsi_initiator_ipv6 {
	union nvm_iscsi_ipv6_addr addr;				 
	union nvm_iscsi_ipv6_addr subnet_mask;			 
	union nvm_iscsi_ipv6_addr gateway;			 
	union nvm_iscsi_ipv6_addr primary_dns;			 
	union nvm_iscsi_ipv6_addr secondary_dns;		 
	union nvm_iscsi_ipv6_addr dhcp_addr;			 
	union nvm_iscsi_ipv6_addr isns_server;			 
	union nvm_iscsi_ipv6_addr slp_server;			 
	union nvm_iscsi_ipv6_addr primay_radius_server;		 
	union nvm_iscsi_ipv6_addr secondary_radius_server;	 
	union nvm_iscsi_ipv6_addr rsvd[3];			 
	u32   config;						 
#define NVM_ISCSI_CFG_INITIATOR_IPV6_SUBNET_MASK_PREFIX_MASK      0x000000FF
#define NVM_ISCSI_CFG_INITIATOR_IPV6_SUBNET_MASK_PREFIX_OFFSET    0
	u32   rsvd_1[3];
};
#define NVM_ISCSI_CFG_ISCSI_NAME_MAX_LEN  256
union nvm_iscsi_name {
	u32 value[NVM_ISCSI_CFG_ISCSI_NAME_MAX_LEN / 4];
	u8  byte[NVM_ISCSI_CFG_ISCSI_NAME_MAX_LEN];
};
#define NVM_ISCSI_CFG_CHAP_NAME_MAX_LEN  256
union nvm_iscsi_chap_name {
	u32 value[NVM_ISCSI_CFG_CHAP_NAME_MAX_LEN / 4];
	u8  byte[NVM_ISCSI_CFG_CHAP_NAME_MAX_LEN];
};
#define NVM_ISCSI_CFG_CHAP_PWD_MAX_LEN  16  
union nvm_iscsi_chap_password {
	u32 value[NVM_ISCSI_CFG_CHAP_PWD_MAX_LEN / 4];
	u8 byte[NVM_ISCSI_CFG_CHAP_PWD_MAX_LEN];
};
union nvm_iscsi_lun {
	u8  byte[8];
	u32 value[2];
};
struct nvm_iscsi_generic {
	u32 ctrl_flags;						 
#define NVM_ISCSI_CFG_GEN_CHAP_ENABLED                 BIT(0)
#define NVM_ISCSI_CFG_GEN_DHCP_TCPIP_CONFIG_ENABLED    BIT(1)
#define NVM_ISCSI_CFG_GEN_DHCP_ISCSI_CONFIG_ENABLED    BIT(2)
#define NVM_ISCSI_CFG_GEN_IPV6_ENABLED                 BIT(3)
#define NVM_ISCSI_CFG_GEN_IPV4_FALLBACK_ENABLED        BIT(4)
#define NVM_ISCSI_CFG_GEN_ISNS_WORLD_LOGIN             BIT(5)
#define NVM_ISCSI_CFG_GEN_ISNS_SELECTIVE_LOGIN         BIT(6)
#define NVM_ISCSI_CFG_GEN_ADDR_REDIRECT_ENABLED	       BIT(7)
#define NVM_ISCSI_CFG_GEN_CHAP_MUTUAL_ENABLED          BIT(8)
	u32 timeout;						 
#define NVM_ISCSI_CFG_GEN_DHCP_REQUEST_TIMEOUT_MASK       0x0000FFFF
#define NVM_ISCSI_CFG_GEN_DHCP_REQUEST_TIMEOUT_OFFSET     0
#define NVM_ISCSI_CFG_GEN_PORT_LOGIN_TIMEOUT_MASK         0xFFFF0000
#define NVM_ISCSI_CFG_GEN_PORT_LOGIN_TIMEOUT_OFFSET       16
	union nvm_iscsi_dhcp_vendor_id  dhcp_vendor_id;		 
	u32 rsvd[62];						 
};
struct nvm_iscsi_initiator {
	struct nvm_iscsi_initiator_ipv4 ipv4;			 
	struct nvm_iscsi_initiator_ipv6 ipv6;			 
	union nvm_iscsi_name           initiator_name;		 
	union nvm_iscsi_chap_name      chap_name;		 
	union nvm_iscsi_chap_password  chap_password;		 
	u32 generic_cont0;					 
#define NVM_ISCSI_CFG_INITIATOR_VLAN_MASK		0x0000FFFF
#define NVM_ISCSI_CFG_INITIATOR_VLAN_OFFSET		0
#define NVM_ISCSI_CFG_INITIATOR_IP_VERSION_MASK		0x00030000
#define NVM_ISCSI_CFG_INITIATOR_IP_VERSION_OFFSET	16
#define NVM_ISCSI_CFG_INITIATOR_IP_VERSION_4		1
#define NVM_ISCSI_CFG_INITIATOR_IP_VERSION_6		2
#define NVM_ISCSI_CFG_INITIATOR_IP_VERSION_4_AND_6	3
	u32 ctrl_flags;
#define NVM_ISCSI_CFG_INITIATOR_IP_VERSION_PRIORITY_V6     BIT(0)
#define NVM_ISCSI_CFG_INITIATOR_VLAN_ENABLED               BIT(1)
	u32 rsvd[116];						 
};
struct nvm_iscsi_target {
	u32 ctrl_flags;						 
#define NVM_ISCSI_CFG_TARGET_ENABLED            BIT(0)
#define NVM_ISCSI_CFG_BOOT_TIME_LOGIN_STATUS    BIT(1)
	u32 generic_cont0;					 
#define NVM_ISCSI_CFG_TARGET_TCP_PORT_MASK      0x0000FFFF
#define NVM_ISCSI_CFG_TARGET_TCP_PORT_OFFSET    0
	u32 ip_ver;
#define NVM_ISCSI_CFG_IPv4       4
#define NVM_ISCSI_CFG_IPv6       6
	u32 rsvd_1[7];						 
	union nvm_iscsi_ipv4_addr ipv4_addr;			 
	union nvm_iscsi_ipv6_addr ipv6_addr;			 
	union nvm_iscsi_lun lun;				 
	union nvm_iscsi_name           target_name;		 
	union nvm_iscsi_chap_name      chap_name;		 
	union nvm_iscsi_chap_password  chap_password;		 
	u32 rsvd_2[107];					 
};
struct nvm_iscsi_block {
	u32 id;							 
#define NVM_ISCSI_CFG_BLK_MAPPED_PF_ID_MASK         0x0000000F
#define NVM_ISCSI_CFG_BLK_MAPPED_PF_ID_OFFSET       0
#define NVM_ISCSI_CFG_BLK_CTRL_FLAG_MASK            0x00000FF0
#define NVM_ISCSI_CFG_BLK_CTRL_FLAG_OFFSET          4
#define NVM_ISCSI_CFG_BLK_CTRL_FLAG_IS_NOT_EMPTY    BIT(0)
#define NVM_ISCSI_CFG_BLK_CTRL_FLAG_PF_MAPPED       BIT(1)
	u32 rsvd_1[5];						 
	struct nvm_iscsi_generic     generic;			 
	struct nvm_iscsi_initiator   initiator;			 
	struct nvm_iscsi_target      target[NUM_OF_ISCSI_TARGET_PER_PF];
	u32 rsvd_2[58];						 
};
struct nvm_iscsi_cfg {
	u32 id;							 
#define NVM_ISCSI_CFG_BLK_VERSION_MINOR_MASK     0x000000FF
#define NVM_ISCSI_CFG_BLK_VERSION_MAJOR_MASK     0x0000FF00
#define NVM_ISCSI_CFG_BLK_SIGNATURE_MASK         0xFFFF0000
#define NVM_ISCSI_CFG_BLK_SIGNATURE              0x49430000  
#define NVM_ISCSI_CFG_BLK_VERSION_MAJOR          0
#define NVM_ISCSI_CFG_BLK_VERSION_MINOR          10
#define NVM_ISCSI_CFG_BLK_VERSION ((NVM_ISCSI_CFG_BLK_VERSION_MAJOR << 8) | \
				   NVM_ISCSI_CFG_BLK_VERSION_MINOR)
	struct nvm_iscsi_block	block[NUM_OF_ISCSI_PF_SUPPORTED];  
};
#endif
