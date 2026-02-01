 
 

#define SYNOPSYS_XPCS_ID		0x7996ced0
#define SYNOPSYS_XPCS_MASK		0xffffffff

 
#define DW_VENDOR			BIT(15)

 
#define DW_USXGMII_RST			BIT(10)
#define DW_USXGMII_EN			BIT(9)
#define DW_VR_XS_PCS_DIG_CTRL1		0x0000
#define DW_VR_RST			BIT(15)
#define DW_EN_VSMMD1			BIT(13)
#define DW_CL37_BP			BIT(12)
#define DW_VR_XS_PCS_DIG_STS		0x0010
#define DW_RXFIFO_ERR			GENMASK(6, 5)
#define DW_PSEQ_ST			GENMASK(4, 2)
#define DW_PSEQ_ST_GOOD			FIELD_PREP(GENMASK(4, 2), 0x4)

 
#define DW_USXGMII_FULL			BIT(8)
#define DW_USXGMII_SS_MASK		(BIT(13) | BIT(6) | BIT(5))
#define DW_USXGMII_10000		(BIT(13) | BIT(6))
#define DW_USXGMII_5000			(BIT(13) | BIT(5))
#define DW_USXGMII_2500			(BIT(5))
#define DW_USXGMII_1000			(BIT(6))
#define DW_USXGMII_100			(BIT(13))
#define DW_USXGMII_10			(0)

 
#define DW_SR_AN_ADV1			0x10
#define DW_SR_AN_ADV2			0x11
#define DW_SR_AN_ADV3			0x12

 
 
#define DW_C73_PAUSE			BIT(10)
#define DW_C73_ASYM_PAUSE		BIT(11)
#define DW_C73_AN_ADV_SF		0x1
 
#define DW_C73_1000KX			BIT(5)
#define DW_C73_10000KX4			BIT(6)
#define DW_C73_10000KR			BIT(7)
 
#define DW_C73_2500KX			BIT(0)
#define DW_C73_5000KR			BIT(1)

 
 
#define DW_VR_MII_MMD_CTRL		0x0000
#define DW_VR_MII_DIG_CTRL1		0x8000
#define DW_VR_MII_AN_CTRL		0x8001
#define DW_VR_MII_AN_INTR_STS		0x8002
 
#define DW_VR_MII_DIG_CTRL1_2G5_EN	BIT(2)
 
#define DW_VR_MII_EEE_MCTRL0		0x8006
#define DW_VR_MII_EEE_MCTRL1		0x800b
#define DW_VR_MII_DIG_CTRL2		0x80e1

 
#define DW_VR_MII_DIG_CTRL1_MAC_AUTO_SW		BIT(9)
#define DW_VR_MII_DIG_CTRL1_PHY_MODE_CTRL	BIT(0)

 
#define DW_VR_MII_DIG_CTRL2_TX_POL_INV		BIT(4)
#define DW_VR_MII_DIG_CTRL2_RX_POL_INV		BIT(0)

 
#define DW_VR_MII_AN_CTRL_8BIT			BIT(8)
#define DW_VR_MII_AN_CTRL_TX_CONFIG_SHIFT	3
#define DW_VR_MII_TX_CONFIG_MASK		BIT(3)
#define DW_VR_MII_TX_CONFIG_PHY_SIDE_SGMII	0x1
#define DW_VR_MII_TX_CONFIG_MAC_SIDE_SGMII	0x0
#define DW_VR_MII_AN_CTRL_PCS_MODE_SHIFT	1
#define DW_VR_MII_PCS_MODE_MASK			GENMASK(2, 1)
#define DW_VR_MII_PCS_MODE_C37_1000BASEX	0x0
#define DW_VR_MII_PCS_MODE_C37_SGMII		0x2
#define DW_VR_MII_AN_INTR_EN			BIT(0)

 
#define DW_VR_MII_AN_STS_C37_ANCMPLT_INTR	BIT(0)
#define DW_VR_MII_AN_STS_C37_ANSGM_FD		BIT(1)
#define DW_VR_MII_AN_STS_C37_ANSGM_SP_SHIFT	2
#define DW_VR_MII_AN_STS_C37_ANSGM_SP		GENMASK(3, 2)
#define DW_VR_MII_C37_ANSGM_SP_10		0x0
#define DW_VR_MII_C37_ANSGM_SP_100		0x1
#define DW_VR_MII_C37_ANSGM_SP_1000		0x2
#define DW_VR_MII_C37_ANSGM_SP_LNKSTS		BIT(4)

 
#define AN_CL37_EN			BIT(12)	 
#define SGMII_SPEED_SS13		BIT(13)	 
#define SGMII_SPEED_SS6			BIT(6)	 

 
#define DW_HALF_DUPLEX			BIT(6)
#define DW_FULL_DUPLEX			BIT(5)

 
#define DW_VR_MII_EEE_LTX_EN			BIT(0)   
#define DW_VR_MII_EEE_LRX_EN			BIT(1)   
#define DW_VR_MII_EEE_TX_QUIET_EN		BIT(2)   
#define DW_VR_MII_EEE_RX_QUIET_EN		BIT(3)   
#define DW_VR_MII_EEE_TX_EN_CTRL		BIT(4)   
#define DW_VR_MII_EEE_RX_EN_CTRL		BIT(7)   

#define DW_VR_MII_EEE_MULT_FACT_100NS_SHIFT	8
#define DW_VR_MII_EEE_MULT_FACT_100NS		GENMASK(11, 8)

 
#define DW_VR_MII_EEE_TRN_LPI		BIT(0)	 

int xpcs_read(struct dw_xpcs *xpcs, int dev, u32 reg);
int xpcs_write(struct dw_xpcs *xpcs, int dev, u32 reg, u16 val);
int xpcs_read_vpcs(struct dw_xpcs *xpcs, int reg);
int xpcs_write_vpcs(struct dw_xpcs *xpcs, int reg, u16 val);
int nxp_sja1105_sgmii_pma_config(struct dw_xpcs *xpcs);
int nxp_sja1110_sgmii_pma_config(struct dw_xpcs *xpcs);
int nxp_sja1110_2500basex_pma_config(struct dw_xpcs *xpcs);
int txgbe_xpcs_switch_mode(struct dw_xpcs *xpcs, phy_interface_t interface);
