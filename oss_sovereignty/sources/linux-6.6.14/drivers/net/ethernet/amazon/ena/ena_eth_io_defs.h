

#ifndef _ENA_ETH_IO_H_
#define _ENA_ETH_IO_H_

enum ena_eth_io_l3_proto_index {
	ENA_ETH_IO_L3_PROTO_UNKNOWN                 = 0,
	ENA_ETH_IO_L3_PROTO_IPV4                    = 8,
	ENA_ETH_IO_L3_PROTO_IPV6                    = 11,
	ENA_ETH_IO_L3_PROTO_FCOE                    = 21,
	ENA_ETH_IO_L3_PROTO_ROCE                    = 22,
};

enum ena_eth_io_l4_proto_index {
	ENA_ETH_IO_L4_PROTO_UNKNOWN                 = 0,
	ENA_ETH_IO_L4_PROTO_TCP                     = 12,
	ENA_ETH_IO_L4_PROTO_UDP                     = 13,
	ENA_ETH_IO_L4_PROTO_ROUTEABLE_ROCE          = 23,
};

struct ena_eth_io_tx_desc {
	
	u32 len_ctrl;

	
	u32 meta_ctrl;

	u32 buff_addr_lo;

	
	u32 buff_addr_hi_hdr_sz;
};

struct ena_eth_io_tx_meta_desc {
	
	u32 len_ctrl;

	
	u32 word1;

	
	u32 word2;

	u32 reserved;
};

struct ena_eth_io_tx_cdesc {
	
	u16 req_id;

	u8 status;

	
	u8 flags;

	u16 sub_qid;

	u16 sq_head_idx;
};

struct ena_eth_io_rx_desc {
	
	u16 length;

	
	u8 reserved2;

	
	u8 ctrl;

	u16 req_id;

	
	u16 reserved6;

	u32 buff_addr_lo;

	u16 buff_addr_hi;

	
	u16 reserved16_w3;
};


struct ena_eth_io_rx_cdesc_base {
	
	u32 status;

	u16 length;

	u16 req_id;

	
	u32 hash;

	u16 sub_qid;

	u8 offset;

	u8 reserved;
};


struct ena_eth_io_rx_cdesc_ext {
	struct ena_eth_io_rx_cdesc_base base;

	u32 buff_addr_lo;

	u16 buff_addr_hi;

	u16 reserved16;

	u32 reserved_w6;

	u32 reserved_w7;
};

struct ena_eth_io_intr_reg {
	
	u32 intr_control;
};

struct ena_eth_io_numa_node_cfg_reg {
	
	u32 numa_cfg;
};


#define ENA_ETH_IO_TX_DESC_LENGTH_MASK                      GENMASK(15, 0)
#define ENA_ETH_IO_TX_DESC_REQ_ID_HI_SHIFT                  16
#define ENA_ETH_IO_TX_DESC_REQ_ID_HI_MASK                   GENMASK(21, 16)
#define ENA_ETH_IO_TX_DESC_META_DESC_SHIFT                  23
#define ENA_ETH_IO_TX_DESC_META_DESC_MASK                   BIT(23)
#define ENA_ETH_IO_TX_DESC_PHASE_SHIFT                      24
#define ENA_ETH_IO_TX_DESC_PHASE_MASK                       BIT(24)
#define ENA_ETH_IO_TX_DESC_FIRST_SHIFT                      26
#define ENA_ETH_IO_TX_DESC_FIRST_MASK                       BIT(26)
#define ENA_ETH_IO_TX_DESC_LAST_SHIFT                       27
#define ENA_ETH_IO_TX_DESC_LAST_MASK                        BIT(27)
#define ENA_ETH_IO_TX_DESC_COMP_REQ_SHIFT                   28
#define ENA_ETH_IO_TX_DESC_COMP_REQ_MASK                    BIT(28)
#define ENA_ETH_IO_TX_DESC_L3_PROTO_IDX_MASK                GENMASK(3, 0)
#define ENA_ETH_IO_TX_DESC_DF_SHIFT                         4
#define ENA_ETH_IO_TX_DESC_DF_MASK                          BIT(4)
#define ENA_ETH_IO_TX_DESC_TSO_EN_SHIFT                     7
#define ENA_ETH_IO_TX_DESC_TSO_EN_MASK                      BIT(7)
#define ENA_ETH_IO_TX_DESC_L4_PROTO_IDX_SHIFT               8
#define ENA_ETH_IO_TX_DESC_L4_PROTO_IDX_MASK                GENMASK(12, 8)
#define ENA_ETH_IO_TX_DESC_L3_CSUM_EN_SHIFT                 13
#define ENA_ETH_IO_TX_DESC_L3_CSUM_EN_MASK                  BIT(13)
#define ENA_ETH_IO_TX_DESC_L4_CSUM_EN_SHIFT                 14
#define ENA_ETH_IO_TX_DESC_L4_CSUM_EN_MASK                  BIT(14)
#define ENA_ETH_IO_TX_DESC_ETHERNET_FCS_DIS_SHIFT           15
#define ENA_ETH_IO_TX_DESC_ETHERNET_FCS_DIS_MASK            BIT(15)
#define ENA_ETH_IO_TX_DESC_L4_CSUM_PARTIAL_SHIFT            17
#define ENA_ETH_IO_TX_DESC_L4_CSUM_PARTIAL_MASK             BIT(17)
#define ENA_ETH_IO_TX_DESC_REQ_ID_LO_SHIFT                  22
#define ENA_ETH_IO_TX_DESC_REQ_ID_LO_MASK                   GENMASK(31, 22)
#define ENA_ETH_IO_TX_DESC_ADDR_HI_MASK                     GENMASK(15, 0)
#define ENA_ETH_IO_TX_DESC_HEADER_LENGTH_SHIFT              24
#define ENA_ETH_IO_TX_DESC_HEADER_LENGTH_MASK               GENMASK(31, 24)


#define ENA_ETH_IO_TX_META_DESC_REQ_ID_LO_MASK              GENMASK(9, 0)
#define ENA_ETH_IO_TX_META_DESC_EXT_VALID_SHIFT             14
#define ENA_ETH_IO_TX_META_DESC_EXT_VALID_MASK              BIT(14)
#define ENA_ETH_IO_TX_META_DESC_MSS_HI_SHIFT                16
#define ENA_ETH_IO_TX_META_DESC_MSS_HI_MASK                 GENMASK(19, 16)
#define ENA_ETH_IO_TX_META_DESC_ETH_META_TYPE_SHIFT         20
#define ENA_ETH_IO_TX_META_DESC_ETH_META_TYPE_MASK          BIT(20)
#define ENA_ETH_IO_TX_META_DESC_META_STORE_SHIFT            21
#define ENA_ETH_IO_TX_META_DESC_META_STORE_MASK             BIT(21)
#define ENA_ETH_IO_TX_META_DESC_META_DESC_SHIFT             23
#define ENA_ETH_IO_TX_META_DESC_META_DESC_MASK              BIT(23)
#define ENA_ETH_IO_TX_META_DESC_PHASE_SHIFT                 24
#define ENA_ETH_IO_TX_META_DESC_PHASE_MASK                  BIT(24)
#define ENA_ETH_IO_TX_META_DESC_FIRST_SHIFT                 26
#define ENA_ETH_IO_TX_META_DESC_FIRST_MASK                  BIT(26)
#define ENA_ETH_IO_TX_META_DESC_LAST_SHIFT                  27
#define ENA_ETH_IO_TX_META_DESC_LAST_MASK                   BIT(27)
#define ENA_ETH_IO_TX_META_DESC_COMP_REQ_SHIFT              28
#define ENA_ETH_IO_TX_META_DESC_COMP_REQ_MASK               BIT(28)
#define ENA_ETH_IO_TX_META_DESC_REQ_ID_HI_MASK              GENMASK(5, 0)
#define ENA_ETH_IO_TX_META_DESC_L3_HDR_LEN_MASK             GENMASK(7, 0)
#define ENA_ETH_IO_TX_META_DESC_L3_HDR_OFF_SHIFT            8
#define ENA_ETH_IO_TX_META_DESC_L3_HDR_OFF_MASK             GENMASK(15, 8)
#define ENA_ETH_IO_TX_META_DESC_L4_HDR_LEN_IN_WORDS_SHIFT   16
#define ENA_ETH_IO_TX_META_DESC_L4_HDR_LEN_IN_WORDS_MASK    GENMASK(21, 16)
#define ENA_ETH_IO_TX_META_DESC_MSS_LO_SHIFT                22
#define ENA_ETH_IO_TX_META_DESC_MSS_LO_MASK                 GENMASK(31, 22)


#define ENA_ETH_IO_TX_CDESC_PHASE_MASK                      BIT(0)


#define ENA_ETH_IO_RX_DESC_PHASE_MASK                       BIT(0)
#define ENA_ETH_IO_RX_DESC_FIRST_SHIFT                      2
#define ENA_ETH_IO_RX_DESC_FIRST_MASK                       BIT(2)
#define ENA_ETH_IO_RX_DESC_LAST_SHIFT                       3
#define ENA_ETH_IO_RX_DESC_LAST_MASK                        BIT(3)
#define ENA_ETH_IO_RX_DESC_COMP_REQ_SHIFT                   4
#define ENA_ETH_IO_RX_DESC_COMP_REQ_MASK                    BIT(4)


#define ENA_ETH_IO_RX_CDESC_BASE_L3_PROTO_IDX_MASK          GENMASK(4, 0)
#define ENA_ETH_IO_RX_CDESC_BASE_SRC_VLAN_CNT_SHIFT         5
#define ENA_ETH_IO_RX_CDESC_BASE_SRC_VLAN_CNT_MASK          GENMASK(6, 5)
#define ENA_ETH_IO_RX_CDESC_BASE_L4_PROTO_IDX_SHIFT         8
#define ENA_ETH_IO_RX_CDESC_BASE_L4_PROTO_IDX_MASK          GENMASK(12, 8)
#define ENA_ETH_IO_RX_CDESC_BASE_L3_CSUM_ERR_SHIFT          13
#define ENA_ETH_IO_RX_CDESC_BASE_L3_CSUM_ERR_MASK           BIT(13)
#define ENA_ETH_IO_RX_CDESC_BASE_L4_CSUM_ERR_SHIFT          14
#define ENA_ETH_IO_RX_CDESC_BASE_L4_CSUM_ERR_MASK           BIT(14)
#define ENA_ETH_IO_RX_CDESC_BASE_IPV4_FRAG_SHIFT            15
#define ENA_ETH_IO_RX_CDESC_BASE_IPV4_FRAG_MASK             BIT(15)
#define ENA_ETH_IO_RX_CDESC_BASE_L4_CSUM_CHECKED_SHIFT      16
#define ENA_ETH_IO_RX_CDESC_BASE_L4_CSUM_CHECKED_MASK       BIT(16)
#define ENA_ETH_IO_RX_CDESC_BASE_PHASE_SHIFT                24
#define ENA_ETH_IO_RX_CDESC_BASE_PHASE_MASK                 BIT(24)
#define ENA_ETH_IO_RX_CDESC_BASE_L3_CSUM2_SHIFT             25
#define ENA_ETH_IO_RX_CDESC_BASE_L3_CSUM2_MASK              BIT(25)
#define ENA_ETH_IO_RX_CDESC_BASE_FIRST_SHIFT                26
#define ENA_ETH_IO_RX_CDESC_BASE_FIRST_MASK                 BIT(26)
#define ENA_ETH_IO_RX_CDESC_BASE_LAST_SHIFT                 27
#define ENA_ETH_IO_RX_CDESC_BASE_LAST_MASK                  BIT(27)
#define ENA_ETH_IO_RX_CDESC_BASE_BUFFER_SHIFT               30
#define ENA_ETH_IO_RX_CDESC_BASE_BUFFER_MASK                BIT(30)


#define ENA_ETH_IO_INTR_REG_RX_INTR_DELAY_MASK              GENMASK(14, 0)
#define ENA_ETH_IO_INTR_REG_TX_INTR_DELAY_SHIFT             15
#define ENA_ETH_IO_INTR_REG_TX_INTR_DELAY_MASK              GENMASK(29, 15)
#define ENA_ETH_IO_INTR_REG_INTR_UNMASK_SHIFT               30
#define ENA_ETH_IO_INTR_REG_INTR_UNMASK_MASK                BIT(30)


#define ENA_ETH_IO_NUMA_NODE_CFG_REG_NUMA_MASK              GENMASK(7, 0)
#define ENA_ETH_IO_NUMA_NODE_CFG_REG_ENABLED_SHIFT          31
#define ENA_ETH_IO_NUMA_NODE_CFG_REG_ENABLED_MASK           BIT(31)

#endif 
