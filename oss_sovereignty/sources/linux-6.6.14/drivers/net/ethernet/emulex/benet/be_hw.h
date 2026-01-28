




#define MPU_MAILBOX_DB_OFFSET	0x160
#define MPU_MAILBOX_DB_RDY_MASK	0x1 	
#define MPU_MAILBOX_DB_HI_MASK	0x2	

#define MPU_EP_CONTROL 		0


#define SLIPORT_SOFTRESET_OFFSET		0x5c	
#define SLIPORT_SEMAPHORE_OFFSET_BEx		0xac  
#define SLIPORT_SEMAPHORE_OFFSET_SH		0x94  
#define POST_STAGE_MASK				0x0000FFFF
#define POST_ERR_MASK				0x1
#define POST_ERR_SHIFT				31
#define POST_ERR_RECOVERY_CODE_MASK		0xFFF


#define SLIPORT_SOFTRESET_SR_MASK		0x00000080	


#define POST_STAGE_AWAITING_HOST_RDY 	0x1 
#define POST_STAGE_HOST_RDY 		0x2 
#define POST_STAGE_BE_RESET		0x3 
#define POST_STAGE_ARMFW_RDY		0xc000	
#define POST_STAGE_RECOVERABLE_ERR	0xE000	

#define POST_STAGE_FAT_LOG_START       0x0D00
#define POST_STAGE_ARMFW_UE            0xF000  


#define SLIPORT_STATUS_OFFSET		0x404
#define SLIPORT_CONTROL_OFFSET		0x408
#define SLIPORT_ERROR1_OFFSET		0x40C
#define SLIPORT_ERROR2_OFFSET		0x410
#define PHYSDEV_CONTROL_OFFSET		0x414

#define SLIPORT_STATUS_ERR_MASK		0x80000000
#define SLIPORT_STATUS_DIP_MASK		0x02000000
#define SLIPORT_STATUS_RN_MASK		0x01000000
#define SLIPORT_STATUS_RDY_MASK		0x00800000
#define SLI_PORT_CONTROL_IP_MASK	0x08000000
#define PHYSDEV_CONTROL_FW_RESET_MASK	0x00000002
#define PHYSDEV_CONTROL_DD_MASK		0x00000004
#define PHYSDEV_CONTROL_INP_MASK	0x40000000

#define SLIPORT_ERROR_NO_RESOURCE1	0x2
#define SLIPORT_ERROR_NO_RESOURCE2	0x9

#define SLIPORT_ERROR_FW_RESET1		0x2
#define SLIPORT_ERROR_FW_RESET2		0x0


#define PCICFG_MEMBAR_CTRL_INT_CTRL_OFFSET 	0xfc

#define MEMBAR_CTRL_INT_CTRL_HOSTINTR_MASK	BIT(29) 


#define BE_FUNCTION_CAPS_RSS			0x2
#define BE_FUNCTION_CAPS_SUPER_NIC		0x40


#define PCICFG_PM_CONTROL_OFFSET		0x44
#define PCICFG_PM_CONTROL_MASK			0x108	


#define PCICFG_ONLINE0				0xB0
#define PCICFG_ONLINE1				0xB4


#define PCICFG_UE_STATUS_LOW			0xA0
#define PCICFG_UE_STATUS_HIGH			0xA4
#define PCICFG_UE_STATUS_LOW_MASK		0xA8
#define PCICFG_UE_STATUS_HI_MASK		0xAC


#define SLI_INTF_REG_OFFSET			0x58
#define SLI_INTF_VALID_MASK			0xE0000000
#define SLI_INTF_VALID				0xC0000000
#define SLI_INTF_HINT2_MASK			0x1F000000
#define SLI_INTF_HINT2_SHIFT			24
#define SLI_INTF_HINT1_MASK			0x00FF0000
#define SLI_INTF_HINT1_SHIFT			16
#define SLI_INTF_FAMILY_MASK			0x00000F00
#define SLI_INTF_FAMILY_SHIFT			8
#define SLI_INTF_IF_TYPE_MASK			0x0000F000
#define SLI_INTF_IF_TYPE_SHIFT			12
#define SLI_INTF_REV_MASK			0x000000F0
#define SLI_INTF_REV_SHIFT			4
#define SLI_INTF_FT_MASK			0x00000001

#define SLI_INTF_TYPE_2		2
#define SLI_INTF_TYPE_3		3


#define CEV_ISR0_OFFSET 			0xC18
#define CEV_ISR_SIZE				4


#define DB_EQ_OFFSET			DB_CQ_OFFSET
#define DB_EQ_RING_ID_MASK		0x1FF	
#define DB_EQ_RING_ID_EXT_MASK		0x3e00  
#define DB_EQ_RING_ID_EXT_MASK_SHIFT	(2) 


#define DB_EQ_CLR_SHIFT			(9)	

#define DB_EQ_EVNT_SHIFT		(10)	

#define DB_EQ_NUM_POPPED_SHIFT		(16)	

#define DB_EQ_REARM_SHIFT		(29)	

#define DB_EQ_R2I_DLY_SHIFT		(30)    


#define	R2I_DLY_ENC_0			0	
#define	R2I_DLY_ENC_1			1	
#define	R2I_DLY_ENC_2			2	
#define	R2I_DLY_ENC_3			3	


#define DB_CQ_OFFSET 			0x120
#define DB_CQ_RING_ID_MASK		0x3FF	
#define DB_CQ_RING_ID_EXT_MASK		0x7C00	
#define DB_CQ_RING_ID_EXT_MASK_SHIFT	(1)	


#define DB_CQ_NUM_POPPED_SHIFT		(16) 	

#define DB_CQ_REARM_SHIFT		(29) 	


#define DB_TXULP1_OFFSET		0x60
#define DB_TXULP_RING_ID_MASK		0x7FF	

#define DB_TXULP_NUM_POSTED_SHIFT	(16)	
#define DB_TXULP_NUM_POSTED_MASK	0x3FFF	


#define DB_RQ_OFFSET 			0x100
#define DB_RQ_RING_ID_MASK		0x3FF	

#define DB_RQ_NUM_POSTED_SHIFT		(24)	


#define DB_MCCQ_OFFSET 			0x140
#define DB_MCCQ_RING_ID_MASK		0x7FF	

#define DB_MCCQ_NUM_POSTED_SHIFT	(16)	


#define SRIOV_VF_PCICFG_OFFSET		(4096)


#define RETRIEVE_FAT	0
#define QUERY_FAT	1


#define BE_UNICAST_PACKET		0
#define BE_MULTICAST_PACKET		1
#define BE_BROADCAST_PACKET		2
#define BE_RSVD_PACKET			3



#define EQ_ENTRY_VALID_MASK 		0x1	
#define EQ_ENTRY_RES_ID_MASK 		0xFFFF	
#define EQ_ENTRY_RES_ID_SHIFT 		16

struct be_eq_entry {
	u32 evt;
};


#define ETH_WRB_FRAG_LEN_MASK		0xFFFF
struct be_eth_wrb {
	__le32 frag_pa_hi;		
	__le32 frag_pa_lo;		
	u32 rsvd0;			
	__le32 frag_len;		
} __packed;


struct amap_eth_hdr_wrb {
	u8 rsvd0[32];		
	u8 rsvd1[32];		
	u8 complete;		
	u8 event;
	u8 crc;
	u8 forward;
	u8 lso6;
	u8 mgmt;
	u8 ipcs;
	u8 udpcs;
	u8 tcpcs;
	u8 lso;
	u8 vlan;
	u8 gso[2];
	u8 num_wrb[5];
	u8 lso_mss[14];
	u8 len[16];		
	u8 vlan_tag[16];
} __packed;

#define TX_HDR_WRB_COMPL		1		
#define TX_HDR_WRB_EVT			BIT(1)		
#define TX_HDR_WRB_NUM_SHIFT		13		
#define TX_HDR_WRB_NUM_MASK		0x1F		

struct be_eth_hdr_wrb {
	__le32 dw[4];
};


#define BE_TX_COMP_HDR_PARSE_ERR	0x2
#define BE_TX_COMP_NDMA_ERR		0x3
#define BE_TX_COMP_ACL_ERR		0x5

#define LANCER_TX_COMP_LSO_ERR			0x1
#define LANCER_TX_COMP_HSW_DROP_MAC_ERR		0x3
#define LANCER_TX_COMP_HSW_DROP_VLAN_ERR	0x5
#define LANCER_TX_COMP_QINQ_ERR			0x7
#define LANCER_TX_COMP_SGE_ERR			0x9
#define LANCER_TX_COMP_PARITY_ERR		0xb
#define LANCER_TX_COMP_DMA_ERR			0xd




struct amap_eth_tx_compl {
	u8 wrb_index[16];	
	u8 ct[2]; 		
	u8 port[2];		
	u8 rsvd0[8];		
	u8 status[4];		
	u8 user_bytes[16];	
	u8 nwh_bytes[8];	
	u8 lso;			
	u8 cast_enc[2];		
	u8 rsvd1[5];		
	u8 rsvd2[32];		
	u8 pkts[16];		
	u8 ringid[11];		
	u8 hash_val[4];		
	u8 valid;		
} __packed;

struct be_eth_tx_compl {
	u32 dw[4];
};


struct be_eth_rx_d {
	u32 fragpa_hi;
	u32 fragpa_lo;
};




struct amap_eth_rx_compl_v0 {
	u8 vlan_tag[16];	
	u8 pktsize[14];		
	u8 port;		
	u8 ip_opt;		
	u8 err;			
	u8 rsshp;		
	u8 ipf;			
	u8 tcpf;		
	u8 udpf;		
	u8 ipcksm;		
	u8 l4_cksm;		
	u8 ip_version;		
	u8 macdst[6];		
	u8 vtp;			
	u8 ip_frag;		
	u8 fragndx[10];		
	u8 ct[2];		
	u8 sw;			
	u8 numfrags[3];		
	u8 rss_flush;		
	u8 cast_enc[2];		
	u8 qnq;			
	u8 rss_bank;		
	u8 rsvd1[23];		
	u8 lro_pkt;		
	u8 rsvd2[2];		
	u8 valid;		
	u8 rsshash[32];		
} __packed;


struct amap_eth_rx_compl_v1 {
	u8 vlan_tag[16];	
	u8 pktsize[14];		
	u8 vtp;			
	u8 ip_opt;		
	u8 err;			
	u8 rsshp;		
	u8 ipf;			
	u8 tcpf;		
	u8 udpf;		
	u8 ipcksm;		
	u8 l4_cksm;		
	u8 ip_version;		
	u8 macdst[7];		
	u8 rsvd0;		
	u8 fragndx[10];		
	u8 ct[2];		
	u8 sw;			
	u8 numfrags[3];		
	u8 rss_flush;		
	u8 cast_enc[2];		
	u8 qnq;			
	u8 rss_bank;		
	u8 port[2];		
	u8 vntagp;		
	u8 header_len[8];	
	u8 header_split[2];	
	u8 rsvd1[12];		
	u8 tunneled;
	u8 valid;		
	u8 rsshash[32];		
} __packed;

struct be_eth_rx_compl {
	u32 dw[4];
};
