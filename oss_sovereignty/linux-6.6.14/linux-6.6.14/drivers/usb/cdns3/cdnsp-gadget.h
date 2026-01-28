#ifndef __LINUX_CDNSP_GADGET_H
#define __LINUX_CDNSP_GADGET_H
#include <linux/io-64-nonatomic-lo-hi.h>
#include <linux/usb/gadget.h>
#include <linux/irq.h>
#define CDNSP_DEV_MAX_SLOTS	1
#define CDNSP_EP0_SETUP_SIZE	512
#define CDNSP_ENDPOINTS_NUM	31
#define CDNSP_DEFAULT_BESL	0
#define CDNSP_CMD_TIMEOUT	(15 * 1000)
#define CDNSP_MAX_HALT_USEC	(16 * 1000)
#define CDNSP_CTX_SIZE	2112
struct cdnsp_cap_regs {
	__le32 hc_capbase;
	__le32 hcs_params1;
	__le32 hcs_params2;
	__le32 hcs_params3;
	__le32 hcc_params;
	__le32 db_off;
	__le32 run_regs_off;
	__le32 hcc_params2;
};
#define HC_LENGTH(p)		(((p) >> 00) & GENMASK(7, 0))
#define HC_VERSION(p)		(((p) >> 16) & GENMASK(15, 1))
#define HCS_ENDPOINTS_MASK	GENMASK(7, 0)
#define HCS_ENDPOINTS(p)	(((p) & HCS_ENDPOINTS_MASK) >> 0)
#define HCC_PARAMS_OFFSET	0x10
#define HCC_64BIT_ADDR(p)	((p) & BIT(0))
#define HCC_64BYTE_CONTEXT(p)	((p) & BIT(2))
#define HCC_MAX_PSA(p)		((((p) >> 12) & 0xf) + 1)
#define HCC_EXT_CAPS(p)		(((p) & GENMASK(31, 16)) >> 16)
#define CTX_SIZE(_hcc)		(HCC_64BYTE_CONTEXT(_hcc) ? 64 : 32)
#define DBOFF_MASK	GENMASK(31, 2)
#define RTSOFF_MASK	GENMASK(31, 5)
struct cdnsp_op_regs {
	__le32 command;
	__le32 status;
	__le32 page_size;
	__le32 reserved1;
	__le32 reserved2;
	__le32 dnctrl;
	__le64 cmd_ring;
	__le32 reserved3[4];
	__le64 dcbaa_ptr;
	__le32 config_reg;
	__le32 reserved4[241];
	__le32 port_reg_base;
};
#define NUM_PORT_REGS	4
struct cdnsp_port_regs {
	__le32 portsc;
	__le32 portpmsc;
	__le32 portli;
	__le32 reserved;
};
#define CDNSP_PORT_RO	(PORT_CONNECT | DEV_SPEED_MASK)
#define CDNSP_PORT_RWS	(PORT_PLS_MASK | PORT_WKCONN_E | PORT_WKDISC_E)
#define CDNSP_PORT_RW1CS (PORT_PED | PORT_CSC | PORT_RC | PORT_PLC)
#define CMD_R_S		BIT(0)
#define CMD_RESET	BIT(1)
#define CMD_INTE	BIT(2)
#define CMD_DSEIE	BIT(3)
#define CMD_CSS		BIT(8)
#define CMD_CRS		BIT(9)
#define CMD_EWE		BIT(10)
#define CMD_DEVEN	BIT(17)
#define CDNSP_IRQS	(CMD_INTE | CMD_DSEIE | CMD_EWE)
#define STS_HALT	BIT(0)
#define STS_FATAL	BIT(2)
#define STS_EINT	BIT(3)
#define STS_PCD		BIT(4)
#define STS_SSS		BIT(8)
#define STS_RSS		BIT(9)
#define STS_SRE		BIT(10)
#define STS_CNR		BIT(11)
#define STS_HCE		BIT(12)
#define CMD_RING_CS		BIT(0)
#define CMD_RING_ABORT		BIT(2)
#define CMD_RING_BUSY(p)	((p) & BIT(4))
#define CMD_RING_RUNNING	BIT(3)
#define CMD_RING_RSVD_BITS	GENMASK(5, 0)
#define MAX_DEVS		GENMASK(7, 0)
#define CONFIG_U3E		BIT(8)
#define PORT_CONNECT		BIT(0)
#define PORT_PED		BIT(1)
#define PORT_RESET		BIT(4)
#define PORT_PLS_MASK		GENMASK(8, 5)
#define XDEV_U0			(0x0 << 5)
#define XDEV_U1			(0x1 << 5)
#define XDEV_U2			(0x2 << 5)
#define XDEV_U3			(0x3 << 5)
#define XDEV_DISABLED		(0x4 << 5)
#define XDEV_RXDETECT		(0x5 << 5)
#define XDEV_INACTIVE		(0x6 << 5)
#define XDEV_POLLING		(0x7 << 5)
#define XDEV_RECOVERY		(0x8 << 5)
#define XDEV_HOT_RESET		(0x9 << 5)
#define XDEV_COMP_MODE		(0xa << 5)
#define XDEV_TEST_MODE		(0xb << 5)
#define XDEV_RESUME		(0xf << 5)
#define PORT_POWER		BIT(9)
#define DEV_SPEED_MASK		GENMASK(13, 10)
#define XDEV_FS			(0x1 << 10)
#define XDEV_HS			(0x3 << 10)
#define XDEV_SS			(0x4 << 10)
#define XDEV_SSP		(0x5 << 10)
#define DEV_UNDEFSPEED(p)	(((p) & DEV_SPEED_MASK) == (0x0 << 10))
#define DEV_FULLSPEED(p)	(((p) & DEV_SPEED_MASK) == XDEV_FS)
#define DEV_HIGHSPEED(p)	(((p) & DEV_SPEED_MASK) == XDEV_HS)
#define DEV_SUPERSPEED(p)	(((p) & DEV_SPEED_MASK) == XDEV_SS)
#define DEV_SUPERSPEEDPLUS(p)	(((p) & DEV_SPEED_MASK) == XDEV_SSP)
#define DEV_SUPERSPEED_ANY(p)	(((p) & DEV_SPEED_MASK) >= XDEV_SS)
#define DEV_PORT_SPEED(p)	(((p) >> 10) & 0x0f)
#define PORT_LINK_STROBE	BIT(16)
#define PORT_CSC		BIT(17)
#define PORT_WRC		BIT(19)
#define PORT_RC			BIT(21)
#define PORT_PLC		BIT(22)
#define PORT_CEC		BIT(23)
#define PORT_WKCONN_E		BIT(25)
#define PORT_WKDISC_E		BIT(26)
#define PORT_WR			BIT(31)
#define PORT_CHANGE_BITS (PORT_CSC | PORT_WRC | PORT_RC | PORT_PLC | PORT_CEC)
#define PORT_U1_TIMEOUT_MASK	GENMASK(7, 0)
#define PORT_U1_TIMEOUT(p)	((p) & PORT_U1_TIMEOUT_MASK)
#define PORT_U2_TIMEOUT_MASK	GENMASK(14, 8)
#define PORT_U2_TIMEOUT(p)	(((p) << 8) & PORT_U2_TIMEOUT_MASK)
#define PORT_L1S_MASK		GENMASK(2, 0)
#define PORT_L1S(p)		((p) & PORT_L1S_MASK)
#define PORT_L1S_ACK		PORT_L1S(1)
#define PORT_L1S_NYET		PORT_L1S(2)
#define PORT_L1S_STALL		PORT_L1S(3)
#define PORT_L1S_TIMEOUT	PORT_L1S(4)
#define PORT_RWE		BIT(3)
#define PORT_BESL(p)		(((p) << 4) & GENMASK(7, 4))
#define PORT_HLE		BIT(16)
#define PORT_RRBESL(p)		(((p) & GENMASK(20, 17)) >> 17)
#define PORT_TEST_MODE_MASK	GENMASK(31, 28)
#define PORT_TEST_MODE(p)	(((p) << 28) & PORT_TEST_MODE_MASK)
struct cdnsp_intr_reg {
	__le32 irq_pending;
	__le32 irq_control;
	__le32 erst_size;
	__le32 rsvd;
	__le64 erst_base;
	__le64 erst_dequeue;
};
#define IMAN_IE			BIT(1)
#define IMAN_IP			BIT(0)
#define IMAN_IE_SET(p)		((p) | IMAN_IE)
#define IMAN_IE_CLEAR(p)	((p) & ~IMAN_IE)
#define IMOD_INTERVAL_MASK	GENMASK(15, 0)
#define IMOD_COUNTER_MASK	GENMASK(31, 16)
#define IMOD_DEFAULT_INTERVAL	0
#define ERST_SIZE_MASK		GENMASK(31, 16)
#define ERST_DESI_MASK		GENMASK(2, 0)
#define ERST_EHB		BIT(3)
#define ERST_PTR_MASK		GENMASK(3, 0)
struct cdnsp_run_regs {
	__le32 microframe_index;
	__le32 rsvd[7];
	struct cdnsp_intr_reg ir_set[128];
};
struct cdnsp_20port_cap {
	__le32 ext_cap;
	__le32 port_reg1;
	__le32 port_reg2;
	__le32 port_reg3;
	__le32 port_reg4;
	__le32 port_reg5;
	__le32 port_reg6;
};
#define EXT_CAPS_ID(p)			(((p) >> 0) & GENMASK(7, 0))
#define EXT_CAPS_NEXT(p)		(((p) >> 8) & GENMASK(7, 0))
#define EXT_CAPS_PROTOCOL		2
#define EXT_CAP_CFG_DEV_20PORT_CAP_ID	0xC1
#define PORT_REG6_L1_L0_HW_EN		BIT(1)
#define PORT_REG6_FORCE_FS		BIT(0)
struct cdnsp_3xport_cap {
	__le32 ext_cap;
	__le32 mode_addr;
	__le32 reserved[52];
	__le32 mode_2;
};
#define D_XEC_CFG_3XPORT_CAP		0xC0
#define CFG_3XPORT_SSP_SUPPORT		BIT(31)
#define CFG_3XPORT_U1_PIPE_CLK_GATE_EN	BIT(0)
#define RTL_REV_CAP			0xC4
#define RTL_REV_CAP_RX_BUFF_CMD_SIZE	BITMASK(31, 24)
#define RTL_REV_CAP_RX_BUFF_SIZE	BITMASK(15, 0)
#define RTL_REV_CAP_TX_BUFF_CMD_SIZE	BITMASK(31, 24)
#define RTL_REV_CAP_TX_BUFF_SIZE	BITMASK(15, 0)
#define CDNSP_VER_1 0x00000000
#define CDNSP_VER_2 0x10000000
#define CDNSP_IF_EP_EXIST(pdev, ep_num, dir) \
			 (readl(&(pdev)->rev_cap->ep_supported) & \
			 (BIT(ep_num) << ((dir) ? 0 : 16)))
struct cdnsp_rev_cap {
	__le32 ext_cap;
	__le32 rtl_revision;
	__le32 rx_buff_size;
	__le32 tx_buff_size;
	__le32 ep_supported;
	__le32 ctrl_revision;
};
#define D_XEC_PRE_REGS_CAP		0xC8
#define REG_CHICKEN_BITS_2_OFFSET	0x48
#define CHICKEN_XDMA_2_TP_CACHE_DIS	BIT(28)
#define XBUF_CAP_ID			0xCB
#define XBUF_RX_TAG_MASK_0_OFFSET	0x1C
#define XBUF_RX_TAG_MASK_1_OFFSET	0x24
#define XBUF_TX_CMD_OFFSET		0x2C
struct cdnsp_doorbell_array {
	__le32 cmd_db;
	__le32 ep_db;
};
#define DB_VALUE(ep, stream)		((((ep) + 1) & 0xff) | ((stream) << 16))
#define DB_VALUE_EP0_OUT(ep, stream)	((ep) & 0xff)
#define DB_VALUE_CMD			0x00000000
struct cdnsp_container_ctx {
	unsigned int type;
#define CDNSP_CTX_TYPE_DEVICE	0x1
#define CDNSP_CTX_TYPE_INPUT	0x2
	int size;
	int ctx_size;
	dma_addr_t dma;
	u8 *bytes;
};
struct cdnsp_slot_ctx {
	__le32 dev_info;
	__le32 dev_port;
	__le32 int_target;
	__le32 dev_state;
	__le32 reserved[4];
};
#define SLOT_SPEED_FS		(XDEV_FS << 10)
#define SLOT_SPEED_HS		(XDEV_HS << 10)
#define SLOT_SPEED_SS		(XDEV_SS << 10)
#define SLOT_SPEED_SSP		(XDEV_SSP << 10)
#define DEV_SPEED		GENMASK(23, 20)
#define GET_DEV_SPEED(n)	(((n) & DEV_SPEED) >> 20)
#define LAST_CTX_MASK		((unsigned int)GENMASK(31, 27))
#define LAST_CTX(p)		((p) << 27)
#define LAST_CTX_TO_EP_NUM(p)	(((p) >> 27) - 1)
#define SLOT_FLAG		BIT(0)
#define EP0_FLAG		BIT(1)
#define DEV_PORT(p)		(((p) & 0xff) << 16)
#define DEV_ADDR_MASK		GENMASK(7, 0)
#define SLOT_STATE		GENMASK(31, 27)
#define GET_SLOT_STATE(p)	(((p) & SLOT_STATE) >> 27)
#define SLOT_STATE_DISABLED	0
#define SLOT_STATE_ENABLED	SLOT_STATE_DISABLED
#define SLOT_STATE_DEFAULT	1
#define SLOT_STATE_ADDRESSED	2
#define SLOT_STATE_CONFIGURED	3
struct cdnsp_ep_ctx {
	__le32 ep_info;
	__le32 ep_info2;
	__le64 deq;
	__le32 tx_info;
	__le32 reserved[3];
};
#define EP_STATE_MASK		GENMASK(3, 0)
#define EP_STATE_DISABLED	0
#define EP_STATE_RUNNING	1
#define EP_STATE_HALTED		2
#define EP_STATE_STOPPED	3
#define EP_STATE_ERROR		4
#define GET_EP_CTX_STATE(ctx)	(le32_to_cpu((ctx)->ep_info) & EP_STATE_MASK)
#define EP_MULT(p)			(((p) << 8) & GENMASK(9, 8))
#define CTX_TO_EP_MULT(p)		(((p) & GENMASK(9, 8)) >> 8)
#define EP_INTERVAL(p)			(((p) << 16) & GENMASK(23, 16))
#define EP_INTERVAL_TO_UFRAMES(p)	(1 << (((p) & GENMASK(23, 16)) >> 16))
#define CTX_TO_EP_INTERVAL(p)		(((p) & GENMASK(23, 16)) >> 16)
#define EP_MAXPSTREAMS_MASK		GENMASK(14, 10)
#define EP_MAXPSTREAMS(p)		(((p) << 10) & EP_MAXPSTREAMS_MASK)
#define CTX_TO_EP_MAXPSTREAMS(p)	(((p) & EP_MAXPSTREAMS_MASK) >> 10)
#define EP_HAS_LSA			BIT(15)
#define ERROR_COUNT(p)		(((p) & 0x3) << 1)
#define CTX_TO_EP_TYPE(p)	(((p) >> 3) & 0x7)
#define EP_TYPE(p)		((p) << 3)
#define ISOC_OUT_EP		1
#define BULK_OUT_EP		2
#define INT_OUT_EP		3
#define CTRL_EP			4
#define ISOC_IN_EP		5
#define BULK_IN_EP		6
#define INT_IN_EP		7
#define MAX_BURST(p)		(((p) << 8) & GENMASK(15, 8))
#define CTX_TO_MAX_BURST(p)	(((p) & GENMASK(15, 8)) >> 8)
#define MAX_PACKET(p)		(((p) << 16) & GENMASK(31, 16))
#define MAX_PACKET_MASK		GENMASK(31, 16)
#define MAX_PACKET_DECODED(p)	(((p) & GENMASK(31, 16)) >> 16)
#define EP_AVG_TRB_LENGTH(p)		((p) & GENMASK(15, 0))
#define EP_MAX_ESIT_PAYLOAD_LO(p)	(((p) << 16) & GENMASK(31, 16))
#define EP_MAX_ESIT_PAYLOAD_HI(p)	((((p) & GENMASK(23, 16)) >> 16) << 24)
#define CTX_TO_MAX_ESIT_PAYLOAD_LO(p)	(((p) & GENMASK(31, 16)) >> 16)
#define CTX_TO_MAX_ESIT_PAYLOAD_HI(p)	(((p) & GENMASK(31, 24)) >> 24)
#define EP_CTX_CYCLE_MASK		BIT(0)
#define CTX_DEQ_MASK			(~0xfL)
struct cdnsp_input_control_ctx {
	__le32 drop_flags;
	__le32 add_flags;
	__le32 rsvd2[6];
};
struct cdnsp_command {
	struct cdnsp_container_ctx *in_ctx;
	u32 status;
	union cdnsp_trb *command_trb;
};
struct cdnsp_stream_ctx {
	__le64 stream_ring;
	__le32 reserved[2];
};
#define SCT_FOR_CTX(p)		(((p) << 1) & GENMASK(3, 1))
#define SCT_SEC_TR		0
#define SCT_PRI_TR		1
struct cdnsp_stream_info {
	struct cdnsp_ring **stream_rings;
	unsigned int num_streams;
	struct cdnsp_stream_ctx *stream_ctx_array;
	unsigned int num_stream_ctxs;
	dma_addr_t ctx_array_dma;
	struct radix_tree_root trb_address_map;
	int td_count;
	u8 first_prime_det;
#define STREAM_DRBL_FIFO_DEPTH 2
	u8 drbls_count;
};
#define STREAM_LOG_STREAMS 4
#define STREAM_NUM_STREAMS BIT(STREAM_LOG_STREAMS)
#if STREAM_LOG_STREAMS > 16 && STREAM_LOG_STREAMS < 1
#error "Not suupported stream value"
#endif
struct cdnsp_ep {
	struct usb_ep endpoint;
	struct list_head pending_list;
	struct cdnsp_device *pdev;
	u8 number;
	u8 idx;
	u32 interval;
	char name[20];
	u8 direction;
	u8 buffering;
	u8 buffering_period;
	struct cdnsp_ep_ctx *in_ctx;
	struct cdnsp_ep_ctx *out_ctx;
	struct cdnsp_ring *ring;
	struct cdnsp_stream_info stream_info;
	unsigned int ep_state;
#define EP_ENABLED		BIT(0)
#define EP_DIS_IN_RROGRESS	BIT(1)
#define EP_HALTED		BIT(2)
#define EP_STOPPED		BIT(3)
#define EP_WEDGE		BIT(4)
#define EP0_HALTED_STATUS	BIT(5)
#define EP_HAS_STREAMS		BIT(6)
#define EP_UNCONFIGURED		BIT(7)
	bool skip;
};
struct cdnsp_device_context_array {
	__le64 dev_context_ptrs[CDNSP_DEV_MAX_SLOTS + 1];
	dma_addr_t dma;
};
struct cdnsp_transfer_event {
	__le64 buffer;
	__le32 transfer_len;
	__le32 flags;
};
#define TRB_EVENT_INVALIDATE 8
#define EVENT_TRB_LEN(p)			((p) & GENMASK(23, 0))
#define COMP_CODE_MASK				(0xff << 24)
#define GET_COMP_CODE(p)			(((p) & COMP_CODE_MASK) >> 24)
#define COMP_INVALID				0
#define COMP_SUCCESS				1
#define COMP_DATA_BUFFER_ERROR			2
#define COMP_BABBLE_DETECTED_ERROR		3
#define COMP_TRB_ERROR				5
#define COMP_RESOURCE_ERROR			7
#define COMP_NO_SLOTS_AVAILABLE_ERROR		9
#define COMP_INVALID_STREAM_TYPE_ERROR		10
#define COMP_SLOT_NOT_ENABLED_ERROR		11
#define COMP_ENDPOINT_NOT_ENABLED_ERROR		12
#define COMP_SHORT_PACKET			13
#define COMP_RING_UNDERRUN			14
#define COMP_RING_OVERRUN			15
#define COMP_VF_EVENT_RING_FULL_ERROR		16
#define COMP_PARAMETER_ERROR			17
#define COMP_CONTEXT_STATE_ERROR		19
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
#define TRB_TO_DEV_STREAM(p)			((p) & GENMASK(16, 0))
#define TRB_TO_HOST_STREAM(p)			((p) & GENMASK(16, 0))
#define STREAM_PRIME_ACK			0xFFFE
#define STREAM_REJECTED				0xFFFF
#define TRB_TO_EP_ID(p)				(((p) & GENMASK(20, 16)) >> 16)
struct cdnsp_link_trb {
	__le64 segment_ptr;
	__le32 intr_target;
	__le32 control;
};
#define LINK_TOGGLE	BIT(1)
struct cdnsp_event_cmd {
	__le64 cmd_trb;
	__le32 status;
	__le32 flags;
};
#define TRB_BSR		BIT(9)
#define TRB_DC		BIT(9)
#define TRB_FH_TO_PACKET_TYPE(p)	((p) & GENMASK(4, 0))
#define TRB_FH_TR_PACKET		0x4
#define TRB_FH_TO_DEVICE_ADDRESS(p)	(((p) << 25) & GENMASK(31, 25))
#define TRB_FH_TR_PACKET_DEV_NOT	0x6
#define TRB_FH_TO_NOT_TYPE(p)		(((p) << 4) & GENMASK(7, 4))
#define TRB_FH_TR_PACKET_FUNCTION_WAKE	0x1
#define TRB_FH_TO_INTERFACE(p)		(((p) << 8) & GENMASK(15, 8))
enum cdnsp_setup_dev {
	SETUP_CONTEXT_ONLY,
	SETUP_CONTEXT_ADDRESS,
};
#define TRB_TO_SLOT_ID(p)		(((p) & GENMASK(31, 24)) >> 24)
#define SLOT_ID_FOR_TRB(p)		(((p) << 24) & GENMASK(31, 24))
#define TRB_TO_EP_INDEX(p)		(((p) >> 16) & 0x1f)
#define EP_ID_FOR_TRB(p)		((((p) + 1) << 16) & GENMASK(20, 16))
#define SUSPEND_PORT_FOR_TRB(p)		(((p) & 1) << 23)
#define TRB_TO_SUSPEND_PORT(p)		(((p) >> 23) & 0x1)
#define LAST_EP_INDEX			30
#define TRB_TO_STREAM_ID(p)		((((p) & GENMASK(31, 16)) >> 16))
#define STREAM_ID_FOR_TRB(p)		((((p)) << 16) & GENMASK(31, 16))
#define SCT_FOR_TRB(p)			(((p) << 1) & 0x7)
#define TRB_TC				BIT(1)
#define GET_PORT_ID(p)			(((p) & GENMASK(31, 24)) >> 24)
#define SET_PORT_ID(p)			(((p) << 24) & GENMASK(31, 24))
#define EVENT_DATA			BIT(2)
#define TRB_LEN(p)			((p) & GENMASK(16, 0))
#define TRB_TD_SIZE(p)			(min((p), (u32)31) << 17)
#define GET_TD_SIZE(p)			(((p) & GENMASK(21, 17)) >> 17)
#define TRB_TD_SIZE_TBC(p)		(min((p), (u32)31) << 17)
#define TRB_INTR_TARGET(p)		(((p) << 22) & GENMASK(31, 22))
#define GET_INTR_TARGET(p)		(((p) & GENMASK(31, 22)) >> 22)
#define TRB_TBC(p)			(((p) & 0x3) << 7)
#define TRB_TLBPC(p)			(((p) & 0xf) << 16)
#define TRB_CYCLE			BIT(0)
#define TRB_ENT				BIT(1)
#define TRB_ISP				BIT(2)
#define TRB_NO_SNOOP			BIT(3)
#define TRB_CHAIN			BIT(4)
#define TRB_IOC				BIT(5)
#define TRB_IDT				BIT(6)
#define TRB_STAT			BIT(7)
#define TRB_BEI				BIT(9)
#define TRB_DIR_IN			BIT(16)
#define TRB_SETUPID_BITMASK		GENMASK(9, 8)
#define TRB_SETUPID(p)			((p) << 8)
#define TRB_SETUPID_TO_TYPE(p)		(((p) & TRB_SETUPID_BITMASK) >> 8)
#define TRB_SETUP_SPEEDID_USB3		0x1
#define TRB_SETUP_SPEEDID_USB2		0x0
#define TRB_SETUP_SPEEDID(p)		((p) & (1 << 7))
#define TRB_SETUPSTAT_ACK		0x1
#define TRB_SETUPSTAT_STALL		0x0
#define TRB_SETUPSTAT(p)		((p) << 6)
#define TRB_SIA				BIT(31)
#define TRB_FRAME_ID(p)			(((p) << 20) & GENMASK(30, 20))
struct cdnsp_generic_trb {
	__le32 field[4];
};
union cdnsp_trb {
	struct cdnsp_link_trb link;
	struct cdnsp_transfer_event trans_event;
	struct cdnsp_event_cmd event_cmd;
	struct cdnsp_generic_trb generic;
};
#define TRB_TYPE_BITMASK	GENMASK(15, 10)
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
#define TRB_FORCE_HEADER	22
#define TRB_CMD_NOOP		23
#define TRB_TRANSFER		32
#define TRB_COMPLETION		33
#define TRB_PORT_STATUS		34
#define TRB_HC_EVENT		37
#define TRB_MFINDEX_WRAP	39
#define TRB_ENDPOINT_NRDY	48
#define TRB_HALT_ENDPOINT	54
#define TRB_DRB_OVERFLOW	57
#define TRB_FLUSH_ENDPOINT	58
#define TRB_TYPE_LINK(x)	(((x) & TRB_TYPE_BITMASK) == TRB_TYPE(TRB_LINK))
#define TRB_TYPE_LINK_LE32(x)	(((x) & cpu_to_le32(TRB_TYPE_BITMASK)) == \
					cpu_to_le32(TRB_TYPE(TRB_LINK)))
#define TRB_TYPE_NOOP_LE32(x)	(((x) & cpu_to_le32(TRB_TYPE_BITMASK)) == \
					cpu_to_le32(TRB_TYPE(TRB_TR_NOOP)))
#define TRBS_PER_SEGMENT		256
#define TRBS_PER_EVENT_SEGMENT		256
#define TRBS_PER_EV_DEQ_UPDATE		100
#define TRB_SEGMENT_SIZE		(TRBS_PER_SEGMENT * 16)
#define TRB_SEGMENT_SHIFT		(ilog2(TRB_SEGMENT_SIZE))
#define TRB_MAX_BUFF_SHIFT		16
#define TRB_MAX_BUFF_SIZE		BIT(TRB_MAX_BUFF_SHIFT)
#define TRB_BUFF_LEN_UP_TO_BOUNDARY(addr) (TRB_MAX_BUFF_SIZE - \
					((addr) & (TRB_MAX_BUFF_SIZE - 1)))
struct cdnsp_segment {
	union cdnsp_trb *trbs;
	struct cdnsp_segment *next;
	dma_addr_t dma;
	dma_addr_t bounce_dma;
	void *bounce_buf;
	unsigned int bounce_offs;
	unsigned int bounce_len;
};
struct cdnsp_td {
	struct list_head td_list;
	struct cdnsp_request *preq;
	struct cdnsp_segment *start_seg;
	union cdnsp_trb *first_trb;
	union cdnsp_trb *last_trb;
	struct cdnsp_segment *bounce_seg;
	bool request_length_set;
	bool drbl;
};
struct cdnsp_dequeue_state {
	struct cdnsp_segment *new_deq_seg;
	union cdnsp_trb *new_deq_ptr;
	int new_cycle_state;
	unsigned int stream_id;
};
enum cdnsp_ring_type {
	TYPE_CTRL = 0,
	TYPE_ISOC,
	TYPE_BULK,
	TYPE_INTR,
	TYPE_STREAM,
	TYPE_COMMAND,
	TYPE_EVENT,
};
struct cdnsp_ring {
	struct cdnsp_segment *first_seg;
	struct cdnsp_segment *last_seg;
	union cdnsp_trb	 *enqueue;
	struct cdnsp_segment *enq_seg;
	union cdnsp_trb	 *dequeue;
	struct cdnsp_segment *deq_seg;
	struct list_head td_list;
	u32 cycle_state;
	unsigned int stream_id;
	unsigned int stream_active;
	unsigned int stream_rejected;
	int num_tds;
	unsigned int num_segs;
	unsigned int num_trbs_free;
	unsigned int bounce_buf_len;
	enum cdnsp_ring_type type;
	bool last_td_was_short;
	struct radix_tree_root *trb_address_map;
};
struct cdnsp_erst_entry {
	__le64 seg_addr;
	__le32 seg_size;
	__le32 rsvd;
};
struct cdnsp_erst {
	struct cdnsp_erst_entry *entries;
	unsigned int num_entries;
	dma_addr_t erst_dma_addr;
};
struct cdnsp_request {
	struct	cdnsp_td td;
	struct usb_request request;
	struct list_head list;
	struct cdnsp_ep	 *pep;
	u8 epnum;
	unsigned direction:1;
};
#define	ERST_NUM_SEGS	1
enum cdnsp_ep0_stage {
	CDNSP_SETUP_STAGE,
	CDNSP_DATA_STAGE,
	CDNSP_STATUS_STAGE,
};
struct cdnsp_port {
	struct cdnsp_port_regs __iomem *regs;
	u8 port_num;
	u8 exist;
	u8 maj_rev;
	u8 min_rev;
};
#define CDNSP_EXT_PORT_MAJOR(x)		(((x) >> 24) & 0xff)
#define CDNSP_EXT_PORT_MINOR(x)		(((x) >> 16) & 0xff)
#define CDNSP_EXT_PORT_OFF(x)		((x) & 0xff)
#define CDNSP_EXT_PORT_COUNT(x)		(((x) >> 8) & 0xff)
struct cdnsp_device {
	struct device *dev;
	struct usb_gadget gadget;
	struct usb_gadget_driver *gadget_driver;
	unsigned int irq;
	void __iomem *regs;
	struct cdnsp_cap_regs __iomem *cap_regs;
	struct cdnsp_op_regs __iomem *op_regs;
	struct cdnsp_run_regs __iomem *run_regs;
	struct cdnsp_doorbell_array __iomem *dba;
	struct	cdnsp_intr_reg __iomem *ir_set;
	struct cdnsp_20port_cap __iomem *port20_regs;
	struct cdnsp_3xport_cap __iomem *port3x_regs;
	struct cdnsp_rev_cap __iomem *rev_cap;
	__u32 hcs_params1;
	__u32 hcs_params3;
	__u32 hcc_params;
	spinlock_t lock;
	struct usb_ctrlrequest setup;
	struct cdnsp_request ep0_preq;
	enum cdnsp_ep0_stage ep0_stage;
	u8 three_stage_setup;
	u8 ep0_expect_in;
	u8 setup_id;
	u8 setup_speed;
	void *setup_buf;
	u8 device_address;
	int may_wakeup;
	u16 hci_version;
	struct cdnsp_device_context_array *dcbaa;
	struct cdnsp_ring *cmd_ring;
	struct cdnsp_command cmd;
	struct cdnsp_ring *event_ring;
	struct cdnsp_erst erst;
	int slot_id;
	struct cdnsp_container_ctx out_ctx;
	struct cdnsp_container_ctx in_ctx;
	struct cdnsp_ep eps[CDNSP_ENDPOINTS_NUM];
	u8 usb2_hw_lpm_capable:1;
	u8 u1_allowed:1;
	u8 u2_allowed:1;
	struct dma_pool *device_pool;
	struct dma_pool	*segment_pool;
#define CDNSP_STATE_HALTED		BIT(1)
#define CDNSP_STATE_DYING		BIT(2)
#define CDNSP_STATE_DISCONNECT_PENDING	BIT(3)
#define CDNSP_WAKEUP_PENDING		BIT(4)
	unsigned int cdnsp_state;
	unsigned int link_state;
	struct cdnsp_port usb2_port;
	struct cdnsp_port usb3_port;
	struct cdnsp_port *active_port;
	u16 test_mode;
};
static inline u64 cdnsp_read_64(__le64 __iomem *regs)
{
	return lo_hi_readq(regs);
}
static inline void cdnsp_write_64(const u64 val, __le64 __iomem *regs)
{
	lo_hi_writeq(val, regs);
}
void cdnsp_mem_cleanup(struct cdnsp_device *pdev);
int cdnsp_mem_init(struct cdnsp_device *pdev);
int cdnsp_setup_addressable_priv_dev(struct cdnsp_device *pdev);
void cdnsp_copy_ep0_dequeue_into_input_ctx(struct cdnsp_device *pdev);
void cdnsp_endpoint_zero(struct cdnsp_device *pdev, struct cdnsp_ep *ep);
int cdnsp_endpoint_init(struct cdnsp_device *pdev,
			struct cdnsp_ep *pep,
			gfp_t mem_flags);
int cdnsp_ring_expansion(struct cdnsp_device *pdev,
			 struct cdnsp_ring *ring,
			 unsigned int num_trbs, gfp_t flags);
struct cdnsp_ring *cdnsp_dma_to_transfer_ring(struct cdnsp_ep *ep, u64 address);
int cdnsp_alloc_stream_info(struct cdnsp_device *pdev,
			    struct cdnsp_ep *pep,
			    unsigned int num_stream_ctxs,
			    unsigned int num_streams);
int cdnsp_alloc_streams(struct cdnsp_device *pdev, struct cdnsp_ep *pep);
void cdnsp_free_endpoint_rings(struct cdnsp_device *pdev, struct cdnsp_ep *pep);
int cdnsp_find_next_ext_cap(void __iomem *base, u32 start, int id);
int cdnsp_halt(struct cdnsp_device *pdev);
void cdnsp_died(struct cdnsp_device *pdev);
int cdnsp_reset(struct cdnsp_device *pdev);
irqreturn_t cdnsp_irq_handler(int irq, void *priv);
int cdnsp_setup_device(struct cdnsp_device *pdev, enum cdnsp_setup_dev setup);
void cdnsp_set_usb2_hardware_lpm(struct cdnsp_device *usbsssp_data,
				 struct usb_request *req, int enable);
irqreturn_t cdnsp_thread_irq_handler(int irq, void *data);
dma_addr_t cdnsp_trb_virt_to_dma(struct cdnsp_segment *seg,
				 union cdnsp_trb *trb);
bool cdnsp_last_trb_on_seg(struct cdnsp_segment *seg, union cdnsp_trb *trb);
bool cdnsp_last_trb_on_ring(struct cdnsp_ring *ring,
			    struct cdnsp_segment *seg,
			    union cdnsp_trb *trb);
int cdnsp_wait_for_cmd_compl(struct cdnsp_device *pdev);
void cdnsp_update_erst_dequeue(struct cdnsp_device *pdev,
			       union cdnsp_trb *event_ring_deq,
			       u8 clear_ehb);
void cdnsp_initialize_ring_info(struct cdnsp_ring *ring);
void cdnsp_ring_cmd_db(struct cdnsp_device *pdev);
void cdnsp_queue_slot_control(struct cdnsp_device *pdev, u32 trb_type);
void cdnsp_queue_address_device(struct cdnsp_device *pdev,
				dma_addr_t in_ctx_ptr,
				enum cdnsp_setup_dev setup);
void cdnsp_queue_stop_endpoint(struct cdnsp_device *pdev,
			       unsigned int ep_index);
int cdnsp_queue_ctrl_tx(struct cdnsp_device *pdev, struct cdnsp_request *preq);
int cdnsp_queue_bulk_tx(struct cdnsp_device *pdev, struct cdnsp_request *preq);
int cdnsp_queue_isoc_tx(struct cdnsp_device *pdev,
			struct cdnsp_request *preq);
void cdnsp_queue_configure_endpoint(struct cdnsp_device *pdev,
				    dma_addr_t in_ctx_ptr);
void cdnsp_queue_reset_ep(struct cdnsp_device *pdev, unsigned int ep_index);
void cdnsp_queue_halt_endpoint(struct cdnsp_device *pdev,
			       unsigned int ep_index);
void cdnsp_queue_flush_endpoint(struct cdnsp_device *pdev,
				unsigned int ep_index);
void cdnsp_force_header_wakeup(struct cdnsp_device *pdev, int intf_num);
void cdnsp_queue_reset_device(struct cdnsp_device *pdev);
void cdnsp_queue_new_dequeue_state(struct cdnsp_device *pdev,
				   struct cdnsp_ep *pep,
				   struct cdnsp_dequeue_state *deq_state);
void cdnsp_ring_doorbell_for_active_rings(struct cdnsp_device *pdev,
					  struct cdnsp_ep *pep);
void cdnsp_inc_deq(struct cdnsp_device *pdev, struct cdnsp_ring *ring);
void cdnsp_set_link_state(struct cdnsp_device *pdev,
			  __le32 __iomem *port_regs, u32 link_state);
u32 cdnsp_port_state_to_neutral(u32 state);
int cdnsp_enable_slot(struct cdnsp_device *pdev);
int cdnsp_disable_slot(struct cdnsp_device *pdev);
struct cdnsp_input_control_ctx
	*cdnsp_get_input_control_ctx(struct cdnsp_container_ctx *ctx);
struct cdnsp_slot_ctx *cdnsp_get_slot_ctx(struct cdnsp_container_ctx *ctx);
struct cdnsp_ep_ctx *cdnsp_get_ep_ctx(struct cdnsp_container_ctx *ctx,
				      unsigned int ep_index);
void cdnsp_suspend_gadget(struct cdnsp_device *pdev);
void cdnsp_resume_gadget(struct cdnsp_device *pdev);
void cdnsp_disconnect_gadget(struct cdnsp_device *pdev);
void cdnsp_gadget_giveback(struct cdnsp_ep *pep, struct cdnsp_request *preq,
			   int status);
int cdnsp_ep_enqueue(struct cdnsp_ep *pep, struct cdnsp_request *preq);
int cdnsp_ep_dequeue(struct cdnsp_ep *pep, struct cdnsp_request *preq);
unsigned int cdnsp_port_speed(unsigned int port_status);
void cdnsp_irq_reset(struct cdnsp_device *pdev);
int cdnsp_halt_endpoint(struct cdnsp_device *pdev,
			struct cdnsp_ep *pep, int value);
int cdnsp_cmd_stop_ep(struct cdnsp_device *pdev, struct cdnsp_ep *pep);
int cdnsp_cmd_flush_ep(struct cdnsp_device *pdev, struct cdnsp_ep *pep);
void cdnsp_setup_analyze(struct cdnsp_device *pdev);
int cdnsp_status_stage(struct cdnsp_device *pdev);
int cdnsp_reset_device(struct cdnsp_device *pdev);
static inline struct cdnsp_request *next_request(struct list_head *list)
{
	return list_first_entry_or_null(list, struct cdnsp_request, list);
}
#define to_cdnsp_ep(ep) (container_of(ep, struct cdnsp_ep, endpoint))
#define gadget_to_cdnsp(g) (container_of(g, struct cdnsp_device, gadget))
#define request_to_cdnsp_request(r) (container_of(r, struct cdnsp_request, \
				     request))
#define to_cdnsp_request(r) (container_of(r, struct cdnsp_request, request))
int cdnsp_remove_request(struct cdnsp_device *pdev, struct cdnsp_request *preq,
			 struct cdnsp_ep *pep);
#endif  
