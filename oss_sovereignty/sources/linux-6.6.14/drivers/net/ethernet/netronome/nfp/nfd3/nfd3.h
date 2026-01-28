


#ifndef _NFP_DP_NFD3_H_
#define _NFP_DP_NFD3_H_

struct sk_buff;
struct net_device;



#define NFD3_DESC_TX_EOP		BIT(7)
#define NFD3_DESC_TX_OFFSET_MASK	GENMASK(6, 0)
#define NFD3_DESC_TX_MSS_MASK		GENMASK(13, 0)


#define NFD3_DESC_TX_CSUM		BIT(7)
#define NFD3_DESC_TX_IP4_CSUM		BIT(6)
#define NFD3_DESC_TX_TCP_CSUM		BIT(5)
#define NFD3_DESC_TX_UDP_CSUM		BIT(4)
#define NFD3_DESC_TX_VLAN		BIT(3)
#define NFD3_DESC_TX_LSO		BIT(2)
#define NFD3_DESC_TX_ENCAP		BIT(1)
#define NFD3_DESC_TX_O_IP4_CSUM	BIT(0)

struct nfp_nfd3_tx_desc {
	union {
		struct {
			u8 dma_addr_hi; 
			__le16 dma_len;	
			u8 offset_eop;	
			__le32 dma_addr_lo; 

			__le16 mss;	
			u8 lso_hdrlen;	
			u8 flags;	
			union {
				struct {
					u8 l3_offset; 
					u8 l4_offset; 
				};
				__le16 vlan; 
			};
			__le16 data_len; 
		} __packed;
		__le32 vals[4];
		__le64 vals8[2];
	};
};


struct nfp_nfd3_tx_buf {
	union {
		struct sk_buff *skb;
		void *frag;
		struct xdp_buff *xdp;
	};
	dma_addr_t dma_addr;
	union {
		struct {
			short int fidx;
			u16 pkt_cnt;
		};
		struct {
			bool is_xsk_tx;
		};
	};
	u32 real_len;
};

void
nfp_nfd3_rx_csum(const struct nfp_net_dp *dp, struct nfp_net_r_vector *r_vec,
		 const struct nfp_net_rx_desc *rxd,
		 const struct nfp_meta_parsed *meta, struct sk_buff *skb);
bool
nfp_nfd3_parse_meta(struct net_device *netdev, struct nfp_meta_parsed *meta,
		    void *data, void *pkt, unsigned int pkt_len, int meta_len);
void nfp_nfd3_tx_complete(struct nfp_net_tx_ring *tx_ring, int budget);
int nfp_nfd3_poll(struct napi_struct *napi, int budget);
netdev_tx_t nfp_nfd3_tx(struct sk_buff *skb, struct net_device *netdev);
bool
nfp_nfd3_ctrl_tx_one(struct nfp_net *nn, struct nfp_net_r_vector *r_vec,
		     struct sk_buff *skb, bool old);
void nfp_nfd3_ctrl_poll(struct tasklet_struct *t);
void nfp_nfd3_rx_ring_fill_freelist(struct nfp_net_dp *dp,
				    struct nfp_net_rx_ring *rx_ring);
void nfp_nfd3_xsk_tx_free(struct nfp_nfd3_tx_buf *txbuf);
int nfp_nfd3_xsk_poll(struct napi_struct *napi, int budget);

#ifndef CONFIG_NFP_NET_IPSEC
static inline void nfp_nfd3_ipsec_tx(struct nfp_nfd3_tx_desc *txd, struct sk_buff *skb)
{
}
#else
void nfp_nfd3_ipsec_tx(struct nfp_nfd3_tx_desc *txd, struct sk_buff *skb);
#endif

#endif
