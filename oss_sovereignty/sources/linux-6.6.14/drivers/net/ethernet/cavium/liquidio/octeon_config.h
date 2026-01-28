


#ifndef __OCTEON_CONFIG_H__
#define __OCTEON_CONFIG_H__






#define   MAX_OCTEON_NICIF             128
#define   MAX_OCTEON_DEVICES           MAX_OCTEON_NICIF
#define   MAX_OCTEON_LINKS	       MAX_OCTEON_NICIF
#define   MAX_OCTEON_MULTICAST_ADDR    32

#define   MAX_OCTEON_FILL_COUNT        8


#define   CN6XXX_MAX_INPUT_QUEUES      32
#define   CN6XXX_MAX_IQ_DESCRIPTORS    2048
#define   CN6XXX_DB_MIN                1
#define   CN6XXX_DB_MAX                8
#define   CN6XXX_DB_TIMEOUT            1


#define   CN6XXX_MAX_OUTPUT_QUEUES     32
#define   CN6XXX_MAX_OQ_DESCRIPTORS    2048
#define   CN6XXX_OQ_BUF_SIZE           1664
#define   CN6XXX_OQ_PKTSPER_INTR       ((CN6XXX_MAX_OQ_DESCRIPTORS < 512) ? \
					(CN6XXX_MAX_OQ_DESCRIPTORS / 4) : 128)
#define   CN6XXX_OQ_REFIL_THRESHOLD    ((CN6XXX_MAX_OQ_DESCRIPTORS < 512) ? \
					(CN6XXX_MAX_OQ_DESCRIPTORS / 4) : 128)

#define   CN6XXX_OQ_INTR_PKT           64
#define   CN6XXX_OQ_INTR_TIME          100
#define   DEFAULT_NUM_NIC_PORTS_66XX   2
#define   DEFAULT_NUM_NIC_PORTS_68XX   4
#define   DEFAULT_NUM_NIC_PORTS_68XX_210NV  2


#define   CN23XX_MAX_VFS_PER_PF_PASS_1_0 8
#define   CN23XX_MAX_VFS_PER_PF_PASS_1_1 31
#define   CN23XX_MAX_VFS_PER_PF          63
#define   CN23XX_MAX_RINGS_PER_VF        8

#define   CN23XX_MAX_RINGS_PER_PF_PASS_1_0 12
#define   CN23XX_MAX_RINGS_PER_PF_PASS_1_1 32
#define   CN23XX_MAX_RINGS_PER_PF          64
#define   CN23XX_MAX_RINGS_PER_VF          8

#define   CN23XX_MAX_INPUT_QUEUES	CN23XX_MAX_RINGS_PER_PF
#define   CN23XX_MAX_IQ_DESCRIPTORS	2048
#define   CN23XX_DEFAULT_IQ_DESCRIPTORS	512
#define   CN23XX_MIN_IQ_DESCRIPTORS	128
#define   CN23XX_DB_MIN                 1
#define   CN23XX_DB_MAX                 8
#define   CN23XX_DB_TIMEOUT             1

#define   CN23XX_MAX_OUTPUT_QUEUES	CN23XX_MAX_RINGS_PER_PF
#define   CN23XX_MAX_OQ_DESCRIPTORS	2048
#define   CN23XX_DEFAULT_OQ_DESCRIPTORS	512
#define   CN23XX_MIN_OQ_DESCRIPTORS	128
#define   CN23XX_OQ_BUF_SIZE		1664
#define   CN23XX_OQ_PKTSPER_INTR	128

#define   CN23XX_OQ_REFIL_THRESHOLD	16

#define   CN23XX_OQ_INTR_PKT		64
#define   CN23XX_OQ_INTR_TIME		100
#define   DEFAULT_NUM_NIC_PORTS_23XX	1

#define   CN23XX_CFG_IO_QUEUES		CN23XX_MAX_RINGS_PER_PF

#define   CN23XX_MAX_MACS		4

#define   CN23XX_DEF_IQ_INTR_THRESHOLD	32
#define   CN23XX_DEF_IQ_INTR_BYTE_THRESHOLD   (64 * 1024)

#define   CN6XXX_CFG_IO_QUEUES         32
#define   OCTEON_32BYTE_INSTR          32
#define   OCTEON_64BYTE_INSTR          64
#define   OCTEON_MAX_BASE_IOQ          4

#define   OCTEON_DMA_INTR_PKT          64
#define   OCTEON_DMA_INTR_TIME         1000

#define MAX_TXQS_PER_INTF  8
#define MAX_RXQS_PER_INTF  8
#define DEF_TXQS_PER_INTF  4
#define DEF_RXQS_PER_INTF  4

#define INVALID_IOQ_NO          0xff

#define   DEFAULT_POW_GRP       0


#define CFG_GET_IQ_CFG(cfg)                      ((cfg)->iq)
#define CFG_GET_IQ_MAX_Q(cfg)                    ((cfg)->iq.max_iqs)
#define CFG_GET_IQ_PENDING_LIST_SIZE(cfg)        ((cfg)->iq.pending_list_size)
#define CFG_GET_IQ_INSTR_TYPE(cfg)               ((cfg)->iq.instr_type)
#define CFG_GET_IQ_DB_MIN(cfg)                   ((cfg)->iq.db_min)
#define CFG_GET_IQ_DB_TIMEOUT(cfg)               ((cfg)->iq.db_timeout)

#define CFG_GET_IQ_INTR_PKT(cfg)                 ((cfg)->iq.iq_intr_pkt)
#define CFG_SET_IQ_INTR_PKT(cfg, val)            (cfg)->iq.iq_intr_pkt = val

#define CFG_GET_OQ_MAX_Q(cfg)                    ((cfg)->oq.max_oqs)
#define CFG_GET_OQ_PKTS_PER_INTR(cfg)            ((cfg)->oq.pkts_per_intr)
#define CFG_GET_OQ_REFILL_THRESHOLD(cfg)         ((cfg)->oq.refill_threshold)
#define CFG_GET_OQ_INTR_PKT(cfg)                 ((cfg)->oq.oq_intr_pkt)
#define CFG_GET_OQ_INTR_TIME(cfg)                ((cfg)->oq.oq_intr_time)
#define CFG_SET_OQ_INTR_PKT(cfg, val)            (cfg)->oq.oq_intr_pkt = val
#define CFG_SET_OQ_INTR_TIME(cfg, val)           (cfg)->oq.oq_intr_time = val

#define CFG_GET_DMA_INTR_PKT(cfg)                ((cfg)->dma.dma_intr_pkt)
#define CFG_GET_DMA_INTR_TIME(cfg)               ((cfg)->dma.dma_intr_time)
#define CFG_GET_NUM_NIC_PORTS(cfg)               ((cfg)->num_nic_ports)
#define CFG_GET_NUM_DEF_TX_DESCS(cfg)            ((cfg)->num_def_tx_descs)
#define CFG_GET_NUM_DEF_RX_DESCS(cfg)            ((cfg)->num_def_rx_descs)
#define CFG_GET_DEF_RX_BUF_SIZE(cfg)             ((cfg)->def_rx_buf_size)

#define CFG_GET_MAX_TXQS_NIC_IF(cfg, idx) \
				((cfg)->nic_if_cfg[idx].max_txqs)
#define CFG_GET_NUM_TXQS_NIC_IF(cfg, idx) \
				((cfg)->nic_if_cfg[idx].num_txqs)
#define CFG_GET_MAX_RXQS_NIC_IF(cfg, idx) \
				((cfg)->nic_if_cfg[idx].max_rxqs)
#define CFG_GET_NUM_RXQS_NIC_IF(cfg, idx) \
				((cfg)->nic_if_cfg[idx].num_rxqs)
#define CFG_GET_NUM_RX_DESCS_NIC_IF(cfg, idx) \
				((cfg)->nic_if_cfg[idx].num_rx_descs)
#define CFG_GET_NUM_TX_DESCS_NIC_IF(cfg, idx) \
				((cfg)->nic_if_cfg[idx].num_tx_descs)
#define CFG_GET_NUM_RX_BUF_SIZE_NIC_IF(cfg, idx) \
				((cfg)->nic_if_cfg[idx].rx_buf_size)
#define CFG_GET_BASE_QUE_NIC_IF(cfg, idx) \
				((cfg)->nic_if_cfg[idx].base_queue)
#define CFG_GET_GMXID_NIC_IF(cfg, idx) \
				((cfg)->nic_if_cfg[idx].gmx_port_id)

#define CFG_GET_CTRL_Q_GRP(cfg)                  ((cfg)->misc.ctrlq_grp)
#define CFG_GET_HOST_LINK_QUERY_INTERVAL(cfg) \
				((cfg)->misc.host_link_query_interval)
#define CFG_GET_OCT_LINK_QUERY_INTERVAL(cfg) \
				((cfg)->misc.oct_link_query_interval)
#define CFG_GET_IS_SLI_BP_ON(cfg)                ((cfg)->misc.enable_sli_oq_bp)

#define CFG_SET_NUM_RX_DESCS_NIC_IF(cfg, idx, value) \
				((cfg)->nic_if_cfg[idx].num_rx_descs = value)
#define CFG_SET_NUM_TX_DESCS_NIC_IF(cfg, idx, value) \
				((cfg)->nic_if_cfg[idx].num_tx_descs = value)


#define MAX_IOQS_PER_NICIF              64

enum lio_card_type {
	LIO_210SV = 0, 
	LIO_210NV,     
	LIO_410NV,     
	LIO_23XX       
};

#define LIO_210SV_NAME "210sv"
#define LIO_210NV_NAME "210nv"
#define LIO_410NV_NAME "410nv"
#define LIO_23XX_NAME  "23xx"


struct octeon_iq_config {
#ifdef __BIG_ENDIAN_BITFIELD
	u64 reserved:16;

	
	u64 iq_intr_pkt:16;

	
	u64 db_timeout:16;

	
	u64 db_min:8;

	
	u64 instr_type:32;

	
	u64 pending_list_size:32;

	
	u64 max_iqs:8;
#else
	
	u64 max_iqs:8;

	
	u64 pending_list_size:32;

	
	u64 instr_type:32;

	
	u64 db_min:8;

	
	u64 db_timeout:16;

	
	u64 iq_intr_pkt:16;

	u64 reserved:16;
#endif
};


struct octeon_oq_config {
#ifdef __BIG_ENDIAN_BITFIELD
	u64 reserved:16;

	u64 pkts_per_intr:16;

	
	u64 oq_intr_time:16;

	
	u64 oq_intr_pkt:16;

	
	u64 refill_threshold:16;

	
	u64 max_oqs:8;

#else
	
	u64 max_oqs:8;

	
	u64 refill_threshold:16;

	
	u64 oq_intr_pkt:16;

	
	u64 oq_intr_time:16;

	u64 pkts_per_intr:16;

	u64 reserved:16;
#endif

};


struct octeon_nic_if_config {
#ifdef __BIG_ENDIAN_BITFIELD
	u64 reserved:56;

	u64 base_queue:16;

	u64 gmx_port_id:8;

	
	u64 rx_buf_size:16;

	
	u64 num_tx_descs:16;

	
	u64 num_rx_descs:16;

	
	u64 num_rxqs:16;

	
	u64 max_rxqs:16;

	
	u64 num_txqs:16;

	
	u64 max_txqs:16;
#else
	
	u64 max_txqs:16;

	
	u64 num_txqs:16;

	
	u64 max_rxqs:16;

	
	u64 num_rxqs:16;

	
	u64 num_rx_descs:16;

	
	u64 num_tx_descs:16;

	
	u64 rx_buf_size:16;

	u64 gmx_port_id:8;

	u64 base_queue:16;

	u64 reserved:56;
#endif

};



struct octeon_misc_config {
#ifdef __BIG_ENDIAN_BITFIELD
	
	u64 host_link_query_interval:32;
	
	u64 oct_link_query_interval:32;

	u64 enable_sli_oq_bp:1;
	
	u64 ctrlq_grp:4;
#else
	
	u64 ctrlq_grp:4;
	
	u64 enable_sli_oq_bp:1;
	
	u64 oct_link_query_interval:32;
	
	u64 host_link_query_interval:32;
#endif
};


struct octeon_config {
	u16 card_type;
	char *card_name;

	
	struct octeon_iq_config iq;

	
	struct octeon_oq_config oq;

	
	struct octeon_nic_if_config nic_if_cfg[MAX_OCTEON_NICIF];

	
	struct octeon_misc_config misc;

	int num_nic_ports;

	int num_def_tx_descs;

	
	int num_def_rx_descs;

	int def_rx_buf_size;

};



#define  BAR1_INDEX_DYNAMIC_MAP          2
#define  BAR1_INDEX_STATIC_MAP          15
#define  OCTEON_BAR1_ENTRY_SIZE         (4 * 1024 * 1024)

#define  MAX_BAR1_IOREMAP_SIZE  (16 * OCTEON_BAR1_ENTRY_SIZE)


#define MAX_RESPONSE_LISTS           6


#define OPCODE_MASK_BITS             6


#define OCTEON_OPCODE_MASK           0x3f


#define DISPATCH_LIST_SIZE                      BIT(OPCODE_MASK_BITS)


#define MAX_OCTEON_INSTR_QUEUES(oct)		\
		(OCTEON_CN23XX_PF(oct) ? CN23XX_MAX_INPUT_QUEUES : \
					CN6XXX_MAX_INPUT_QUEUES)


#define MAX_OCTEON_OUTPUT_QUEUES(oct)		\
		(OCTEON_CN23XX_PF(oct) ? CN23XX_MAX_OUTPUT_QUEUES : \
					CN6XXX_MAX_OUTPUT_QUEUES)

#define MAX_POSSIBLE_OCTEON_INSTR_QUEUES	CN23XX_MAX_INPUT_QUEUES
#define MAX_POSSIBLE_OCTEON_OUTPUT_QUEUES	CN23XX_MAX_OUTPUT_QUEUES

#define MAX_POSSIBLE_VFS			64

#endif 
