#ifndef __LINUX_CDNS2_GADGET
#define __LINUX_CDNS2_GADGET
#include <linux/usb/gadget.h>
#include <linux/dma-direction.h>
struct cdns2_ep0_regs {
	__u8 rxbc;
	__u8 txbc;
	__u8 cs;
	__u8 reserved1[4];
	__u8 fifo;
	__le32 reserved2[94];
	__u8 setupdat[8];
	__u8 reserved4[88];
	__u8 maxpack;
} __packed __aligned(4);
#define EP0CS_STALL	BIT(0)
#define EP0CS_HSNAK	BIT(1)
#define EP0CS_TXBSY_MSK	BIT(2)
#define EP0CS_RXBSY_MSK	BIT(3)
#define EP0CS_DSTALL	BIT(4)
#define EP0CS_CHGSET	BIT(7)
#define EP0_FIFO_IO_TX	BIT(4)
#define EP0_FIFO_AUTO	BIT(5)
#define EP0_FIFO_COMMIT	BIT(6)
#define EP0_FIFO_ACCES	BIT(7)
struct cdns2_epx_base {
	__le16 rxbc;
	__u8 rxcon;
	__u8 rxcs;
	__le16 txbc;
	__u8 txcon;
	__u8 txcs;
} __packed __aligned(4);
#define EPX_CON_BUF		GENMASK(1, 0)
#define EPX_CON_TYPE		GENMASK(3, 2)
#define EPX_CON_TYPE_ISOC	0x4
#define EPX_CON_TYPE_BULK	0x8
#define EPX_CON_TYPE_INT	0xC
#define EPX_CON_ISOD		GENMASK(5, 4)
#define EPX_CON_ISOD_SHIFT	0x4
#define EPX_CON_STALL		BIT(6)
#define EPX_CON_VAL		BIT(7)
#define EPX_CS_ERR(p)		((p) & BIT(0))
struct cdns2_epx_regs {
	__le32 reserved[2];
	struct cdns2_epx_base ep[15];
	__u8 reserved2[290];
	__u8 endprst;
	__u8 reserved3[41];
	__le16 isoautoarm;
	__u8 reserved4[10];
	__le16 isodctrl;
	__le16 reserved5;
	__le16 isoautodump;
	__le32 reserved6;
	__le16 rxmaxpack[15];
	__le32 reserved7[65];
	__le32 rxstaddr[15];
	__u8 reserved8[4];
	__le32 txstaddr[15];
	__u8 reserved9[98];
	__le16 txmaxpack[15];
} __packed __aligned(4);
#define ENDPRST_EP	GENMASK(3, 0)
#define ENDPRST_IO_TX	BIT(4)
#define ENDPRST_TOGRST	BIT(5)
#define ENDPRST_FIFORST	BIT(6)
#define ENDPRST_TOGSETQ	BIT(7)
struct cdns2_interrupt_regs {
	__u8 reserved[396];
	__u8 usbirq;
	__u8 extirq;
	__le16 rxpngirq;
	__le16 reserved1[4];
	__u8 usbien;
	__u8 extien;
	__le16 reserved2[3];
	__u8 usbivect;
} __packed __aligned(4);
#define EXTIRQ_VBUSFAULT_FALL BIT(0)
#define EXTIRQ_VBUSFAULT_RISE BIT(1)
#define EXTIRQ_WAKEUP	BIT(7)
#define USBIRQ_SUDAV	BIT(0)
#define USBIRQ_SOF	BIT(1)
#define USBIRQ_SUTOK	BIT(2)
#define USBIRQ_SUSPEND	BIT(3)
#define USBIRQ_URESET	BIT(4)
#define USBIRQ_HSPEED	BIT(5)
#define USBIRQ_LPM	BIT(7)
#define USB_IEN_INIT (USBIRQ_SUDAV | USBIRQ_SUSPEND | USBIRQ_URESET \
		      | USBIRQ_HSPEED | USBIRQ_LPM)
struct cdns2_usb_regs {
	__u8 reserved[4];
	__u16 lpmctrl;
	__u8 lpmclock;
	__u8 reserved2[411];
	__u8 endprst;
	__u8 usbcs;
	__le16 frmnr;
	__u8 fnaddr;
	__u8 clkgate;
	__u8 fifoctrl;
	__u8 speedctrl;
	__u8 sleep_clkgate;
	__u8 reserved3[533];
	__u8 cpuctrl;
} __packed __aligned(4);
#define LPMCTRLLL_HIRD		GENMASK(7, 4)
#define LPMCTRLLH_BREMOTEWAKEUP	BIT(8)
#define LPMCTRLLH_LPMNYET	BIT(16)
#define LPMCLOCK_SLEEP_ENTRY	BIT(7)
#define USBCS_LPMNYET		BIT(2)
#define USBCS_SIGRSUME		BIT(5)
#define USBCS_DISCON		BIT(6)
#define USBCS_WAKESRC		BIT(7)
#define FIFOCTRL_EP		GENMASK(3, 0)
#define FIFOCTRL_IO_TX		BIT(4)
#define FIFOCTRL_FIFOAUTO	BIT(5)
#define FIFOCTRL_FIFOCMIT	BIT(6)
#define FIFOCTRL_FIFOACC	BIT(7)
#define SPEEDCTRL_FS		BIT(1)
#define SPEEDCTRL_HS		BIT(2)
#define SPEEDCTRL_HSDISABLE	BIT(7)
#define CPUCTRL_SW_RST		BIT(1)
struct cdns2_adma_regs {
	__le32 conf;
	__le32 sts;
	__le32 reserved1[5];
	__le32 ep_sel;
	__le32 ep_traddr;
	__le32 ep_cfg;
	__le32 ep_cmd;
	__le32 ep_sts;
	__le32 reserved2;
	__le32 ep_sts_en;
	__le32 drbl;
	__le32 ep_ien;
	__le32 ep_ists;
	__le32 axim_ctrl;
	__le32 axim_id;
	__le32 reserved3;
	__le32 axim_cap;
	__le32 reserved4;
	__le32 axim_ctrl0;
	__le32 axim_ctrl1;
};
#define CDNS2_ADMA_REGS_OFFSET	0x400
#define DMA_CONF_CFGRST		BIT(0)
#define DMA_CONF_DSING		BIT(8)
#define DMA_CONF_DMULT		BIT(9)
#define DMA_EP_CFG_ENABLE	BIT(0)
#define DMA_EP_CMD_EPRST	BIT(0)
#define DMA_EP_CMD_DRDY		BIT(6)
#define DMA_EP_CMD_DFLUSH	BIT(7)
#define DMA_EP_STS_IOC		BIT(2)
#define DMA_EP_STS_ISP		BIT(3)
#define DMA_EP_STS_DESCMIS	BIT(4)
#define DMA_EP_STS_TRBERR	BIT(7)
#define DMA_EP_STS_DBUSY	BIT(9)
#define DMA_EP_STS_CCS(p)	((p) & BIT(11))
#define DMA_EP_STS_OUTSMM	BIT(14)
#define DMA_EP_STS_ISOERR	BIT(15)
#define DMA_EP_STS_EN_DESCMISEN	BIT(4)
#define DMA_EP_STS_EN_TRBERREN	BIT(7)
#define DMA_EP_STS_EN_OUTSMMEN	BIT(14)
#define DMA_EP_STS_EN_ISOERREN	BIT(15)
#define DMA_EP_IEN(index)	(1 << (index))
#define DMA_EP_IEN_EP_OUT0	BIT(0)
#define DMA_EP_IEN_EP_IN0	BIT(16)
#define DMA_EP_ISTS(index)	(1 << (index))
#define DMA_EP_ISTS_EP_OUT0	BIT(0)
#define DMA_EP_ISTS_EP_IN0	BIT(16)
#define gadget_to_cdns2_device(g) (container_of(g, struct cdns2_device, gadget))
#define ep_to_cdns2_ep(ep) (container_of(ep, struct cdns2_endpoint, endpoint))
#define TRBS_PER_SEGMENT	600
#define ISO_MAX_INTERVAL	8
#define MAX_TRB_LENGTH		BIT(16)
#define MAX_ISO_SIZE		3076
#define TRB_MAX_ISO_BUFF_SHIFT	12
#define TRB_MAX_ISO_BUFF_SIZE	BIT(TRB_MAX_ISO_BUFF_SHIFT)
#define TRB_BUFF_LEN_UP_TO_BOUNDARY(addr) (TRB_MAX_ISO_BUFF_SIZE - \
					((addr) & (TRB_MAX_ISO_BUFF_SIZE - 1)))
#if TRBS_PER_SEGMENT < 2
#error "Incorrect TRBS_PER_SEGMENT. Minimal Transfer Ring size is 2."
#endif
struct cdns2_trb {
	__le32 buffer;
	__le32 length;
	__le32 control;
};
#define TRB_SIZE		(sizeof(struct cdns2_trb))
#define TRB_ISO_RESERVED	2
#define TR_SEG_SIZE		(TRB_SIZE * (TRBS_PER_SEGMENT + TRB_ISO_RESERVED))
#define TRB_TYPE_BITMASK	GENMASK(15, 10)
#define TRB_TYPE(p)		((p) << 10)
#define TRB_FIELD_TO_TYPE(p)	(((p) & TRB_TYPE_BITMASK) >> 10)
#define TRB_NORMAL		1
#define TRB_LINK		6
#define TRB_CYCLE		BIT(0)
#define TRB_TOGGLE		BIT(1)
#define TRB_ISP			BIT(2)
#define TRB_CHAIN		BIT(4)
#define TRB_IOC			BIT(5)
#define TRB_LEN(p)		((p) & GENMASK(16, 0))
#define TRB_BURST(p)		(((p) << 24) & GENMASK(31, 24))
#define TRB_FIELD_TO_BURST(p)	(((p) & GENMASK(31, 24)) >> 24)
#define TRB_BUFFER(p)		((p) & GENMASK(31, 0))
#define USB_DEVICE_MAX_ADDRESS	127
#define CDNS2_ENDPOINTS_NUM	31
#define CDNS2_EP_ZLP_BUF_SIZE	512
struct cdns2_device;
struct cdns2_ring {
	struct cdns2_trb *trbs;
	dma_addr_t dma;
	int free_trbs;
	u8 pcs;
	u8 ccs;
	int enqueue;
	int dequeue;
};
struct cdns2_endpoint {
	struct usb_ep endpoint;
	struct list_head pending_list;
	struct list_head deferred_list;
	struct cdns2_device	*pdev;
	char name[20];
	struct cdns2_ring ring;
#define EP_ENABLED		BIT(0)
#define EP_STALLED		BIT(1)
#define EP_STALL_PENDING	BIT(2)
#define EP_WEDGE		BIT(3)
#define	EP_CLAIMED		BIT(4)
#define EP_RING_FULL		BIT(5)
#define EP_DEFERRED_DRDY	BIT(6)
	u32 ep_state;
	u8 idx;
	u8 dir;
	u8 num;
	u8 type;
	int interval;
	u8 buffering;
	u8 trb_burst_size;
	bool skip;
	unsigned int wa1_set:1;
	struct cdns2_trb *wa1_trb;
	unsigned int wa1_trb_index;
	unsigned int wa1_cycle_bit:1;
};
struct cdns2_request {
	struct usb_request request;
	struct cdns2_endpoint *pep;
	struct cdns2_trb *trb;
	int start_trb;
	int end_trb;
	struct list_head list;
	int finished_trb;
	int num_of_trb;
};
#define to_cdns2_request(r) (container_of(r, struct cdns2_request, request))
#define CDNS2_SETUP_STAGE		0x0
#define CDNS2_DATA_STAGE		0x1
#define CDNS2_STATUS_STAGE		0x2
struct cdns2_device {
	struct device *dev;
	struct usb_gadget gadget;
	struct usb_gadget_driver *gadget_driver;
	spinlock_t lock;
	int irq;
	void __iomem *regs;
	struct cdns2_usb_regs __iomem *usb_regs;
	struct cdns2_ep0_regs __iomem *ep0_regs;
	struct cdns2_epx_regs __iomem *epx_regs;
	struct cdns2_interrupt_regs __iomem *interrupt_regs;
	struct cdns2_adma_regs __iomem *adma_regs;
	struct dma_pool *eps_dma_pool;
	struct usb_ctrlrequest setup;
	struct cdns2_request ep0_preq;
	u8 ep0_stage;
	void *zlp_buf;
	u8 dev_address;
	struct cdns2_endpoint eps[CDNS2_ENDPOINTS_NUM];
	u32 selected_ep;
	bool is_selfpowered;
	bool may_wakeup;
	bool status_completion_no_call;
	bool in_lpm;
	struct work_struct pending_status_wq;
	struct usb_request *pending_status_request;
	u32 eps_supported;
	u8 burst_opt[MAX_ISO_SIZE + 1];
	u16 onchip_tx_buf;
	u16 onchip_rx_buf;
};
#define CDNS2_IF_EP_EXIST(pdev, ep_num, dir) \
			 ((pdev)->eps_supported & \
			 (BIT(ep_num) << ((dir) ? 0 : 16)))
dma_addr_t cdns2_trb_virt_to_dma(struct cdns2_endpoint *pep,
				 struct cdns2_trb *trb);
void cdns2_pending_setup_status_handler(struct work_struct *work);
void cdns2_select_ep(struct cdns2_device *pdev, u32 ep);
struct cdns2_request *cdns2_next_preq(struct list_head *list);
struct usb_request *cdns2_gadget_ep_alloc_request(struct usb_ep *ep,
						  gfp_t gfp_flags);
void cdns2_gadget_ep_free_request(struct usb_ep *ep,
				  struct usb_request *request);
int cdns2_gadget_ep_dequeue(struct usb_ep *ep, struct usb_request *request);
void cdns2_gadget_giveback(struct cdns2_endpoint *pep,
			   struct cdns2_request *priv_req,
			   int status);
void cdns2_init_ep0(struct cdns2_device *pdev, struct cdns2_endpoint *pep);
void cdns2_ep0_config(struct cdns2_device *pdev);
void cdns2_handle_ep0_interrupt(struct cdns2_device *pdev, int dir);
void cdns2_handle_setup_packet(struct cdns2_device *pdev);
int cdns2_gadget_resume(struct cdns2_device *pdev, bool hibernated);
int cdns2_gadget_suspend(struct cdns2_device *pdev);
void cdns2_gadget_remove(struct cdns2_device *pdev);
int cdns2_gadget_init(struct cdns2_device *pdev);
void set_reg_bit_8(void __iomem *ptr, u8 mask);
int cdns2_halt_endpoint(struct cdns2_device *pdev, struct cdns2_endpoint *pep,
			int value);
#endif  
