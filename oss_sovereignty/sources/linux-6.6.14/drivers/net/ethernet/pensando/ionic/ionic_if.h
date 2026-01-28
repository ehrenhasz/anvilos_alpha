


#ifndef _IONIC_IF_H_
#define _IONIC_IF_H_

#define IONIC_DEV_INFO_SIGNATURE		0x44455649      
#define IONIC_DEV_INFO_VERSION			1
#define IONIC_IFNAMSIZ				16


enum ionic_cmd_opcode {
	IONIC_CMD_NOP				= 0,

	
	IONIC_CMD_IDENTIFY			= 1,
	IONIC_CMD_INIT				= 2,
	IONIC_CMD_RESET				= 3,
	IONIC_CMD_GETATTR			= 4,
	IONIC_CMD_SETATTR			= 5,

	
	IONIC_CMD_PORT_IDENTIFY			= 10,
	IONIC_CMD_PORT_INIT			= 11,
	IONIC_CMD_PORT_RESET			= 12,
	IONIC_CMD_PORT_GETATTR			= 13,
	IONIC_CMD_PORT_SETATTR			= 14,

	
	IONIC_CMD_LIF_IDENTIFY			= 20,
	IONIC_CMD_LIF_INIT			= 21,
	IONIC_CMD_LIF_RESET			= 22,
	IONIC_CMD_LIF_GETATTR			= 23,
	IONIC_CMD_LIF_SETATTR			= 24,
	IONIC_CMD_LIF_SETPHC			= 25,

	IONIC_CMD_RX_MODE_SET			= 30,
	IONIC_CMD_RX_FILTER_ADD			= 31,
	IONIC_CMD_RX_FILTER_DEL			= 32,

	
	IONIC_CMD_Q_IDENTIFY			= 39,
	IONIC_CMD_Q_INIT			= 40,
	IONIC_CMD_Q_CONTROL			= 41,

	
	IONIC_CMD_RDMA_RESET_LIF		= 50,
	IONIC_CMD_RDMA_CREATE_EQ		= 51,
	IONIC_CMD_RDMA_CREATE_CQ		= 52,
	IONIC_CMD_RDMA_CREATE_ADMINQ		= 53,

	
	IONIC_CMD_VF_GETATTR			= 60,
	IONIC_CMD_VF_SETATTR			= 61,
	IONIC_CMD_VF_CTRL			= 62,

	
	IONIC_CMD_QOS_CLASS_IDENTIFY		= 240,
	IONIC_CMD_QOS_CLASS_INIT		= 241,
	IONIC_CMD_QOS_CLASS_RESET		= 242,
	IONIC_CMD_QOS_CLASS_UPDATE		= 243,
	IONIC_CMD_QOS_CLEAR_STATS		= 244,
	IONIC_CMD_QOS_RESET			= 245,

	
	IONIC_CMD_FW_DOWNLOAD                   = 252,
	IONIC_CMD_FW_CONTROL                    = 253,
	IONIC_CMD_FW_DOWNLOAD_V1		= 254,
	IONIC_CMD_FW_CONTROL_V1		        = 255,
};


enum ionic_status_code {
	IONIC_RC_SUCCESS	= 0,	
	IONIC_RC_EVERSION	= 1,	
	IONIC_RC_EOPCODE	= 2,	
	IONIC_RC_EIO		= 3,	
	IONIC_RC_EPERM		= 4,	
	IONIC_RC_EQID		= 5,	
	IONIC_RC_EQTYPE		= 6,	
	IONIC_RC_ENOENT		= 7,	
	IONIC_RC_EINTR		= 8,	
	IONIC_RC_EAGAIN		= 9,	
	IONIC_RC_ENOMEM		= 10,	
	IONIC_RC_EFAULT		= 11,	
	IONIC_RC_EBUSY		= 12,	
	IONIC_RC_EEXIST		= 13,	
	IONIC_RC_EINVAL		= 14,	
	IONIC_RC_ENOSPC		= 15,	
	IONIC_RC_ERANGE		= 16,	
	IONIC_RC_BAD_ADDR	= 17,	
	IONIC_RC_DEV_CMD	= 18,	
	IONIC_RC_ENOSUPP	= 19,	
	IONIC_RC_ERROR		= 29,	
	IONIC_RC_ERDMA		= 30,	
	IONIC_RC_EVFID		= 31,	
	IONIC_RC_EBAD_FW	= 32,	
};

enum ionic_notifyq_opcode {
	IONIC_EVENT_LINK_CHANGE		= 1,
	IONIC_EVENT_RESET		= 2,
	IONIC_EVENT_HEARTBEAT		= 3,
	IONIC_EVENT_LOG			= 4,
	IONIC_EVENT_XCVR		= 5,
};


struct ionic_admin_cmd {
	u8     opcode;
	u8     rsvd;
	__le16 lif_index;
	u8     cmd_data[60];
};


struct ionic_admin_comp {
	u8     status;
	u8     rsvd;
	__le16 comp_index;
	u8     cmd_data[11];
	u8     color;
#define IONIC_COMP_COLOR_MASK  0x80
};

static inline u8 color_match(u8 color, u8 done_color)
{
	return (!!(color & IONIC_COMP_COLOR_MASK)) == done_color;
}


struct ionic_nop_cmd {
	u8 opcode;
	u8 rsvd[63];
};


struct ionic_nop_comp {
	u8 status;
	u8 rsvd[15];
};


struct ionic_dev_init_cmd {
	u8     opcode;
	u8     type;
	u8     rsvd[62];
};


struct ionic_dev_init_comp {
	u8 status;
	u8 rsvd[15];
};


struct ionic_dev_reset_cmd {
	u8 opcode;
	u8 rsvd[63];
};


struct ionic_dev_reset_comp {
	u8 status;
	u8 rsvd[15];
};

#define IONIC_IDENTITY_VERSION_1	1
#define IONIC_DEV_IDENTITY_VERSION_2	2


struct ionic_dev_identify_cmd {
	u8 opcode;
	u8 ver;
	u8 rsvd[62];
};


struct ionic_dev_identify_comp {
	u8 status;
	u8 ver;
	u8 rsvd[14];
};

enum ionic_os_type {
	IONIC_OS_TYPE_LINUX   = 1,
	IONIC_OS_TYPE_WIN     = 2,
	IONIC_OS_TYPE_DPDK    = 3,
	IONIC_OS_TYPE_FREEBSD = 4,
	IONIC_OS_TYPE_IPXE    = 5,
	IONIC_OS_TYPE_ESXI    = 6,
};


union ionic_drv_identity {
	struct {
		__le32 os_type;
		__le32 os_dist;
		char   os_dist_str[128];
		__le32 kernel_ver;
		char   kernel_ver_str[32];
		char   driver_ver_str[32];
	};
	__le32 words[478];
};


enum ionic_dev_capability {
	IONIC_DEV_CAP_VF_CTRL        = BIT(0),
};


union ionic_dev_identity {
	struct {
		u8     version;
		u8     type;
		u8     rsvd[2];
		u8     nports;
		u8     rsvd2[3];
		__le32 nlifs;
		__le32 nintrs;
		__le32 ndbpgs_per_lif;
		__le32 intr_coal_mult;
		__le32 intr_coal_div;
		__le32 eq_count;
		__le64 hwstamp_mask;
		__le32 hwstamp_mult;
		__le32 hwstamp_shift;
		__le64 capabilities;
	};
	__le32 words[478];
};

enum ionic_lif_type {
	IONIC_LIF_TYPE_CLASSIC = 0,
	IONIC_LIF_TYPE_MACVLAN = 1,
	IONIC_LIF_TYPE_NETQUEUE = 2,
};


struct ionic_lif_identify_cmd {
	u8 opcode;
	u8 type;
	u8 ver;
	u8 rsvd[61];
};


struct ionic_lif_identify_comp {
	u8 status;
	u8 ver;
	u8 rsvd2[14];
};


enum ionic_lif_capability {
	IONIC_LIF_CAP_ETH        = BIT(0),
	IONIC_LIF_CAP_RDMA       = BIT(1),
};


enum ionic_logical_qtype {
	IONIC_QTYPE_ADMINQ  = 0,
	IONIC_QTYPE_NOTIFYQ = 1,
	IONIC_QTYPE_RXQ     = 2,
	IONIC_QTYPE_TXQ     = 3,
	IONIC_QTYPE_EQ      = 4,
	IONIC_QTYPE_MAX     = 16,
};


enum ionic_q_feature {
	IONIC_QIDENT_F_CQ		= BIT_ULL(0),
	IONIC_QIDENT_F_SG		= BIT_ULL(1),
	IONIC_QIDENT_F_EQ		= BIT_ULL(2),
	IONIC_QIDENT_F_CMB		= BIT_ULL(3),
	IONIC_Q_F_2X_DESC		= BIT_ULL(4),
	IONIC_Q_F_2X_CQ_DESC		= BIT_ULL(5),
	IONIC_Q_F_2X_SG_DESC		= BIT_ULL(6),
	IONIC_Q_F_4X_DESC		= BIT_ULL(7),
	IONIC_Q_F_4X_CQ_DESC		= BIT_ULL(8),
	IONIC_Q_F_4X_SG_DESC		= BIT_ULL(9),
};


enum ionic_rxq_feature {
	IONIC_RXQ_F_HWSTAMP		= BIT_ULL(16),
};


enum ionic_txq_feature {
	IONIC_TXQ_F_HWSTAMP		= BIT(16),
};


enum ionic_hwstamp_bits {
	IONIC_HWSTAMP_INVALID	    = ~0ull,
	IONIC_HWSTAMP_CQ_NEGOFFSET  = 8,
};


struct ionic_lif_logical_qtype {
	u8     qtype;
	u8     rsvd[3];
	__le32 qid_count;
	__le32 qid_base;
};


enum ionic_lif_state {
	IONIC_LIF_QUIESCE	= 0,
	IONIC_LIF_ENABLE	= 1,
	IONIC_LIF_DISABLE	= 2,
};


union ionic_lif_config {
	struct {
		u8     state;
		u8     rsvd[3];
		char   name[IONIC_IFNAMSIZ];
		__le32 mtu;
		u8     mac[6];
		__le16 vlan;
		__le64 features;
		__le32 queue_count[IONIC_QTYPE_MAX];
	} __packed;
	__le32 words[64];
};


union ionic_lif_identity {
	struct {
		__le64 capabilities;

		struct {
			u8 version;
			u8 rsvd[3];
			__le32 max_ucast_filters;
			__le32 max_mcast_filters;
			__le16 rss_ind_tbl_sz;
			__le32 min_frame_size;
			__le32 max_frame_size;
			u8 rsvd2[2];
			__le64 hwstamp_tx_modes;
			__le64 hwstamp_rx_filters;
			u8 rsvd3[88];
			union ionic_lif_config config;
		} __packed eth;

		struct {
			u8 version;
			u8 qp_opcodes;
			u8 admin_opcodes;
			u8 rsvd;
			__le32 npts_per_lif;
			__le32 nmrs_per_lif;
			__le32 nahs_per_lif;
			u8 max_stride;
			u8 cl_stride;
			u8 pte_stride;
			u8 rrq_stride;
			u8 rsq_stride;
			u8 dcqcn_profiles;
			u8 rsvd_dimensions[10];
			struct ionic_lif_logical_qtype aq_qtype;
			struct ionic_lif_logical_qtype sq_qtype;
			struct ionic_lif_logical_qtype rq_qtype;
			struct ionic_lif_logical_qtype cq_qtype;
			struct ionic_lif_logical_qtype eq_qtype;
		} __packed rdma;
	} __packed;
	__le32 words[478];
};


struct ionic_lif_init_cmd {
	u8     opcode;
	u8     type;
	__le16 index;
	__le32 rsvd;
	__le64 info_pa;
	u8     rsvd2[48];
};


struct ionic_lif_init_comp {
	u8 status;
	u8 rsvd;
	__le16 hw_index;
	u8 rsvd2[12];
};


struct ionic_q_identify_cmd {
	u8     opcode;
	u8     rsvd;
	__le16 lif_type;
	u8     type;
	u8     ver;
	u8     rsvd2[58];
};


struct ionic_q_identify_comp {
	u8     status;
	u8     rsvd;
	__le16 comp_index;
	u8     ver;
	u8     rsvd2[11];
};


union ionic_q_identity {
	struct {
		u8      version;
		u8      supported;
		u8      rsvd[6];
		__le64  features;
		__le16  desc_sz;
		__le16  comp_sz;
		__le16  sg_desc_sz;
		__le16  max_sg_elems;
		__le16  sg_desc_stride;
	};
	__le32 words[478];
};


struct ionic_q_init_cmd {
	u8     opcode;
	u8     rsvd;
	__le16 lif_index;
	u8     type;
	u8     ver;
	u8     rsvd1[2];
	__le32 index;
	__le16 pid;
	__le16 intr_index;
	__le16 flags;
#define IONIC_QINIT_F_IRQ	0x01	
#define IONIC_QINIT_F_ENA	0x02	
#define IONIC_QINIT_F_SG	0x04	
#define IONIC_QINIT_F_EQ	0x08	
#define IONIC_QINIT_F_CMB	0x10	
#define IONIC_QINIT_F_DEBUG	0x80	
	u8     cos;
	u8     ring_size;
	__le64 ring_base;
	__le64 cq_ring_base;
	__le64 sg_ring_base;
	u8     rsvd2[12];
	__le64 features;
} __packed;


struct ionic_q_init_comp {
	u8     status;
	u8     rsvd;
	__le16 comp_index;
	__le32 hw_index;
	u8     hw_type;
	u8     rsvd2[6];
	u8     color;
};


#define IONIC_ADDR_LEN		52
#define IONIC_ADDR_MASK		(BIT_ULL(IONIC_ADDR_LEN) - 1)

enum ionic_txq_desc_opcode {
	IONIC_TXQ_DESC_OPCODE_CSUM_NONE = 0,
	IONIC_TXQ_DESC_OPCODE_CSUM_PARTIAL = 1,
	IONIC_TXQ_DESC_OPCODE_CSUM_HW = 2,
	IONIC_TXQ_DESC_OPCODE_TSO = 3,
};


struct ionic_txq_desc {
	__le64  cmd;
#define IONIC_TXQ_DESC_OPCODE_MASK		0xf
#define IONIC_TXQ_DESC_OPCODE_SHIFT		4
#define IONIC_TXQ_DESC_FLAGS_MASK		0xf
#define IONIC_TXQ_DESC_FLAGS_SHIFT		0
#define IONIC_TXQ_DESC_NSGE_MASK		0xf
#define IONIC_TXQ_DESC_NSGE_SHIFT		8
#define IONIC_TXQ_DESC_ADDR_MASK		(BIT_ULL(IONIC_ADDR_LEN) - 1)
#define IONIC_TXQ_DESC_ADDR_SHIFT		12


#define IONIC_TXQ_DESC_FLAG_VLAN		0x1
#define IONIC_TXQ_DESC_FLAG_ENCAP		0x2


#define IONIC_TXQ_DESC_FLAG_CSUM_L3		0x4
#define IONIC_TXQ_DESC_FLAG_CSUM_L4		0x8


#define IONIC_TXQ_DESC_FLAG_TSO_SOT		0x4
#define IONIC_TXQ_DESC_FLAG_TSO_EOT		0x8

	__le16  len;
	union {
		__le16  vlan_tci;
		__le16  hword0;
	};
	union {
		__le16  csum_start;
		__le16  hdr_len;
		__le16  hword1;
	};
	union {
		__le16  csum_offset;
		__le16  mss;
		__le16  hword2;
	};
};

static inline u64 encode_txq_desc_cmd(u8 opcode, u8 flags,
				      u8 nsge, u64 addr)
{
	u64 cmd;

	cmd = (opcode & IONIC_TXQ_DESC_OPCODE_MASK) << IONIC_TXQ_DESC_OPCODE_SHIFT;
	cmd |= (flags & IONIC_TXQ_DESC_FLAGS_MASK) << IONIC_TXQ_DESC_FLAGS_SHIFT;
	cmd |= (nsge & IONIC_TXQ_DESC_NSGE_MASK) << IONIC_TXQ_DESC_NSGE_SHIFT;
	cmd |= (addr & IONIC_TXQ_DESC_ADDR_MASK) << IONIC_TXQ_DESC_ADDR_SHIFT;

	return cmd;
};

static inline void decode_txq_desc_cmd(u64 cmd, u8 *opcode, u8 *flags,
				       u8 *nsge, u64 *addr)
{
	*opcode = (cmd >> IONIC_TXQ_DESC_OPCODE_SHIFT) & IONIC_TXQ_DESC_OPCODE_MASK;
	*flags = (cmd >> IONIC_TXQ_DESC_FLAGS_SHIFT) & IONIC_TXQ_DESC_FLAGS_MASK;
	*nsge = (cmd >> IONIC_TXQ_DESC_NSGE_SHIFT) & IONIC_TXQ_DESC_NSGE_MASK;
	*addr = (cmd >> IONIC_TXQ_DESC_ADDR_SHIFT) & IONIC_TXQ_DESC_ADDR_MASK;
};


struct ionic_txq_sg_elem {
	__le64 addr;
	__le16 len;
	__le16 rsvd[3];
};


struct ionic_txq_sg_desc {
#define IONIC_TX_MAX_SG_ELEMS		8
#define IONIC_TX_SG_DESC_STRIDE		8
	struct ionic_txq_sg_elem elems[IONIC_TX_MAX_SG_ELEMS];
};

struct ionic_txq_sg_desc_v1 {
#define IONIC_TX_MAX_SG_ELEMS_V1		15
#define IONIC_TX_SG_DESC_STRIDE_V1		16
	struct ionic_txq_sg_elem elems[IONIC_TX_SG_DESC_STRIDE_V1];
};


struct ionic_txq_comp {
	u8     status;
	u8     rsvd;
	__le16 comp_index;
	u8     rsvd2[11];
	u8     color;
};

enum ionic_rxq_desc_opcode {
	IONIC_RXQ_DESC_OPCODE_SIMPLE = 0,
	IONIC_RXQ_DESC_OPCODE_SG = 1,
};


struct ionic_rxq_desc {
	u8     opcode;
	u8     rsvd[5];
	__le16 len;
	__le64 addr;
};


struct ionic_rxq_sg_elem {
	__le64 addr;
	__le16 len;
	__le16 rsvd[3];
};


struct ionic_rxq_sg_desc {
#define IONIC_RX_MAX_SG_ELEMS		8
#define IONIC_RX_SG_DESC_STRIDE		8
	struct ionic_rxq_sg_elem elems[IONIC_RX_SG_DESC_STRIDE];
};


struct ionic_rxq_comp {
	u8     status;
	u8     num_sg_elems;
	__le16 comp_index;
	__le32 rss_hash;
	__le16 csum;
	__le16 vlan_tci;
	__le16 len;
	u8     csum_flags;
#define IONIC_RXQ_COMP_CSUM_F_TCP_OK	0x01
#define IONIC_RXQ_COMP_CSUM_F_TCP_BAD	0x02
#define IONIC_RXQ_COMP_CSUM_F_UDP_OK	0x04
#define IONIC_RXQ_COMP_CSUM_F_UDP_BAD	0x08
#define IONIC_RXQ_COMP_CSUM_F_IP_OK	0x10
#define IONIC_RXQ_COMP_CSUM_F_IP_BAD	0x20
#define IONIC_RXQ_COMP_CSUM_F_VLAN	0x40
#define IONIC_RXQ_COMP_CSUM_F_CALC	0x80
	u8     pkt_type_color;
#define IONIC_RXQ_COMP_PKT_TYPE_MASK	0x7f
};

enum ionic_pkt_type {
	IONIC_PKT_TYPE_NON_IP		= 0x00,
	IONIC_PKT_TYPE_IPV4		= 0x01,
	IONIC_PKT_TYPE_IPV4_TCP		= 0x03,
	IONIC_PKT_TYPE_IPV4_UDP		= 0x05,
	IONIC_PKT_TYPE_IPV6		= 0x08,
	IONIC_PKT_TYPE_IPV6_TCP		= 0x18,
	IONIC_PKT_TYPE_IPV6_UDP		= 0x28,
	
	IONIC_PKT_TYPE_ENCAP_NON_IP	= 0x40,
	IONIC_PKT_TYPE_ENCAP_IPV4	= 0x41,
	IONIC_PKT_TYPE_ENCAP_IPV4_TCP	= 0x43,
	IONIC_PKT_TYPE_ENCAP_IPV4_UDP	= 0x45,
	IONIC_PKT_TYPE_ENCAP_IPV6	= 0x48,
	IONIC_PKT_TYPE_ENCAP_IPV6_TCP	= 0x58,
	IONIC_PKT_TYPE_ENCAP_IPV6_UDP	= 0x68,
};

enum ionic_eth_hw_features {
	IONIC_ETH_HW_VLAN_TX_TAG	= BIT(0),
	IONIC_ETH_HW_VLAN_RX_STRIP	= BIT(1),
	IONIC_ETH_HW_VLAN_RX_FILTER	= BIT(2),
	IONIC_ETH_HW_RX_HASH		= BIT(3),
	IONIC_ETH_HW_RX_CSUM		= BIT(4),
	IONIC_ETH_HW_TX_SG		= BIT(5),
	IONIC_ETH_HW_RX_SG		= BIT(6),
	IONIC_ETH_HW_TX_CSUM		= BIT(7),
	IONIC_ETH_HW_TSO		= BIT(8),
	IONIC_ETH_HW_TSO_IPV6		= BIT(9),
	IONIC_ETH_HW_TSO_ECN		= BIT(10),
	IONIC_ETH_HW_TSO_GRE		= BIT(11),
	IONIC_ETH_HW_TSO_GRE_CSUM	= BIT(12),
	IONIC_ETH_HW_TSO_IPXIP4		= BIT(13),
	IONIC_ETH_HW_TSO_IPXIP6		= BIT(14),
	IONIC_ETH_HW_TSO_UDP		= BIT(15),
	IONIC_ETH_HW_TSO_UDP_CSUM	= BIT(16),
	IONIC_ETH_HW_RX_CSUM_GENEVE	= BIT(17),
	IONIC_ETH_HW_TX_CSUM_GENEVE	= BIT(18),
	IONIC_ETH_HW_TSO_GENEVE		= BIT(19),
	IONIC_ETH_HW_TIMESTAMP		= BIT(20),
};


enum ionic_pkt_class {
	IONIC_PKT_CLS_NTP_ALL		= BIT(0),

	IONIC_PKT_CLS_PTP1_SYNC		= BIT(1),
	IONIC_PKT_CLS_PTP1_DREQ		= BIT(2),
	IONIC_PKT_CLS_PTP1_ALL		= BIT(3) |
		IONIC_PKT_CLS_PTP1_SYNC | IONIC_PKT_CLS_PTP1_DREQ,

	IONIC_PKT_CLS_PTP2_L4_SYNC	= BIT(4),
	IONIC_PKT_CLS_PTP2_L4_DREQ	= BIT(5),
	IONIC_PKT_CLS_PTP2_L4_ALL	= BIT(6) |
		IONIC_PKT_CLS_PTP2_L4_SYNC | IONIC_PKT_CLS_PTP2_L4_DREQ,

	IONIC_PKT_CLS_PTP2_L2_SYNC	= BIT(7),
	IONIC_PKT_CLS_PTP2_L2_DREQ	= BIT(8),
	IONIC_PKT_CLS_PTP2_L2_ALL	= BIT(9) |
		IONIC_PKT_CLS_PTP2_L2_SYNC | IONIC_PKT_CLS_PTP2_L2_DREQ,

	IONIC_PKT_CLS_PTP2_SYNC		=
		IONIC_PKT_CLS_PTP2_L4_SYNC | IONIC_PKT_CLS_PTP2_L2_SYNC,
	IONIC_PKT_CLS_PTP2_DREQ		=
		IONIC_PKT_CLS_PTP2_L4_DREQ | IONIC_PKT_CLS_PTP2_L2_DREQ,
	IONIC_PKT_CLS_PTP2_ALL		=
		IONIC_PKT_CLS_PTP2_L4_ALL | IONIC_PKT_CLS_PTP2_L2_ALL,

	IONIC_PKT_CLS_PTP_SYNC		=
		IONIC_PKT_CLS_PTP1_SYNC | IONIC_PKT_CLS_PTP2_SYNC,
	IONIC_PKT_CLS_PTP_DREQ		=
		IONIC_PKT_CLS_PTP1_DREQ | IONIC_PKT_CLS_PTP2_DREQ,
	IONIC_PKT_CLS_PTP_ALL		=
		IONIC_PKT_CLS_PTP1_ALL | IONIC_PKT_CLS_PTP2_ALL,
};


struct ionic_q_control_cmd {
	u8     opcode;
	u8     type;
	__le16 lif_index;
	__le32 index;
	u8     oper;
	u8     rsvd[55];
};

typedef struct ionic_admin_comp ionic_q_control_comp;

enum q_control_oper {
	IONIC_Q_DISABLE		= 0,
	IONIC_Q_ENABLE		= 1,
	IONIC_Q_HANG_RESET	= 2,
};


enum ionic_phy_type {
	IONIC_PHY_TYPE_NONE	= 0,
	IONIC_PHY_TYPE_COPPER	= 1,
	IONIC_PHY_TYPE_FIBER	= 2,
};


enum ionic_xcvr_state {
	IONIC_XCVR_STATE_REMOVED	 = 0,
	IONIC_XCVR_STATE_INSERTED	 = 1,
	IONIC_XCVR_STATE_PENDING	 = 2,
	IONIC_XCVR_STATE_SPROM_READ	 = 3,
	IONIC_XCVR_STATE_SPROM_READ_ERR	 = 4,
};


enum ionic_xcvr_pid {
	IONIC_XCVR_PID_UNKNOWN           = 0,

	
	IONIC_XCVR_PID_QSFP_100G_CR4     = 1,
	IONIC_XCVR_PID_QSFP_40GBASE_CR4  = 2,
	IONIC_XCVR_PID_SFP_25GBASE_CR_S  = 3,
	IONIC_XCVR_PID_SFP_25GBASE_CR_L  = 4,
	IONIC_XCVR_PID_SFP_25GBASE_CR_N  = 5,

	
	IONIC_XCVR_PID_QSFP_100G_AOC    = 50,
	IONIC_XCVR_PID_QSFP_100G_ACC    = 51,
	IONIC_XCVR_PID_QSFP_100G_SR4    = 52,
	IONIC_XCVR_PID_QSFP_100G_LR4    = 53,
	IONIC_XCVR_PID_QSFP_100G_ER4    = 54,
	IONIC_XCVR_PID_QSFP_40GBASE_ER4 = 55,
	IONIC_XCVR_PID_QSFP_40GBASE_SR4 = 56,
	IONIC_XCVR_PID_QSFP_40GBASE_LR4 = 57,
	IONIC_XCVR_PID_QSFP_40GBASE_AOC = 58,
	IONIC_XCVR_PID_SFP_25GBASE_SR   = 59,
	IONIC_XCVR_PID_SFP_25GBASE_LR   = 60,
	IONIC_XCVR_PID_SFP_25GBASE_ER   = 61,
	IONIC_XCVR_PID_SFP_25GBASE_AOC  = 62,
	IONIC_XCVR_PID_SFP_10GBASE_SR   = 63,
	IONIC_XCVR_PID_SFP_10GBASE_LR   = 64,
	IONIC_XCVR_PID_SFP_10GBASE_LRM  = 65,
	IONIC_XCVR_PID_SFP_10GBASE_ER   = 66,
	IONIC_XCVR_PID_SFP_10GBASE_AOC  = 67,
	IONIC_XCVR_PID_SFP_10GBASE_CU   = 68,
	IONIC_XCVR_PID_QSFP_100G_CWDM4  = 69,
	IONIC_XCVR_PID_QSFP_100G_PSM4   = 70,
	IONIC_XCVR_PID_SFP_25GBASE_ACC  = 71,
	IONIC_XCVR_PID_SFP_10GBASE_T    = 72,
	IONIC_XCVR_PID_SFP_1000BASE_T   = 73,
};


enum ionic_port_type {
	IONIC_PORT_TYPE_NONE = 0,
	IONIC_PORT_TYPE_ETH  = 1,
	IONIC_PORT_TYPE_MGMT = 2,
};


enum ionic_port_admin_state {
	IONIC_PORT_ADMIN_STATE_NONE = 0,
	IONIC_PORT_ADMIN_STATE_DOWN = 1,
	IONIC_PORT_ADMIN_STATE_UP   = 2,
};


enum ionic_port_oper_status {
	IONIC_PORT_OPER_STATUS_NONE  = 0,
	IONIC_PORT_OPER_STATUS_UP    = 1,
	IONIC_PORT_OPER_STATUS_DOWN  = 2,
};


enum ionic_port_fec_type {
	IONIC_PORT_FEC_TYPE_NONE = 0,
	IONIC_PORT_FEC_TYPE_FC   = 1,
	IONIC_PORT_FEC_TYPE_RS   = 2,
};


enum ionic_port_pause_type {
	IONIC_PORT_PAUSE_TYPE_NONE = 0,
	IONIC_PORT_PAUSE_TYPE_LINK = 1,
	IONIC_PORT_PAUSE_TYPE_PFC  = 2,
};


enum ionic_port_loopback_mode {
	IONIC_PORT_LOOPBACK_MODE_NONE = 0,
	IONIC_PORT_LOOPBACK_MODE_MAC  = 1,
	IONIC_PORT_LOOPBACK_MODE_PHY  = 2,
};


struct ionic_xcvr_status {
	u8     state;
	u8     phy;
	__le16 pid;
	u8     sprom[256];
};


union ionic_port_config {
	struct {
#define IONIC_SPEED_100G	100000	
#define IONIC_SPEED_50G		50000	
#define IONIC_SPEED_40G		40000	
#define IONIC_SPEED_25G		25000	
#define IONIC_SPEED_10G		10000	
#define IONIC_SPEED_1G		1000	
		__le32 speed;
		__le32 mtu;
		u8     state;
		u8     an_enable;
		u8     fec_type;
#define IONIC_PAUSE_TYPE_MASK		0x0f
#define IONIC_PAUSE_FLAGS_MASK		0xf0
#define IONIC_PAUSE_F_TX		0x10
#define IONIC_PAUSE_F_RX		0x20
		u8     pause_type;
		u8     loopback_mode;
	};
	__le32 words[64];
};


struct ionic_port_status {
	__le32 id;
	__le32 speed;
	u8     status;
	__le16 link_down_count;
	u8     fec_type;
	u8     rsvd[48];
	struct ionic_xcvr_status  xcvr;
} __packed;


struct ionic_port_identify_cmd {
	u8 opcode;
	u8 index;
	u8 ver;
	u8 rsvd[61];
};


struct ionic_port_identify_comp {
	u8 status;
	u8 ver;
	u8 rsvd[14];
};


struct ionic_port_init_cmd {
	u8     opcode;
	u8     index;
	u8     rsvd[6];
	__le64 info_pa;
	u8     rsvd2[48];
};


struct ionic_port_init_comp {
	u8 status;
	u8 rsvd[15];
};


struct ionic_port_reset_cmd {
	u8 opcode;
	u8 index;
	u8 rsvd[62];
};


struct ionic_port_reset_comp {
	u8 status;
	u8 rsvd[15];
};


enum ionic_stats_ctl_cmd {
	IONIC_STATS_CTL_RESET		= 0,
};


enum ionic_txstamp_mode {
	IONIC_TXSTAMP_OFF		= 0,
	IONIC_TXSTAMP_ON		= 1,
	IONIC_TXSTAMP_ONESTEP_SYNC	= 2,
	IONIC_TXSTAMP_ONESTEP_P2P	= 3,
};


enum ionic_port_attr {
	IONIC_PORT_ATTR_STATE		= 0,
	IONIC_PORT_ATTR_SPEED		= 1,
	IONIC_PORT_ATTR_MTU		= 2,
	IONIC_PORT_ATTR_AUTONEG		= 3,
	IONIC_PORT_ATTR_FEC		= 4,
	IONIC_PORT_ATTR_PAUSE		= 5,
	IONIC_PORT_ATTR_LOOPBACK	= 6,
	IONIC_PORT_ATTR_STATS_CTRL	= 7,
};


struct ionic_port_setattr_cmd {
	u8     opcode;
	u8     index;
	u8     attr;
	u8     rsvd;
	union {
		u8      state;
		__le32  speed;
		__le32  mtu;
		u8      an_enable;
		u8      fec_type;
		u8      pause_type;
		u8      loopback_mode;
		u8      stats_ctl;
		u8      rsvd2[60];
	};
};


struct ionic_port_setattr_comp {
	u8     status;
	u8     rsvd[14];
	u8     color;
};


struct ionic_port_getattr_cmd {
	u8     opcode;
	u8     index;
	u8     attr;
	u8     rsvd[61];
};


struct ionic_port_getattr_comp {
	u8     status;
	u8     rsvd[3];
	union {
		u8      state;
		__le32  speed;
		__le32  mtu;
		u8      an_enable;
		u8      fec_type;
		u8      pause_type;
		u8      loopback_mode;
		u8      rsvd2[11];
	} __packed;
	u8     color;
};


struct ionic_lif_status {
	__le64 eid;
	u8     port_num;
	u8     rsvd;
	__le16 link_status;
	__le32 link_speed;		
	__le16 link_down_count;
	u8      rsvd2[46];
};


struct ionic_lif_reset_cmd {
	u8     opcode;
	u8     rsvd;
	__le16 index;
	__le32 rsvd2[15];
};

typedef struct ionic_admin_comp ionic_lif_reset_comp;

enum ionic_dev_state {
	IONIC_DEV_DISABLE	= 0,
	IONIC_DEV_ENABLE	= 1,
	IONIC_DEV_HANG_RESET	= 2,
};


enum ionic_dev_attr {
	IONIC_DEV_ATTR_STATE    = 0,
	IONIC_DEV_ATTR_NAME     = 1,
	IONIC_DEV_ATTR_FEATURES = 2,
};


struct ionic_dev_setattr_cmd {
	u8     opcode;
	u8     attr;
	__le16 rsvd;
	union {
		u8      state;
		char    name[IONIC_IFNAMSIZ];
		__le64  features;
		u8      rsvd2[60];
	} __packed;
};


struct ionic_dev_setattr_comp {
	u8     status;
	u8     rsvd[3];
	union {
		__le64  features;
		u8      rsvd2[11];
	} __packed;
	u8     color;
};


struct ionic_dev_getattr_cmd {
	u8     opcode;
	u8     attr;
	u8     rsvd[62];
};


struct ionic_dev_getattr_comp {
	u8     status;
	u8     rsvd[3];
	union {
		__le64  features;
		u8      rsvd2[11];
	} __packed;
	u8     color;
};


#define IONIC_RSS_HASH_KEY_SIZE		40

enum ionic_rss_hash_types {
	IONIC_RSS_TYPE_IPV4	= BIT(0),
	IONIC_RSS_TYPE_IPV4_TCP	= BIT(1),
	IONIC_RSS_TYPE_IPV4_UDP	= BIT(2),
	IONIC_RSS_TYPE_IPV6	= BIT(3),
	IONIC_RSS_TYPE_IPV6_TCP	= BIT(4),
	IONIC_RSS_TYPE_IPV6_UDP	= BIT(5),
};


enum ionic_lif_attr {
	IONIC_LIF_ATTR_STATE        = 0,
	IONIC_LIF_ATTR_NAME         = 1,
	IONIC_LIF_ATTR_MTU          = 2,
	IONIC_LIF_ATTR_MAC          = 3,
	IONIC_LIF_ATTR_FEATURES     = 4,
	IONIC_LIF_ATTR_RSS          = 5,
	IONIC_LIF_ATTR_STATS_CTRL   = 6,
	IONIC_LIF_ATTR_TXSTAMP      = 7,
};


struct ionic_lif_setattr_cmd {
	u8     opcode;
	u8     attr;
	__le16 index;
	union {
		u8      state;
		char    name[IONIC_IFNAMSIZ];
		__le32  mtu;
		u8      mac[6];
		__le64  features;
		struct {
			__le16 types;
			u8     key[IONIC_RSS_HASH_KEY_SIZE];
			u8     rsvd[6];
			__le64 addr;
		} rss;
		u8      stats_ctl;
		__le16 txstamp_mode;
		u8      rsvd[60];
	} __packed;
};


struct ionic_lif_setattr_comp {
	u8     status;
	u8     rsvd;
	__le16 comp_index;
	union {
		__le64  features;
		u8      rsvd2[11];
	} __packed;
	u8     color;
};


struct ionic_lif_getattr_cmd {
	u8     opcode;
	u8     attr;
	__le16 index;
	u8     rsvd[60];
};


struct ionic_lif_getattr_comp {
	u8     status;
	u8     rsvd;
	__le16 comp_index;
	union {
		u8      state;
		__le32  mtu;
		u8      mac[6];
		__le64  features;
		__le16  txstamp_mode;
		u8      rsvd2[11];
	} __packed;
	u8     color;
};


struct ionic_lif_setphc_cmd {
	u8	opcode;
	u8	rsvd1;
	__le16  lif_index;
	u8      rsvd2[4];
	__le64	tick;
	__le64	nsec;
	__le64	frac;
	__le32	mult;
	__le32	shift;
	u8     rsvd3[24];
};

enum ionic_rx_mode {
	IONIC_RX_MODE_F_UNICAST		= BIT(0),
	IONIC_RX_MODE_F_MULTICAST	= BIT(1),
	IONIC_RX_MODE_F_BROADCAST	= BIT(2),
	IONIC_RX_MODE_F_PROMISC		= BIT(3),
	IONIC_RX_MODE_F_ALLMULTI	= BIT(4),
	IONIC_RX_MODE_F_RDMA_SNIFFER	= BIT(5),
};


struct ionic_rx_mode_set_cmd {
	u8     opcode;
	u8     rsvd;
	__le16 lif_index;
	__le16 rx_mode;
	__le16 rsvd2[29];
};

typedef struct ionic_admin_comp ionic_rx_mode_set_comp;

enum ionic_rx_filter_match_type {
	IONIC_RX_FILTER_MATCH_VLAN	= 0x0,
	IONIC_RX_FILTER_MATCH_MAC	= 0x1,
	IONIC_RX_FILTER_MATCH_MAC_VLAN	= 0x2,
	IONIC_RX_FILTER_STEER_PKTCLASS	= 0x10,
};


struct ionic_rx_filter_add_cmd {
	u8     opcode;
	u8     qtype;
	__le16 lif_index;
	__le32 qid;
	__le16 match;
	union {
		struct {
			__le16 vlan;
		} vlan;
		struct {
			u8     addr[6];
		} mac;
		struct {
			__le16 vlan;
			u8     addr[6];
		} mac_vlan;
		__le64 pkt_class;
		u8 rsvd[54];
	} __packed;
};


struct ionic_rx_filter_add_comp {
	u8     status;
	u8     rsvd;
	__le16 comp_index;
	__le32 filter_id;
	u8     rsvd2[7];
	u8     color;
};


struct ionic_rx_filter_del_cmd {
	u8     opcode;
	u8     rsvd;
	__le16 lif_index;
	__le32 filter_id;
	u8     rsvd2[56];
};

typedef struct ionic_admin_comp ionic_rx_filter_del_comp;

enum ionic_vf_attr {
	IONIC_VF_ATTR_SPOOFCHK	= 1,
	IONIC_VF_ATTR_TRUST	= 2,
	IONIC_VF_ATTR_MAC	= 3,
	IONIC_VF_ATTR_LINKSTATE	= 4,
	IONIC_VF_ATTR_VLAN	= 5,
	IONIC_VF_ATTR_RATE	= 6,
	IONIC_VF_ATTR_STATSADDR	= 7,
};


enum ionic_vf_link_status {
	IONIC_VF_LINK_STATUS_AUTO = 0,
	IONIC_VF_LINK_STATUS_UP   = 1,
	IONIC_VF_LINK_STATUS_DOWN = 2,
};


struct ionic_vf_setattr_cmd {
	u8     opcode;
	u8     attr;
	__le16 vf_index;
	union {
		u8     macaddr[6];
		__le16 vlanid;
		__le32 maxrate;
		u8     spoofchk;
		u8     trust;
		u8     linkstate;
		__le64 stats_pa;
		u8     pad[60];
	} __packed;
};

struct ionic_vf_setattr_comp {
	u8     status;
	u8     attr;
	__le16 vf_index;
	__le16 comp_index;
	u8     rsvd[9];
	u8     color;
};


struct ionic_vf_getattr_cmd {
	u8     opcode;
	u8     attr;
	__le16 vf_index;
	u8     rsvd[60];
};

struct ionic_vf_getattr_comp {
	u8     status;
	u8     attr;
	__le16 vf_index;
	union {
		u8     macaddr[6];
		__le16 vlanid;
		__le32 maxrate;
		u8     spoofchk;
		u8     trust;
		u8     linkstate;
		__le64 stats_pa;
		u8     pad[11];
	} __packed;
	u8     color;
};

enum ionic_vf_ctrl_opcode {
	IONIC_VF_CTRL_START_ALL	= 0,
	IONIC_VF_CTRL_START	= 1,
};


struct ionic_vf_ctrl_cmd {
	u8	opcode;
	u8	ctrl_opcode;
	__le16	vf_index;
	
	u8	rsvd1[60];
};


struct ionic_vf_ctrl_comp {
	u8	status;
	
	u8      rsvd[15];
};


struct ionic_qos_identify_cmd {
	u8 opcode;
	u8 ver;
	u8 rsvd[62];
};


struct ionic_qos_identify_comp {
	u8 status;
	u8 ver;
	u8 rsvd[14];
};

#define IONIC_QOS_TC_MAX		8
#define IONIC_QOS_ALL_TC		0xFF

#define IONIC_QOS_CLASS_MAX		7
#define IONIC_QOS_PCP_MAX		8
#define IONIC_QOS_CLASS_NAME_SZ	32
#define IONIC_QOS_DSCP_MAX		64
#define IONIC_QOS_ALL_PCP		0xFF
#define IONIC_DSCP_BLOCK_SIZE		8


enum ionic_qos_class {
	IONIC_QOS_CLASS_DEFAULT		= 0,
	IONIC_QOS_CLASS_USER_DEFINED_1	= 1,
	IONIC_QOS_CLASS_USER_DEFINED_2	= 2,
	IONIC_QOS_CLASS_USER_DEFINED_3	= 3,
	IONIC_QOS_CLASS_USER_DEFINED_4	= 4,
	IONIC_QOS_CLASS_USER_DEFINED_5	= 5,
	IONIC_QOS_CLASS_USER_DEFINED_6	= 6,
};


enum ionic_qos_class_type {
	IONIC_QOS_CLASS_TYPE_NONE	= 0,
	IONIC_QOS_CLASS_TYPE_PCP	= 1,
	IONIC_QOS_CLASS_TYPE_DSCP	= 2,
};


enum ionic_qos_sched_type {
	IONIC_QOS_SCHED_TYPE_STRICT	= 0,
	IONIC_QOS_SCHED_TYPE_DWRR	= 1,
};


union ionic_qos_config {
	struct {
#define IONIC_QOS_CONFIG_F_ENABLE		BIT(0)
#define IONIC_QOS_CONFIG_F_NO_DROP		BIT(1)

#define IONIC_QOS_CONFIG_F_RW_DOT1Q_PCP		BIT(2)
#define IONIC_QOS_CONFIG_F_RW_IP_DSCP		BIT(3)

#define IONIC_QOS_CONFIG_F_NON_DISRUPTIVE	BIT(4)
		u8      flags;
		u8      sched_type;
		u8      class_type;
		u8      pause_type;
		char    name[IONIC_QOS_CLASS_NAME_SZ];
		__le32  mtu;
		
		u8      pfc_cos;
		
		union {
			u8      dwrr_weight;
			__le64  strict_rlmt;
		};
		
		
		union {
			u8      rw_dot1q_pcp;
			u8      rw_ip_dscp;
		};
		
		union {
			u8      dot1q_pcp;
			struct {
				u8      ndscp;
				u8      ip_dscp[IONIC_QOS_DSCP_MAX];
			};
		};
	};
	__le32  words[64];
};


union ionic_qos_identity {
	struct {
		u8     version;
		u8     type;
		u8     rsvd[62];
		union ionic_qos_config config[IONIC_QOS_CLASS_MAX];
	};
	__le32 words[478];
};


struct ionic_qos_init_cmd {
	u8     opcode;
	u8     group;
	u8     rsvd[6];
	__le64 info_pa;
	u8     rsvd1[48];
};

typedef struct ionic_admin_comp ionic_qos_init_comp;


struct ionic_qos_reset_cmd {
	u8    opcode;
	u8    group;
	u8    rsvd[62];
};


struct ionic_qos_clear_stats_cmd {
	u8    opcode;
	u8    group_bitmap;
	u8    rsvd[62];
};

typedef struct ionic_admin_comp ionic_qos_reset_comp;


struct ionic_fw_download_cmd {
	u8     opcode;
	u8     rsvd[3];
	__le32 offset;
	__le64 addr;
	__le32 length;
};

typedef struct ionic_admin_comp ionic_fw_download_comp;


enum ionic_fw_control_oper {
	IONIC_FW_RESET			= 0,
	IONIC_FW_INSTALL		= 1,
	IONIC_FW_ACTIVATE		= 2,
	IONIC_FW_INSTALL_ASYNC		= 3,
	IONIC_FW_INSTALL_STATUS		= 4,
	IONIC_FW_ACTIVATE_ASYNC		= 5,
	IONIC_FW_ACTIVATE_STATUS	= 6,
	IONIC_FW_UPDATE_CLEANUP		= 7,
};


struct ionic_fw_control_cmd {
	u8  opcode;
	u8  rsvd[3];
	u8  oper;
	u8  slot;
	u8  rsvd1[58];
};


struct ionic_fw_control_comp {
	u8     status;
	u8     rsvd;
	__le16 comp_index;
	u8     slot;
	u8     rsvd1[10];
	u8     color;
};




struct ionic_rdma_reset_cmd {
	u8     opcode;
	u8     rsvd;
	__le16 lif_index;
	u8     rsvd2[60];
};


struct ionic_rdma_queue_cmd {
	u8     opcode;
	u8     rsvd;
	__le16 lif_index;
	__le32 qid_ver;
	__le32 cid;
	__le16 dbid;
	u8     depth_log2;
	u8     stride_log2;
	__le64 dma_addr;
	u8     rsvd2[40];
};




struct ionic_notifyq_event {
	__le64 eid;
	__le16 ecode;
	u8     data[54];
};


struct ionic_link_change_event {
	__le64 eid;
	__le16 ecode;
	__le16 link_status;
	__le32 link_speed;	
	u8     rsvd[48];
};


struct ionic_reset_event {
	__le64 eid;
	__le16 ecode;
	u8     reset_code;
	u8     state;
	u8     rsvd[52];
};


struct ionic_heartbeat_event {
	__le64 eid;
	__le16 ecode;
	u8     rsvd[54];
};


struct ionic_log_event {
	__le64 eid;
	__le16 ecode;
	u8     data[54];
};


struct ionic_xcvr_event {
	__le64 eid;
	__le16 ecode;
	u8     rsvd[54];
};


struct ionic_port_stats {
	__le64 frames_rx_ok;
	__le64 frames_rx_all;
	__le64 frames_rx_bad_fcs;
	__le64 frames_rx_bad_all;
	__le64 octets_rx_ok;
	__le64 octets_rx_all;
	__le64 frames_rx_unicast;
	__le64 frames_rx_multicast;
	__le64 frames_rx_broadcast;
	__le64 frames_rx_pause;
	__le64 frames_rx_bad_length;
	__le64 frames_rx_undersized;
	__le64 frames_rx_oversized;
	__le64 frames_rx_fragments;
	__le64 frames_rx_jabber;
	__le64 frames_rx_pripause;
	__le64 frames_rx_stomped_crc;
	__le64 frames_rx_too_long;
	__le64 frames_rx_vlan_good;
	__le64 frames_rx_dropped;
	__le64 frames_rx_less_than_64b;
	__le64 frames_rx_64b;
	__le64 frames_rx_65b_127b;
	__le64 frames_rx_128b_255b;
	__le64 frames_rx_256b_511b;
	__le64 frames_rx_512b_1023b;
	__le64 frames_rx_1024b_1518b;
	__le64 frames_rx_1519b_2047b;
	__le64 frames_rx_2048b_4095b;
	__le64 frames_rx_4096b_8191b;
	__le64 frames_rx_8192b_9215b;
	__le64 frames_rx_other;
	__le64 frames_tx_ok;
	__le64 frames_tx_all;
	__le64 frames_tx_bad;
	__le64 octets_tx_ok;
	__le64 octets_tx_total;
	__le64 frames_tx_unicast;
	__le64 frames_tx_multicast;
	__le64 frames_tx_broadcast;
	__le64 frames_tx_pause;
	__le64 frames_tx_pripause;
	__le64 frames_tx_vlan;
	__le64 frames_tx_less_than_64b;
	__le64 frames_tx_64b;
	__le64 frames_tx_65b_127b;
	__le64 frames_tx_128b_255b;
	__le64 frames_tx_256b_511b;
	__le64 frames_tx_512b_1023b;
	__le64 frames_tx_1024b_1518b;
	__le64 frames_tx_1519b_2047b;
	__le64 frames_tx_2048b_4095b;
	__le64 frames_tx_4096b_8191b;
	__le64 frames_tx_8192b_9215b;
	__le64 frames_tx_other;
	__le64 frames_tx_pri_0;
	__le64 frames_tx_pri_1;
	__le64 frames_tx_pri_2;
	__le64 frames_tx_pri_3;
	__le64 frames_tx_pri_4;
	__le64 frames_tx_pri_5;
	__le64 frames_tx_pri_6;
	__le64 frames_tx_pri_7;
	__le64 frames_rx_pri_0;
	__le64 frames_rx_pri_1;
	__le64 frames_rx_pri_2;
	__le64 frames_rx_pri_3;
	__le64 frames_rx_pri_4;
	__le64 frames_rx_pri_5;
	__le64 frames_rx_pri_6;
	__le64 frames_rx_pri_7;
	__le64 tx_pripause_0_1us_count;
	__le64 tx_pripause_1_1us_count;
	__le64 tx_pripause_2_1us_count;
	__le64 tx_pripause_3_1us_count;
	__le64 tx_pripause_4_1us_count;
	__le64 tx_pripause_5_1us_count;
	__le64 tx_pripause_6_1us_count;
	__le64 tx_pripause_7_1us_count;
	__le64 rx_pripause_0_1us_count;
	__le64 rx_pripause_1_1us_count;
	__le64 rx_pripause_2_1us_count;
	__le64 rx_pripause_3_1us_count;
	__le64 rx_pripause_4_1us_count;
	__le64 rx_pripause_5_1us_count;
	__le64 rx_pripause_6_1us_count;
	__le64 rx_pripause_7_1us_count;
	__le64 rx_pause_1us_count;
	__le64 frames_tx_truncated;
};

struct ionic_mgmt_port_stats {
	__le64 frames_rx_ok;
	__le64 frames_rx_all;
	__le64 frames_rx_bad_fcs;
	__le64 frames_rx_bad_all;
	__le64 octets_rx_ok;
	__le64 octets_rx_all;
	__le64 frames_rx_unicast;
	__le64 frames_rx_multicast;
	__le64 frames_rx_broadcast;
	__le64 frames_rx_pause;
	__le64 frames_rx_bad_length;
	__le64 frames_rx_undersized;
	__le64 frames_rx_oversized;
	__le64 frames_rx_fragments;
	__le64 frames_rx_jabber;
	__le64 frames_rx_64b;
	__le64 frames_rx_65b_127b;
	__le64 frames_rx_128b_255b;
	__le64 frames_rx_256b_511b;
	__le64 frames_rx_512b_1023b;
	__le64 frames_rx_1024b_1518b;
	__le64 frames_rx_gt_1518b;
	__le64 frames_rx_fifo_full;
	__le64 frames_tx_ok;
	__le64 frames_tx_all;
	__le64 frames_tx_bad;
	__le64 octets_tx_ok;
	__le64 octets_tx_total;
	__le64 frames_tx_unicast;
	__le64 frames_tx_multicast;
	__le64 frames_tx_broadcast;
	__le64 frames_tx_pause;
};

enum ionic_pb_buffer_drop_stats {
	IONIC_BUFFER_INTRINSIC_DROP = 0,
	IONIC_BUFFER_DISCARDED,
	IONIC_BUFFER_ADMITTED,
	IONIC_BUFFER_OUT_OF_CELLS_DROP,
	IONIC_BUFFER_OUT_OF_CELLS_DROP_2,
	IONIC_BUFFER_OUT_OF_CREDIT_DROP,
	IONIC_BUFFER_TRUNCATION_DROP,
	IONIC_BUFFER_PORT_DISABLED_DROP,
	IONIC_BUFFER_COPY_TO_CPU_TAIL_DROP,
	IONIC_BUFFER_SPAN_TAIL_DROP,
	IONIC_BUFFER_MIN_SIZE_VIOLATION_DROP,
	IONIC_BUFFER_ENQUEUE_ERROR_DROP,
	IONIC_BUFFER_INVALID_PORT_DROP,
	IONIC_BUFFER_INVALID_OUTPUT_QUEUE_DROP,
	IONIC_BUFFER_DROP_MAX,
};

enum ionic_oflow_drop_stats {
	IONIC_OFLOW_OCCUPANCY_DROP,
	IONIC_OFLOW_EMERGENCY_STOP_DROP,
	IONIC_OFLOW_WRITE_BUFFER_ACK_FILL_UP_DROP,
	IONIC_OFLOW_WRITE_BUFFER_ACK_FULL_DROP,
	IONIC_OFLOW_WRITE_BUFFER_FULL_DROP,
	IONIC_OFLOW_CONTROL_FIFO_FULL_DROP,
	IONIC_OFLOW_DROP_MAX,
};


struct ionic_port_pb_stats {
	__le64 sop_count_in;
	__le64 eop_count_in;
	__le64 sop_count_out;
	__le64 eop_count_out;
	__le64 drop_counts[IONIC_BUFFER_DROP_MAX];
	__le64 input_queue_buffer_occupancy[IONIC_QOS_TC_MAX];
	__le64 input_queue_port_monitor[IONIC_QOS_TC_MAX];
	__le64 output_queue_port_monitor[IONIC_QOS_TC_MAX];
	__le64 oflow_drop_counts[IONIC_OFLOW_DROP_MAX];
	__le64 input_queue_good_pkts_in[IONIC_QOS_TC_MAX];
	__le64 input_queue_good_pkts_out[IONIC_QOS_TC_MAX];
	__le64 input_queue_err_pkts_in[IONIC_QOS_TC_MAX];
	__le64 input_queue_fifo_depth[IONIC_QOS_TC_MAX];
	__le64 input_queue_max_fifo_depth[IONIC_QOS_TC_MAX];
	__le64 input_queue_peak_occupancy[IONIC_QOS_TC_MAX];
	__le64 output_queue_buffer_occupancy[IONIC_QOS_TC_MAX];
};


union ionic_port_identity {
	struct {
		u8     version;
		u8     type;
		u8     num_lanes;
		u8     autoneg;
		__le32 min_frame_size;
		__le32 max_frame_size;
		u8     fec_type[4];
		u8     pause_type[2];
		u8     loopback_mode[2];
		__le32 speeds[16];
		u8     rsvd2[44];
		union ionic_port_config config;
	};
	__le32 words[478];
};


struct ionic_port_info {
	union ionic_port_config config;
	struct ionic_port_status status;
	union {
		struct ionic_port_stats      stats;
		struct ionic_mgmt_port_stats mgmt_stats;
	};
	
	u8                          rsvd[760];
	struct ionic_port_pb_stats  pb_stats;
};


struct ionic_lif_stats {
	
	__le64 rx_ucast_bytes;
	__le64 rx_ucast_packets;
	__le64 rx_mcast_bytes;
	__le64 rx_mcast_packets;
	__le64 rx_bcast_bytes;
	__le64 rx_bcast_packets;
	__le64 rsvd0;
	__le64 rsvd1;
	
	__le64 rx_ucast_drop_bytes;
	__le64 rx_ucast_drop_packets;
	__le64 rx_mcast_drop_bytes;
	__le64 rx_mcast_drop_packets;
	__le64 rx_bcast_drop_bytes;
	__le64 rx_bcast_drop_packets;
	__le64 rx_dma_error;
	__le64 rsvd2;
	
	__le64 tx_ucast_bytes;
	__le64 tx_ucast_packets;
	__le64 tx_mcast_bytes;
	__le64 tx_mcast_packets;
	__le64 tx_bcast_bytes;
	__le64 tx_bcast_packets;
	__le64 rsvd3;
	__le64 rsvd4;
	
	__le64 tx_ucast_drop_bytes;
	__le64 tx_ucast_drop_packets;
	__le64 tx_mcast_drop_bytes;
	__le64 tx_mcast_drop_packets;
	__le64 tx_bcast_drop_bytes;
	__le64 tx_bcast_drop_packets;
	__le64 tx_dma_error;
	__le64 rsvd5;
	
	__le64 rx_queue_disabled;
	__le64 rx_queue_empty;
	__le64 rx_queue_error;
	__le64 rx_desc_fetch_error;
	__le64 rx_desc_data_error;
	__le64 rsvd6;
	__le64 rsvd7;
	__le64 rsvd8;
	
	__le64 tx_queue_disabled;
	__le64 tx_queue_error;
	__le64 tx_desc_fetch_error;
	__le64 tx_desc_data_error;
	__le64 tx_queue_empty;
	__le64 rsvd10;
	__le64 rsvd11;
	__le64 rsvd12;

	
	__le64 tx_rdma_ucast_bytes;
	__le64 tx_rdma_ucast_packets;
	__le64 tx_rdma_mcast_bytes;
	__le64 tx_rdma_mcast_packets;
	__le64 tx_rdma_cnp_packets;
	__le64 rsvd13;
	__le64 rsvd14;
	__le64 rsvd15;

	
	__le64 rx_rdma_ucast_bytes;
	__le64 rx_rdma_ucast_packets;
	__le64 rx_rdma_mcast_bytes;
	__le64 rx_rdma_mcast_packets;
	__le64 rx_rdma_cnp_packets;
	__le64 rx_rdma_ecn_packets;
	__le64 rsvd16;
	__le64 rsvd17;

	__le64 rsvd18;
	__le64 rsvd19;
	__le64 rsvd20;
	__le64 rsvd21;
	__le64 rsvd22;
	__le64 rsvd23;
	__le64 rsvd24;
	__le64 rsvd25;

	__le64 rsvd26;
	__le64 rsvd27;
	__le64 rsvd28;
	__le64 rsvd29;
	__le64 rsvd30;
	__le64 rsvd31;
	__le64 rsvd32;
	__le64 rsvd33;

	__le64 rsvd34;
	__le64 rsvd35;
	__le64 rsvd36;
	__le64 rsvd37;
	__le64 rsvd38;
	__le64 rsvd39;
	__le64 rsvd40;
	__le64 rsvd41;

	__le64 rsvd42;
	__le64 rsvd43;
	__le64 rsvd44;
	__le64 rsvd45;
	__le64 rsvd46;
	__le64 rsvd47;
	__le64 rsvd48;
	__le64 rsvd49;

	
	__le64 rdma_req_rx_pkt_seq_err;
	__le64 rdma_req_rx_rnr_retry_err;
	__le64 rdma_req_rx_remote_access_err;
	__le64 rdma_req_rx_remote_inv_req_err;
	__le64 rdma_req_rx_remote_oper_err;
	__le64 rdma_req_rx_implied_nak_seq_err;
	__le64 rdma_req_rx_cqe_err;
	__le64 rdma_req_rx_cqe_flush_err;

	__le64 rdma_req_rx_dup_responses;
	__le64 rdma_req_rx_invalid_packets;
	__le64 rdma_req_tx_local_access_err;
	__le64 rdma_req_tx_local_oper_err;
	__le64 rdma_req_tx_memory_mgmt_err;
	__le64 rsvd52;
	__le64 rsvd53;
	__le64 rsvd54;

	
	__le64 rdma_resp_rx_dup_requests;
	__le64 rdma_resp_rx_out_of_buffer;
	__le64 rdma_resp_rx_out_of_seq_pkts;
	__le64 rdma_resp_rx_cqe_err;
	__le64 rdma_resp_rx_cqe_flush_err;
	__le64 rdma_resp_rx_local_len_err;
	__le64 rdma_resp_rx_inv_request_err;
	__le64 rdma_resp_rx_local_qp_oper_err;

	__le64 rdma_resp_rx_out_of_atomic_resource;
	__le64 rdma_resp_tx_pkt_seq_err;
	__le64 rdma_resp_tx_remote_inv_req_err;
	__le64 rdma_resp_tx_remote_access_err;
	__le64 rdma_resp_tx_remote_oper_err;
	__le64 rdma_resp_tx_rnr_retry_err;
	__le64 rsvd57;
	__le64 rsvd58;
};


struct ionic_lif_info {
	union ionic_lif_config config;
	struct ionic_lif_status status;
	struct ionic_lif_stats stats;
};

union ionic_dev_cmd {
	u32 words[16];
	struct ionic_admin_cmd cmd;
	struct ionic_nop_cmd nop;

	struct ionic_dev_identify_cmd identify;
	struct ionic_dev_init_cmd init;
	struct ionic_dev_reset_cmd reset;
	struct ionic_dev_getattr_cmd getattr;
	struct ionic_dev_setattr_cmd setattr;

	struct ionic_port_identify_cmd port_identify;
	struct ionic_port_init_cmd port_init;
	struct ionic_port_reset_cmd port_reset;
	struct ionic_port_getattr_cmd port_getattr;
	struct ionic_port_setattr_cmd port_setattr;

	struct ionic_vf_setattr_cmd vf_setattr;
	struct ionic_vf_getattr_cmd vf_getattr;
	struct ionic_vf_ctrl_cmd vf_ctrl;

	struct ionic_lif_identify_cmd lif_identify;
	struct ionic_lif_init_cmd lif_init;
	struct ionic_lif_reset_cmd lif_reset;

	struct ionic_qos_identify_cmd qos_identify;
	struct ionic_qos_init_cmd qos_init;
	struct ionic_qos_reset_cmd qos_reset;
	struct ionic_qos_clear_stats_cmd qos_clear_stats;

	struct ionic_q_identify_cmd q_identify;
	struct ionic_q_init_cmd q_init;
	struct ionic_q_control_cmd q_control;

	struct ionic_fw_download_cmd fw_download;
	struct ionic_fw_control_cmd fw_control;
};

union ionic_dev_cmd_comp {
	u32 words[4];
	u8 status;
	struct ionic_admin_comp comp;
	struct ionic_nop_comp nop;

	struct ionic_dev_identify_comp identify;
	struct ionic_dev_init_comp init;
	struct ionic_dev_reset_comp reset;
	struct ionic_dev_getattr_comp getattr;
	struct ionic_dev_setattr_comp setattr;

	struct ionic_port_identify_comp port_identify;
	struct ionic_port_init_comp port_init;
	struct ionic_port_reset_comp port_reset;
	struct ionic_port_getattr_comp port_getattr;
	struct ionic_port_setattr_comp port_setattr;

	struct ionic_vf_setattr_comp vf_setattr;
	struct ionic_vf_getattr_comp vf_getattr;
	struct ionic_vf_ctrl_comp vf_ctrl;

	struct ionic_lif_identify_comp lif_identify;
	struct ionic_lif_init_comp lif_init;
	ionic_lif_reset_comp lif_reset;

	struct ionic_qos_identify_comp qos_identify;
	ionic_qos_init_comp qos_init;
	ionic_qos_reset_comp qos_reset;

	struct ionic_q_identify_comp q_identify;
	struct ionic_q_init_comp q_init;

	ionic_fw_download_comp fw_download;
	struct ionic_fw_control_comp fw_control;
};


struct ionic_hwstamp_regs {
	u32    tick_low;
	u32    tick_high;
};


union ionic_dev_info_regs {
#define IONIC_DEVINFO_FWVERS_BUFLEN 32
#define IONIC_DEVINFO_SERIAL_BUFLEN 32
	struct {
		u32    signature;
		u8     version;
		u8     asic_type;
		u8     asic_rev;
#define IONIC_FW_STS_F_RUNNING		0x01
#define IONIC_FW_STS_F_GENERATION	0xF0
		u8     fw_status;
		u32    fw_heartbeat;
		char   fw_version[IONIC_DEVINFO_FWVERS_BUFLEN];
		char   serial_num[IONIC_DEVINFO_SERIAL_BUFLEN];
		u8     rsvd_pad1024[948];
		struct ionic_hwstamp_regs hwstamp;
	};
	u32 words[512];
};


union ionic_dev_cmd_regs {
	struct {
		u32                   doorbell;
		u32                   done;
		union ionic_dev_cmd         cmd;
		union ionic_dev_cmd_comp    comp;
		u8                    rsvd[48];
		u32                   data[478];
	} __packed;
	u32 words[512];
};


union ionic_dev_regs {
	struct {
		union ionic_dev_info_regs info;
		union ionic_dev_cmd_regs  devcmd;
	} __packed;
	__le32 words[1024];
};

union ionic_adminq_cmd {
	struct ionic_admin_cmd cmd;
	struct ionic_nop_cmd nop;
	struct ionic_q_identify_cmd q_identify;
	struct ionic_q_init_cmd q_init;
	struct ionic_q_control_cmd q_control;
	struct ionic_lif_setattr_cmd lif_setattr;
	struct ionic_lif_getattr_cmd lif_getattr;
	struct ionic_lif_setphc_cmd lif_setphc;
	struct ionic_rx_mode_set_cmd rx_mode_set;
	struct ionic_rx_filter_add_cmd rx_filter_add;
	struct ionic_rx_filter_del_cmd rx_filter_del;
	struct ionic_rdma_reset_cmd rdma_reset;
	struct ionic_rdma_queue_cmd rdma_queue;
	struct ionic_fw_download_cmd fw_download;
	struct ionic_fw_control_cmd fw_control;
};

union ionic_adminq_comp {
	struct ionic_admin_comp comp;
	struct ionic_nop_comp nop;
	struct ionic_q_identify_comp q_identify;
	struct ionic_q_init_comp q_init;
	struct ionic_lif_setattr_comp lif_setattr;
	struct ionic_lif_getattr_comp lif_getattr;
	struct ionic_admin_comp lif_setphc;
	struct ionic_rx_filter_add_comp rx_filter_add;
	struct ionic_fw_control_comp fw_control;
};

#define IONIC_BARS_MAX			6
#define IONIC_PCI_BAR_DBELL		1
#define IONIC_PCI_BAR_CMB		2

#define IONIC_BAR0_SIZE				0x8000
#define IONIC_BAR2_SIZE				0x800000

#define IONIC_BAR0_DEV_INFO_REGS_OFFSET		0x0000
#define IONIC_BAR0_DEV_CMD_REGS_OFFSET		0x0800
#define IONIC_BAR0_DEV_CMD_DATA_REGS_OFFSET	0x0c00
#define IONIC_BAR0_INTR_STATUS_OFFSET		0x1000
#define IONIC_BAR0_INTR_CTRL_OFFSET		0x2000
#define IONIC_DEV_CMD_DONE			0x00000001

#define IONIC_ASIC_TYPE_CAPRI			0


struct ionic_doorbell {
	__le16 p_index;
	u8     ring;
	u8     qid_lo;
	__le16 qid_hi;
	u16    rsvd2;
};

struct ionic_intr_status {
	u32 status[2];
};

struct ionic_notifyq_cmd {
	__le32 data;	
};

union ionic_notifyq_comp {
	struct ionic_notifyq_event event;
	struct ionic_link_change_event link_change;
	struct ionic_reset_event reset;
	struct ionic_heartbeat_event heartbeat;
	struct ionic_log_event log;
};


struct ionic_identity {
	union ionic_drv_identity drv;
	union ionic_dev_identity dev;
	union ionic_lif_identity lif;
	union ionic_port_identity port;
	union ionic_qos_identity qos;
	union ionic_q_identity txq;
};

#endif 
