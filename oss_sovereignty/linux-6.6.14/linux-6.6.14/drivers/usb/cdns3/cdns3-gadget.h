#ifndef __LINUX_CDNS3_GADGET
#define __LINUX_CDNS3_GADGET
#include <linux/usb/gadget.h>
#include <linux/dma-direction.h>
struct cdns3_usb_regs {
	__le32 usb_conf;
	__le32 usb_sts;
	__le32 usb_cmd;
	__le32 usb_itpn;
	__le32 usb_lpm;
	__le32 usb_ien;
	__le32 usb_ists;
	__le32 ep_sel;
	__le32 ep_traddr;
	__le32 ep_cfg;
	__le32 ep_cmd;
	__le32 ep_sts;
	__le32 ep_sts_sid;
	__le32 ep_sts_en;
	__le32 drbl;
	__le32 ep_ien;
	__le32 ep_ists;
	__le32 usb_pwr;
	__le32 usb_conf2;
	__le32 usb_cap1;
	__le32 usb_cap2;
	__le32 usb_cap3;
	__le32 usb_cap4;
	__le32 usb_cap5;
	__le32 usb_cap6;
	__le32 usb_cpkt1;
	__le32 usb_cpkt2;
	__le32 usb_cpkt3;
	__le32 ep_dma_ext_addr;
	__le32 buf_addr;
	__le32 buf_data;
	__le32 buf_ctrl;
	__le32 dtrans;
	__le32 tdl_from_trb;
	__le32 tdl_beh;
	__le32 ep_tdl;
	__le32 tdl_beh2;
	__le32 dma_adv_td;
	__le32 reserved1[26];
	__le32 cfg_reg1;
	__le32 dbg_link1;
	__le32 dbg_link2;
	__le32 cfg_regs[74];
	__le32 reserved2[51];
	__le32 dma_axi_ctrl;
	__le32 dma_axi_id;
	__le32 dma_axi_cap;
	__le32 dma_axi_ctrl0;
	__le32 dma_axi_ctrl1;
};
#define USB_CONF_CFGRST		BIT(0)
#define USB_CONF_CFGSET		BIT(1)
#define USB_CONF_USB3DIS	BIT(3)
#define USB_CONF_USB2DIS	BIT(4)
#define USB_CONF_LENDIAN	BIT(5)
#define USB_CONF_BENDIAN	BIT(6)
#define USB_CONF_SWRST		BIT(7)
#define USB_CONF_DSING		BIT(8)
#define USB_CONF_DMULT		BIT(9)
#define USB_CONF_DMAOFFEN	BIT(10)
#define USB_CONF_DMAOFFDS	BIT(11)
#define USB_CONF_CFORCE_FS	BIT(12)
#define USB_CONF_SFORCE_FS	BIT(13)
#define USB_CONF_DEVEN		BIT(14)
#define USB_CONF_DEVDS		BIT(15)
#define USB_CONF_L1EN		BIT(16)
#define USB_CONF_L1DS		BIT(17)
#define USB_CONF_CLK2OFFEN	BIT(18)
#define USB_CONF_CLK2OFFDS	BIT(19)
#define USB_CONF_LGO_L0		BIT(20)
#define USB_CONF_CLK3OFFEN	BIT(21)
#define USB_CONF_CLK3OFFDS	BIT(22)
#define USB_CONF_U1EN		BIT(24)
#define USB_CONF_U1DS		BIT(25)
#define USB_CONF_U2EN		BIT(26)
#define USB_CONF_U2DS		BIT(27)
#define USB_CONF_LGO_U0		BIT(28)
#define USB_CONF_LGO_U1		BIT(29)
#define USB_CONF_LGO_U2		BIT(30)
#define USB_CONF_LGO_SSINACT	BIT(31)
#define USB_STS_CFGSTS_MASK	BIT(0)
#define USB_STS_CFGSTS(p)	((p) & USB_STS_CFGSTS_MASK)
#define USB_STS_OV_MASK		BIT(1)
#define USB_STS_OV(p)		((p) & USB_STS_OV_MASK)
#define USB_STS_USB3CONS_MASK	BIT(2)
#define USB_STS_USB3CONS(p)	((p) & USB_STS_USB3CONS_MASK)
#define USB_STS_DTRANS_MASK	BIT(3)
#define USB_STS_DTRANS(p)	((p) & USB_STS_DTRANS_MASK)
#define USB_STS_USBSPEED_MASK	GENMASK(6, 4)
#define USB_STS_USBSPEED(p)	(((p) & USB_STS_USBSPEED_MASK) >> 4)
#define USB_STS_LS		(0x1 << 4)
#define USB_STS_FS		(0x2 << 4)
#define USB_STS_HS		(0x3 << 4)
#define USB_STS_SS		(0x4 << 4)
#define DEV_UNDEFSPEED(p)	(((p) & USB_STS_USBSPEED_MASK) == (0x0 << 4))
#define DEV_LOWSPEED(p)		(((p) & USB_STS_USBSPEED_MASK) == USB_STS_LS)
#define DEV_FULLSPEED(p)	(((p) & USB_STS_USBSPEED_MASK) == USB_STS_FS)
#define DEV_HIGHSPEED(p)	(((p) & USB_STS_USBSPEED_MASK) == USB_STS_HS)
#define DEV_SUPERSPEED(p)	(((p) & USB_STS_USBSPEED_MASK) == USB_STS_SS)
#define USB_STS_ENDIAN_MASK	BIT(7)
#define USB_STS_ENDIAN(p)	((p) & USB_STS_ENDIAN_MASK)
#define USB_STS_CLK2OFF_MASK	BIT(8)
#define USB_STS_CLK2OFF(p)	((p) & USB_STS_CLK2OFF_MASK)
#define USB_STS_CLK3OFF_MASK	BIT(9)
#define USB_STS_CLK3OFF(p)	((p) & USB_STS_CLK3OFF_MASK)
#define USB_STS_IN_RST_MASK	BIT(10)
#define USB_STS_IN_RST(p)	((p) & USB_STS_IN_RST_MASK)
#define USB_STS_TDL_TRB_ENABLED	BIT(11)
#define USB_STS_DEVS_MASK	BIT(14)
#define USB_STS_DEVS(p)		((p) & USB_STS_DEVS_MASK)
#define USB_STS_ADDRESSED_MASK	BIT(15)
#define USB_STS_ADDRESSED(p)	((p) & USB_STS_ADDRESSED_MASK)
#define USB_STS_L1ENS_MASK	BIT(16)
#define USB_STS_L1ENS(p)	((p) & USB_STS_L1ENS_MASK)
#define USB_STS_VBUSS_MASK	BIT(17)
#define USB_STS_VBUSS(p)	((p) & USB_STS_VBUSS_MASK)
#define USB_STS_LPMST_MASK	GENMASK(19, 18)
#define DEV_L0_STATE(p)		(((p) & USB_STS_LPMST_MASK) == (0x0 << 18))
#define DEV_L1_STATE(p)		(((p) & USB_STS_LPMST_MASK) == (0x1 << 18))
#define DEV_L2_STATE(p)		(((p) & USB_STS_LPMST_MASK) == (0x2 << 18))
#define DEV_L3_STATE(p)		(((p) & USB_STS_LPMST_MASK) == (0x3 << 18))
#define USB_STS_USB2CONS_MASK	BIT(20)
#define USB_STS_USB2CONS(p)	((p) & USB_STS_USB2CONS_MASK)
#define USB_STS_DISABLE_HS_MASK	BIT(21)
#define USB_STS_DISABLE_HS(p)	((p) & USB_STS_DISABLE_HS_MASK)
#define USB_STS_U1ENS_MASK	BIT(24)
#define USB_STS_U1ENS(p)	((p) & USB_STS_U1ENS_MASK)
#define USB_STS_U2ENS_MASK	BIT(25)
#define USB_STS_U2ENS(p)	((p) & USB_STS_U2ENS_MASK)
#define USB_STS_LST_MASK	GENMASK(29, 26)
#define DEV_LST_U0		(((p) & USB_STS_LST_MASK) == (0x0 << 26))
#define DEV_LST_U1		(((p) & USB_STS_LST_MASK) == (0x1 << 26))
#define DEV_LST_U2		(((p) & USB_STS_LST_MASK) == (0x2 << 26))
#define DEV_LST_U3		(((p) & USB_STS_LST_MASK) == (0x3 << 26))
#define DEV_LST_DISABLED	(((p) & USB_STS_LST_MASK) == (0x4 << 26))
#define DEV_LST_RXDETECT	(((p) & USB_STS_LST_MASK) == (0x5 << 26))
#define DEV_LST_INACTIVE	(((p) & USB_STS_LST_MASK) == (0x6 << 26))
#define DEV_LST_POLLING		(((p) & USB_STS_LST_MASK) == (0x7 << 26))
#define DEV_LST_RECOVERY	(((p) & USB_STS_LST_MASK) == (0x8 << 26))
#define DEV_LST_HOT_RESET	(((p) & USB_STS_LST_MASK) == (0x9 << 26))
#define DEV_LST_COMP_MODE	(((p) & USB_STS_LST_MASK) == (0xa << 26))
#define DEV_LST_LB_STATE	(((p) & USB_STS_LST_MASK) == (0xb << 26))
#define USB_STS_DMAOFF_MASK	BIT(30)
#define USB_STS_DMAOFF(p)	((p) & USB_STS_DMAOFF_MASK)
#define USB_STS_ENDIAN2_MASK	BIT(31)
#define USB_STS_ENDIAN2(p)	((p) & USB_STS_ENDIAN2_MASK)
#define USB_CMD_SET_ADDR	BIT(0)
#define USB_CMD_FADDR_MASK	GENMASK(7, 1)
#define USB_CMD_FADDR(p)	(((p) << 1) & USB_CMD_FADDR_MASK)
#define USB_CMD_SDNFW		BIT(8)
#define USB_CMD_STMODE		BIT(9)
#define USB_STS_TMODE_SEL_MASK	GENMASK(11, 10)
#define USB_STS_TMODE_SEL(p)	(((p) << 10) & USB_STS_TMODE_SEL_MASK)
#define USB_CMD_SDNLTM		BIT(12)
#define USB_CMD_SPKT		BIT(13)
#define USB_CMD_DNFW_INT_MASK	GENMASK(23, 16)
#define USB_STS_DNFW_INT(p)	(((p) << 16) & USB_CMD_DNFW_INT_MASK)
#define USB_CMD_DNLTM_BELT_MASK	GENMASK(27, 16)
#define USB_STS_DNLTM_BELT(p)	(((p) << 16) & USB_CMD_DNLTM_BELT_MASK)
#define USB_ITPN_MASK		GENMASK(13, 0)
#define USB_ITPN(p)		((p) & USB_ITPN_MASK)
#define USB_LPM_HIRD_MASK	GENMASK(3, 0)
#define USB_LPM_HIRD(p)		((p) & USB_LPM_HIRD_MASK)
#define USB_LPM_BRW		BIT(4)
#define USB_IEN_CONIEN		BIT(0)
#define USB_IEN_DISIEN		BIT(1)
#define USB_IEN_UWRESIEN	BIT(2)
#define USB_IEN_UHRESIEN	BIT(3)
#define USB_IEN_U3ENTIEN	BIT(4)
#define USB_IEN_U3EXTIEN	BIT(5)
#define USB_IEN_U2ENTIEN	BIT(6)
#define USB_IEN_U2EXTIEN	BIT(7)
#define USB_IEN_U1ENTIEN	BIT(8)
#define USB_IEN_U1EXTIEN	BIT(9)
#define USB_IEN_ITPIEN		BIT(10)
#define USB_IEN_WAKEIEN		BIT(11)
#define USB_IEN_SPKTIEN		BIT(12)
#define USB_IEN_CON2IEN		BIT(16)
#define USB_IEN_DIS2IEN		BIT(17)
#define USB_IEN_U2RESIEN	BIT(18)
#define USB_IEN_L2ENTIEN	BIT(20)
#define USB_IEN_L2EXTIEN	BIT(21)
#define USB_IEN_L1ENTIEN	BIT(24)
#define USB_IEN_L1EXTIEN	BIT(25)
#define USB_IEN_CFGRESIEN	BIT(26)
#define USB_IEN_UWRESSIEN	BIT(28)
#define USB_IEN_UWRESEIEN	BIT(29)
#define USB_IEN_INIT  (USB_IEN_U2RESIEN | USB_ISTS_DIS2I | USB_IEN_CON2IEN \
		       | USB_IEN_UHRESIEN | USB_IEN_UWRESIEN | USB_IEN_DISIEN \
		       | USB_IEN_CONIEN | USB_IEN_U3EXTIEN | USB_IEN_L2ENTIEN \
		       | USB_IEN_L2EXTIEN | USB_IEN_L1ENTIEN | USB_IEN_U3ENTIEN)
#define USB_ISTS_CONI		BIT(0)
#define USB_ISTS_DISI		BIT(1)
#define USB_ISTS_UWRESI		BIT(2)
#define USB_ISTS_UHRESI		BIT(3)
#define USB_ISTS_U3ENTI		BIT(4)
#define USB_ISTS_U3EXTI		BIT(5)
#define USB_ISTS_U2ENTI		BIT(6)
#define USB_ISTS_U2EXTI		BIT(7)
#define USB_ISTS_U1ENTI		BIT(8)
#define USB_ISTS_U1EXTI		BIT(9)
#define USB_ISTS_ITPI		BIT(10)
#define USB_ISTS_WAKEI		BIT(11)
#define USB_ISTS_SPKTI		BIT(12)
#define USB_ISTS_CON2I		BIT(16)
#define USB_ISTS_DIS2I		BIT(17)
#define USB_ISTS_U2RESI		BIT(18)
#define USB_ISTS_L2ENTI		BIT(20)
#define USB_ISTS_L2EXTI		BIT(21)
#define USB_ISTS_L1ENTI		BIT(24)
#define USB_ISTS_L1EXTI		BIT(25)
#define USB_ISTS_CFGRESI	BIT(26)
#define USB_ISTS_UWRESSI	BIT(28)
#define USB_ISTS_UWRESEI	BIT(29)
#define EP_SEL_EPNO_MASK	GENMASK(3, 0)
#define EP_SEL_EPNO(p)		((p) & EP_SEL_EPNO_MASK)
#define EP_SEL_DIR		BIT(7)
#define select_ep_in(nr)	(EP_SEL_EPNO(p) | EP_SEL_DIR)
#define select_ep_out		(EP_SEL_EPNO(p))
#define EP_TRADDR_TRADDR(p)	((p))
#define EP_CFG_ENABLE		BIT(0)
#define EP_CFG_EPTYPE_MASK	GENMASK(2, 1)
#define EP_CFG_EPTYPE(p)	(((p) << 1)  & EP_CFG_EPTYPE_MASK)
#define EP_CFG_STREAM_EN	BIT(3)
#define EP_CFG_TDL_CHK		BIT(4)
#define EP_CFG_SID_CHK		BIT(5)
#define EP_CFG_EPENDIAN		BIT(7)
#define EP_CFG_MAXBURST_MASK	GENMASK(11, 8)
#define EP_CFG_MAXBURST(p)	(((p) << 8) & EP_CFG_MAXBURST_MASK)
#define EP_CFG_MAXBURST_MAX	15
#define EP_CFG_MULT_MASK	GENMASK(15, 14)
#define EP_CFG_MULT(p)		(((p) << 14) & EP_CFG_MULT_MASK)
#define EP_CFG_MULT_MAX		2
#define EP_CFG_MAXPKTSIZE_MASK	GENMASK(26, 16)
#define EP_CFG_MAXPKTSIZE(p)	(((p) << 16) & EP_CFG_MAXPKTSIZE_MASK)
#define EP_CFG_BUFFERING_MASK	GENMASK(31, 27)
#define EP_CFG_BUFFERING(p)	(((p) << 27) & EP_CFG_BUFFERING_MASK)
#define EP_CFG_BUFFERING_MAX	15
#define EP_CMD_EPRST		BIT(0)
#define EP_CMD_SSTALL		BIT(1)
#define EP_CMD_CSTALL		BIT(2)
#define EP_CMD_ERDY		BIT(3)
#define EP_CMD_REQ_CMPL		BIT(5)
#define EP_CMD_DRDY		BIT(6)
#define EP_CMD_DFLUSH		BIT(7)
#define EP_CMD_STDL		BIT(8)
#define EP_CMD_TDL_MASK		GENMASK(15, 9)
#define EP_CMD_TDL_SET(p)	(((p) << 9) & EP_CMD_TDL_MASK)
#define EP_CMD_TDL_GET(p)	(((p) & EP_CMD_TDL_MASK) >> 9)
#define EP_CMD_TDL_MAX		(EP_CMD_TDL_MASK >> 9)
#define EP_CMD_ERDY_SID_MASK	GENMASK(31, 16)
#define EP_CMD_ERDY_SID(p)	(((p) << 16) & EP_CMD_ERDY_SID_MASK)
#define EP_STS_SETUP		BIT(0)
#define EP_STS_STALL(p)		((p) & BIT(1))
#define EP_STS_IOC		BIT(2)
#define EP_STS_ISP		BIT(3)
#define EP_STS_DESCMIS		BIT(4)
#define EP_STS_STREAMR		BIT(5)
#define EP_STS_MD_EXIT		BIT(6)
#define EP_STS_TRBERR		BIT(7)
#define EP_STS_NRDY		BIT(8)
#define EP_STS_DBUSY		BIT(9)
#define EP_STS_BUFFEMPTY(p)	((p) & BIT(10))
#define EP_STS_CCS(p)		((p) & BIT(11))
#define EP_STS_PRIME		BIT(12)
#define EP_STS_SIDERR		BIT(13)
#define EP_STS_OUTSMM		BIT(14)
#define EP_STS_ISOERR		BIT(15)
#define EP_STS_HOSTPP(p)	((p) & BIT(16))
#define EP_STS_SPSMST_MASK		GENMASK(18, 17)
#define EP_STS_SPSMST_DISABLED(p)	(((p) & EP_STS_SPSMST_MASK) >> 17)
#define EP_STS_SPSMST_IDLE(p)		(((p) & EP_STS_SPSMST_MASK) >> 17)
#define EP_STS_SPSMST_START_STREAM(p)	(((p) & EP_STS_SPSMST_MASK) >> 17)
#define EP_STS_SPSMST_MOVE_DATA(p)	(((p) & EP_STS_SPSMST_MASK) >> 17)
#define EP_STS_IOT		BIT(19)
#define EP_STS_OUTQ_NO_MASK	GENMASK(27, 24)
#define EP_STS_OUTQ_NO(p)	(((p) & EP_STS_OUTQ_NO_MASK) >> 24)
#define EP_STS_OUTQ_VAL_MASK	BIT(28)
#define EP_STS_OUTQ_VAL(p)	((p) & EP_STS_OUTQ_VAL_MASK)
#define EP_STS_STPWAIT		BIT(31)
#define EP_STS_SID_MASK		GENMASK(15, 0)
#define EP_STS_SID(p)		((p) & EP_STS_SID_MASK)
#define EP_STS_EN_SETUPEN	BIT(0)
#define EP_STS_EN_DESCMISEN	BIT(4)
#define EP_STS_EN_STREAMREN	BIT(5)
#define EP_STS_EN_MD_EXITEN	BIT(6)
#define EP_STS_EN_TRBERREN	BIT(7)
#define EP_STS_EN_NRDYEN	BIT(8)
#define EP_STS_EN_PRIMEEEN	BIT(12)
#define EP_STS_EN_SIDERREN	BIT(13)
#define EP_STS_EN_OUTSMMEN	BIT(14)
#define EP_STS_EN_ISOERREN	BIT(15)
#define EP_STS_EN_IOTEN		BIT(19)
#define EP_STS_EN_STPWAITEN	BIT(31)
#define DB_VALUE_BY_INDEX(index) (1 << (index))
#define DB_VALUE_EP0_OUT	BIT(0)
#define DB_VALUE_EP0_IN		BIT(16)
#define EP_IEN(index)		(1 << (index))
#define EP_IEN_EP_OUT0		BIT(0)
#define EP_IEN_EP_IN0		BIT(16)
#define EP_ISTS(index)		(1 << (index))
#define EP_ISTS_EP_OUT0		BIT(0)
#define EP_ISTS_EP_IN0		BIT(16)
#define PUSB_PWR_PSO_EN		BIT(0)
#define PUSB_PWR_PSO_DS		BIT(1)
#define PUSB_PWR_STB_CLK_SWITCH_EN	BIT(8)
#define PUSB_PWR_STB_CLK_SWITCH_DONE	BIT(9)
#define PUSB_PWR_FST_REG_ACCESS_STAT	BIT(30)
#define PUSB_PWR_FST_REG_ACCESS		BIT(31)
#define USB_CONF2_DIS_TDL_TRB		BIT(1)
#define USB_CONF2_EN_TDL_TRB		BIT(2)
#define USB_CAP1_SFR_TYPE_MASK	GENMASK(3, 0)
#define DEV_SFR_TYPE_OCP(p)	(((p) & USB_CAP1_SFR_TYPE_MASK) == 0x0)
#define DEV_SFR_TYPE_AHB(p)	(((p) & USB_CAP1_SFR_TYPE_MASK) == 0x1)
#define DEV_SFR_TYPE_PLB(p)	(((p) & USB_CAP1_SFR_TYPE_MASK) == 0x2)
#define DEV_SFR_TYPE_AXI(p)	(((p) & USB_CAP1_SFR_TYPE_MASK) == 0x3)
#define USB_CAP1_SFR_WIDTH_MASK	GENMASK(7, 4)
#define DEV_SFR_WIDTH_8(p)	(((p) & USB_CAP1_SFR_WIDTH_MASK) == (0x0 << 4))
#define DEV_SFR_WIDTH_16(p)	(((p) & USB_CAP1_SFR_WIDTH_MASK) == (0x1 << 4))
#define DEV_SFR_WIDTH_32(p)	(((p) & USB_CAP1_SFR_WIDTH_MASK) == (0x2 << 4))
#define DEV_SFR_WIDTH_64(p)	(((p) & USB_CAP1_SFR_WIDTH_MASK) == (0x3 << 4))
#define USB_CAP1_DMA_TYPE_MASK	GENMASK(11, 8)
#define DEV_DMA_TYPE_OCP(p)	(((p) & USB_CAP1_DMA_TYPE_MASK) == (0x0 << 8))
#define DEV_DMA_TYPE_AHB(p)	(((p) & USB_CAP1_DMA_TYPE_MASK) == (0x1 << 8))
#define DEV_DMA_TYPE_PLB(p)	(((p) & USB_CAP1_DMA_TYPE_MASK) == (0x2 << 8))
#define DEV_DMA_TYPE_AXI(p)	(((p) & USB_CAP1_DMA_TYPE_MASK) == (0x3 << 8))
#define USB_CAP1_DMA_WIDTH_MASK	GENMASK(15, 12)
#define DEV_DMA_WIDTH_32(p)	(((p) & USB_CAP1_DMA_WIDTH_MASK) == (0x2 << 12))
#define DEV_DMA_WIDTH_64(p)	(((p) & USB_CAP1_DMA_WIDTH_MASK) == (0x3 << 12))
#define USB_CAP1_U3PHY_TYPE_MASK GENMASK(19, 16)
#define DEV_U3PHY_PIPE(p) (((p) & USB_CAP1_U3PHY_TYPE_MASK) == (0x0 << 16))
#define DEV_U3PHY_RMMI(p) (((p) & USB_CAP1_U3PHY_TYPE_MASK) == (0x1 << 16))
#define USB_CAP1_U3PHY_WIDTH_MASK GENMASK(23, 20)
#define DEV_U3PHY_WIDTH_8(p) \
	(((p) & USB_CAP1_U3PHY_WIDTH_MASK) == (0x0 << 20))
#define DEV_U3PHY_WIDTH_16(p) \
	(((p) & USB_CAP1_U3PHY_WIDTH_MASK) == (0x1 << 16))
#define DEV_U3PHY_WIDTH_32(p) \
	(((p) & USB_CAP1_U3PHY_WIDTH_MASK) == (0x2 << 20))
#define DEV_U3PHY_WIDTH_64(p) \
	(((p) & USB_CAP1_U3PHY_WIDTH_MASK) == (0x3 << 16))
#define USB_CAP1_U2PHY_EN(p)	((p) & BIT(24))
#define DEV_U2PHY_ULPI(p)	((p) & BIT(25))
#define DEV_U2PHY_WIDTH_16(p)	((p) & BIT(26))
#define USB_CAP1_OTG_READY(p)	((p) & BIT(27))
#define USB_CAP1_TDL_FROM_TRB(p)	((p) & BIT(28))
#define USB_CAP2_ACTUAL_MEM_SIZE(p) ((p) & GENMASK(7, 0))
#define USB_CAP2_MAX_MEM_SIZE(p) ((p) & GENMASK(11, 8))
#define EP_IS_IMPLEMENTED(reg, index) ((reg) & (1 << (index)))
#define EP_SUPPORT_ISO(reg, index) ((reg) & (1 << (index)))
#define EP_SUPPORT_STREAM(reg, index) ((reg) & (1 << (index)))
#define GET_DEV_BASE_VERSION(p) ((p) & GENMASK(23, 0))
#define GET_DEV_CUSTOM_VERSION(p) ((p) & GENMASK(31, 24))
#define DEV_VER_NXP_V1		0x00024502
#define DEV_VER_TI_V1		0x00024509
#define DEV_VER_V2		0x0002450C
#define DEV_VER_V3		0x0002450d
#define DBG_LINK1_LFPS_MIN_DET_U1_EXIT(p)	((p) & GENMASK(7, 0))
#define DBG_LINK1_LFPS_MIN_GEN_U1_EXIT_MASK	GENMASK(15, 8)
#define DBG_LINK1_LFPS_MIN_GEN_U1_EXIT(p)	(((p) << 8) & GENMASK(15, 8))
#define DBG_LINK1_RXDET_BREAK_DIS		BIT(16)
#define DBG_LINK1_LFPS_GEN_PING(p)		(((p) << 17) & GENMASK(21, 17))
#define DBG_LINK1_LFPS_MIN_DET_U1_EXIT_SET	BIT(24)
#define DBG_LINK1_LFPS_MIN_GEN_U1_EXIT_SET	BIT(25)
#define DBG_LINK1_RXDET_BREAK_DIS_SET		BIT(26)
#define DBG_LINK1_LFPS_GEN_PING_SET		BIT(27)
#define DMA_AXI_CTRL_MARPROT(p) ((p) & GENMASK(2, 0))
#define DMA_AXI_CTRL_MAWPROT(p) (((p) & GENMASK(2, 0)) << 16)
#define DMA_AXI_CTRL_NON_SECURE 0x02
#define gadget_to_cdns3_device(g) (container_of(g, struct cdns3_device, gadget))
#define ep_to_cdns3_ep(ep) (container_of(ep, struct cdns3_endpoint, endpoint))
#define TRBS_PER_SEGMENT	600
#define ISO_MAX_INTERVAL	10
#define MAX_TRB_LENGTH          BIT(16)
#if TRBS_PER_SEGMENT < 2
#error "Incorrect TRBS_PER_SEGMENT. Minimal Transfer Ring size is 2."
#endif
#define TRBS_PER_STREAM_SEGMENT 2
#if TRBS_PER_STREAM_SEGMENT < 2
#error "Incorrect TRBS_PER_STREAMS_SEGMENT. Minimal Transfer Ring size is 2."
#endif
#define TRBS_PER_ISOC_SEGMENT	(ISO_MAX_INTERVAL * 8)
#define GET_TRBS_PER_SEGMENT(ep_type) ((ep_type) == USB_ENDPOINT_XFER_ISOC ? \
				      TRBS_PER_ISOC_SEGMENT : TRBS_PER_SEGMENT)
struct cdns3_trb {
	__le32 buffer;
	__le32 length;
	__le32 control;
};
#define TRB_SIZE		(sizeof(struct cdns3_trb))
#define TRB_RING_SIZE		(TRB_SIZE * TRBS_PER_SEGMENT)
#define TRB_STREAM_RING_SIZE	(TRB_SIZE * TRBS_PER_STREAM_SEGMENT)
#define TRB_ISO_RING_SIZE	(TRB_SIZE * TRBS_PER_ISOC_SEGMENT)
#define TRB_CTRL_RING_SIZE	(TRB_SIZE * 2)
#define TRB_TYPE_BITMASK	GENMASK(15, 10)
#define TRB_TYPE(p)		((p) << 10)
#define TRB_FIELD_TO_TYPE(p)	(((p) & TRB_TYPE_BITMASK) >> 10)
#define TRB_NORMAL		1
#define TRB_LINK		6
#define TRB_CYCLE		BIT(0)
#define TRB_TOGGLE		BIT(1)
#define TRB_SMM			BIT(1)
#define TRB_SP			BIT(1)
#define TRB_ISP			BIT(2)
#define TRB_FIFO_MODE		BIT(3)
#define TRB_CHAIN		BIT(4)
#define TRB_IOC			BIT(5)
#define TRB_STREAM_ID_BITMASK		GENMASK(31, 16)
#define TRB_STREAM_ID(p)		((p) << 16)
#define TRB_FIELD_TO_STREAMID(p)	(((p) & TRB_STREAM_ID_BITMASK) >> 16)
#define TRB_TDL_HS_SIZE(p)	(((p) << 16) & GENMASK(31, 16))
#define TRB_TDL_HS_SIZE_GET(p)	(((p) & GENMASK(31, 16)) >> 16)
#define TRB_LEN(p)		((p) & GENMASK(16, 0))
#define TRB_TDL_SS_SIZE(p)	(((p) << 17) & GENMASK(23, 17))
#define TRB_TDL_SS_SIZE_GET(p)	(((p) & GENMASK(23, 17)) >> 17)
#define TRB_BURST_LEN(p)	((unsigned int)((p) << 24) & GENMASK(31, 24))
#define TRB_BURST_LEN_GET(p)	(((p) & GENMASK(31, 24)) >> 24)
#define TRB_BUFFER(p)		((p) & GENMASK(31, 0))
#define USB_DEVICE_MAX_ADDRESS		127
#define CDNS3_EP_MAX_PACKET_LIMIT	1024
#define CDNS3_EP_MAX_STREAMS		15
#define CDNS3_EP0_MAX_PACKET_LIMIT	512
#define CDNS3_ENDPOINTS_MAX_COUNT	32
#define CDNS3_EP_ZLP_BUF_SIZE		1024
#define CDNS3_MAX_NUM_DESCMISS_BUF	32
#define CDNS3_DESCMIS_BUF_SIZE		2048	 
#define CDNS3_WA2_NUM_BUFFERS		128
struct cdns3_device;
struct cdns3_endpoint {
	struct usb_ep		endpoint;
	struct list_head	pending_req_list;
	struct list_head	deferred_req_list;
	struct list_head	wa2_descmiss_req_list;
	int			wa2_counter;
	struct cdns3_trb	*trb_pool;
	dma_addr_t		trb_pool_dma;
	struct cdns3_device	*cdns3_dev;
	char			name[20];
#define EP_ENABLED		BIT(0)
#define EP_STALLED		BIT(1)
#define EP_STALL_PENDING	BIT(2)
#define EP_WEDGE		BIT(3)
#define EP_TRANSFER_STARTED	BIT(4)
#define EP_UPDATE_EP_TRBADDR	BIT(5)
#define EP_PENDING_REQUEST	BIT(6)
#define EP_RING_FULL		BIT(7)
#define EP_CLAIMED		BIT(8)
#define EP_DEFERRED_DRDY	BIT(9)
#define EP_QUIRK_ISO_OUT_EN	BIT(10)
#define EP_QUIRK_END_TRANSFER	BIT(11)
#define EP_QUIRK_EXTRA_BUF_DET	BIT(12)
#define EP_QUIRK_EXTRA_BUF_EN	BIT(13)
#define EP_TDLCHK_EN		BIT(15)
#define EP_CONFIGURED		BIT(16)
	u32			flags;
	struct cdns3_request	*descmis_req;
	u8			dir;
	u8			num;
	u8			type;
	u8			mult;
	u8			bMaxBurst;
	u16			wMaxPacketSize;
	int			interval;
	int			free_trbs;
	int			num_trbs;
	int			alloc_ring_size;
	u8			pcs;
	u8			ccs;
	int			enqueue;
	int			dequeue;
	u8			trb_burst_size;
	unsigned int		wa1_set:1;
	struct cdns3_trb	*wa1_trb;
	unsigned int		wa1_trb_index;
	unsigned int		wa1_cycle_bit:1;
	unsigned int		use_streams:1;
	unsigned int		prime_flag:1;
	u32			ep_sts_pending;
	u16			last_stream_id;
	u16			pending_tdl;
	unsigned int		stream_sg_idx;
};
struct cdns3_aligned_buf {
	void			*buf;
	dma_addr_t		dma;
	u32			size;
	enum dma_data_direction dir;
	unsigned		in_use:1;
	struct list_head	list;
};
struct cdns3_request {
	struct usb_request		request;
	struct cdns3_endpoint		*priv_ep;
	struct cdns3_trb		*trb;
	int				start_trb;
	int				end_trb;
	struct cdns3_aligned_buf	*aligned_buf;
#define REQUEST_PENDING			BIT(0)
#define REQUEST_INTERNAL		BIT(1)
#define REQUEST_INTERNAL_CH		BIT(2)
#define REQUEST_ZLP			BIT(3)
#define REQUEST_UNALIGNED		BIT(4)
	u32				flags;
	struct list_head		list;
	int				finished_trb;
	int				num_of_trb;
};
#define to_cdns3_request(r) (container_of(r, struct cdns3_request, request))
#define CDNS3_SETUP_STAGE		0x0
#define CDNS3_DATA_STAGE		0x1
#define CDNS3_STATUS_STAGE		0x2
struct cdns3_device {
	struct device			*dev;
	struct device			*sysdev;
	struct usb_gadget		gadget;
	struct usb_gadget_driver	*gadget_driver;
#define CDNS_REVISION_V0		0x00024501
#define CDNS_REVISION_V1		0x00024509
	u32				dev_ver;
	spinlock_t			lock;
	struct cdns3_usb_regs		__iomem *regs;
	struct dma_pool			*eps_dma_pool;
	struct usb_ctrlrequest		*setup_buf;
	dma_addr_t			setup_dma;
	void				*zlp_buf;
	u8				ep0_stage;
	int				ep0_data_dir;
	struct cdns3_endpoint		*eps[CDNS3_ENDPOINTS_MAX_COUNT];
	struct list_head		aligned_buf_list;
	struct work_struct		aligned_buf_wq;
	u32				selected_ep;
	u16				isoch_delay;
	unsigned			wait_for_setup:1;
	unsigned			u1_allowed:1;
	unsigned			u2_allowed:1;
	unsigned			is_selfpowered:1;
	unsigned			setup_pending:1;
	unsigned			hw_configured_flag:1;
	unsigned			wake_up_flag:1;
	unsigned			status_completion_no_call:1;
	unsigned			using_streams:1;
	int				out_mem_is_allocated;
	struct work_struct		pending_status_wq;
	struct usb_request		*pending_status_request;
	u16				onchip_buffers;
	u16				onchip_used_size;
	u16				ep_buf_size;
	u16				ep_iso_burst;
};
void cdns3_set_register_bit(void __iomem *ptr, u32 mask);
dma_addr_t cdns3_trb_virt_to_dma(struct cdns3_endpoint *priv_ep,
				 struct cdns3_trb *trb);
enum usb_device_speed cdns3_get_speed(struct cdns3_device *priv_dev);
void cdns3_pending_setup_status_handler(struct work_struct *work);
void cdns3_hw_reset_eps_config(struct cdns3_device *priv_dev);
void cdns3_set_hw_configuration(struct cdns3_device *priv_dev);
void cdns3_select_ep(struct cdns3_device *priv_dev, u32 ep);
void cdns3_allow_enable_l1(struct cdns3_device *priv_dev, int enable);
struct usb_request *cdns3_next_request(struct list_head *list);
void cdns3_rearm_transfer(struct cdns3_endpoint *priv_ep, u8 rearm);
int cdns3_allocate_trb_pool(struct cdns3_endpoint *priv_ep);
u8 cdns3_ep_addr_to_index(u8 ep_addr);
int cdns3_gadget_ep_set_wedge(struct usb_ep *ep);
int cdns3_gadget_ep_set_halt(struct usb_ep *ep, int value);
void __cdns3_gadget_ep_set_halt(struct cdns3_endpoint *priv_ep);
int __cdns3_gadget_ep_clear_halt(struct cdns3_endpoint *priv_ep);
struct usb_request *cdns3_gadget_ep_alloc_request(struct usb_ep *ep,
						  gfp_t gfp_flags);
void cdns3_gadget_ep_free_request(struct usb_ep *ep,
				  struct usb_request *request);
int cdns3_gadget_ep_dequeue(struct usb_ep *ep, struct usb_request *request);
void cdns3_gadget_giveback(struct cdns3_endpoint *priv_ep,
			   struct cdns3_request *priv_req,
			   int status);
int cdns3_init_ep0(struct cdns3_device *priv_dev,
		   struct cdns3_endpoint *priv_ep);
void cdns3_ep0_config(struct cdns3_device *priv_dev);
int cdns3_ep_config(struct cdns3_endpoint *priv_ep, bool enable);
void cdns3_check_ep0_interrupt_proceed(struct cdns3_device *priv_dev, int dir);
int __cdns3_gadget_wakeup(struct cdns3_device *priv_dev);
#endif  
