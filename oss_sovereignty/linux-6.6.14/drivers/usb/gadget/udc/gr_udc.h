
 

 

#define GR_MAXEP	16	 

struct gr_epregs {
	u32 epctrl;
	union {
		struct {  
			u32 slvctrl;
			u32 slvdata;
		};
		struct {  
			u32 dmactrl;
			u32 dmaaddr;
		};
	};
	u32 epstat;
};

struct gr_regs {
	struct gr_epregs	epo[GR_MAXEP];	 
	struct gr_epregs	epi[GR_MAXEP];	 
	u32			control;	 
	u32			status;		 
};

#define GR_EPCTRL_BUFSZ_SCALER	8
#define GR_EPCTRL_BUFSZ_MASK	0xffe00000
#define GR_EPCTRL_BUFSZ_POS	21
#define GR_EPCTRL_PI		BIT(20)
#define GR_EPCTRL_CB		BIT(19)
#define GR_EPCTRL_CS		BIT(18)
#define GR_EPCTRL_MAXPL_MASK	0x0003ff80
#define GR_EPCTRL_MAXPL_POS	7
#define GR_EPCTRL_NT_MASK	0x00000060
#define GR_EPCTRL_NT_POS	5
#define GR_EPCTRL_TT_MASK	0x00000018
#define GR_EPCTRL_TT_POS	3
#define GR_EPCTRL_EH		BIT(2)
#define GR_EPCTRL_ED		BIT(1)
#define GR_EPCTRL_EV		BIT(0)

#define GR_DMACTRL_AE		BIT(10)
#define GR_DMACTRL_AD		BIT(3)
#define GR_DMACTRL_AI		BIT(2)
#define GR_DMACTRL_IE		BIT(1)
#define GR_DMACTRL_DA		BIT(0)

#define GR_EPSTAT_PT		BIT(29)
#define GR_EPSTAT_PR		BIT(29)
#define GR_EPSTAT_B1CNT_MASK	0x1fff0000
#define GR_EPSTAT_B1CNT_POS	16
#define GR_EPSTAT_B0CNT_MASK	0x0000fff8
#define GR_EPSTAT_B0CNT_POS	3
#define GR_EPSTAT_B1		BIT(2)
#define GR_EPSTAT_B0		BIT(1)
#define GR_EPSTAT_BS		BIT(0)

#define GR_CONTROL_SI		BIT(31)
#define GR_CONTROL_UI		BIT(30)
#define GR_CONTROL_VI		BIT(29)
#define GR_CONTROL_SP		BIT(28)
#define GR_CONTROL_FI		BIT(27)
#define GR_CONTROL_EP		BIT(14)
#define GR_CONTROL_DH		BIT(13)
#define GR_CONTROL_RW		BIT(12)
#define GR_CONTROL_TS_MASK	0x00000e00
#define GR_CONTROL_TS_POS	9
#define GR_CONTROL_TM		BIT(8)
#define GR_CONTROL_UA_MASK	0x000000fe
#define GR_CONTROL_UA_POS	1
#define GR_CONTROL_SU		BIT(0)

#define GR_STATUS_NEPI_MASK	0xf0000000
#define GR_STATUS_NEPI_POS	28
#define GR_STATUS_NEPO_MASK	0x0f000000
#define GR_STATUS_NEPO_POS	24
#define GR_STATUS_DM		BIT(23)
#define GR_STATUS_SU		BIT(17)
#define GR_STATUS_UR		BIT(16)
#define GR_STATUS_VB		BIT(15)
#define GR_STATUS_SP		BIT(14)
#define GR_STATUS_AF_MASK	0x00003800
#define GR_STATUS_AF_POS	11
#define GR_STATUS_FN_MASK	0x000007ff
#define GR_STATUS_FN_POS	0


#define MAX_CTRL_PL_SIZE 64  

 

 

struct gr_dma_desc {
	u32 ctrl;
	u32 data;
	u32 next;

	 
	u32 paddr;
	struct gr_dma_desc *next_desc;
};

#define GR_DESC_OUT_CTRL_SE		BIT(17)
#define GR_DESC_OUT_CTRL_IE		BIT(15)
#define GR_DESC_OUT_CTRL_NX		BIT(14)
#define GR_DESC_OUT_CTRL_EN		BIT(13)
#define GR_DESC_OUT_CTRL_LEN_MASK	0x00001fff

#define GR_DESC_IN_CTRL_MO		BIT(18)
#define GR_DESC_IN_CTRL_PI		BIT(17)
#define GR_DESC_IN_CTRL_ML		BIT(16)
#define GR_DESC_IN_CTRL_IE		BIT(15)
#define GR_DESC_IN_CTRL_NX		BIT(14)
#define GR_DESC_IN_CTRL_EN		BIT(13)
#define GR_DESC_IN_CTRL_LEN_MASK	0x00001fff

#define GR_DESC_DMAADDR_MASK		0xfffffffc

struct gr_ep {
	struct usb_ep ep;
	struct gr_udc *dev;
	u16 bytes_per_buffer;
	unsigned int dma_start;
	struct gr_epregs __iomem *regs;

	unsigned num:8;
	unsigned is_in:1;
	unsigned stopped:1;
	unsigned wedged:1;
	unsigned callback:1;

	 
	struct list_head queue;

	struct list_head ep_list;

	 
	void *tailbuf;
	dma_addr_t tailbuf_paddr;
};

struct gr_request {
	struct usb_request req;
	struct list_head queue;

	 
	struct gr_dma_desc *first_desc;  
	struct gr_dma_desc *curr_desc;  
	struct gr_dma_desc *last_desc;  

	u16 evenlen;  
	u16 oddlen;  

	u8 setup;  
};

enum gr_ep0state {
	GR_EP0_DISCONNECT = 0,	 
	GR_EP0_SETUP,		 
	GR_EP0_IDATA,		 
	GR_EP0_ODATA,		 
	GR_EP0_ISTATUS,		 
	GR_EP0_OSTATUS,		 
	GR_EP0_STALL,		 
	GR_EP0_SUSPEND,		 
};

struct gr_udc {
	struct usb_gadget gadget;
	struct gr_ep epi[GR_MAXEP];
	struct gr_ep epo[GR_MAXEP];
	struct usb_gadget_driver *driver;
	struct dma_pool *desc_pool;
	struct device *dev;

	enum gr_ep0state ep0state;
	struct gr_request *ep0reqo;
	struct gr_request *ep0reqi;

	struct gr_regs __iomem *regs;
	int irq;
	int irqi;
	int irqo;

	unsigned added:1;
	unsigned irq_enabled:1;
	unsigned remote_wakeup:1;

	u8 test_mode;

	enum usb_device_state suspended_from;

	unsigned int nepi;
	unsigned int nepo;

	struct list_head ep_list;

	spinlock_t lock;  
};

#define to_gr_udc(gadget)	(container_of((gadget), struct gr_udc, gadget))
