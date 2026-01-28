#ifndef _LINUX_EMXX_H
#define _LINUX_EMXX_H
#define	USE_DMA	1
#define USE_SUSPEND_WAIT	1
#define	VBUS_VALUE		GPIO_VBUS
#define GPIO_VBUS 0  
#define INT_VBUS 0  
#define VBUS_CHATTERING_MDELAY		1
#define DMA_DISABLE_TIME		10
#define NUM_ENDPOINTS		14		 
#define REG_EP_NUM		15		 
#define DMA_MAX_COUNT		256		 
#define EPC_RST_DISABLE_TIME		1	 
#define EPC_DIRPD_DISABLE_TIME		1	 
#define EPC_PLL_LOCK_COUNT		1000	 
#define IN_DATA_EMPTY_COUNT		1000	 
#define CHATGER_TIME			700	 
#define USB_SUSPEND_TIME		2000	 
#define U2F_ENABLE		1
#define U2F_DISABLE		0
#define TEST_FORCE_ENABLE		(BIT(18) | BIT(16))
#define INT_SEL				BIT(10)
#define CONSTFS				BIT(9)
#define SOF_RCV				BIT(8)
#define RSUM_IN				BIT(7)
#define SUSPEND				BIT(6)
#define CONF				BIT(5)
#define DEFAULT				BIT(4)
#define CONNECTB			BIT(3)
#define PUE2				BIT(2)
#define MAX_TEST_MODE_NUM		0x05
#define TEST_MODE_SHIFT			16
#define SPEED_MODE			BIT(6)
#define HIGH_SPEED			BIT(6)
#define CONF				BIT(5)
#define DEFAULT				BIT(4)
#define USB_RST				BIT(3)
#define SPND_OUT			BIT(2)
#define RSUM_OUT			BIT(1)
#define USB_ADDR			0x007F0000
#define SOF_STATUS			BIT(15)
#define UFRAME				(BIT(14) | BIT(13) | BIT(12))
#define FRAME				0x000007FF
#define USB_ADRS_SHIFT			16
#define SQUSET				(BIT(7) | BIT(6) | BIT(5) | BIT(4))
#define USB_SQUSET			(BIT(6) | BIT(5) | BIT(4))
#define FORCEHS				BIT(2)
#define CS_TESTMODEEN			BIT(1)
#define LOOPBACK			BIT(0)
#define EPN_INT				0x00FFFF00
#define EP15_INT			BIT(23)
#define EP14_INT			BIT(22)
#define EP13_INT			BIT(21)
#define EP12_INT			BIT(20)
#define EP11_INT			BIT(19)
#define EP10_INT			BIT(18)
#define EP9_INT				BIT(17)
#define EP8_INT				BIT(16)
#define EP7_INT				BIT(15)
#define EP6_INT				BIT(14)
#define EP5_INT				BIT(13)
#define EP4_INT				BIT(12)
#define EP3_INT				BIT(11)
#define EP2_INT				BIT(10)
#define EP1_INT				BIT(9)
#define EP0_INT				BIT(8)
#define SPEED_MODE_INT			BIT(6)
#define SOF_ERROR_INT			BIT(5)
#define SOF_INT				BIT(4)
#define USB_RST_INT			BIT(3)
#define SPND_INT			BIT(2)
#define RSUM_INT			BIT(1)
#define USB_INT_STA_RW			0x7E
#define EP15_0_EN			0x00FFFF00
#define EP15_EN				BIT(23)
#define EP14_EN				BIT(22)
#define EP13_EN				BIT(21)
#define EP12_EN				BIT(20)
#define EP11_EN				BIT(19)
#define EP10_EN				BIT(18)
#define EP9_EN				BIT(17)
#define EP8_EN				BIT(16)
#define EP7_EN				BIT(15)
#define EP6_EN				BIT(14)
#define EP5_EN				BIT(13)
#define EP4_EN				BIT(12)
#define EP3_EN				BIT(11)
#define EP2_EN				BIT(10)
#define EP1_EN				BIT(9)
#define EP0_EN				BIT(8)
#define SPEED_MODE_EN			BIT(6)
#define SOF_ERROR_EN			BIT(5)
#define SOF_EN				BIT(4)
#define USB_RST_EN			BIT(3)
#define SPND_EN				BIT(2)
#define RSUM_EN				BIT(1)
#define USB_INT_EN_BIT	\
	(EP0_EN | SPEED_MODE_EN | USB_RST_EN | SPND_EN | RSUM_EN)
#define EP0_STGSEL			BIT(18)
#define EP0_OVERSEL			BIT(17)
#define EP0_AUTO			BIT(16)
#define EP0_PIDCLR			BIT(9)
#define EP0_BCLR			BIT(8)
#define EP0_DEND			BIT(7)
#define EP0_DW				(BIT(6) | BIT(5))
#define EP0_DW4				0
#define EP0_DW3				(BIT(6) | BIT(5))
#define EP0_DW2				BIT(6)
#define EP0_DW1				BIT(5)
#define EP0_INAK_EN			BIT(4)
#define EP0_PERR_NAK_CLR		BIT(3)
#define EP0_STL				BIT(2)
#define EP0_INAK			BIT(1)
#define EP0_ONAK			BIT(0)
#define EP0_PID				BIT(18)
#define EP0_PERR_NAK			BIT(17)
#define EP0_PERR_NAK_INT		BIT(16)
#define EP0_OUT_NAK_INT			BIT(15)
#define EP0_OUT_NULL			BIT(14)
#define EP0_OUT_FULL			BIT(13)
#define EP0_OUT_EMPTY			BIT(12)
#define EP0_IN_NAK_INT			BIT(11)
#define EP0_IN_DATA			BIT(10)
#define EP0_IN_FULL			BIT(9)
#define EP0_IN_EMPTY			BIT(8)
#define EP0_OUT_NULL_INT		BIT(7)
#define EP0_OUT_OR_INT			BIT(6)
#define EP0_OUT_INT			BIT(5)
#define EP0_IN_INT			BIT(4)
#define EP0_STALL_INT			BIT(3)
#define STG_END_INT			BIT(2)
#define STG_START_INT			BIT(1)
#define SETUP_INT			BIT(0)
#define EP0_STATUS_RW_BIT	(BIT(16) | BIT(15) | BIT(11) | 0xFF)
#define EP0_PERR_NAK_EN			BIT(16)
#define EP0_OUT_NAK_EN			BIT(15)
#define EP0_IN_NAK_EN			BIT(11)
#define EP0_OUT_NULL_EN			BIT(7)
#define EP0_OUT_OR_EN			BIT(6)
#define EP0_OUT_EN			BIT(5)
#define EP0_IN_EN			BIT(4)
#define EP0_STALL_EN			BIT(3)
#define STG_END_EN			BIT(2)
#define STG_START_EN			BIT(1)
#define SETUP_EN			BIT(0)
#define EP0_INT_EN_BIT	\
	(EP0_OUT_OR_EN | EP0_OUT_EN | EP0_IN_EN | STG_END_EN | SETUP_EN)
#define EP0_LDATA			0x0000007F
#define EPN_EN				BIT(31)
#define EPN_BUF_TYPE			BIT(30)
#define EPN_BUF_SINGLE			BIT(30)
#define EPN_DIR0			BIT(26)
#define EPN_MODE			(BIT(25) | BIT(24))
#define EPN_BULK			0
#define EPN_INTERRUPT			BIT(24)
#define EPN_ISO				BIT(25)
#define EPN_OVERSEL			BIT(17)
#define EPN_AUTO			BIT(16)
#define EPN_IPIDCLR			BIT(11)
#define EPN_OPIDCLR			BIT(10)
#define EPN_BCLR			BIT(9)
#define EPN_CBCLR			BIT(8)
#define EPN_DEND			BIT(7)
#define EPN_DW				(BIT(6) | BIT(5))
#define EPN_DW4				0
#define EPN_DW3				(BIT(6) | BIT(5))
#define EPN_DW2				BIT(6)
#define EPN_DW1				BIT(5)
#define EPN_OSTL_EN			BIT(4)
#define EPN_ISTL			BIT(3)
#define EPN_OSTL			BIT(2)
#define EPN_ONAK			BIT(0)
#define EPN_ISO_PIDERR			BIT(29)		 
#define EPN_OPID			BIT(28)		 
#define EPN_OUT_NOTKN			BIT(27)		 
#define EPN_ISO_OR			BIT(26)		 
#define EPN_ISO_CRC			BIT(24)		 
#define EPN_OUT_END_INT			BIT(23)		 
#define EPN_OUT_OR_INT			BIT(22)		 
#define EPN_OUT_NAK_ERR_INT		BIT(21)		 
#define EPN_OUT_STALL_INT		BIT(20)		 
#define EPN_OUT_INT			BIT(19)		 
#define EPN_OUT_NULL_INT		BIT(18)		 
#define EPN_OUT_FULL			BIT(17)		 
#define EPN_OUT_EMPTY			BIT(16)		 
#define EPN_IPID			BIT(10)		 
#define EPN_IN_NOTKN			BIT(9)		 
#define EPN_ISO_UR			BIT(8)		 
#define EPN_IN_END_INT			BIT(7)		 
#define EPN_IN_NAK_ERR_INT		BIT(5)		 
#define EPN_IN_STALL_INT		BIT(4)		 
#define EPN_IN_INT			BIT(3)		 
#define EPN_IN_DATA			BIT(2)		 
#define EPN_IN_FULL			BIT(1)		 
#define EPN_IN_EMPTY			BIT(0)		 
#define EPN_INT_EN	\
	(EPN_OUT_END_INT | EPN_OUT_INT | EPN_IN_END_INT | EPN_IN_INT)
#define EPN_OUT_END_EN			BIT(23)		 
#define EPN_OUT_OR_EN			BIT(22)		 
#define EPN_OUT_NAK_ERR_EN		BIT(21)		 
#define EPN_OUT_STALL_EN		BIT(20)		 
#define EPN_OUT_EN			BIT(19)		 
#define EPN_OUT_NULL_EN			BIT(18)		 
#define EPN_IN_END_EN			BIT(7)		 
#define EPN_IN_NAK_ERR_EN		BIT(5)		 
#define EPN_IN_STALL_EN			BIT(4)		 
#define EPN_IN_EN			BIT(3)		 
#define EPN_STOP_MODE			BIT(11)
#define EPN_DEND_SET			BIT(10)
#define EPN_BURST_SET			BIT(9)
#define EPN_STOP_SET			BIT(8)
#define EPN_DMA_EN			BIT(4)
#define EPN_DMAMODE0			BIT(0)
#define EPN_BASEAD			0x1FFF0000
#define EPN_MPKT			0x000007FF
#define EPN_DMACNT			0x01FF0000
#define EPN_LDATA			0x000007FF
#define WAIT_MODE			BIT(0)
#define ARBITER_CTR			BIT(31)		 
#define MCYCLE_RST			BIT(12)		 
#define ENDIAN_CTR			(BIT(9) | BIT(8))	 
#define ENDIAN_BYTE_SWAP		BIT(9)
#define ENDIAN_HALF_WORD_SWAP		ENDIAN_CTR
#define HBUSREQ_MODE			BIT(5)		 
#define HTRANS_MODE			BIT(4)		 
#define WBURST_TYPE			BIT(2)		 
#define BURST_TYPE			(BIT(1) | BIT(0))	 
#define BURST_MAX_16			0
#define BURST_MAX_8			BIT(0)
#define BURST_MAX_4			BIT(1)
#define BURST_SINGLE			BURST_TYPE
#define DMA_ENDINT			0xFFFE0000	 
#define AHB_VBUS_INT			BIT(13)		 
#define MBUS_ERRINT			BIT(6)		 
#define SBUS_ERRINT0			BIT(4)		 
#define ERR_MASTER			0x0000000F	 
#define DMA_ENDINTEN			0xFFFE0000	 
#define VBUS_INTEN			BIT(13)		 
#define MBUS_ERRINTEN			BIT(6)		 
#define SBUS_ERRINT0EN			BIT(4)		 
#define DIRPD				BIT(12)		 
#define VBUS_LEVEL			BIT(8)		 
#define PLL_RESUME			BIT(5)		 
#define PLL_LOCK			BIT(4)		 
#define EPC_RST				BIT(0)		 
#define LINESTATE			(BIT(9) | BIT(8))	 
#define DM_LEVEL			BIT(9)		 
#define DP_LEVEL			BIT(8)		 
#define PHY_TST				BIT(1)		 
#define PHY_TSTCLK			BIT(0)		 
#define AHBB_VER			0x00FF0000	 
#define EPC_VER				0x0000FF00	 
#define SS_VER				0x000000FF	 
#define EP_AVAILABLE			0xFFFF0000	 
#define DMA_AVAILABLE			0x0000FFFF	 
#define DCR1_EPN_DMACNT			0x00FF0000	 
#define DCR1_EPN_DIR0			BIT(1)		 
#define DCR1_EPN_REQEN			BIT(0)		 
#define DCR2_EPN_LMPKT			0x07FF0000	 
#define DCR2_EPN_MPKT			0x000007FF	 
#define EPN_TADR			0xFFFFFFFF	 
struct ep_regs {
	u32 EP_CONTROL;			 
	u32 EP_STATUS;			 
	u32 EP_INT_ENA;			 
	u32 EP_DMA_CTRL;		 
	u32 EP_PCKT_ADRS;		 
	u32 EP_LEN_DCNT;		 
	u32 EP_READ;			 
	u32 EP_WRITE;			 
};
struct ep_dcr {
	u32 EP_DCR1;			 
	u32 EP_DCR2;			 
	u32 EP_TADR;			 
	u32 Reserved;			 
};
struct fc_regs {
	u32 USB_CONTROL;		 
	u32 USB_STATUS;			 
	u32 USB_ADDRESS;		 
	u32 UTMI_CHARACTER_1;		 
	u32 TEST_CONTROL;		 
	u32 reserved_14;		 
	u32 SETUP_DATA0;		 
	u32 SETUP_DATA1;		 
	u32 USB_INT_STA;		 
	u32 USB_INT_ENA;		 
	u32 EP0_CONTROL;		 
	u32 EP0_STATUS;			 
	u32 EP0_INT_ENA;		 
	u32 EP0_LENGTH;			 
	u32 EP0_READ;			 
	u32 EP0_WRITE;			 
	struct ep_regs EP_REGS[REG_EP_NUM];	 
	u8 reserved_220[0x1000 - 0x220];	 
	u32 AHBSCTR;			 
	u32 AHBMCTR;			 
	u32 AHBBINT;			 
	u32 AHBBINTEN;			 
	u32 EPCTR;			 
	u32 USBF_EPTEST;		 
	u8 reserved_1018[0x20 - 0x18];	 
	u32 USBSSVER;			 
	u32 USBSSCONF;			 
	u8 reserved_1028[0x110 - 0x28];	 
	struct ep_dcr EP_DCR[REG_EP_NUM];	 
	u8 reserved_1200[0x1000 - 0x200];	 
} __aligned(32);
#define EP0_PACKETSIZE			64
#define EP_PACKETSIZE			1024
#define D_RAM_SIZE_CTRL			64
#define D_FS_RAM_SIZE_BULK		64
#define D_HS_RAM_SIZE_BULK		512
struct nbu2ss_udc;
enum ep0_state {
	EP0_IDLE,
	EP0_IN_DATA_PHASE,
	EP0_OUT_DATA_PHASE,
	EP0_IN_STATUS_PHASE,
	EP0_OUT_STATUS_PAHSE,
	EP0_END_XFER,
	EP0_SUSPEND,
	EP0_STALL,
};
struct nbu2ss_req {
	struct usb_request		req;
	struct list_head		queue;
	u32			div_len;
	bool		dma_flag;
	bool		zero;
	bool		unaligned;
	unsigned			mapped:1;
};
struct nbu2ss_ep {
	struct usb_ep			ep;
	struct list_head		queue;
	struct nbu2ss_udc		*udc;
	const struct usb_endpoint_descriptor *desc;
	u8		epnum;
	u8		direct;
	u8		ep_type;
	unsigned		wedged:1;
	unsigned		halted:1;
	unsigned		stalled:1;
	u8		*virt_buf;
	dma_addr_t	phys_buf;
};
struct nbu2ss_udc {
	struct usb_gadget gadget;
	struct usb_gadget_driver *driver;
	struct platform_device *pdev;
	struct device *dev;
	spinlock_t lock;  
	struct completion		*pdone;
	enum ep0_state			ep0state;
	enum usb_device_state	devstate;
	struct usb_ctrlrequest	ctrl;
	struct nbu2ss_req		ep0_req;
	u8		ep0_buf[EP0_PACKETSIZE];
	struct nbu2ss_ep	ep[NUM_ENDPOINTS];
	unsigned		softconnect:1;
	unsigned		vbus_active:1;
	unsigned		linux_suspended:1;
	unsigned		linux_resume:1;
	unsigned		usb_suspended:1;
	unsigned		remote_wakeup:1;
	unsigned		udc_enabled:1;
	unsigned int		mA;
	u32		curr_config;	 
	struct fc_regs __iomem *p_regs;
};
union usb_reg_access {
	struct {
		unsigned char	DATA[4];
	} byte;
	unsigned int		dw;
};
#endif   
