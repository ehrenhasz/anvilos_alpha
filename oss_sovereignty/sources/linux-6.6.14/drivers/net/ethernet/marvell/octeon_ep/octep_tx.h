


#ifndef _OCTEP_TX_H_
#define _OCTEP_TX_H_

#define IQ_SEND_OK          0
#define IQ_SEND_STOP        1
#define IQ_SEND_FAILED     -1

#define TX_BUFTYPE_NONE          0
#define TX_BUFTYPE_NET           1
#define TX_BUFTYPE_NET_SG        2
#define NUM_TX_BUFTYPES          3


struct octep_tx_sglist_desc {
	u16 len[4];
	dma_addr_t dma_ptr[4];
};


#define OCTEP_SGLIST_ENTRIES_PER_PKT ((MAX_SKB_FRAGS + 1 + 3) / 4)
#define OCTEP_SGLIST_SIZE_PER_PKT \
	(OCTEP_SGLIST_ENTRIES_PER_PKT * sizeof(struct octep_tx_sglist_desc))

struct octep_tx_buffer {
	struct sk_buff *skb;
	dma_addr_t dma;
	struct octep_tx_sglist_desc *sglist;
	dma_addr_t sglist_dma;
	u8 gather;
};

#define OCTEP_IQ_TXBUFF_INFO_SIZE (sizeof(struct octep_tx_buffer))


struct octep_iface_tx_stats {
	
	u64 xscol;

	
	u64 xsdef;

	
	u64 mcol;

	
	u64 scol;

	
	u64 octs;

	
	u64 pkts;

	
	u64 hist_lt64;

	
	u64 hist_eq64;

	
	u64 hist_65to127;

	
	u64 hist_128to255;

	
	u64 hist_256to511;

	
	u64 hist_512to1023;

	
	u64 hist_1024to1518;

	
	u64 hist_gt1518;

	
	u64 bcst;

	
	u64 mcst;

	
	u64 undflw;

	
	u64 ctl;
};


struct octep_iq_stats {
	
	u64 instr_posted;

	
	u64 instr_completed;

	
	u64 instr_dropped;

	
	u64 bytes_sent;

	
	u64 sgentry_sent;

	
	u64 tx_busy;

	
	u64 restart_cnt;
};


struct octep_iq {
	u32 q_no;

	struct octep_device *octep_dev;
	struct net_device *netdev;
	struct device *dev;
	struct netdev_queue *netdev_q;

	
	u16 host_write_index;

	
	u16 octep_read_index;

	
	u16 flush_index;

	
	struct octep_iq_stats stats;

	
	atomic_t instr_pending;

	
	struct octep_tx_desc_hw *desc_ring;

	
	dma_addr_t desc_ring_dma;

	
	struct octep_tx_buffer *buff_info;

	
	struct octep_tx_sglist_desc *sglist;

	
	dma_addr_t sglist_dma;

	
	u8 __iomem *doorbell_reg;

	
	u8 __iomem *inst_cnt_reg;

	
	u8 __iomem *intr_lvl_reg;

	
	u32 max_count;
	u32 ring_size_mask;

	u32 pkt_in_done;
	u32 pkts_processed;

	u32 status;

	
	u32 fill_cnt;

	
	u32 fill_threshold;
};


struct octep_instr_hdr {
	
	u64 tlen:16;

	
	u64 rsvd:20;

	
	u64 pkind:6;

	
	u64 fsz:6;

	
	u64 gsz:14;

	
	u64 gather:1;

	
	u64 reserved3:1;
};


struct octep_instr_resp_hdr {
	
	u64 rid:16;

	
	u64 pcie_port:3;

	
	u64 scatter:1;

	
	u64 rlenssz:14;

	
	u64 dport:6;

	
	u64 param:8;

	
	u64 opcode:16;
};


struct octep_tx_desc_hw {
	
	u64 dptr;

	
	union {
		struct octep_instr_hdr ih;
		u64 ih64;
	};

	
	u64 rptr;

	
	struct octep_instr_resp_hdr irh;

	
	u64 exhdr[4];
};

#define OCTEP_IQ_DESC_SIZE (sizeof(struct octep_tx_desc_hw))
#endif 
