


#ifndef	__LINUX_BDC_H__
#define	__LINUX_BDC_H__

#include <linux/kernel.h>
#include <linux/usb.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/dma-mapping.h>
#include <linux/mm.h>
#include <linux/debugfs.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <asm/unaligned.h>

#define BRCM_BDC_NAME "bdc"
#define BRCM_BDC_DESC "Broadcom USB Device Controller driver"

#define DMA_ADDR_INVALID        (~(dma_addr_t)0)


#define BDC_CMD_TIMEOUT	1000

#define BDC_COP_TIMEOUT	500


#define EP0_RESPONSE_BUFF  6

#define EP0_MAX_PKT_SIZE 512


#define NUM_SR_ENTRIES	64


#define NUM_BDS_PER_TABLE	64


#define NUM_TABLES	2


#define NUM_TABLES_ISOCH	6


#define U1_TIMEOUT	0xf8


#define	INT_CLS	500



#define BDC_BDCCFG0	0x00
#define BDC_BDCCFG1	0x04
#define BDC_BDCCAP0	0x08
#define BDC_BDCCAP1	0x0c
#define BDC_CMDPAR0	0x10
#define BDC_CMDPAR1	0x14
#define BDC_CMDPAR2	0x18
#define BDC_CMDSC	0x1c
#define BDC_USPC	0x20
#define BDC_USPPMS	0x28
#define BDC_USPPM2	0x2c
#define BDC_SPBBAL	0x38
#define BDC_SPBBAH	0x3c
#define BDC_BDCSC	0x40
#define BDC_XSFNTF	0x4c

#define BDC_DVCSA	0x50
#define BDC_DVCSB	0x54
#define BDC_EPSTS0	0x60
#define BDC_EPSTS1	0x64
#define BDC_EPSTS2	0x68
#define BDC_EPSTS3	0x6c
#define BDC_EPSTS4	0x70
#define BDC_EPSTS5	0x74
#define BDC_EPSTS6	0x78
#define BDC_EPSTS7	0x7c
#define BDC_SRRBAL(n)	(0x200 + ((n) * 0x10))
#define BDC_SRRBAH(n)	(0x204 + ((n) * 0x10))
#define BDC_SRRINT(n)	(0x208 + ((n) * 0x10))
#define BDC_INTCTLS(n)	(0x20c + ((n) * 0x10))


#define BDC_FSCNOC	0xcd4
#define BDC_FSCNIC	0xce4
#define NUM_NCS(p)	((p) >> 28)



#define BDC_PGS(p)	(((p) & (0x7 << 8)) >> 8)
#define BDC_SPB(p)	((p) & 0x7)


#define BDC_P64		BIT(0)


#define BDC_CMD_FH	0xe
#define BDC_CMD_DNC	0x6
#define BDC_CMD_EPO	0x4
#define BDC_CMD_BLA	0x3
#define BDC_CMD_EPC	0x2
#define BDC_CMD_DVC	0x1
#define BDC_CMD_CWS		BIT(5)
#define BDC_CMD_CST(p)		(((p) & (0xf << 6))>>6)
#define BDC_CMD_EPN(p)		(((p) & 0x1f) << 10)
#define BDC_SUB_CMD_ADD		(0x1 << 17)
#define BDC_SUB_CMD_FWK		(0x4 << 17)

#define BDC_CMD_EPO_RST_SN	(0x1 << 16)
#define BDC_CMD_EP0_XSD		(0x1 << 16)
#define BDC_SUB_CMD_ADD_EP	(0x1 << 17)
#define BDC_SUB_CMD_DRP_EP	(0x2 << 17)
#define BDC_SUB_CMD_EP_STP	(0x2 << 17)
#define BDC_SUB_CMD_EP_STL	(0x4 << 17)
#define BDC_SUB_CMD_EP_RST	(0x1 << 17)
#define BDC_CMD_SRD		BIT(27)


#define BDC_CMDS_SUCC	0x1
#define BDC_CMDS_PARA	0x3
#define BDC_CMDS_STAT	0x4
#define BDC_CMDS_FAIL	0x5
#define BDC_CMDS_INTL	0x6
#define BDC_CMDS_BUSY	0xf


#define EPT_SHIFT	22
#define MP_SHIFT	10
#define MB_SHIFT	6
#define EPM_SHIFT	4


#define BDC_VBC		BIT(31)
#define BDC_PRC		BIT(30)
#define BDC_PCE		BIT(29)
#define BDC_CFC		BIT(28)
#define BDC_PCC		BIT(27)
#define BDC_PSC		BIT(26)
#define BDC_VBS		BIT(25)
#define BDC_PRS		BIT(24)
#define BDC_PCS		BIT(23)
#define BDC_PSP(p)	(((p) & (0x7 << 20))>>20)
#define BDC_SCN		BIT(8)
#define BDC_SDC		BIT(7)
#define BDC_SWS		BIT(4)

#define BDC_USPSC_RW	(BDC_SCN|BDC_SDC|BDC_SWS|0xf)
#define BDC_PSP(p)	(((p) & (0x7 << 20))>>20)

#define BDC_SPEED_FS	0x1
#define BDC_SPEED_LS	0x2
#define BDC_SPEED_HS	0x3
#define BDC_SPEED_SS	0x4

#define BDC_PST(p)	((p) & 0xf)
#define BDC_PST_MASK	0xf


#define BDC_U2E		BIT(31)
#define BDC_U1E		BIT(30)
#define BDC_U2A		BIT(29)
#define BDC_PORT_W1S	BIT(17)
#define BDC_U1T(p)	((p) & 0xff)
#define BDC_U2T(p)	(((p) & 0xff) << 8)
#define BDC_U1T_MASK	0xff



#define BDC_HLE		BIT(16)


#define BDC_COP_RST	(1 << 29)
#define BDC_COP_RUN	(2 << 29)
#define BDC_COP_STP	(4 << 29)

#define BDC_COP_MASK (BDC_COP_RST|BDC_COP_RUN|BDC_COP_STP)

#define BDC_COS		BIT(28)
#define BDC_CSTS(p)	(((p) & (0x7 << 20)) >> 20)
#define BDC_MASK_MCW	BIT(7)
#define BDC_GIE		BIT(1)
#define BDC_GIP		BIT(0)

#define BDC_HLT	1
#define BDC_NOR	2
#define BDC_OIP	7


#define BD_TYPE_BITMASK	(0xf)
#define BD_CHAIN	0xf

#define BD_TFS_SHIFT	4
#define BD_SOT		BIT(26)
#define BD_EOT		BIT(27)
#define BD_ISP		BIT(29)
#define BD_IOC		BIT(30)
#define BD_SBF		BIT(31)

#define BD_INTR_TARGET(p)	(((p) & 0x1f) << 27)

#define BDC_SRR_RWS		BIT(4)
#define BDC_SRR_RST		BIT(3)
#define BDC_SRR_ISR		BIT(2)
#define BDC_SRR_IE		BIT(1)
#define BDC_SRR_IP		BIT(0)
#define BDC_SRR_EPI(p)	(((p) & (0xff << 24)) >> 24)
#define BDC_SRR_DPI(p) (((p) & (0xff << 16)) >> 16)
#define BDC_SRR_DPI_MASK	0x00ff0000

#define MARK_CHAIN_BD	(BD_CHAIN|BD_EOT|BD_SOT)


#define BD_DIR_IN		BIT(25)

#define BDC_PTC_MASK	0xf0000000


#define SR_XSF		0
#define SR_USPC		4
#define SR_BD_LEN(p)    ((p) & 0xffffff)

#define XSF_SUCC	0x1
#define XSF_SHORT	0x3
#define XSF_BABB	0x4
#define XSF_SETUP_RECV	0x6
#define XSF_DATA_START	0x7
#define XSF_STATUS_START 0x8

#define XSF_STS(p) (((p) >> 28) & 0xf)


#define BD_LEN(p) ((p) & 0x1ffff)
#define BD_LTF		BIT(25)
#define BD_TYPE_DS	0x1
#define BD_TYPE_SS	0x2

#define BDC_EP_ENABLED     BIT(0)
#define BDC_EP_STALL       BIT(1)
#define BDC_EP_STOP        BIT(2)


#define BD_MAX_BUFF_SIZE	(1 << 16)

#define MAX_XFR_LEN		16777215


#define DEV_NOTF_TYPE 6
#define FWK_SUBTYPE  1
#define TRA_PACKET   4

#define to_bdc_ep(e)		container_of(e, struct bdc_ep, usb_ep)
#define to_bdc_req(r)		container_of(r, struct bdc_req, usb_req)
#define gadget_to_bdc(g)	container_of(g, struct bdc, gadget)


#define BDC_TNOTIFY 2500 

#define REMOTE_WAKEUP_ISSUED	BIT(16)
#define DEVICE_SUSPENDED	BIT(17)
#define FUNC_WAKE_ISSUED	BIT(18)
#define REMOTE_WAKE_ENABLE	(1 << USB_DEVICE_REMOTE_WAKEUP)


#define DEVSTATUS_CLEAR		(1 << USB_DEVICE_SELF_POWERED)



struct bdc_bd {
	__le32 offset[4];
};


struct bdc_sr {
	__le32 offset[4];
};


struct bd_table {
	struct bdc_bd *start_bd;
	
	dma_addr_t dma;
};


struct bd_list {
	
	struct bd_table **bd_table_array;
	
	int num_tabs;
	
	int max_bdi;
	
	int eqp_bdi;
	
	int hwd_bdi;
	
	int num_bds_table;
};

struct bdc_req;


struct bd_transfer {
	struct bdc_req *req;
	
	int start_bdi;
	
	int next_hwd_bdi;
	
	int num_bds;
};


struct bdc_req {
	struct usb_request	usb_req;
	struct list_head	queue;
	struct bdc_ep		*ep;
	
	struct bd_transfer bd_xfr;
	int	epnum;
};


struct bdc_scratchpad {
	dma_addr_t sp_dma;
	void *buff;
	u32 size;
};


struct bdc_ep {
	struct usb_ep	usb_ep;
	struct list_head queue;
	struct bdc *bdc;
	u8  ep_type;
	u8  dir;
	u8  ep_num;
	const struct usb_ss_ep_comp_descriptor	*comp_desc;
	const struct usb_endpoint_descriptor	*desc;
	unsigned int flags;
	char name[20];
	
	struct bd_list bd_list;
	
	bool ignore_next_sr;
};


struct bdc_cmd_params {
	u32	param2;
	u32	param1;
	u32	param0;
};


struct srr {
	struct bdc_sr *sr_bds;
	u16	eqp_index;
	u16	dqp_index;
	dma_addr_t	dma_addr;
};


enum bdc_ep0_state {
	WAIT_FOR_SETUP = 0,
	WAIT_FOR_DATA_START,
	WAIT_FOR_DATA_XMIT,
	WAIT_FOR_STATUS_START,
	WAIT_FOR_STATUS_XMIT,
	STATUS_PENDING
};


enum bdc_link_state {
	BDC_LINK_STATE_U0	= 0x00,
	BDC_LINK_STATE_U3	= 0x03,
	BDC_LINK_STATE_RX_DET	= 0x05,
	BDC_LINK_STATE_RESUME	= 0x0f
};


struct bdc {
	struct usb_gadget	gadget;
	struct usb_gadget_driver	*gadget_driver;
	struct device	*dev;
	
	spinlock_t	lock;

	
	struct phy      **phys;
	int num_phys;
	
	unsigned int num_eps;
	
	struct bdc_ep		**bdc_ep_array;
	void __iomem		*regs;
	struct bdc_scratchpad	scratchpad;
	u32	sp_buff_size;
	
	struct srr	srr;
	
	struct	usb_ctrlrequest setup_pkt;
	struct	bdc_req ep0_req;
	struct	bdc_req status_req;
	enum	bdc_ep0_state ep0_state;
	bool	delayed_status;
	bool	zlp_needed;
	bool	reinit;
	bool	pullup;
	
	u32	devstatus;
	int	irq;
	void	*mem;
	u32	dev_addr;
	
	struct dma_pool	*bd_table_pool;
	u8		test_mode;
	
	void (*sr_handler[2])(struct bdc *, struct bdc_sr *);
	
	void (*sr_xsf_ep0[3])(struct bdc *, struct bdc_sr *);
	
	unsigned char		ep0_response_buff[EP0_RESPONSE_BUFF];
	
	struct delayed_work	func_wake_notify;
	struct clk		*clk;
};

static inline u32 bdc_readl(void __iomem *base, u32 offset)
{
	return readl(base + offset);
}

static inline void bdc_writel(void __iomem *base, u32 offset, u32 value)
{
	writel(value, base + offset);
}


void bdc_notify_xfr(struct bdc *bdc, u32 epnum);
void bdc_softconn(struct bdc *bdc);
void bdc_softdisconn(struct bdc *bdc);
int bdc_run(struct bdc *bdc);
int bdc_stop(struct bdc *bdc);
int bdc_reset(struct bdc *bdc);
int bdc_udc_init(struct bdc *bdc);
void bdc_udc_exit(struct bdc *bdc);
int bdc_reinit(struct bdc *bdc);



void bdc_sr_uspc(struct bdc *bdc, struct bdc_sr *sreport);

void bdc_sr_xsf(struct bdc *bdc, struct bdc_sr *sreport);

void bdc_xsf_ep0_setup_recv(struct bdc *bdc, struct bdc_sr *sreport);
void bdc_xsf_ep0_data_start(struct bdc *bdc, struct bdc_sr *sreport);
void bdc_xsf_ep0_status_start(struct bdc *bdc, struct bdc_sr *sreport);

#endif 
