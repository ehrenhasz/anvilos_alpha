



#ifndef __LINUX_XHCI_HCD_H
#define __LINUX_XHCI_HCD_H

#include <linux/usb.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/usb/hcd.h>
#include <linux/io-64-nonatomic-lo-hi.h>


#include	"xhci-ext-caps.h"
#include "pci-quirks.h"


#define XHCI_MSG_MAX		500


#define XHCI_SBRN_OFFSET	(0x60)


#define MAX_HC_SLOTS		256

#define MAX_HC_PORTS		127




struct xhci_cap_regs {
	__le32	hc_capbase;
	__le32	hcs_params1;
	__le32	hcs_params2;
	__le32	hcs_params3;
	__le32	hcc_params;
	__le32	db_off;
	__le32	run_regs_off;
	__le32	hcc_params2; 
	
};



#define HC_LENGTH(p)		XHCI_HC_LENGTH(p)

#define HC_VERSION(p)		(((p) >> 16) & 0xffff)



#define HCS_MAX_SLOTS(p)	(((p) >> 0) & 0xff)
#define HCS_SLOTS_MASK		0xff

#define HCS_MAX_INTRS(p)	(((p) >> 8) & 0x7ff)

#define HCS_MAX_PORTS(p)	(((p) >> 24) & 0x7f)



#define HCS_IST(p)		(((p) >> 0) & 0xf)

#define HCS_ERST_MAX(p)		(((p) >> 4) & 0xf)



#define HCS_MAX_SCRATCHPAD(p)   ((((p) >> 16) & 0x3e0) | (((p) >> 27) & 0x1f))



#define HCS_U1_LATENCY(p)	(((p) >> 0) & 0xff)

#define HCS_U2_LATENCY(p)	(((p) >> 16) & 0xffff)



#define HCC_64BIT_ADDR(p)	((p) & (1 << 0))

#define HCC_BANDWIDTH_NEG(p)	((p) & (1 << 1))

#define HCC_64BYTE_CONTEXT(p)	((p) & (1 << 2))

#define HCC_PPC(p)		((p) & (1 << 3))

#define HCS_INDICATOR(p)	((p) & (1 << 4))

#define HCC_LIGHT_RESET(p)	((p) & (1 << 5))

#define HCC_LTC(p)		((p) & (1 << 6))

#define HCC_NSS(p)		((p) & (1 << 7))

#define HCC_SPC(p)		((p) & (1 << 9))

#define HCC_CFC(p)		((p) & (1 << 11))

#define HCC_MAX_PSA(p)		(1 << ((((p) >> 12) & 0xf) + 1))

#define HCC_EXT_CAPS(p)		XHCI_HCC_EXT_CAPS(p)

#define CTX_SIZE(_hcc)		(HCC_64BYTE_CONTEXT(_hcc) ? 64 : 32)


#define	DBOFF_MASK	(~0x3)


#define	RTSOFF_MASK	(~0x1f)



#define	HCC2_U3C(p)		((p) & (1 << 0))

#define	HCC2_CMC(p)		((p) & (1 << 1))

#define	HCC2_FSC(p)		((p) & (1 << 2))

#define	HCC2_CTC(p)		((p) & (1 << 3))

#define	HCC2_LEC(p)		((p) & (1 << 4))

#define	HCC2_CIC(p)		((p) & (1 << 5))

#define	HCC2_ETC(p)		((p) & (1 << 6))


#define	NUM_PORT_REGS	4

#define PORTSC		0
#define PORTPMSC	1
#define PORTLI		2
#define PORTHLPMC	3


struct xhci_op_regs {
	__le32	command;
	__le32	status;
	__le32	page_size;
	__le32	reserved1;
	__le32	reserved2;
	__le32	dev_notification;
	__le64	cmd_ring;
	
	__le32	reserved3[4];
	__le64	dcbaa_ptr;
	__le32	config_reg;
	
	__le32	reserved4[241];
	
	__le32	port_status_base;
	__le32	port_power_base;
	__le32	port_link_base;
	__le32	reserved5;
	
	__le32	reserved6[NUM_PORT_REGS*254];
};



#define CMD_RUN		XHCI_CMD_RUN

#define CMD_RESET	(1 << 1)

#define CMD_EIE		XHCI_CMD_EIE

#define CMD_HSEIE	XHCI_CMD_HSEIE


#define CMD_LRESET	(1 << 7)

#define CMD_CSS		(1 << 8)
#define CMD_CRS		(1 << 9)

#define CMD_EWE		XHCI_CMD_EWE

#define CMD_PM_INDEX	(1 << 11)

#define CMD_ETE		(1 << 14)


#define XHCI_RESET_LONG_USEC		(10 * 1000 * 1000)
#define XHCI_RESET_SHORT_USEC		(250 * 1000)


#define IMAN_IE		(1 << 1)
#define IMAN_IP		(1 << 0)



#define STS_HALT	XHCI_STS_HALT

#define STS_FATAL	(1 << 2)

#define STS_EINT	(1 << 3)

#define STS_PORT	(1 << 4)


#define STS_SAVE	(1 << 8)

#define STS_RESTORE	(1 << 9)

#define STS_SRE		(1 << 10)

#define STS_CNR		XHCI_STS_CNR

#define STS_HCE		(1 << 12)



#define	DEV_NOTE_MASK		(0xffff)
#define ENABLE_DEV_NOTE(x)	(1 << (x))

#define	DEV_NOTE_FWAKE		ENABLE_DEV_NOTE(1)




#define CMD_RING_PAUSE		(1 << 1)

#define CMD_RING_ABORT		(1 << 2)

#define CMD_RING_RUNNING	(1 << 3)


#define CMD_RING_RSVD_BITS	(0x3f)



#define MAX_DEVS(p)	((p) & 0xff)

#define CONFIG_U3E		(1 << 8)

#define CONFIG_CIE		(1 << 9)




#define PORT_CONNECT	(1 << 0)

#define PORT_PE		(1 << 1)


#define PORT_OC		(1 << 3)

#define PORT_RESET	(1 << 4)

#define PORT_PLS_MASK	(0xf << 5)
#define XDEV_U0		(0x0 << 5)
#define XDEV_U1		(0x1 << 5)
#define XDEV_U2		(0x2 << 5)
#define XDEV_U3		(0x3 << 5)
#define XDEV_DISABLED	(0x4 << 5)
#define XDEV_RXDETECT	(0x5 << 5)
#define XDEV_INACTIVE	(0x6 << 5)
#define XDEV_POLLING	(0x7 << 5)
#define XDEV_RECOVERY	(0x8 << 5)
#define XDEV_HOT_RESET	(0x9 << 5)
#define XDEV_COMP_MODE	(0xa << 5)
#define XDEV_TEST_MODE	(0xb << 5)
#define XDEV_RESUME	(0xf << 5)


#define PORT_POWER	(1 << 9)

#define DEV_SPEED_MASK		(0xf << 10)
#define	XDEV_FS			(0x1 << 10)
#define	XDEV_LS			(0x2 << 10)
#define	XDEV_HS			(0x3 << 10)
#define	XDEV_SS			(0x4 << 10)
#define	XDEV_SSP		(0x5 << 10)
#define DEV_UNDEFSPEED(p)	(((p) & DEV_SPEED_MASK) == (0x0<<10))
#define DEV_FULLSPEED(p)	(((p) & DEV_SPEED_MASK) == XDEV_FS)
#define DEV_LOWSPEED(p)		(((p) & DEV_SPEED_MASK) == XDEV_LS)
#define DEV_HIGHSPEED(p)	(((p) & DEV_SPEED_MASK) == XDEV_HS)
#define DEV_SUPERSPEED(p)	(((p) & DEV_SPEED_MASK) == XDEV_SS)
#define DEV_SUPERSPEEDPLUS(p)	(((p) & DEV_SPEED_MASK) == XDEV_SSP)
#define DEV_SUPERSPEED_ANY(p)	(((p) & DEV_SPEED_MASK) >= XDEV_SS)
#define DEV_PORT_SPEED(p)	(((p) >> 10) & 0x0f)


#define	SLOT_SPEED_FS		(XDEV_FS << 10)
#define	SLOT_SPEED_LS		(XDEV_LS << 10)
#define	SLOT_SPEED_HS		(XDEV_HS << 10)
#define	SLOT_SPEED_SS		(XDEV_SS << 10)
#define	SLOT_SPEED_SSP		(XDEV_SSP << 10)

#define PORT_LED_OFF	(0 << 14)
#define PORT_LED_AMBER	(1 << 14)
#define PORT_LED_GREEN	(2 << 14)
#define PORT_LED_MASK	(3 << 14)

#define PORT_LINK_STROBE	(1 << 16)

#define PORT_CSC	(1 << 17)

#define PORT_PEC	(1 << 18)

#define PORT_WRC	(1 << 19)

#define PORT_OCC	(1 << 20)

#define PORT_RC		(1 << 21)

#define PORT_PLC	(1 << 22)

#define PORT_CEC	(1 << 23)
#define PORT_CHANGE_MASK	(PORT_CSC | PORT_PEC | PORT_WRC | PORT_OCC | \
				 PORT_RC | PORT_PLC | PORT_CEC)



#define PORT_CAS	(1 << 24)

#define PORT_WKCONN_E	(1 << 25)

#define PORT_WKDISC_E	(1 << 26)

#define PORT_WKOC_E	(1 << 27)


#define PORT_DEV_REMOVE	(1 << 30)

#define PORT_WR		(1 << 31)


#define DUPLICATE_ENTRY ((u8)(-1))



#define PORT_U1_TIMEOUT(p)	((p) & 0xff)
#define PORT_U1_TIMEOUT_MASK	0xff

#define PORT_U2_TIMEOUT(p)	(((p) & 0xff) << 8)
#define PORT_U2_TIMEOUT_MASK	(0xff << 8)



#define	PORT_L1S_MASK		7
#define	PORT_L1S_SUCCESS	1
#define	PORT_RWE		(1 << 3)
#define	PORT_HIRD(p)		(((p) & 0xf) << 4)
#define	PORT_HIRD_MASK		(0xf << 4)
#define	PORT_L1DS_MASK		(0xff << 8)
#define	PORT_L1DS(p)		(((p) & 0xff) << 8)
#define	PORT_HLE		(1 << 16)
#define PORT_TEST_MODE_SHIFT	28


#define PORT_RX_LANES(p)	(((p) >> 16) & 0xf)
#define PORT_TX_LANES(p)	(((p) >> 20) & 0xf)


#define PORT_HIRDM(p)((p) & 3)
#define PORT_L1_TIMEOUT(p)(((p) & 0xff) << 2)
#define PORT_BESLD(p)(((p) & 0xf) << 10)


#define XHCI_L1_TIMEOUT		512


#define XHCI_DEFAULT_BESL	4


#define XHCI_PORT_POLLING_LFPS_TIME  36


struct xhci_intr_reg {
	__le32	irq_pending;
	__le32	irq_control;
	__le32	erst_size;
	__le32	rsvd;
	__le64	erst_base;
	__le64	erst_dequeue;
};


#define	ER_IRQ_PENDING(p)	((p) & 0x1)


#define	ER_IRQ_CLEAR(p)		((p) & 0xfffffffe)
#define	ER_IRQ_ENABLE(p)	((ER_IRQ_CLEAR(p)) | 0x2)
#define	ER_IRQ_DISABLE(p)	((ER_IRQ_CLEAR(p)) & ~(0x2))



#define ER_IRQ_INTERVAL_MASK	(0xffff)

#define ER_IRQ_COUNTER_MASK	(0xffff << 16)



#define	ERST_SIZE_MASK		(0xffff << 16)


#define ERST_BASE_RSVDP		(GENMASK_ULL(5, 0))



#define ERST_DESI_MASK		(0x7)

#define ERST_EHB		(1 << 3)
#define ERST_PTR_MASK		(0xf)


struct xhci_run_regs {
	__le32			microframe_index;
	__le32			rsvd[7];
	struct xhci_intr_reg	ir_set[128];
};


struct xhci_doorbell_array {
	__le32	doorbell[256];
};

#define DB_VALUE(ep, stream)	((((ep) + 1) & 0xff) | ((stream) << 16))
#define DB_VALUE_HOST		0x00000000


struct xhci_protocol_caps {
	u32	revision;
	u32	name_string;
	u32	port_info;
};

#define	XHCI_EXT_PORT_MAJOR(x)	(((x) >> 24) & 0xff)
#define	XHCI_EXT_PORT_MINOR(x)	(((x) >> 16) & 0xff)
#define	XHCI_EXT_PORT_PSIC(x)	(((x) >> 28) & 0x0f)
#define	XHCI_EXT_PORT_OFF(x)	((x) & 0xff)
#define	XHCI_EXT_PORT_COUNT(x)	(((x) >> 8) & 0xff)

#define	XHCI_EXT_PORT_PSIV(x)	(((x) >> 0) & 0x0f)
#define	XHCI_EXT_PORT_PSIE(x)	(((x) >> 4) & 0x03)
#define	XHCI_EXT_PORT_PLT(x)	(((x) >> 6) & 0x03)
#define	XHCI_EXT_PORT_PFD(x)	(((x) >> 8) & 0x01)
#define	XHCI_EXT_PORT_LP(x)	(((x) >> 14) & 0x03)
#define	XHCI_EXT_PORT_PSIM(x)	(((x) >> 16) & 0xffff)

#define PLT_MASK        (0x03 << 6)
#define PLT_SYM         (0x00 << 6)
#define PLT_ASYM_RX     (0x02 << 6)
#define PLT_ASYM_TX     (0x03 << 6)


struct xhci_container_ctx {
	unsigned type;
#define XHCI_CTX_TYPE_DEVICE  0x1
#define XHCI_CTX_TYPE_INPUT   0x2

	int size;

	u8 *bytes;
	dma_addr_t dma;
};


struct xhci_slot_ctx {
	__le32	dev_info;
	__le32	dev_info2;
	__le32	tt_info;
	__le32	dev_state;
	
	__le32	reserved[4];
};



#define ROUTE_STRING_MASK	(0xfffff)

#define DEV_SPEED	(0xf << 20)
#define GET_DEV_SPEED(n) (((n) & DEV_SPEED) >> 20)


#define DEV_MTT		(0x1 << 25)

#define DEV_HUB		(0x1 << 26)

#define LAST_CTX_MASK	(0x1f << 27)
#define LAST_CTX(p)	((p) << 27)
#define LAST_CTX_TO_EP_NUM(p)	(((p) >> 27) - 1)
#define SLOT_FLAG	(1 << 0)
#define EP0_FLAG	(1 << 1)



#define MAX_EXIT	(0xffff)

#define ROOT_HUB_PORT(p)	(((p) & 0xff) << 16)
#define DEVINFO_TO_ROOT_HUB_PORT(p)	(((p) >> 16) & 0xff)

#define XHCI_MAX_PORTS(p)	(((p) & 0xff) << 24)
#define DEVINFO_TO_MAX_PORTS(p)	(((p) & (0xff << 24)) >> 24)



#define TT_SLOT		(0xff)

#define TT_PORT		(0xff << 8)
#define TT_THINK_TIME(p)	(((p) & 0x3) << 16)
#define GET_TT_THINK_TIME(p)	(((p) & (0x3 << 16)) >> 16)



#define DEV_ADDR_MASK	(0xff)


#define SLOT_STATE	(0x1f << 27)
#define GET_SLOT_STATE(p)	(((p) & (0x1f << 27)) >> 27)

#define SLOT_STATE_DISABLED	0
#define SLOT_STATE_ENABLED	SLOT_STATE_DISABLED
#define SLOT_STATE_DEFAULT	1
#define SLOT_STATE_ADDRESSED	2
#define SLOT_STATE_CONFIGURED	3


struct xhci_ep_ctx {
	__le32	ep_info;
	__le32	ep_info2;
	__le64	deq;
	__le32	tx_info;
	
	__le32	reserved[3];
};



#define EP_STATE_MASK		(0x7)
#define EP_STATE_DISABLED	0
#define EP_STATE_RUNNING	1
#define EP_STATE_HALTED		2
#define EP_STATE_STOPPED	3
#define EP_STATE_ERROR		4
#define GET_EP_CTX_STATE(ctx)	(le32_to_cpu((ctx)->ep_info) & EP_STATE_MASK)


#define EP_MULT(p)		(((p) & 0x3) << 8)
#define CTX_TO_EP_MULT(p)	(((p) >> 8) & 0x3)



#define EP_INTERVAL(p)			(((p) & 0xff) << 16)
#define EP_INTERVAL_TO_UFRAMES(p)	(1 << (((p) >> 16) & 0xff))
#define CTX_TO_EP_INTERVAL(p)		(((p) >> 16) & 0xff)
#define EP_MAXPSTREAMS_MASK		(0x1f << 10)
#define EP_MAXPSTREAMS(p)		(((p) << 10) & EP_MAXPSTREAMS_MASK)
#define CTX_TO_EP_MAXPSTREAMS(p)	(((p) & EP_MAXPSTREAMS_MASK) >> 10)

#define	EP_HAS_LSA		(1 << 15)

#define CTX_TO_MAX_ESIT_PAYLOAD_HI(p)	(((p) >> 24) & 0xff)



#define	FORCE_EVENT	(0x1)
#define ERROR_COUNT(p)	(((p) & 0x3) << 1)
#define CTX_TO_EP_TYPE(p)	(((p) >> 3) & 0x7)
#define EP_TYPE(p)	((p) << 3)
#define ISOC_OUT_EP	1
#define BULK_OUT_EP	2
#define INT_OUT_EP	3
#define CTRL_EP		4
#define ISOC_IN_EP	5
#define BULK_IN_EP	6
#define INT_IN_EP	7


#define MAX_BURST(p)	(((p)&0xff) << 8)
#define CTX_TO_MAX_BURST(p)	(((p) >> 8) & 0xff)
#define MAX_PACKET(p)	(((p)&0xffff) << 16)
#define MAX_PACKET_MASK		(0xffff << 16)
#define MAX_PACKET_DECODED(p)	(((p) >> 16) & 0xffff)


#define EP_AVG_TRB_LENGTH(p)		((p) & 0xffff)
#define EP_MAX_ESIT_PAYLOAD_LO(p)	(((p) & 0xffff) << 16)
#define EP_MAX_ESIT_PAYLOAD_HI(p)	((((p) >> 16) & 0xff) << 24)
#define CTX_TO_MAX_ESIT_PAYLOAD(p)	(((p) >> 16) & 0xffff)


#define EP_CTX_CYCLE_MASK		(1 << 0)
#define SCTX_DEQ_MASK			(~0xfL)



struct xhci_input_control_ctx {
	__le32	drop_flags;
	__le32	add_flags;
	__le32	rsvd2[6];
};

#define	EP_IS_ADDED(ctrl_ctx, i) \
	(le32_to_cpu(ctrl_ctx->add_flags) & (1 << (i + 1)))
#define	EP_IS_DROPPED(ctrl_ctx, i)       \
	(le32_to_cpu(ctrl_ctx->drop_flags) & (1 << (i + 1)))


struct xhci_command {
	
	struct xhci_container_ctx	*in_ctx;
	u32				status;
	int				slot_id;
	
	struct completion		*completion;
	union xhci_trb			*command_trb;
	struct list_head		cmd_list;
};


#define	DROP_EP(x)	(0x1 << x)

#define	ADD_EP(x)	(0x1 << x)

struct xhci_stream_ctx {
	
	__le64	stream_ring;
	
	__le32	reserved[2];
};


#define	SCT_FOR_CTX(p)		(((p) & 0x7) << 1)

#define	SCT_SEC_TR		0

#define	SCT_PRI_TR		1

#define SCT_SSA_8		2
#define SCT_SSA_16		3
#define SCT_SSA_32		4
#define SCT_SSA_64		5
#define SCT_SSA_128		6
#define SCT_SSA_256		7


struct xhci_stream_info {
	struct xhci_ring		**stream_rings;
	
	unsigned int			num_streams;
	
	struct xhci_stream_ctx		*stream_ctx_array;
	unsigned int			num_stream_ctxs;
	dma_addr_t			ctx_array_dma;
	
	struct radix_tree_root		trb_address_map;
	struct xhci_command		*free_streams_command;
};

#define	SMALL_STREAM_ARRAY_SIZE		256
#define	MEDIUM_STREAM_ARRAY_SIZE	1024


struct xhci_bw_info {
	
	unsigned int		ep_interval;
	
	unsigned int		mult;
	unsigned int		num_packets;
	unsigned int		max_packet_size;
	unsigned int		max_esit_payload;
	unsigned int		type;
};


#define	FS_BLOCK	1
#define	HS_BLOCK	4
#define	SS_BLOCK	16
#define	DMI_BLOCK	32


#define DMI_OVERHEAD 8
#define DMI_OVERHEAD_BURST 4
#define SS_OVERHEAD 8
#define SS_OVERHEAD_BURST 32
#define HS_OVERHEAD 26
#define FS_OVERHEAD 20
#define LS_OVERHEAD 128

#define TT_HS_OVERHEAD (31 + 94)
#define TT_DMI_OVERHEAD (25 + 12)


#define FS_BW_LIMIT		1285
#define TT_BW_LIMIT		1320
#define HS_BW_LIMIT		1607
#define SS_BW_LIMIT_IN		3906
#define DMI_BW_LIMIT_IN		3906
#define SS_BW_LIMIT_OUT		3906
#define DMI_BW_LIMIT_OUT	3906


#define FS_BW_RESERVED		10
#define HS_BW_RESERVED		20
#define SS_BW_RESERVED		10

struct xhci_virt_ep {
	struct xhci_virt_device		*vdev;	
	unsigned int			ep_index;
	struct xhci_ring		*ring;
	
	struct xhci_stream_info		*stream_info;
	
	struct xhci_ring		*new_ring;
	unsigned int			err_count;
	unsigned int			ep_state;
#define SET_DEQ_PENDING		(1 << 0)
#define EP_HALTED		(1 << 1)	
#define EP_STOP_CMD_PENDING	(1 << 2)	

#define EP_GETTING_STREAMS	(1 << 3)
#define EP_HAS_STREAMS		(1 << 4)

#define EP_GETTING_NO_STREAMS	(1 << 5)
#define EP_HARD_CLEAR_TOGGLE	(1 << 6)
#define EP_SOFT_CLEAR_TOGGLE	(1 << 7)

#define EP_CLEARING_TT		(1 << 8)
	
	struct list_head	cancelled_td_list;
	struct xhci_hcd		*xhci;
	
	struct xhci_segment	*queued_deq_seg;
	union xhci_trb		*queued_deq_ptr;
	
	bool			skip;
	
	struct xhci_bw_info	bw_info;
	struct list_head	bw_endpoint_list;
	
	int			next_frame_id;
	
	bool			use_extended_tbc;
};

enum xhci_overhead_type {
	LS_OVERHEAD_TYPE = 0,
	FS_OVERHEAD_TYPE,
	HS_OVERHEAD_TYPE,
};

struct xhci_interval_bw {
	unsigned int		num_packets;
	
	struct list_head	endpoints;
	
	unsigned int		overhead[3];
};

#define	XHCI_MAX_INTERVAL	16

struct xhci_interval_bw_table {
	unsigned int		interval0_esit_payload;
	struct xhci_interval_bw	interval_bw[XHCI_MAX_INTERVAL];
	
	unsigned int		bw_used;
	unsigned int		ss_bw_in;
	unsigned int		ss_bw_out;
};

#define EP_CTX_PER_DEV		31

struct xhci_virt_device {
	int				slot_id;
	struct usb_device		*udev;
	
	struct xhci_container_ctx       *out_ctx;
	
	struct xhci_container_ctx       *in_ctx;
	struct xhci_virt_ep		eps[EP_CTX_PER_DEV];
	u8				fake_port;
	u8				real_port;
	struct xhci_interval_bw_table	*bw_table;
	struct xhci_tt_bw_info		*tt_info;
	
	unsigned long			flags;
#define VDEV_PORT_ERROR			BIT(0) 

	
	u16				current_mel;
	
	void				*debugfs_private;
};


struct xhci_root_port_bw_info {
	struct list_head		tts;
	unsigned int			num_active_tts;
	struct xhci_interval_bw_table	bw_table;
};

struct xhci_tt_bw_info {
	struct list_head		tt_list;
	int				slot_id;
	int				ttport;
	struct xhci_interval_bw_table	bw_table;
	int				active_eps;
};



struct xhci_device_context_array {
	
	__le64			dev_context_ptrs[MAX_HC_SLOTS];
	
	dma_addr_t	dma;
};




struct xhci_transfer_event {
	
	__le64	buffer;
	__le32	transfer_len;
	
	__le32	flags;
};



#define	EVENT_TRB_LEN(p)		((p) & 0xffffff)


#define	TRB_TO_EP_ID(p)	(((p) >> 16) & 0x1f)


#define	COMP_CODE_MASK		(0xff << 24)
#define GET_COMP_CODE(p)	(((p) & COMP_CODE_MASK) >> 24)
#define COMP_INVALID				0
#define COMP_SUCCESS				1
#define COMP_DATA_BUFFER_ERROR			2
#define COMP_BABBLE_DETECTED_ERROR		3
#define COMP_USB_TRANSACTION_ERROR		4
#define COMP_TRB_ERROR				5
#define COMP_STALL_ERROR			6
#define COMP_RESOURCE_ERROR			7
#define COMP_BANDWIDTH_ERROR			8
#define COMP_NO_SLOTS_AVAILABLE_ERROR		9
#define COMP_INVALID_STREAM_TYPE_ERROR		10
#define COMP_SLOT_NOT_ENABLED_ERROR		11
#define COMP_ENDPOINT_NOT_ENABLED_ERROR		12
#define COMP_SHORT_PACKET			13
#define COMP_RING_UNDERRUN			14
#define COMP_RING_OVERRUN			15
#define COMP_VF_EVENT_RING_FULL_ERROR		16
#define COMP_PARAMETER_ERROR			17
#define COMP_BANDWIDTH_OVERRUN_ERROR		18
#define COMP_CONTEXT_STATE_ERROR		19
#define COMP_NO_PING_RESPONSE_ERROR		20
#define COMP_EVENT_RING_FULL_ERROR		21
#define COMP_INCOMPATIBLE_DEVICE_ERROR		22
#define COMP_MISSED_SERVICE_ERROR		23
#define COMP_COMMAND_RING_STOPPED		24
#define COMP_COMMAND_ABORTED			25
#define COMP_STOPPED				26
#define COMP_STOPPED_LENGTH_INVALID		27
#define COMP_STOPPED_SHORT_PACKET		28
#define COMP_MAX_EXIT_LATENCY_TOO_LARGE_ERROR	29
#define COMP_ISOCH_BUFFER_OVERRUN		31
#define COMP_EVENT_LOST_ERROR			32
#define COMP_UNDEFINED_ERROR			33
#define COMP_INVALID_STREAM_ID_ERROR		34
#define COMP_SECONDARY_BANDWIDTH_ERROR		35
#define COMP_SPLIT_TRANSACTION_ERROR		36

static inline const char *xhci_trb_comp_code_string(u8 status)
{
	switch (status) {
	case COMP_INVALID:
		return "Invalid";
	case COMP_SUCCESS:
		return "Success";
	case COMP_DATA_BUFFER_ERROR:
		return "Data Buffer Error";
	case COMP_BABBLE_DETECTED_ERROR:
		return "Babble Detected";
	case COMP_USB_TRANSACTION_ERROR:
		return "USB Transaction Error";
	case COMP_TRB_ERROR:
		return "TRB Error";
	case COMP_STALL_ERROR:
		return "Stall Error";
	case COMP_RESOURCE_ERROR:
		return "Resource Error";
	case COMP_BANDWIDTH_ERROR:
		return "Bandwidth Error";
	case COMP_NO_SLOTS_AVAILABLE_ERROR:
		return "No Slots Available Error";
	case COMP_INVALID_STREAM_TYPE_ERROR:
		return "Invalid Stream Type Error";
	case COMP_SLOT_NOT_ENABLED_ERROR:
		return "Slot Not Enabled Error";
	case COMP_ENDPOINT_NOT_ENABLED_ERROR:
		return "Endpoint Not Enabled Error";
	case COMP_SHORT_PACKET:
		return "Short Packet";
	case COMP_RING_UNDERRUN:
		return "Ring Underrun";
	case COMP_RING_OVERRUN:
		return "Ring Overrun";
	case COMP_VF_EVENT_RING_FULL_ERROR:
		return "VF Event Ring Full Error";
	case COMP_PARAMETER_ERROR:
		return "Parameter Error";
	case COMP_BANDWIDTH_OVERRUN_ERROR:
		return "Bandwidth Overrun Error";
	case COMP_CONTEXT_STATE_ERROR:
		return "Context State Error";
	case COMP_NO_PING_RESPONSE_ERROR:
		return "No Ping Response Error";
	case COMP_EVENT_RING_FULL_ERROR:
		return "Event Ring Full Error";
	case COMP_INCOMPATIBLE_DEVICE_ERROR:
		return "Incompatible Device Error";
	case COMP_MISSED_SERVICE_ERROR:
		return "Missed Service Error";
	case COMP_COMMAND_RING_STOPPED:
		return "Command Ring Stopped";
	case COMP_COMMAND_ABORTED:
		return "Command Aborted";
	case COMP_STOPPED:
		return "Stopped";
	case COMP_STOPPED_LENGTH_INVALID:
		return "Stopped - Length Invalid";
	case COMP_STOPPED_SHORT_PACKET:
		return "Stopped - Short Packet";
	case COMP_MAX_EXIT_LATENCY_TOO_LARGE_ERROR:
		return "Max Exit Latency Too Large Error";
	case COMP_ISOCH_BUFFER_OVERRUN:
		return "Isoch Buffer Overrun";
	case COMP_EVENT_LOST_ERROR:
		return "Event Lost Error";
	case COMP_UNDEFINED_ERROR:
		return "Undefined Error";
	case COMP_INVALID_STREAM_ID_ERROR:
		return "Invalid Stream ID Error";
	case COMP_SECONDARY_BANDWIDTH_ERROR:
		return "Secondary Bandwidth Error";
	case COMP_SPLIT_TRANSACTION_ERROR:
		return "Split Transaction Error";
	default:
		return "Unknown!!";
	}
}

struct xhci_link_trb {
	
	__le64 segment_ptr;
	__le32 intr_target;
	__le32 control;
};


#define LINK_TOGGLE	(0x1<<1)


struct xhci_event_cmd {
	
	__le64 cmd_trb;
	__le32 status;
	__le32 flags;
};




#define TRB_BSR		(1<<9)


#define TRB_DC		(1<<9)


#define TRB_TSP		(1<<9)

enum xhci_ep_reset_type {
	EP_HARD_RESET,
	EP_SOFT_RESET,
};


#define TRB_TO_VF_INTR_TARGET(p)	(((p) & (0x3ff << 22)) >> 22)
#define TRB_TO_VF_ID(p)			(((p) & (0xff << 16)) >> 16)


#define TRB_TO_BELT(p)			(((p) & (0xfff << 16)) >> 16)


#define TRB_TO_DEV_SPEED(p)		(((p) & (0xf << 16)) >> 16)


#define TRB_TO_PACKET_TYPE(p)		((p) & 0x1f)
#define TRB_TO_ROOTHUB_PORT(p)		(((p) & (0xff << 24)) >> 24)

enum xhci_setup_dev {
	SETUP_CONTEXT_ONLY,
	SETUP_CONTEXT_ADDRESS,
};



#define TRB_TO_SLOT_ID(p)	(((p) & (0xff<<24)) >> 24)
#define SLOT_ID_FOR_TRB(p)	(((p) & 0xff) << 24)


#define TRB_TO_EP_INDEX(p)		((((p) & (0x1f << 16)) >> 16) - 1)
#define	EP_ID_FOR_TRB(p)		((((p) + 1) & 0x1f) << 16)

#define SUSPEND_PORT_FOR_TRB(p)		(((p) & 1) << 23)
#define TRB_TO_SUSPEND_PORT(p)		(((p) & (1 << 23)) >> 23)
#define LAST_EP_INDEX			30


#define TRB_TO_STREAM_ID(p)		((((p) & (0xffff << 16)) >> 16))
#define STREAM_ID_FOR_TRB(p)		((((p)) & 0xffff) << 16)
#define SCT_FOR_TRB(p)			(((p) << 1) & 0x7)


#define TRB_TC			(1<<1)



#define GET_PORT_ID(p)		(((p) & (0xff << 24)) >> 24)

#define EVENT_DATA		(1 << 2)



#define	TRB_LEN(p)		((p) & 0x1ffff)

#define TRB_TD_SIZE(p)          (min((p), (u32)31) << 17)
#define GET_TD_SIZE(p)		(((p) & 0x3e0000) >> 17)

#define TRB_TD_SIZE_TBC(p)      (min((p), (u32)31) << 17)

#define TRB_INTR_TARGET(p)	(((p) & 0x3ff) << 22)
#define GET_INTR_TARGET(p)	(((p) >> 22) & 0x3ff)

#define TRB_TBC(p)		(((p) & 0x3) << 7)
#define TRB_TLBPC(p)		(((p) & 0xf) << 16)


#define TRB_CYCLE		(1<<0)

#define TRB_ENT			(1<<1)

#define TRB_ISP			(1<<2)

#define TRB_NO_SNOOP		(1<<3)

#define TRB_CHAIN		(1<<4)

#define TRB_IOC			(1<<5)

#define TRB_IDT			(1<<6)

#define TRB_IDT_MAX_SIZE	8


#define	TRB_BEI			(1<<9)


#define TRB_DIR_IN		(1<<16)
#define	TRB_TX_TYPE(p)		((p) << 16)
#define	TRB_DATA_OUT		2
#define	TRB_DATA_IN		3


#define TRB_SIA			(1<<31)
#define TRB_FRAME_ID(p)		(((p) & 0x7ff) << 20)


#define TRB_CACHE_SIZE_HS	8
#define TRB_CACHE_SIZE_SS	16

struct xhci_generic_trb {
	__le32 field[4];
};

union xhci_trb {
	struct xhci_link_trb		link;
	struct xhci_transfer_event	trans_event;
	struct xhci_event_cmd		event_cmd;
	struct xhci_generic_trb		generic;
};


#define	TRB_TYPE_BITMASK	(0xfc00)
#define TRB_TYPE(p)		((p) << 10)
#define TRB_FIELD_TO_TYPE(p)	(((p) & TRB_TYPE_BITMASK) >> 10)


#define TRB_NORMAL		1

#define TRB_SETUP		2

#define TRB_DATA		3

#define TRB_STATUS		4

#define TRB_ISOC		5

#define TRB_LINK		6
#define TRB_EVENT_DATA		7

#define TRB_TR_NOOP		8


#define TRB_ENABLE_SLOT		9

#define TRB_DISABLE_SLOT	10

#define TRB_ADDR_DEV		11

#define TRB_CONFIG_EP		12

#define TRB_EVAL_CONTEXT	13

#define TRB_RESET_EP		14

#define TRB_STOP_RING		15

#define TRB_SET_DEQ		16

#define TRB_RESET_DEV		17

#define TRB_FORCE_EVENT		18

#define TRB_NEG_BANDWIDTH	19

#define TRB_SET_LT		20

#define TRB_GET_BW		21

#define TRB_FORCE_HEADER	22

#define TRB_CMD_NOOP		23



#define TRB_TRANSFER		32

#define TRB_COMPLETION		33

#define TRB_PORT_STATUS		34

#define TRB_BANDWIDTH_EVENT	35

#define TRB_DOORBELL		36

#define TRB_HC_EVENT		37

#define TRB_DEV_NOTE		38

#define TRB_MFINDEX_WRAP	39

#define TRB_VENDOR_DEFINED_LOW	48

#define	TRB_NEC_CMD_COMP	48

#define	TRB_NEC_GET_FW		49

static inline const char *xhci_trb_type_string(u8 type)
{
	switch (type) {
	case TRB_NORMAL:
		return "Normal";
	case TRB_SETUP:
		return "Setup Stage";
	case TRB_DATA:
		return "Data Stage";
	case TRB_STATUS:
		return "Status Stage";
	case TRB_ISOC:
		return "Isoch";
	case TRB_LINK:
		return "Link";
	case TRB_EVENT_DATA:
		return "Event Data";
	case TRB_TR_NOOP:
		return "No-Op";
	case TRB_ENABLE_SLOT:
		return "Enable Slot Command";
	case TRB_DISABLE_SLOT:
		return "Disable Slot Command";
	case TRB_ADDR_DEV:
		return "Address Device Command";
	case TRB_CONFIG_EP:
		return "Configure Endpoint Command";
	case TRB_EVAL_CONTEXT:
		return "Evaluate Context Command";
	case TRB_RESET_EP:
		return "Reset Endpoint Command";
	case TRB_STOP_RING:
		return "Stop Ring Command";
	case TRB_SET_DEQ:
		return "Set TR Dequeue Pointer Command";
	case TRB_RESET_DEV:
		return "Reset Device Command";
	case TRB_FORCE_EVENT:
		return "Force Event Command";
	case TRB_NEG_BANDWIDTH:
		return "Negotiate Bandwidth Command";
	case TRB_SET_LT:
		return "Set Latency Tolerance Value Command";
	case TRB_GET_BW:
		return "Get Port Bandwidth Command";
	case TRB_FORCE_HEADER:
		return "Force Header Command";
	case TRB_CMD_NOOP:
		return "No-Op Command";
	case TRB_TRANSFER:
		return "Transfer Event";
	case TRB_COMPLETION:
		return "Command Completion Event";
	case TRB_PORT_STATUS:
		return "Port Status Change Event";
	case TRB_BANDWIDTH_EVENT:
		return "Bandwidth Request Event";
	case TRB_DOORBELL:
		return "Doorbell Event";
	case TRB_HC_EVENT:
		return "Host Controller Event";
	case TRB_DEV_NOTE:
		return "Device Notification Event";
	case TRB_MFINDEX_WRAP:
		return "MFINDEX Wrap Event";
	case TRB_NEC_CMD_COMP:
		return "NEC Command Completion Event";
	case TRB_NEC_GET_FW:
		return "NET Get Firmware Revision Command";
	default:
		return "UNKNOWN";
	}
}

#define TRB_TYPE_LINK(x)	(((x) & TRB_TYPE_BITMASK) == TRB_TYPE(TRB_LINK))

#define TRB_TYPE_LINK_LE32(x)	(((x) & cpu_to_le32(TRB_TYPE_BITMASK)) == \
				 cpu_to_le32(TRB_TYPE(TRB_LINK)))
#define TRB_TYPE_NOOP_LE32(x)	(((x) & cpu_to_le32(TRB_TYPE_BITMASK)) == \
				 cpu_to_le32(TRB_TYPE(TRB_TR_NOOP)))

#define NEC_FW_MINOR(p)		(((p) >> 0) & 0xff)
#define NEC_FW_MAJOR(p)		(((p) >> 8) & 0xff)


#define TRBS_PER_SEGMENT	256

#define MAX_RSVD_CMD_TRBS	(TRBS_PER_SEGMENT - 3)
#define TRB_SEGMENT_SIZE	(TRBS_PER_SEGMENT*16)
#define TRB_SEGMENT_SHIFT	(ilog2(TRB_SEGMENT_SIZE))

#define TRB_MAX_BUFF_SHIFT		16
#define TRB_MAX_BUFF_SIZE	(1 << TRB_MAX_BUFF_SHIFT)

#define TRB_BUFF_LEN_UP_TO_BOUNDARY(addr)	(TRB_MAX_BUFF_SIZE - \
					(addr & (TRB_MAX_BUFF_SIZE - 1)))
#define MAX_SOFT_RETRY		3

#define AVOID_BEI_INTERVAL_MIN	8
#define AVOID_BEI_INTERVAL_MAX	32

struct xhci_segment {
	union xhci_trb		*trbs;
	
	struct xhci_segment	*next;
	dma_addr_t		dma;
	
	dma_addr_t		bounce_dma;
	void			*bounce_buf;
	unsigned int		bounce_offs;
	unsigned int		bounce_len;
};

enum xhci_cancelled_td_status {
	TD_DIRTY = 0,
	TD_HALTED,
	TD_CLEARING_CACHE,
	TD_CLEARED,
};

struct xhci_td {
	struct list_head	td_list;
	struct list_head	cancelled_td_list;
	int			status;
	enum xhci_cancelled_td_status	cancel_status;
	struct urb		*urb;
	struct xhci_segment	*start_seg;
	union xhci_trb		*first_trb;
	union xhci_trb		*last_trb;
	struct xhci_segment	*last_trb_seg;
	struct xhci_segment	*bounce_seg;
	
	bool			urb_length_set;
	unsigned int		num_trbs;
};


#define XHCI_CMD_DEFAULT_TIMEOUT	(5 * HZ)


struct xhci_cd {
	struct xhci_command	*command;
	union xhci_trb		*cmd_trb;
};

enum xhci_ring_type {
	TYPE_CTRL = 0,
	TYPE_ISOC,
	TYPE_BULK,
	TYPE_INTR,
	TYPE_STREAM,
	TYPE_COMMAND,
	TYPE_EVENT,
};

static inline const char *xhci_ring_type_string(enum xhci_ring_type type)
{
	switch (type) {
	case TYPE_CTRL:
		return "CTRL";
	case TYPE_ISOC:
		return "ISOC";
	case TYPE_BULK:
		return "BULK";
	case TYPE_INTR:
		return "INTR";
	case TYPE_STREAM:
		return "STREAM";
	case TYPE_COMMAND:
		return "CMD";
	case TYPE_EVENT:
		return "EVENT";
	}

	return "UNKNOWN";
}

struct xhci_ring {
	struct xhci_segment	*first_seg;
	struct xhci_segment	*last_seg;
	union  xhci_trb		*enqueue;
	struct xhci_segment	*enq_seg;
	union  xhci_trb		*dequeue;
	struct xhci_segment	*deq_seg;
	struct list_head	td_list;
	
	u32			cycle_state;
	unsigned int		stream_id;
	unsigned int		num_segs;
	unsigned int		num_trbs_free; 
	unsigned int		bounce_buf_len;
	enum xhci_ring_type	type;
	bool			last_td_was_short;
	struct radix_tree_root	*trb_address_map;
};

struct xhci_erst_entry {
	
	__le64	seg_addr;
	__le32	seg_size;
	
	__le32	rsvd;
};

struct xhci_erst {
	struct xhci_erst_entry	*entries;
	unsigned int		num_entries;
	
	dma_addr_t		erst_dma_addr;
	
	unsigned int		erst_size;
};

struct xhci_scratchpad {
	u64 *sp_array;
	dma_addr_t sp_dma;
	void **sp_buffers;
};

struct urb_priv {
	int	num_tds;
	int	num_tds_done;
	struct	xhci_td	td[];
};


#define	ERST_NUM_SEGS	1

#define	POLL_TIMEOUT	60

#define XHCI_STOP_EP_CMD_TIMEOUT	5


struct s3_save {
	u32	command;
	u32	dev_nt;
	u64	dcbaa_ptr;
	u32	config_reg;
};


struct dev_info {
	u32			dev_id;
	struct	list_head	list;
};

struct xhci_bus_state {
	unsigned long		bus_suspended;
	unsigned long		next_statechange;

	
	
	u32			port_c_suspend;
	u32			suspended_ports;
	u32			port_remote_wakeup;
	
	unsigned long		resuming_ports;
};

struct xhci_interrupter {
	struct xhci_ring	*event_ring;
	struct xhci_erst	erst;
	struct xhci_intr_reg __iomem *ir_set;
	unsigned int		intr_num;
	
	u32	s3_irq_pending;
	u32	s3_irq_control;
	u32	s3_erst_size;
	u64	s3_erst_base;
	u64	s3_erst_dequeue;
};

#define	XHCI_MAX_REXIT_TIMEOUT_MS	20
struct xhci_port_cap {
	u32			*psi;	
	u8			psi_count;
	u8			psi_uid_count;
	u8			maj_rev;
	u8			min_rev;
};

struct xhci_port {
	__le32 __iomem		*addr;
	int			hw_portnum;
	int			hcd_portnum;
	struct xhci_hub		*rhub;
	struct xhci_port_cap	*port_cap;
	unsigned int		lpm_incapable:1;
	unsigned long		resume_timestamp;
	bool			rexit_active;
	struct completion	rexit_done;
	struct completion	u3exit_done;
};

struct xhci_hub {
	struct xhci_port	**ports;
	unsigned int		num_ports;
	struct usb_hcd		*hcd;
	
	struct xhci_bus_state   bus_state;
	
	u8			maj_rev;
	u8			min_rev;
};


struct xhci_hcd {
	struct usb_hcd *main_hcd;
	struct usb_hcd *shared_hcd;
	
	struct xhci_cap_regs __iomem *cap_regs;
	struct xhci_op_regs __iomem *op_regs;
	struct xhci_run_regs __iomem *run_regs;
	struct xhci_doorbell_array __iomem *dba;

	
	__u32		hcs_params1;
	__u32		hcs_params2;
	__u32		hcs_params3;
	__u32		hcc_params;
	__u32		hcc_params2;

	spinlock_t	lock;

	
	u8		sbrn;
	u16		hci_version;
	u8		max_slots;
	u16		max_interrupters;
	u8		max_ports;
	u8		isoc_threshold;
	
	u32		imod_interval;
	u32		isoc_bei_interval;
	int		event_ring_max;
	
	int		page_size;
	
	int		page_shift;
	
	int		msix_count;
	
	struct clk		*clk;
	struct clk		*reg_clk;
	
	struct reset_control *reset;
	
	struct xhci_device_context_array *dcbaa;
	struct xhci_interrupter *interrupter;
	struct xhci_ring	*cmd_ring;
	unsigned int            cmd_ring_state;
#define CMD_RING_STATE_RUNNING         (1 << 0)
#define CMD_RING_STATE_ABORTED         (1 << 1)
#define CMD_RING_STATE_STOPPED         (1 << 2)
	struct list_head        cmd_list;
	unsigned int		cmd_ring_reserved_trbs;
	struct delayed_work	cmd_timer;
	struct completion	cmd_ring_stop_completion;
	struct xhci_command	*current_cmd;

	
	struct xhci_scratchpad  *scratchpad;

	
	
	struct mutex mutex;
	
	struct xhci_virt_device	*devs[MAX_HC_SLOTS];
	
	struct xhci_root_port_bw_info	*rh_bw;

	
	struct dma_pool	*device_pool;
	struct dma_pool	*segment_pool;
	struct dma_pool	*small_streams_pool;
	struct dma_pool	*medium_streams_pool;

	
	unsigned int		xhc_state;
	unsigned long		run_graceperiod;
	struct s3_save		s3;

#define XHCI_STATE_DYING	(1 << 0)
#define XHCI_STATE_HALTED	(1 << 1)
#define XHCI_STATE_REMOVING	(1 << 2)
	unsigned long long	quirks;
#define	XHCI_LINK_TRB_QUIRK	BIT_ULL(0)
#define XHCI_RESET_EP_QUIRK	BIT_ULL(1) 
#define XHCI_NEC_HOST		BIT_ULL(2)
#define XHCI_AMD_PLL_FIX	BIT_ULL(3)
#define XHCI_SPURIOUS_SUCCESS	BIT_ULL(4)

#define XHCI_EP_LIMIT_QUIRK	BIT_ULL(5)
#define XHCI_BROKEN_MSI		BIT_ULL(6)
#define XHCI_RESET_ON_RESUME	BIT_ULL(7)
#define	XHCI_SW_BW_CHECKING	BIT_ULL(8)
#define XHCI_AMD_0x96_HOST	BIT_ULL(9)
#define XHCI_TRUST_TX_LENGTH	BIT_ULL(10)
#define XHCI_LPM_SUPPORT	BIT_ULL(11)
#define XHCI_INTEL_HOST		BIT_ULL(12)
#define XHCI_SPURIOUS_REBOOT	BIT_ULL(13)
#define XHCI_COMP_MODE_QUIRK	BIT_ULL(14)
#define XHCI_AVOID_BEI		BIT_ULL(15)
#define XHCI_PLAT		BIT_ULL(16) 
#define XHCI_SLOW_SUSPEND	BIT_ULL(17)
#define XHCI_SPURIOUS_WAKEUP	BIT_ULL(18)

#define XHCI_BROKEN_STREAMS	BIT_ULL(19)
#define XHCI_PME_STUCK_QUIRK	BIT_ULL(20)
#define XHCI_MTK_HOST		BIT_ULL(21)
#define XHCI_SSIC_PORT_UNUSED	BIT_ULL(22)
#define XHCI_NO_64BIT_SUPPORT	BIT_ULL(23)
#define XHCI_MISSING_CAS	BIT_ULL(24)

#define XHCI_BROKEN_PORT_PED	BIT_ULL(25)
#define XHCI_LIMIT_ENDPOINT_INTERVAL_7	BIT_ULL(26)
#define XHCI_U2_DISABLE_WAKE	BIT_ULL(27)
#define XHCI_ASMEDIA_MODIFY_FLOWCONTROL	BIT_ULL(28)
#define XHCI_HW_LPM_DISABLE	BIT_ULL(29)
#define XHCI_SUSPEND_DELAY	BIT_ULL(30)
#define XHCI_INTEL_USB_ROLE_SW	BIT_ULL(31)
#define XHCI_ZERO_64B_REGS	BIT_ULL(32)
#define XHCI_DEFAULT_PM_RUNTIME_ALLOW	BIT_ULL(33)
#define XHCI_RESET_PLL_ON_DISCONNECT	BIT_ULL(34)
#define XHCI_SNPS_BROKEN_SUSPEND    BIT_ULL(35)
#define XHCI_RENESAS_FW_QUIRK	BIT_ULL(36)
#define XHCI_SKIP_PHY_INIT	BIT_ULL(37)
#define XHCI_DISABLE_SPARSE	BIT_ULL(38)
#define XHCI_SG_TRB_CACHE_SIZE_QUIRK	BIT_ULL(39)
#define XHCI_NO_SOFT_RETRY	BIT_ULL(40)
#define XHCI_BROKEN_D3COLD_S2I	BIT_ULL(41)
#define XHCI_EP_CTX_BROKEN_DCS	BIT_ULL(42)
#define XHCI_SUSPEND_RESUME_CLKS	BIT_ULL(43)
#define XHCI_RESET_TO_DEFAULT	BIT_ULL(44)
#define XHCI_ZHAOXIN_TRB_FETCH	BIT_ULL(45)
#define XHCI_ZHAOXIN_HOST	BIT_ULL(46)

	unsigned int		num_active_eps;
	unsigned int		limit_active_eps;
	struct xhci_port	*hw_ports;
	struct xhci_hub		usb2_rhub;
	struct xhci_hub		usb3_rhub;
	
	unsigned		hw_lpm_support:1;
	
	unsigned		broken_suspend:1;
	
	unsigned		allow_single_roothub:1;
	
	u32                     *ext_caps;
	unsigned int            num_ext_caps;
	
	struct xhci_port_cap	*port_caps;
	unsigned int		num_port_caps;
	
	struct timer_list	comp_mode_recovery_timer;
	u32			port_status_u0;
	u16			test_mode;

#define COMP_MODE_RCVRY_MSECS 2000

	struct dentry		*debugfs_root;
	struct dentry		*debugfs_slots;
	struct list_head	regset_list;

	void			*dbc;
	
	unsigned long		priv[] __aligned(sizeof(s64));
};


struct xhci_driver_overrides {
	size_t extra_priv_size;
	int (*reset)(struct usb_hcd *hcd);
	int (*start)(struct usb_hcd *hcd);
	int (*add_endpoint)(struct usb_hcd *hcd, struct usb_device *udev,
			    struct usb_host_endpoint *ep);
	int (*drop_endpoint)(struct usb_hcd *hcd, struct usb_device *udev,
			     struct usb_host_endpoint *ep);
	int (*check_bandwidth)(struct usb_hcd *, struct usb_device *);
	void (*reset_bandwidth)(struct usb_hcd *, struct usb_device *);
	int (*update_hub_device)(struct usb_hcd *hcd, struct usb_device *hdev,
			    struct usb_tt *tt, gfp_t mem_flags);
	int (*hub_control)(struct usb_hcd *hcd, u16 typeReq, u16 wValue,
			   u16 wIndex, char *buf, u16 wLength);
};

#define	XHCI_CFC_DELAY		10


static inline struct xhci_hcd *hcd_to_xhci(struct usb_hcd *hcd)
{
	struct usb_hcd *primary_hcd;

	if (usb_hcd_is_primary_hcd(hcd))
		primary_hcd = hcd;
	else
		primary_hcd = hcd->primary_hcd;

	return (struct xhci_hcd *) (primary_hcd->hcd_priv);
}

static inline struct usb_hcd *xhci_to_hcd(struct xhci_hcd *xhci)
{
	return xhci->main_hcd;
}

static inline struct usb_hcd *xhci_get_usb3_hcd(struct xhci_hcd *xhci)
{
	if (xhci->shared_hcd)
		return xhci->shared_hcd;

	if (!xhci->usb2_rhub.num_ports)
		return xhci->main_hcd;

	return NULL;
}

static inline bool xhci_hcd_is_usb3(struct usb_hcd *hcd)
{
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);

	return hcd == xhci_get_usb3_hcd(xhci);
}

static inline bool xhci_has_one_roothub(struct xhci_hcd *xhci)
{
	return xhci->allow_single_roothub &&
	       (!xhci->usb2_rhub.num_ports || !xhci->usb3_rhub.num_ports);
}

#define xhci_dbg(xhci, fmt, args...) \
	dev_dbg(xhci_to_hcd(xhci)->self.controller , fmt , ## args)
#define xhci_err(xhci, fmt, args...) \
	dev_err(xhci_to_hcd(xhci)->self.controller , fmt , ## args)
#define xhci_warn(xhci, fmt, args...) \
	dev_warn(xhci_to_hcd(xhci)->self.controller , fmt , ## args)
#define xhci_warn_ratelimited(xhci, fmt, args...) \
	dev_warn_ratelimited(xhci_to_hcd(xhci)->self.controller , fmt , ## args)
#define xhci_info(xhci, fmt, args...) \
	dev_info(xhci_to_hcd(xhci)->self.controller , fmt , ## args)


static inline u64 xhci_read_64(const struct xhci_hcd *xhci,
		__le64 __iomem *regs)
{
	return lo_hi_readq(regs);
}
static inline void xhci_write_64(struct xhci_hcd *xhci,
				 const u64 val, __le64 __iomem *regs)
{
	lo_hi_writeq(val, regs);
}

static inline int xhci_link_trb_quirk(struct xhci_hcd *xhci)
{
	return xhci->quirks & XHCI_LINK_TRB_QUIRK;
}


char *xhci_get_slot_state(struct xhci_hcd *xhci,
		struct xhci_container_ctx *ctx);
void xhci_dbg_trace(struct xhci_hcd *xhci, void (*trace)(struct va_format *),
			const char *fmt, ...);


void xhci_mem_cleanup(struct xhci_hcd *xhci);
int xhci_mem_init(struct xhci_hcd *xhci, gfp_t flags);
void xhci_free_virt_device(struct xhci_hcd *xhci, int slot_id);
int xhci_alloc_virt_device(struct xhci_hcd *xhci, int slot_id, struct usb_device *udev, gfp_t flags);
int xhci_setup_addressable_virt_dev(struct xhci_hcd *xhci, struct usb_device *udev);
void xhci_copy_ep0_dequeue_into_input_ctx(struct xhci_hcd *xhci,
		struct usb_device *udev);
unsigned int xhci_get_endpoint_index(struct usb_endpoint_descriptor *desc);
unsigned int xhci_last_valid_endpoint(u32 added_ctxs);
void xhci_endpoint_zero(struct xhci_hcd *xhci, struct xhci_virt_device *virt_dev, struct usb_host_endpoint *ep);
void xhci_update_tt_active_eps(struct xhci_hcd *xhci,
		struct xhci_virt_device *virt_dev,
		int old_active_eps);
void xhci_clear_endpoint_bw_info(struct xhci_bw_info *bw_info);
void xhci_update_bw_info(struct xhci_hcd *xhci,
		struct xhci_container_ctx *in_ctx,
		struct xhci_input_control_ctx *ctrl_ctx,
		struct xhci_virt_device *virt_dev);
void xhci_endpoint_copy(struct xhci_hcd *xhci,
		struct xhci_container_ctx *in_ctx,
		struct xhci_container_ctx *out_ctx,
		unsigned int ep_index);
void xhci_slot_copy(struct xhci_hcd *xhci,
		struct xhci_container_ctx *in_ctx,
		struct xhci_container_ctx *out_ctx);
int xhci_endpoint_init(struct xhci_hcd *xhci, struct xhci_virt_device *virt_dev,
		struct usb_device *udev, struct usb_host_endpoint *ep,
		gfp_t mem_flags);
struct xhci_ring *xhci_ring_alloc(struct xhci_hcd *xhci,
		unsigned int num_segs, unsigned int cycle_state,
		enum xhci_ring_type type, unsigned int max_packet, gfp_t flags);
void xhci_ring_free(struct xhci_hcd *xhci, struct xhci_ring *ring);
int xhci_ring_expansion(struct xhci_hcd *xhci, struct xhci_ring *ring,
		unsigned int num_trbs, gfp_t flags);
int xhci_alloc_erst(struct xhci_hcd *xhci,
		struct xhci_ring *evt_ring,
		struct xhci_erst *erst,
		gfp_t flags);
void xhci_initialize_ring_info(struct xhci_ring *ring,
			unsigned int cycle_state);
void xhci_free_erst(struct xhci_hcd *xhci, struct xhci_erst *erst);
void xhci_free_endpoint_ring(struct xhci_hcd *xhci,
		struct xhci_virt_device *virt_dev,
		unsigned int ep_index);
struct xhci_stream_info *xhci_alloc_stream_info(struct xhci_hcd *xhci,
		unsigned int num_stream_ctxs,
		unsigned int num_streams,
		unsigned int max_packet, gfp_t flags);
void xhci_free_stream_info(struct xhci_hcd *xhci,
		struct xhci_stream_info *stream_info);
void xhci_setup_streams_ep_input_ctx(struct xhci_hcd *xhci,
		struct xhci_ep_ctx *ep_ctx,
		struct xhci_stream_info *stream_info);
void xhci_setup_no_streams_ep_input_ctx(struct xhci_ep_ctx *ep_ctx,
		struct xhci_virt_ep *ep);
void xhci_free_device_endpoint_resources(struct xhci_hcd *xhci,
	struct xhci_virt_device *virt_dev, bool drop_control_ep);
struct xhci_ring *xhci_dma_to_transfer_ring(
		struct xhci_virt_ep *ep,
		u64 address);
struct xhci_command *xhci_alloc_command(struct xhci_hcd *xhci,
		bool allocate_completion, gfp_t mem_flags);
struct xhci_command *xhci_alloc_command_with_ctx(struct xhci_hcd *xhci,
		bool allocate_completion, gfp_t mem_flags);
void xhci_urb_free_priv(struct urb_priv *urb_priv);
void xhci_free_command(struct xhci_hcd *xhci,
		struct xhci_command *command);
struct xhci_container_ctx *xhci_alloc_container_ctx(struct xhci_hcd *xhci,
		int type, gfp_t flags);
void xhci_free_container_ctx(struct xhci_hcd *xhci,
		struct xhci_container_ctx *ctx);


typedef void (*xhci_get_quirks_t)(struct device *, struct xhci_hcd *);
int xhci_handshake(void __iomem *ptr, u32 mask, u32 done, u64 timeout_us);
void xhci_quiesce(struct xhci_hcd *xhci);
int xhci_halt(struct xhci_hcd *xhci);
int xhci_start(struct xhci_hcd *xhci);
int xhci_reset(struct xhci_hcd *xhci, u64 timeout_us);
int xhci_run(struct usb_hcd *hcd);
int xhci_gen_setup(struct usb_hcd *hcd, xhci_get_quirks_t get_quirks);
void xhci_shutdown(struct usb_hcd *hcd);
void xhci_stop(struct usb_hcd *hcd);
void xhci_init_driver(struct hc_driver *drv,
		      const struct xhci_driver_overrides *over);
int xhci_add_endpoint(struct usb_hcd *hcd, struct usb_device *udev,
		      struct usb_host_endpoint *ep);
int xhci_drop_endpoint(struct usb_hcd *hcd, struct usb_device *udev,
		       struct usb_host_endpoint *ep);
int xhci_check_bandwidth(struct usb_hcd *hcd, struct usb_device *udev);
void xhci_reset_bandwidth(struct usb_hcd *hcd, struct usb_device *udev);
int xhci_update_hub_device(struct usb_hcd *hcd, struct usb_device *hdev,
			   struct usb_tt *tt, gfp_t mem_flags);
int xhci_disable_slot(struct xhci_hcd *xhci, u32 slot_id);
int xhci_ext_cap_init(struct xhci_hcd *xhci);

int xhci_suspend(struct xhci_hcd *xhci, bool do_wakeup);
int xhci_resume(struct xhci_hcd *xhci, pm_message_t msg);

irqreturn_t xhci_irq(struct usb_hcd *hcd);
irqreturn_t xhci_msi_irq(int irq, void *hcd);
int xhci_alloc_dev(struct usb_hcd *hcd, struct usb_device *udev);
int xhci_alloc_tt_info(struct xhci_hcd *xhci,
		struct xhci_virt_device *virt_dev,
		struct usb_device *hdev,
		struct usb_tt *tt, gfp_t mem_flags);


dma_addr_t xhci_trb_virt_to_dma(struct xhci_segment *seg, union xhci_trb *trb);
struct xhci_segment *trb_in_td(struct xhci_hcd *xhci,
		struct xhci_segment *start_seg, union xhci_trb *start_trb,
		union xhci_trb *end_trb, dma_addr_t suspect_dma, bool debug);
int xhci_is_vendor_info_code(struct xhci_hcd *xhci, unsigned int trb_comp_code);
void xhci_ring_cmd_db(struct xhci_hcd *xhci);
int xhci_queue_slot_control(struct xhci_hcd *xhci, struct xhci_command *cmd,
		u32 trb_type, u32 slot_id);
int xhci_queue_address_device(struct xhci_hcd *xhci, struct xhci_command *cmd,
		dma_addr_t in_ctx_ptr, u32 slot_id, enum xhci_setup_dev);
int xhci_queue_vendor_command(struct xhci_hcd *xhci, struct xhci_command *cmd,
		u32 field1, u32 field2, u32 field3, u32 field4);
int xhci_queue_stop_endpoint(struct xhci_hcd *xhci, struct xhci_command *cmd,
		int slot_id, unsigned int ep_index, int suspend);
int xhci_queue_ctrl_tx(struct xhci_hcd *xhci, gfp_t mem_flags, struct urb *urb,
		int slot_id, unsigned int ep_index);
int xhci_queue_bulk_tx(struct xhci_hcd *xhci, gfp_t mem_flags, struct urb *urb,
		int slot_id, unsigned int ep_index);
int xhci_queue_intr_tx(struct xhci_hcd *xhci, gfp_t mem_flags, struct urb *urb,
		int slot_id, unsigned int ep_index);
int xhci_queue_isoc_tx_prepare(struct xhci_hcd *xhci, gfp_t mem_flags,
		struct urb *urb, int slot_id, unsigned int ep_index);
int xhci_queue_configure_endpoint(struct xhci_hcd *xhci,
		struct xhci_command *cmd, dma_addr_t in_ctx_ptr, u32 slot_id,
		bool command_must_succeed);
int xhci_queue_evaluate_context(struct xhci_hcd *xhci, struct xhci_command *cmd,
		dma_addr_t in_ctx_ptr, u32 slot_id, bool command_must_succeed);
int xhci_queue_reset_ep(struct xhci_hcd *xhci, struct xhci_command *cmd,
		int slot_id, unsigned int ep_index,
		enum xhci_ep_reset_type reset_type);
int xhci_queue_reset_device(struct xhci_hcd *xhci, struct xhci_command *cmd,
		u32 slot_id);
void xhci_cleanup_stalled_ring(struct xhci_hcd *xhci, unsigned int slot_id,
			       unsigned int ep_index, unsigned int stream_id,
			       struct xhci_td *td);
void xhci_stop_endpoint_command_watchdog(struct timer_list *t);
void xhci_handle_command_timeout(struct work_struct *work);

void xhci_ring_ep_doorbell(struct xhci_hcd *xhci, unsigned int slot_id,
		unsigned int ep_index, unsigned int stream_id);
void xhci_ring_doorbell_for_active_rings(struct xhci_hcd *xhci,
		unsigned int slot_id,
		unsigned int ep_index);
void xhci_cleanup_command_queue(struct xhci_hcd *xhci);
void inc_deq(struct xhci_hcd *xhci, struct xhci_ring *ring);
unsigned int count_trbs(u64 addr, u64 len);


void xhci_set_link_state(struct xhci_hcd *xhci, struct xhci_port *port,
				u32 link_state);
void xhci_test_and_clear_bit(struct xhci_hcd *xhci, struct xhci_port *port,
				u32 port_bit);
int xhci_hub_control(struct usb_hcd *hcd, u16 typeReq, u16 wValue, u16 wIndex,
		char *buf, u16 wLength);
int xhci_hub_status_data(struct usb_hcd *hcd, char *buf);
int xhci_find_raw_port_number(struct usb_hcd *hcd, int port1);
struct xhci_hub *xhci_get_rhub(struct usb_hcd *hcd);

void xhci_hc_died(struct xhci_hcd *xhci);

#ifdef CONFIG_PM
int xhci_bus_suspend(struct usb_hcd *hcd);
int xhci_bus_resume(struct usb_hcd *hcd);
unsigned long xhci_get_resuming_ports(struct usb_hcd *hcd);
#else
#define	xhci_bus_suspend	NULL
#define	xhci_bus_resume		NULL
#define	xhci_get_resuming_ports	NULL
#endif	

u32 xhci_port_state_to_neutral(u32 state);
int xhci_find_slot_id_by_port(struct usb_hcd *hcd, struct xhci_hcd *xhci,
		u16 port);
void xhci_ring_device(struct xhci_hcd *xhci, int slot_id);


struct xhci_input_control_ctx *xhci_get_input_control_ctx(struct xhci_container_ctx *ctx);
struct xhci_slot_ctx *xhci_get_slot_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx);
struct xhci_ep_ctx *xhci_get_ep_ctx(struct xhci_hcd *xhci, struct xhci_container_ctx *ctx, unsigned int ep_index);

struct xhci_ring *xhci_triad_to_transfer_ring(struct xhci_hcd *xhci,
		unsigned int slot_id, unsigned int ep_index,
		unsigned int stream_id);

static inline struct xhci_ring *xhci_urb_to_transfer_ring(struct xhci_hcd *xhci,
								struct urb *urb)
{
	return xhci_triad_to_transfer_ring(xhci, urb->dev->slot_id,
					xhci_get_endpoint_index(&urb->ep->desc),
					urb->stream_id);
}


static inline bool xhci_urb_suitable_for_idt(struct urb *urb)
{
	if (!usb_endpoint_xfer_isoc(&urb->ep->desc) && usb_urb_dir_out(urb) &&
	    usb_endpoint_maxp(&urb->ep->desc) >= TRB_IDT_MAX_SIZE &&
	    urb->transfer_buffer_length <= TRB_IDT_MAX_SIZE &&
	    !(urb->transfer_flags & URB_NO_TRANSFER_DMA_MAP) &&
	    !urb->num_sgs)
		return true;

	return false;
}

static inline char *xhci_slot_state_string(u32 state)
{
	switch (state) {
	case SLOT_STATE_ENABLED:
		return "enabled/disabled";
	case SLOT_STATE_DEFAULT:
		return "default";
	case SLOT_STATE_ADDRESSED:
		return "addressed";
	case SLOT_STATE_CONFIGURED:
		return "configured";
	default:
		return "reserved";
	}
}

static inline const char *xhci_decode_trb(char *str, size_t size,
					  u32 field0, u32 field1, u32 field2, u32 field3)
{
	int type = TRB_FIELD_TO_TYPE(field3);

	switch (type) {
	case TRB_LINK:
		snprintf(str, size,
			"LINK %08x%08x intr %d type '%s' flags %c:%c:%c:%c",
			field1, field0, GET_INTR_TARGET(field2),
			xhci_trb_type_string(type),
			field3 & TRB_IOC ? 'I' : 'i',
			field3 & TRB_CHAIN ? 'C' : 'c',
			field3 & TRB_TC ? 'T' : 't',
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_TRANSFER:
	case TRB_COMPLETION:
	case TRB_PORT_STATUS:
	case TRB_BANDWIDTH_EVENT:
	case TRB_DOORBELL:
	case TRB_HC_EVENT:
	case TRB_DEV_NOTE:
	case TRB_MFINDEX_WRAP:
		snprintf(str, size,
			"TRB %08x%08x status '%s' len %d slot %d ep %d type '%s' flags %c:%c",
			field1, field0,
			xhci_trb_comp_code_string(GET_COMP_CODE(field2)),
			EVENT_TRB_LEN(field2), TRB_TO_SLOT_ID(field3),
			
			TRB_TO_EP_INDEX(field3) + 1,
			xhci_trb_type_string(type),
			field3 & EVENT_DATA ? 'E' : 'e',
			field3 & TRB_CYCLE ? 'C' : 'c');

		break;
	case TRB_SETUP:
		snprintf(str, size,
			"bRequestType %02x bRequest %02x wValue %02x%02x wIndex %02x%02x wLength %d length %d TD size %d intr %d type '%s' flags %c:%c:%c",
				field0 & 0xff,
				(field0 & 0xff00) >> 8,
				(field0 & 0xff000000) >> 24,
				(field0 & 0xff0000) >> 16,
				(field1 & 0xff00) >> 8,
				field1 & 0xff,
				(field1 & 0xff000000) >> 16 |
				(field1 & 0xff0000) >> 16,
				TRB_LEN(field2), GET_TD_SIZE(field2),
				GET_INTR_TARGET(field2),
				xhci_trb_type_string(type),
				field3 & TRB_IDT ? 'I' : 'i',
				field3 & TRB_IOC ? 'I' : 'i',
				field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_DATA:
		snprintf(str, size,
			 "Buffer %08x%08x length %d TD size %d intr %d type '%s' flags %c:%c:%c:%c:%c:%c:%c",
				field1, field0, TRB_LEN(field2), GET_TD_SIZE(field2),
				GET_INTR_TARGET(field2),
				xhci_trb_type_string(type),
				field3 & TRB_IDT ? 'I' : 'i',
				field3 & TRB_IOC ? 'I' : 'i',
				field3 & TRB_CHAIN ? 'C' : 'c',
				field3 & TRB_NO_SNOOP ? 'S' : 's',
				field3 & TRB_ISP ? 'I' : 'i',
				field3 & TRB_ENT ? 'E' : 'e',
				field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_STATUS:
		snprintf(str, size,
			 "Buffer %08x%08x length %d TD size %d intr %d type '%s' flags %c:%c:%c:%c",
				field1, field0, TRB_LEN(field2), GET_TD_SIZE(field2),
				GET_INTR_TARGET(field2),
				xhci_trb_type_string(type),
				field3 & TRB_IOC ? 'I' : 'i',
				field3 & TRB_CHAIN ? 'C' : 'c',
				field3 & TRB_ENT ? 'E' : 'e',
				field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_NORMAL:
	case TRB_ISOC:
	case TRB_EVENT_DATA:
	case TRB_TR_NOOP:
		snprintf(str, size,
			"Buffer %08x%08x length %d TD size %d intr %d type '%s' flags %c:%c:%c:%c:%c:%c:%c:%c",
			field1, field0, TRB_LEN(field2), GET_TD_SIZE(field2),
			GET_INTR_TARGET(field2),
			xhci_trb_type_string(type),
			field3 & TRB_BEI ? 'B' : 'b',
			field3 & TRB_IDT ? 'I' : 'i',
			field3 & TRB_IOC ? 'I' : 'i',
			field3 & TRB_CHAIN ? 'C' : 'c',
			field3 & TRB_NO_SNOOP ? 'S' : 's',
			field3 & TRB_ISP ? 'I' : 'i',
			field3 & TRB_ENT ? 'E' : 'e',
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;

	case TRB_CMD_NOOP:
	case TRB_ENABLE_SLOT:
		snprintf(str, size,
			"%s: flags %c",
			xhci_trb_type_string(type),
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_DISABLE_SLOT:
	case TRB_NEG_BANDWIDTH:
		snprintf(str, size,
			"%s: slot %d flags %c",
			xhci_trb_type_string(type),
			TRB_TO_SLOT_ID(field3),
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_ADDR_DEV:
		snprintf(str, size,
			"%s: ctx %08x%08x slot %d flags %c:%c",
			xhci_trb_type_string(type),
			field1, field0,
			TRB_TO_SLOT_ID(field3),
			field3 & TRB_BSR ? 'B' : 'b',
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_CONFIG_EP:
		snprintf(str, size,
			"%s: ctx %08x%08x slot %d flags %c:%c",
			xhci_trb_type_string(type),
			field1, field0,
			TRB_TO_SLOT_ID(field3),
			field3 & TRB_DC ? 'D' : 'd',
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_EVAL_CONTEXT:
		snprintf(str, size,
			"%s: ctx %08x%08x slot %d flags %c",
			xhci_trb_type_string(type),
			field1, field0,
			TRB_TO_SLOT_ID(field3),
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_RESET_EP:
		snprintf(str, size,
			"%s: ctx %08x%08x slot %d ep %d flags %c:%c",
			xhci_trb_type_string(type),
			field1, field0,
			TRB_TO_SLOT_ID(field3),
			
			TRB_TO_EP_INDEX(field3) + 1,
			field3 & TRB_TSP ? 'T' : 't',
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_STOP_RING:
		snprintf(str, size,
			"%s: slot %d sp %d ep %d flags %c",
			xhci_trb_type_string(type),
			TRB_TO_SLOT_ID(field3),
			TRB_TO_SUSPEND_PORT(field3),
			
			TRB_TO_EP_INDEX(field3) + 1,
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_SET_DEQ:
		snprintf(str, size,
			"%s: deq %08x%08x stream %d slot %d ep %d flags %c",
			xhci_trb_type_string(type),
			field1, field0,
			TRB_TO_STREAM_ID(field2),
			TRB_TO_SLOT_ID(field3),
			
			TRB_TO_EP_INDEX(field3) + 1,
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_RESET_DEV:
		snprintf(str, size,
			"%s: slot %d flags %c",
			xhci_trb_type_string(type),
			TRB_TO_SLOT_ID(field3),
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_FORCE_EVENT:
		snprintf(str, size,
			"%s: event %08x%08x vf intr %d vf id %d flags %c",
			xhci_trb_type_string(type),
			field1, field0,
			TRB_TO_VF_INTR_TARGET(field2),
			TRB_TO_VF_ID(field3),
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_SET_LT:
		snprintf(str, size,
			"%s: belt %d flags %c",
			xhci_trb_type_string(type),
			TRB_TO_BELT(field3),
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_GET_BW:
		snprintf(str, size,
			"%s: ctx %08x%08x slot %d speed %d flags %c",
			xhci_trb_type_string(type),
			field1, field0,
			TRB_TO_SLOT_ID(field3),
			TRB_TO_DEV_SPEED(field3),
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	case TRB_FORCE_HEADER:
		snprintf(str, size,
			"%s: info %08x%08x%08x pkt type %d roothub port %d flags %c",
			xhci_trb_type_string(type),
			field2, field1, field0 & 0xffffffe0,
			TRB_TO_PACKET_TYPE(field0),
			TRB_TO_ROOTHUB_PORT(field3),
			field3 & TRB_CYCLE ? 'C' : 'c');
		break;
	default:
		snprintf(str, size,
			"type '%s' -> raw %08x %08x %08x %08x",
			xhci_trb_type_string(type),
			field0, field1, field2, field3);
	}

	return str;
}

static inline const char *xhci_decode_ctrl_ctx(char *str,
		unsigned long drop, unsigned long add)
{
	unsigned int	bit;
	int		ret = 0;

	str[0] = '\0';

	if (drop) {
		ret = sprintf(str, "Drop:");
		for_each_set_bit(bit, &drop, 32)
			ret += sprintf(str + ret, " %d%s",
				       bit / 2,
				       bit % 2 ? "in":"out");
		ret += sprintf(str + ret, ", ");
	}

	if (add) {
		ret += sprintf(str + ret, "Add:%s%s",
			       (add & SLOT_FLAG) ? " slot":"",
			       (add & EP0_FLAG) ? " ep0":"");
		add &= ~(SLOT_FLAG | EP0_FLAG);
		for_each_set_bit(bit, &add, 32)
			ret += sprintf(str + ret, " %d%s",
				       bit / 2,
				       bit % 2 ? "in":"out");
	}
	return str;
}

static inline const char *xhci_decode_slot_context(char *str,
		u32 info, u32 info2, u32 tt_info, u32 state)
{
	u32 speed;
	u32 hub;
	u32 mtt;
	int ret = 0;

	speed = info & DEV_SPEED;
	hub = info & DEV_HUB;
	mtt = info & DEV_MTT;

	ret = sprintf(str, "RS %05x %s%s%s Ctx Entries %d MEL %d us Port# %d/%d",
			info & ROUTE_STRING_MASK,
			({ char *s;
			switch (speed) {
			case SLOT_SPEED_FS:
				s = "full-speed";
				break;
			case SLOT_SPEED_LS:
				s = "low-speed";
				break;
			case SLOT_SPEED_HS:
				s = "high-speed";
				break;
			case SLOT_SPEED_SS:
				s = "super-speed";
				break;
			case SLOT_SPEED_SSP:
				s = "super-speed plus";
				break;
			default:
				s = "UNKNOWN speed";
			} s; }),
			mtt ? " multi-TT" : "",
			hub ? " Hub" : "",
			(info & LAST_CTX_MASK) >> 27,
			info2 & MAX_EXIT,
			DEVINFO_TO_ROOT_HUB_PORT(info2),
			DEVINFO_TO_MAX_PORTS(info2));

	ret += sprintf(str + ret, " [TT Slot %d Port# %d TTT %d Intr %d] Addr %d State %s",
			tt_info & TT_SLOT, (tt_info & TT_PORT) >> 8,
			GET_TT_THINK_TIME(tt_info), GET_INTR_TARGET(tt_info),
			state & DEV_ADDR_MASK,
			xhci_slot_state_string(GET_SLOT_STATE(state)));

	return str;
}


static inline const char *xhci_portsc_link_state_string(u32 portsc)
{
	switch (portsc & PORT_PLS_MASK) {
	case XDEV_U0:
		return "U0";
	case XDEV_U1:
		return "U1";
	case XDEV_U2:
		return "U2";
	case XDEV_U3:
		return "U3";
	case XDEV_DISABLED:
		return "Disabled";
	case XDEV_RXDETECT:
		return "RxDetect";
	case XDEV_INACTIVE:
		return "Inactive";
	case XDEV_POLLING:
		return "Polling";
	case XDEV_RECOVERY:
		return "Recovery";
	case XDEV_HOT_RESET:
		return "Hot Reset";
	case XDEV_COMP_MODE:
		return "Compliance mode";
	case XDEV_TEST_MODE:
		return "Test mode";
	case XDEV_RESUME:
		return "Resume";
	default:
		break;
	}
	return "Unknown";
}

static inline const char *xhci_decode_portsc(char *str, u32 portsc)
{
	int ret;

	ret = sprintf(str, "%s %s %s Link:%s PortSpeed:%d ",
		      portsc & PORT_POWER	? "Powered" : "Powered-off",
		      portsc & PORT_CONNECT	? "Connected" : "Not-connected",
		      portsc & PORT_PE		? "Enabled" : "Disabled",
		      xhci_portsc_link_state_string(portsc),
		      DEV_PORT_SPEED(portsc));

	if (portsc & PORT_OC)
		ret += sprintf(str + ret, "OverCurrent ");
	if (portsc & PORT_RESET)
		ret += sprintf(str + ret, "In-Reset ");

	ret += sprintf(str + ret, "Change: ");
	if (portsc & PORT_CSC)
		ret += sprintf(str + ret, "CSC ");
	if (portsc & PORT_PEC)
		ret += sprintf(str + ret, "PEC ");
	if (portsc & PORT_WRC)
		ret += sprintf(str + ret, "WRC ");
	if (portsc & PORT_OCC)
		ret += sprintf(str + ret, "OCC ");
	if (portsc & PORT_RC)
		ret += sprintf(str + ret, "PRC ");
	if (portsc & PORT_PLC)
		ret += sprintf(str + ret, "PLC ");
	if (portsc & PORT_CEC)
		ret += sprintf(str + ret, "CEC ");
	if (portsc & PORT_CAS)
		ret += sprintf(str + ret, "CAS ");

	ret += sprintf(str + ret, "Wake: ");
	if (portsc & PORT_WKCONN_E)
		ret += sprintf(str + ret, "WCE ");
	if (portsc & PORT_WKDISC_E)
		ret += sprintf(str + ret, "WDE ");
	if (portsc & PORT_WKOC_E)
		ret += sprintf(str + ret, "WOE ");

	return str;
}

static inline const char *xhci_decode_usbsts(char *str, u32 usbsts)
{
	int ret = 0;

	ret = sprintf(str, " 0x%08x", usbsts);

	if (usbsts == ~(u32)0)
		return str;

	if (usbsts & STS_HALT)
		ret += sprintf(str + ret, " HCHalted");
	if (usbsts & STS_FATAL)
		ret += sprintf(str + ret, " HSE");
	if (usbsts & STS_EINT)
		ret += sprintf(str + ret, " EINT");
	if (usbsts & STS_PORT)
		ret += sprintf(str + ret, " PCD");
	if (usbsts & STS_SAVE)
		ret += sprintf(str + ret, " SSS");
	if (usbsts & STS_RESTORE)
		ret += sprintf(str + ret, " RSS");
	if (usbsts & STS_SRE)
		ret += sprintf(str + ret, " SRE");
	if (usbsts & STS_CNR)
		ret += sprintf(str + ret, " CNR");
	if (usbsts & STS_HCE)
		ret += sprintf(str + ret, " HCE");

	return str;
}

static inline const char *xhci_decode_doorbell(char *str, u32 slot, u32 doorbell)
{
	u8 ep;
	u16 stream;
	int ret;

	ep = (doorbell & 0xff);
	stream = doorbell >> 16;

	if (slot == 0) {
		sprintf(str, "Command Ring %d", doorbell);
		return str;
	}
	ret = sprintf(str, "Slot %d ", slot);
	if (ep > 0 && ep < 32)
		ret = sprintf(str + ret, "ep%d%s",
			      ep / 2,
			      ep % 2 ? "in" : "out");
	else if (ep == 0 || ep < 248)
		ret = sprintf(str + ret, "Reserved %d", ep);
	else
		ret = sprintf(str + ret, "Vendor Defined %d", ep);
	if (stream)
		ret = sprintf(str + ret, " Stream %d", stream);

	return str;
}

static inline const char *xhci_ep_state_string(u8 state)
{
	switch (state) {
	case EP_STATE_DISABLED:
		return "disabled";
	case EP_STATE_RUNNING:
		return "running";
	case EP_STATE_HALTED:
		return "halted";
	case EP_STATE_STOPPED:
		return "stopped";
	case EP_STATE_ERROR:
		return "error";
	default:
		return "INVALID";
	}
}

static inline const char *xhci_ep_type_string(u8 type)
{
	switch (type) {
	case ISOC_OUT_EP:
		return "Isoc OUT";
	case BULK_OUT_EP:
		return "Bulk OUT";
	case INT_OUT_EP:
		return "Int OUT";
	case CTRL_EP:
		return "Ctrl";
	case ISOC_IN_EP:
		return "Isoc IN";
	case BULK_IN_EP:
		return "Bulk IN";
	case INT_IN_EP:
		return "Int IN";
	default:
		return "INVALID";
	}
}

static inline const char *xhci_decode_ep_context(char *str, u32 info,
		u32 info2, u64 deq, u32 tx_info)
{
	int ret;

	u32 esit;
	u16 maxp;
	u16 avg;

	u8 max_pstr;
	u8 ep_state;
	u8 interval;
	u8 ep_type;
	u8 burst;
	u8 cerr;
	u8 mult;

	bool lsa;
	bool hid;

	esit = CTX_TO_MAX_ESIT_PAYLOAD_HI(info) << 16 |
		CTX_TO_MAX_ESIT_PAYLOAD(tx_info);

	ep_state = info & EP_STATE_MASK;
	max_pstr = CTX_TO_EP_MAXPSTREAMS(info);
	interval = CTX_TO_EP_INTERVAL(info);
	mult = CTX_TO_EP_MULT(info) + 1;
	lsa = !!(info & EP_HAS_LSA);

	cerr = (info2 & (3 << 1)) >> 1;
	ep_type = CTX_TO_EP_TYPE(info2);
	hid = !!(info2 & (1 << 7));
	burst = CTX_TO_MAX_BURST(info2);
	maxp = MAX_PACKET_DECODED(info2);

	avg = EP_AVG_TRB_LENGTH(tx_info);

	ret = sprintf(str, "State %s mult %d max P. Streams %d %s",
			xhci_ep_state_string(ep_state), mult,
			max_pstr, lsa ? "LSA " : "");

	ret += sprintf(str + ret, "interval %d us max ESIT payload %d CErr %d ",
			(1 << interval) * 125, esit, cerr);

	ret += sprintf(str + ret, "Type %s %sburst %d maxp %d deq %016llx ",
			xhci_ep_type_string(ep_type), hid ? "HID" : "",
			burst, maxp, deq);

	ret += sprintf(str + ret, "avg trb len %d", avg);

	return str;
}

#endif 
