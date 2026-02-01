 
 

#ifndef __SPL2SW_DEFINE_H__
#define __SPL2SW_DEFINE_H__

#define MAX_NETDEV_NUM			2	 

 
#define MAC_INT_DAISY_MODE_CHG		BIT(31)  
#define MAC_INT_IP_CHKSUM_ERR		BIT(23)  
#define MAC_INT_WDOG_TIMER1_EXP		BIT(22)  
#define MAC_INT_WDOG_TIMER0_EXP		BIT(21)  
#define MAC_INT_INTRUDER_ALERT		BIT(20)  
#define MAC_INT_PORT_ST_CHG		BIT(19)  
#define MAC_INT_BC_STORM		BIT(18)  
#define MAC_INT_MUST_DROP_LAN		BIT(17)  
#define MAC_INT_GLOBAL_QUE_FULL		BIT(16)  
#define MAC_INT_TX_SOC_PAUSE_ON		BIT(15)  
#define MAC_INT_RX_SOC_QUE_FULL		BIT(14)  
#define MAC_INT_TX_LAN1_QUE_FULL	BIT(9)   
#define MAC_INT_TX_LAN0_QUE_FULL	BIT(8)   
#define MAC_INT_RX_L_DESCF		BIT(7)   
#define MAC_INT_RX_H_DESCF		BIT(6)   
#define MAC_INT_RX_DONE_L		BIT(5)   
#define MAC_INT_RX_DONE_H		BIT(4)   
#define MAC_INT_TX_DONE_L		BIT(3)   
#define MAC_INT_TX_DONE_H		BIT(2)   
#define MAC_INT_TX_DES_ERR		BIT(1)   
#define MAC_INT_RX_DES_ERR		BIT(0)   

#define MAC_INT_RX			(MAC_INT_RX_DONE_H | MAC_INT_RX_DONE_L | \
					MAC_INT_RX_DES_ERR)
#define MAC_INT_TX			(MAC_INT_TX_DONE_L | MAC_INT_TX_DONE_H | \
					MAC_INT_TX_DES_ERR)
#define MAC_INT_MASK_DEF		(MAC_INT_DAISY_MODE_CHG | MAC_INT_IP_CHKSUM_ERR | \
					MAC_INT_WDOG_TIMER1_EXP | MAC_INT_WDOG_TIMER0_EXP | \
					MAC_INT_INTRUDER_ALERT | MAC_INT_PORT_ST_CHG | \
					MAC_INT_BC_STORM | MAC_INT_MUST_DROP_LAN | \
					MAC_INT_GLOBAL_QUE_FULL | MAC_INT_TX_SOC_PAUSE_ON | \
					MAC_INT_RX_SOC_QUE_FULL | MAC_INT_TX_LAN1_QUE_FULL | \
					MAC_INT_TX_LAN0_QUE_FULL | MAC_INT_RX_L_DESCF | \
					MAC_INT_RX_H_DESCF)

 
#define MAC_ADDR_LOOKUP_IDLE		BIT(2)
#define MAC_SEARCH_NEXT_ADDR		BIT(1)
#define MAC_BEGIN_SEARCH_ADDR		BIT(0)

 
#define MAC_HASH_LOOKUP_ADDR		GENMASK(31, 22)
#define MAC_R_PORT_MAP			GENMASK(13, 12)
#define MAC_R_CPU_PORT			GENMASK(11, 10)
#define MAC_R_VID			GENMASK(9, 7)
#define MAC_R_AGE			GENMASK(6, 4)
#define MAC_R_PROXY			BIT(3)
#define MAC_R_MC_INGRESS		BIT(2)
#define MAC_AT_TABLE_END		BIT(1)
#define MAC_AT_DATA_READY		BIT(0)

 
#define MAC_W_PORT_MAP			GENMASK(13, 12)
#define MAC_W_LAN_PORT_1		BIT(13)
#define MAC_W_LAN_PORT_0		BIT(12)
#define MAC_W_CPU_PORT			GENMASK(11, 10)
#define MAC_W_CPU_PORT_1		BIT(11)
#define MAC_W_CPU_PORT_0		BIT(10)
#define MAC_W_VID			GENMASK(9, 7)
#define MAC_W_AGE			GENMASK(6, 4)
#define MAC_W_PROXY			BIT(3)
#define MAC_W_MC_INGRESS		BIT(2)
#define MAC_W_MAC_DONE			BIT(1)
#define MAC_W_MAC_CMD			BIT(0)

 
#define MAC_W_MAC_15_0			GENMASK(15, 0)

 
#define MAC_W_MAC_47_16			GENMASK(31, 0)

 
#define MAC_P1_PVID			GENMASK(6, 4)
#define MAC_P0_PVID			GENMASK(2, 0)

 
#define MAC_VLAN_MEMSET_3		GENMASK(27, 24)
#define MAC_VLAN_MEMSET_2		GENMASK(19, 16)
#define MAC_VLAN_MEMSET_1		GENMASK(11, 8)
#define MAC_VLAN_MEMSET_0		GENMASK(3, 0)

 
#define MAC_VLAN_MEMSET_5		GENMASK(11, 8)
#define MAC_VLAN_MEMSET_4		GENMASK(3, 0)

 
#define MAC_PORT_ABILITY_LINK_ST	GENMASK(25, 24)

 
#define MAC_EN_SOC1_AGING		BIT(15)
#define MAC_EN_SOC0_AGING		BIT(14)
#define MAC_DIS_LRN_SOC1		BIT(13)
#define MAC_DIS_LRN_SOC0		BIT(12)
#define MAC_EN_CRC_SOC1			BIT(9)
#define MAC_EN_CRC_SOC0			BIT(8)
#define MAC_DIS_SOC1_CPU		BIT(7)
#define MAC_DIS_SOC0_CPU		BIT(6)
#define MAC_DIS_BC2CPU_P1		BIT(5)
#define MAC_DIS_BC2CPU_P0		BIT(4)
#define MAC_DIS_MC2CPU			GENMASK(3, 2)
#define MAC_DIS_MC2CPU_P1		BIT(3)
#define MAC_DIS_MC2CPU_P0		BIT(2)
#define MAC_DIS_UN2CPU			GENMASK(1, 0)

 
#define MAC_DIS_PORT			GENMASK(25, 24)
#define MAC_DIS_PORT1			BIT(25)
#define MAC_DIS_PORT0			BIT(24)
#define MAC_DIS_RMC2CPU_P1		BIT(17)
#define MAC_DIS_RMC2CPU_P0		BIT(16)
#define MAC_EN_FLOW_CTL_P1		BIT(9)
#define MAC_EN_FLOW_CTL_P0		BIT(8)
#define MAC_EN_BACK_PRESS_P1		BIT(1)
#define MAC_EN_BACK_PRESS_P0		BIT(0)

 
#define MAC_DIS_SA_LRN_P1		BIT(9)
#define MAC_DIS_SA_LRN_P0		BIT(8)

 
#define MAC_EN_AGING_P1			BIT(9)
#define MAC_EN_AGING_P0			BIT(8)

 
#define MAC_RMC_TB_FAULT_RULE		GENMASK(26, 25)
#define MAC_LED_FLASH_TIME		GENMASK(24, 23)
#define MAC_BC_STORM_PREV		GENMASK(5, 4)

 
#define MAC_LED_ACT_HI			BIT(28)

 
#define MAC_CPU_PHY_WT_DATA		GENMASK(31, 16)
#define MAC_CPU_PHY_CMD			GENMASK(14, 13)
#define MAC_CPU_PHY_REG_ADDR		GENMASK(12, 8)
#define MAC_CPU_PHY_ADDR		GENMASK(4, 0)

 
#define MAC_CPU_PHY_RD_DATA		GENMASK(31, 16)
#define MAC_PHY_RD_RDY			BIT(1)
#define MAC_PHY_WT_DONE			BIT(0)

 
#define MAC_EXT_PHY1_ADDR		GENMASK(28, 24)
#define MAC_EXT_PHY0_ADDR		GENMASK(20, 16)
#define MAC_FORCE_RMII_LINK		GENMASK(9, 8)
#define MAC_FORCE_RMII_EN_1		BIT(7)
#define MAC_FORCE_RMII_EN_0		BIT(6)
#define MAC_FORCE_RMII_FC		GENMASK(5, 4)
#define MAC_FORCE_RMII_DPX		GENMASK(3, 2)
#define MAC_FORCE_RMII_SPD		GENMASK(1, 0)

 
#define MAC_TRIG_L_SOC0			BIT(1)
#define MAC_TRIG_H_SOC0			BIT(0)

 
#define TX_DESC_NUM			16	 
#define MAC_GUARD_DESC_NUM		2	 
#define RX_QUEUE0_DESC_NUM		16	 
#define RX_QUEUE1_DESC_NUM		16	 
#define TX_DESC_QUEUE_NUM		1	 
#define RX_DESC_QUEUE_NUM		2	 

#define MAC_RX_LEN_MAX			2047	 

 
 
#define TXD_OWN				BIT(31)
#define TXD_ERR_CODE			GENMASK(29, 26)
#define TXD_SOP				BIT(25)		 
#define TXD_EOP				BIT(24)		 
#define TXD_VLAN			GENMASK(17, 12)
#define TXD_PKT_LEN			GENMASK(10, 0)	 
 
#define TXD_EOR				BIT(31)		 
#define TXD_BUF_LEN2			GENMASK(22, 12)
#define TXD_BUF_LEN1			GENMASK(10, 0)

 
 
#define RXD_OWN				BIT(31)
#define RXD_ERR_CODE			GENMASK(29, 26)
#define RXD_TCP_UDP_CHKSUM		BIT(23)
#define RXD_PROXY			BIT(22)
#define RXD_PROTOCOL			GENMASK(21, 20)
#define RXD_VLAN_TAG			BIT(19)
#define RXD_IP_CHKSUM			BIT(18)
#define RXD_ROUTE_TYPE			GENMASK(17, 16)
#define RXD_PKT_SP			GENMASK(14, 12)	 
#define RXD_PKT_LEN			GENMASK(10, 0)	 
 
#define RXD_EOR				BIT(31)		 
#define RXD_BUF_LEN2			GENMASK(22, 12)
#define RXD_BUF_LEN1			GENMASK(10, 0)

 
struct spl2sw_mac_desc {
	u32 cmd1;
	u32 cmd2;
	u32 addr1;
	u32 addr2;
};

struct spl2sw_skb_info {
	struct sk_buff *skb;
	u32 mapping;
	u32 len;
};

struct spl2sw_common {
	void __iomem *l2sw_reg_base;

	struct platform_device *pdev;
	struct reset_control *rstc;
	struct clk *clk;

	void *desc_base;
	dma_addr_t desc_dma;
	s32 desc_size;
	struct spl2sw_mac_desc *rx_desc[RX_DESC_QUEUE_NUM];
	struct spl2sw_skb_info *rx_skb_info[RX_DESC_QUEUE_NUM];
	u32 rx_pos[RX_DESC_QUEUE_NUM];
	u32 rx_desc_num[RX_DESC_QUEUE_NUM];
	u32 rx_desc_buff_size;

	struct spl2sw_mac_desc *tx_desc;
	struct spl2sw_skb_info tx_temp_skb_info[TX_DESC_NUM];
	u32 tx_done_pos;
	u32 tx_pos;
	u32 tx_desc_full;

	struct net_device *ndev[MAX_NETDEV_NUM];
	struct mii_bus *mii_bus;

	struct napi_struct rx_napi;
	struct napi_struct tx_napi;

	spinlock_t tx_lock;		 
	spinlock_t mdio_lock;		 
	spinlock_t int_mask_lock;	 

	u8 enable;
};

struct spl2sw_mac {
	struct net_device *ndev;
	struct spl2sw_common *comm;

	u8 mac_addr[ETH_ALEN];
	phy_interface_t phy_mode;
	struct device_node *phy_node;

	u8 lan_port;
	u8 to_vlan;
	u8 vlan_id;
};

#endif
