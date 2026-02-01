 
#ifndef __ASPEED_VHUB_H
#define __ASPEED_VHUB_H

#include <linux/usb.h>
#include <linux/usb/ch11.h>

 

#define	AST_VHUB_CTRL		0x00	 
#define	AST_VHUB_CONF		0x04	 
#define	AST_VHUB_IER		0x08	 
#define	AST_VHUB_ISR		0x0C	 
#define	AST_VHUB_EP_ACK_IER	0x10	 
#define	AST_VHUB_EP_NACK_IER	0x14	 
#define AST_VHUB_EP_ACK_ISR	0x18	 
#define AST_VHUB_EP_NACK_ISR	0x1C	 
#define AST_VHUB_SW_RESET	0x20	 
#define AST_VHUB_USBSTS		0x24	 
#define AST_VHUB_EP_TOGGLE	0x28	 
#define AST_VHUB_ISO_FAIL_ACC	0x2C	 
#define AST_VHUB_EP0_CTRL	0x30	 
#define AST_VHUB_EP0_DATA	0x34	 
#define AST_VHUB_EP1_CTRL	0x38	 
#define AST_VHUB_EP1_STS_CHG	0x3C	 
#define AST_VHUB_SETUP0		0x80	 
#define AST_VHUB_SETUP1		0x84	 

 
#define VHUB_CTRL_PHY_CLK			(1 << 31)
#define VHUB_CTRL_PHY_LOOP_TEST			(1 << 25)
#define VHUB_CTRL_DN_PWN			(1 << 24)
#define VHUB_CTRL_DP_PWN			(1 << 23)
#define VHUB_CTRL_LONG_DESC			(1 << 18)
#define VHUB_CTRL_ISO_RSP_CTRL			(1 << 17)
#define VHUB_CTRL_SPLIT_IN			(1 << 16)
#define VHUB_CTRL_LOOP_T_RESULT			(1 << 15)
#define VHUB_CTRL_LOOP_T_STS			(1 << 14)
#define VHUB_CTRL_PHY_BIST_RESULT		(1 << 13)
#define VHUB_CTRL_PHY_BIST_CTRL			(1 << 12)
#define VHUB_CTRL_PHY_RESET_DIS			(1 << 11)
#define VHUB_CTRL_SET_TEST_MODE(x)		((x) << 8)
#define VHUB_CTRL_MANUAL_REMOTE_WAKEUP		(1 << 4)
#define VHUB_CTRL_AUTO_REMOTE_WAKEUP		(1 << 3)
#define VHUB_CTRL_CLK_STOP_SUSPEND		(1 << 2)
#define VHUB_CTRL_FULL_SPEED_ONLY		(1 << 1)
#define VHUB_CTRL_UPSTREAM_CONNECT		(1 << 0)

 
#define VHUB_IRQ_DEV1_BIT			9
#define VHUB_IRQ_USB_CMD_DEADLOCK		(1 << 18)
#define VHUB_IRQ_EP_POOL_NAK			(1 << 17)
#define VHUB_IRQ_EP_POOL_ACK_STALL		(1 << 16)
#define VHUB_IRQ_DEVICE1			(1 << (VHUB_IRQ_DEV1_BIT))
#define VHUB_IRQ_BUS_RESUME			(1 << 8)
#define VHUB_IRQ_BUS_SUSPEND 			(1 << 7)
#define VHUB_IRQ_BUS_RESET 			(1 << 6)
#define VHUB_IRQ_HUB_EP1_IN_DATA_ACK		(1 << 5)
#define VHUB_IRQ_HUB_EP0_IN_DATA_NAK		(1 << 4)
#define VHUB_IRQ_HUB_EP0_IN_ACK_STALL		(1 << 3)
#define VHUB_IRQ_HUB_EP0_OUT_NAK		(1 << 2)
#define VHUB_IRQ_HUB_EP0_OUT_ACK_STALL		(1 << 1)
#define VHUB_IRQ_HUB_EP0_SETUP			(1 << 0)
#define VHUB_IRQ_ACK_ALL			0x1ff

 
#define VHUB_DEV_IRQ(n)				(VHUB_IRQ_DEVICE1 << (n))

 
#define VHUB_SW_RESET_EP_POOL			(1 << 9)
#define VHUB_SW_RESET_DMA_CONTROLLER		(1 << 8)
#define VHUB_SW_RESET_DEVICE5			(1 << 5)
#define VHUB_SW_RESET_DEVICE4			(1 << 4)
#define VHUB_SW_RESET_DEVICE3			(1 << 3)
#define VHUB_SW_RESET_DEVICE2			(1 << 2)
#define VHUB_SW_RESET_DEVICE1			(1 << 1)
#define VHUB_SW_RESET_ROOT_HUB			(1 << 0)

 
#define VHUB_EP_IRQ(n)				(1 << (n))

 
#define VHUB_USBSTS_HISPEED			(1 << 27)

 
#define VHUB_EP_TOGGLE_VALUE			(1 << 8)
#define VHUB_EP_TOGGLE_SET_EPNUM(x)		((x) & 0x1f)

 
#define VHUB_EP0_CTRL_STALL			(1 << 0)
#define VHUB_EP0_TX_BUFF_RDY			(1 << 1)
#define VHUB_EP0_RX_BUFF_RDY			(1 << 2)
#define VHUB_EP0_RX_LEN(x)			(((x) >> 16) & 0x7f)
#define VHUB_EP0_SET_TX_LEN(x)			(((x) & 0x7f) << 8)

 
#define VHUB_EP1_CTRL_RESET_TOGGLE		(1 << 2)
#define VHUB_EP1_CTRL_STALL			(1 << 1)
#define VHUB_EP1_CTRL_ENABLE			(1 << 0)

 
#define AST_VHUB_DEV_EN_CTRL		0x00
#define AST_VHUB_DEV_ISR		0x04
#define AST_VHUB_DEV_EP0_CTRL		0x08
#define AST_VHUB_DEV_EP0_DATA		0x0c

 
#define VHUB_DEV_EN_SET_ADDR(x)			((x) << 8)
#define VHUB_DEV_EN_ADDR_MASK			((0xff) << 8)
#define VHUB_DEV_EN_EP0_NAK_IRQEN		(1 << 6)
#define VHUB_DEV_EN_EP0_IN_ACK_IRQEN		(1 << 5)
#define VHUB_DEV_EN_EP0_OUT_NAK_IRQEN		(1 << 4)
#define VHUB_DEV_EN_EP0_OUT_ACK_IRQEN		(1 << 3)
#define VHUB_DEV_EN_EP0_SETUP_IRQEN		(1 << 2)
#define VHUB_DEV_EN_SPEED_SEL_HIGH		(1 << 1)
#define VHUB_DEV_EN_ENABLE_PORT			(1 << 0)

 
#define VHUV_DEV_IRQ_EP0_IN_DATA_NACK		(1 << 4)
#define VHUV_DEV_IRQ_EP0_IN_ACK_STALL		(1 << 3)
#define VHUV_DEV_IRQ_EP0_OUT_DATA_NACK		(1 << 2)
#define VHUV_DEV_IRQ_EP0_OUT_ACK_STALL		(1 << 1)
#define VHUV_DEV_IRQ_EP0_SETUP			(1 << 0)

 
#define VHUB_DEV_EP0_CTRL_STALL			VHUB_EP0_CTRL_STALL
#define VHUB_DEV_EP0_TX_BUFF_RDY		VHUB_EP0_TX_BUFF_RDY
#define VHUB_DEV_EP0_RX_BUFF_RDY		VHUB_EP0_RX_BUFF_RDY
#define VHUB_DEV_EP0_RX_LEN(x)			VHUB_EP0_RX_LEN(x)
#define VHUB_DEV_EP0_SET_TX_LEN(x)		VHUB_EP0_SET_TX_LEN(x)

 

#define AST_VHUB_EP_CONFIG		0x00
#define AST_VHUB_EP_DMA_CTLSTAT		0x04
#define AST_VHUB_EP_DESC_BASE		0x08
#define AST_VHUB_EP_DESC_STATUS		0x0C

 
#define VHUB_EP_CFG_SET_MAX_PKT(x)	(((x) & 0x3ff) << 16)
#define VHUB_EP_CFG_AUTO_DATA_DISABLE	(1 << 13)
#define VHUB_EP_CFG_STALL_CTRL		(1 << 12)
#define VHUB_EP_CFG_SET_EP_NUM(x)	(((x) & 0xf) << 8)
#define VHUB_EP_CFG_SET_TYPE(x)		((x) << 5)
#define   EP_TYPE_OFF			0
#define   EP_TYPE_BULK			1
#define   EP_TYPE_INT			2
#define   EP_TYPE_ISO			3
#define VHUB_EP_CFG_DIR_OUT		(1 << 4)
#define VHUB_EP_CFG_SET_DEV(x)		((x) << 1)
#define VHUB_EP_CFG_ENABLE		(1 << 0)

 
#define VHUB_EP_DMA_PROC_STATUS(x)	(((x) >> 4) & 0xf)
#define   EP_DMA_PROC_RX_IDLE		0
#define   EP_DMA_PROC_TX_IDLE		8
#define VHUB_EP_DMA_IN_LONG_MODE	(1 << 3)
#define VHUB_EP_DMA_OUT_CONTIG_MODE	(1 << 3)
#define VHUB_EP_DMA_CTRL_RESET		(1 << 2)
#define VHUB_EP_DMA_SINGLE_STAGE	(1 << 1)
#define VHUB_EP_DMA_DESC_MODE		(1 << 0)

 
#define VHUB_EP_DMA_SET_TX_SIZE(x)	((x) << 16)
#define VHUB_EP_DMA_TX_SIZE(x)		(((x) >> 16) & 0x7ff)
#define VHUB_EP_DMA_RPTR(x)		(((x) >> 8) & 0xff)
#define VHUB_EP_DMA_SET_RPTR(x)		(((x) & 0xff) << 8)
#define VHUB_EP_DMA_SET_CPU_WPTR(x)	(x)
#define VHUB_EP_DMA_SINGLE_KICK		(1 << 0)  

 

 
#define VHUB_DSC1_IN_INTERRUPT		(1 << 31)
#define VHUB_DSC1_IN_SPID_DATA0		(0 << 14)
#define VHUB_DSC1_IN_SPID_DATA2		(1 << 14)
#define VHUB_DSC1_IN_SPID_DATA1		(2 << 14)
#define VHUB_DSC1_IN_SPID_MDATA		(3 << 14)
#define VHUB_DSC1_IN_SET_LEN(x)		((x) & 0xfff)
#define VHUB_DSC1_IN_LEN(x)		((x) & 0xfff)

 

 
#define AST_VHUB_NUM_GEN_EPs	15	 
#define AST_VHUB_NUM_PORTS	5	 
#define AST_VHUB_EP0_MAX_PACKET	64	 
#define AST_VHUB_EPn_MAX_PACKET	1024	 
#define AST_VHUB_DESCS_COUNT	256	 

struct ast_vhub;
struct ast_vhub_dev;

 
struct ast_vhub_desc {
	__le32	w0;
	__le32	w1;
};

 
struct ast_vhub_req {
	struct usb_request	req;
	struct list_head	queue;

	 
	unsigned int		act_count;

	 
	int			last_desc;

	 
	bool			active  : 1;

	 
	bool			internal : 1;
};
#define to_ast_req(__ureq) container_of(__ureq, struct ast_vhub_req, req)

 
enum ep0_state {
	ep0_state_token,
	ep0_state_data,
	ep0_state_status,
	ep0_state_stall,
};

 
struct ast_vhub_ep {
	struct usb_ep		ep;

	 
	struct list_head	queue;

	 
	unsigned int		d_idx;

	 
	struct ast_vhub_dev	*dev;

	 
	struct ast_vhub		*vhub;

	 
	void			*buf;
	dma_addr_t		buf_dma;

	 
	union {
		 
		struct {
			 
			void __iomem		*ctlstat;
			void __iomem		*setup;

			 
			enum ep0_state		state;
			bool			dir_in;

			 
			struct ast_vhub_req	req;
		} ep0;

		 
		struct {
			 
			void __iomem   		*regs;

			 
			unsigned int		g_idx;

			 
			struct ast_vhub_desc	*descs;
			dma_addr_t		descs_dma;
			unsigned int		d_next;
			unsigned int		d_last;
			unsigned int		dma_conf;

			 
			unsigned int		chunk_max;

			 
			bool			is_in :  1;
			bool			is_iso : 1;
			bool			stalled : 1;
			bool			wedged : 1;
			bool			enabled : 1;
			bool			desc_mode : 1;
		} epn;
	};
};
#define to_ast_ep(__uep) container_of(__uep, struct ast_vhub_ep, ep)

 
struct ast_vhub_dev {
	struct ast_vhub			*vhub;
	void __iomem			*regs;

	 
	unsigned int			index;
	const char			*name;

	 
	struct device			*port_dev;

	 
	struct usb_gadget		gadget;
	struct usb_gadget_driver	*driver;
	bool				registered : 1;
	bool				wakeup_en : 1;
	bool				enabled : 1;

	 
	struct ast_vhub_ep		ep0;
	struct ast_vhub_ep		**epns;
	u32				max_epns;

};
#define to_ast_dev(__g) container_of(__g, struct ast_vhub_dev, gadget)

 
struct ast_vhub_port {
	 
	u16			status;
	u16			change;

	 
	struct ast_vhub_dev	dev;
};

struct ast_vhub_full_cdesc {
	struct usb_config_descriptor	cfg;
	struct usb_interface_descriptor intf;
	struct usb_endpoint_descriptor	ep;
} __packed;

 
struct ast_vhub {
	struct platform_device		*pdev;
	void __iomem			*regs;
	int				irq;
	spinlock_t			lock;
	struct work_struct		wake_work;
	struct clk			*clk;

	 
	void				*ep0_bufs;
	dma_addr_t			ep0_bufs_dma;

	 
	struct ast_vhub_ep		ep0;

	 
	bool				ep1_stalled : 1;

	 
	struct ast_vhub_port		*ports;
	u32				max_ports;
	u32				port_irq_mask;

	 
	struct ast_vhub_ep		*epns;
	u32				max_epns;

	 
	bool				suspended : 1;

	 
	bool				wakeup_en : 1;

	 
	bool				force_usb1 : 1;

	 
	unsigned int			speed;

	 
	struct usb_device_descriptor	vhub_dev_desc;
	struct ast_vhub_full_cdesc	vhub_conf_desc;
	struct usb_hub_descriptor	vhub_hub_desc;
	struct list_head		vhub_str_desc;
	struct usb_qualifier_descriptor	vhub_qual_desc;
};

 
enum std_req_rc {
	std_req_stall = -1,	 
	std_req_complete = 0,	 
	std_req_data = 1,	 
	std_req_driver = 2,	 
};

#ifdef CONFIG_USB_GADGET_VERBOSE
#define UDCVDBG(u, fmt...)	dev_dbg(&(u)->pdev->dev, fmt)

#define EPVDBG(ep, fmt, ...)	do {			\
	dev_dbg(&(ep)->vhub->pdev->dev,			\
		"%s:EP%d " fmt,				\
		(ep)->dev ? (ep)->dev->name : "hub",	\
		(ep)->d_idx, ##__VA_ARGS__);		\
	} while(0)

#define DVDBG(d, fmt, ...)	do {			\
	dev_dbg(&(d)->vhub->pdev->dev,			\
		"%s " fmt, (d)->name,			\
		##__VA_ARGS__);				\
	} while(0)

#else
#define UDCVDBG(u, fmt...)	do { } while(0)
#define EPVDBG(ep, fmt, ...)	do { } while(0)
#define DVDBG(d, fmt, ...)	do { } while(0)
#endif

#ifdef CONFIG_USB_GADGET_DEBUG
#define UDCDBG(u, fmt...)	dev_dbg(&(u)->pdev->dev, fmt)

#define EPDBG(ep, fmt, ...)	do {			\
	dev_dbg(&(ep)->vhub->pdev->dev,			\
		"%s:EP%d " fmt,				\
		(ep)->dev ? (ep)->dev->name : "hub",	\
		(ep)->d_idx, ##__VA_ARGS__);		\
	} while(0)

#define DDBG(d, fmt, ...)	do {			\
	dev_dbg(&(d)->vhub->pdev->dev,			\
		"%s " fmt, (d)->name,			\
		##__VA_ARGS__);				\
	} while(0)
#else
#define UDCDBG(u, fmt...)	do { } while(0)
#define EPDBG(ep, fmt, ...)	do { } while(0)
#define DDBG(d, fmt, ...)	do { } while(0)
#endif

static inline void vhub_dma_workaround(void *addr)
{
	 
	mb();
	(void)__raw_readl((void __iomem *)addr);
}

 
void ast_vhub_done(struct ast_vhub_ep *ep, struct ast_vhub_req *req,
		   int status);
void ast_vhub_nuke(struct ast_vhub_ep *ep, int status);
struct usb_request *ast_vhub_alloc_request(struct usb_ep *u_ep,
					   gfp_t gfp_flags);
void ast_vhub_free_request(struct usb_ep *u_ep, struct usb_request *u_req);
void ast_vhub_init_hw(struct ast_vhub *vhub);

 
void ast_vhub_ep0_handle_ack(struct ast_vhub_ep *ep, bool in_ack);
void ast_vhub_ep0_handle_setup(struct ast_vhub_ep *ep);
void ast_vhub_reset_ep0(struct ast_vhub_dev *dev);
void ast_vhub_init_ep0(struct ast_vhub *vhub, struct ast_vhub_ep *ep,
		       struct ast_vhub_dev *dev);
int ast_vhub_reply(struct ast_vhub_ep *ep, char *ptr, int len);
int __ast_vhub_simple_reply(struct ast_vhub_ep *ep, int len, ...);
#define ast_vhub_simple_reply(udc, ...)					       \
	__ast_vhub_simple_reply((udc),					       \
			       sizeof((u8[]) { __VA_ARGS__ })/sizeof(u8),      \
			       __VA_ARGS__)

 
int ast_vhub_init_hub(struct ast_vhub *vhub);
enum std_req_rc ast_vhub_std_hub_request(struct ast_vhub_ep *ep,
					 struct usb_ctrlrequest *crq);
enum std_req_rc ast_vhub_class_hub_request(struct ast_vhub_ep *ep,
					   struct usb_ctrlrequest *crq);
void ast_vhub_device_connect(struct ast_vhub *vhub, unsigned int port,
			     bool on);
void ast_vhub_hub_suspend(struct ast_vhub *vhub);
void ast_vhub_hub_resume(struct ast_vhub *vhub);
void ast_vhub_hub_reset(struct ast_vhub *vhub);
void ast_vhub_hub_wake_all(struct ast_vhub *vhub);

 
int ast_vhub_init_dev(struct ast_vhub *vhub, unsigned int idx);
void ast_vhub_del_dev(struct ast_vhub_dev *d);
void ast_vhub_dev_irq(struct ast_vhub_dev *d);
int ast_vhub_std_dev_request(struct ast_vhub_ep *ep,
			     struct usb_ctrlrequest *crq);

 
void ast_vhub_epn_ack_irq(struct ast_vhub_ep *ep);
void ast_vhub_update_epn_stall(struct ast_vhub_ep *ep);
struct ast_vhub_ep *ast_vhub_alloc_epn(struct ast_vhub_dev *d, u8 addr);
void ast_vhub_dev_suspend(struct ast_vhub_dev *d);
void ast_vhub_dev_resume(struct ast_vhub_dev *d);
void ast_vhub_dev_reset(struct ast_vhub_dev *d);

#endif  
