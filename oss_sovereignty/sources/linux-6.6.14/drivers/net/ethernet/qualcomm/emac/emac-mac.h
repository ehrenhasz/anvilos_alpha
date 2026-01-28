




#ifndef _EMAC_HW_H_
#define _EMAC_HW_H_


#define EMAC_EMAC_WRAPPER_CSR1                                0x000000
#define EMAC_EMAC_WRAPPER_CSR2                                0x000004
#define EMAC_EMAC_WRAPPER_TX_TS_LO                            0x000104
#define EMAC_EMAC_WRAPPER_TX_TS_HI                            0x000108
#define EMAC_EMAC_WRAPPER_TX_TS_INX                           0x00010c


enum emac_dma_order {
	emac_dma_ord_in = 1,
	emac_dma_ord_enh = 2,
	emac_dma_ord_out = 4
};

enum emac_dma_req_block {
	emac_dma_req_128 = 0,
	emac_dma_req_256 = 1,
	emac_dma_req_512 = 2,
	emac_dma_req_1024 = 3,
	emac_dma_req_2048 = 4,
	emac_dma_req_4096 = 5
};


#define BITS_GET(val, lo, hi) ((le32_to_cpu(val) & GENMASK((hi), (lo))) >> lo)
#define BITS_SET(val, lo, hi, new_val) \
	val = cpu_to_le32((le32_to_cpu(val) & (~GENMASK((hi), (lo)))) |	\
		(((new_val) << (lo)) & GENMASK((hi), (lo))))


struct emac_rrd {
	u32	word[6];


#define RRD_NOR(rrd)			BITS_GET((rrd)->word[0], 16, 19)

#define RRD_SI(rrd)			BITS_GET((rrd)->word[0], 20, 31)

#define RRD_CVALN_TAG(rrd)		BITS_GET((rrd)->word[2], 0, 15)

#define RRD_PKT_SIZE(rrd)		BITS_GET((rrd)->word[3], 0, 13)

#define RRD_L4F(rrd)			BITS_GET((rrd)->word[3], 14, 14)

#define RRD_CVTAG(rrd)			BITS_GET((rrd)->word[3], 16, 16)

#define RRD_UPDT(rrd)			BITS_GET((rrd)->word[3], 31, 31)
#define RRD_UPDT_SET(rrd, val)		BITS_SET((rrd)->word[3], 31, 31, val)

#define RRD_TS_LOW(rrd)			BITS_GET((rrd)->word[4], 0, 29)

#define RRD_TS_HI(rrd)			le32_to_cpu((rrd)->word[5])
};


struct emac_tpd {
	u32				word[4];


#define TPD_BUF_LEN_SET(tpd, val)	BITS_SET((tpd)->word[0], 0, 15, val)

#define TPD_CSX_SET(tpd, val)		BITS_SET((tpd)->word[1], 8, 8, val)

#define TPD_LSO(tpd)			BITS_GET((tpd)->word[1], 12, 12)
#define TPD_LSO_SET(tpd, val)		BITS_SET((tpd)->word[1], 12, 12, val)

#define TPD_LSOV_SET(tpd, val)		BITS_SET((tpd)->word[1], 13, 13, val)

#define TPD_IPV4_SET(tpd, val)		BITS_SET((tpd)->word[1], 16, 16, val)

#define TPD_TYP_SET(tpd, val)		BITS_SET((tpd)->word[1], 17, 17, val)

#define TPD_BUFFER_ADDR_L_SET(tpd, val)	((tpd)->word[2] = cpu_to_le32(val))

#define TPD_CVLAN_TAG_SET(tpd, val)	BITS_SET((tpd)->word[3], 0, 15, val)

#define TPD_INSTC_SET(tpd, val)		BITS_SET((tpd)->word[3], 17, 17, val)

#define TPD_BUFFER_ADDR_H_SET(tpd, val)	BITS_SET((tpd)->word[3], 18, 31, val)

#define TPD_PAYLOAD_OFFSET_SET(tpd, val) BITS_SET((tpd)->word[1], 0, 7, val)

#define TPD_CXSUM_OFFSET_SET(tpd, val)	BITS_SET((tpd)->word[1], 18, 25, val)


#define TPD_TCPHDR_OFFSET_SET(tpd, val)	BITS_SET((tpd)->word[1], 0, 7, val)

#define TPD_MSS_SET(tpd, val)		BITS_SET((tpd)->word[1], 18, 30, val)

#define TPD_PKT_LEN_SET(tpd, val)	((tpd)->word[2] = cpu_to_le32(val))
};


struct emac_ring_header {
	void			*v_addr;	
	dma_addr_t		dma_addr;	
	size_t			size;		
	size_t			used;
};


struct emac_buffer {
	struct sk_buff		*skb;		
	u16			length;		
	dma_addr_t		dma_addr;	
};


struct emac_rfd_ring {
	struct emac_buffer	*rfbuff;
	u32			*v_addr;	
	dma_addr_t		dma_addr;	
	size_t			size;		
	unsigned int		count;		
	unsigned int		produce_idx;
	unsigned int		process_idx;
	unsigned int		consume_idx;	
};


struct emac_rrd_ring {
	u32			*v_addr;	
	dma_addr_t		dma_addr;	
	size_t			size;		
	unsigned int		count;		
	unsigned int		produce_idx;	
	unsigned int		consume_idx;
};


struct emac_rx_queue {
	struct net_device	*netdev;	
	struct emac_rrd_ring	rrd;
	struct emac_rfd_ring	rfd;
	struct napi_struct	napi;
	struct emac_irq		*irq;

	u32			intr;
	u32			produce_mask;
	u32			process_mask;
	u32			consume_mask;

	u16			produce_reg;
	u16			process_reg;
	u16			consume_reg;

	u8			produce_shift;
	u8			process_shft;
	u8			consume_shift;
};


struct emac_tpd_ring {
	struct emac_buffer	*tpbuff;
	u32			*v_addr;	
	dma_addr_t		dma_addr;	

	size_t			size;		
	unsigned int		count;		
	unsigned int		produce_idx;
	unsigned int		consume_idx;
	unsigned int		last_produce_idx;
};


struct emac_tx_queue {
	struct emac_tpd_ring	tpd;

	u32			produce_mask;
	u32			consume_mask;

	u16			max_packets;	
	u16			produce_reg;
	u16			consume_reg;

	u8			produce_shift;
	u8			consume_shift;
};

struct emac_adapter;

int  emac_mac_up(struct emac_adapter *adpt);
void emac_mac_down(struct emac_adapter *adpt);
void emac_mac_reset(struct emac_adapter *adpt);
void emac_mac_stop(struct emac_adapter *adpt);
void emac_mac_mode_config(struct emac_adapter *adpt);
void emac_mac_rx_process(struct emac_adapter *adpt, struct emac_rx_queue *rx_q,
			 int *num_pkts, int max_pkts);
netdev_tx_t emac_mac_tx_buf_send(struct emac_adapter *adpt,
				 struct emac_tx_queue *tx_q,
				 struct sk_buff *skb);
void emac_mac_tx_process(struct emac_adapter *adpt, struct emac_tx_queue *tx_q);
void emac_mac_rx_tx_ring_init_all(struct platform_device *pdev,
				  struct emac_adapter *adpt);
int  emac_mac_rx_tx_rings_alloc_all(struct emac_adapter *adpt);
void emac_mac_rx_tx_rings_free_all(struct emac_adapter *adpt);
void emac_mac_multicast_addr_clear(struct emac_adapter *adpt);
void emac_mac_multicast_addr_set(struct emac_adapter *adpt, u8 *addr);

#endif 
